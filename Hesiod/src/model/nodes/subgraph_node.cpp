/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "hesiod/model/nodes/subgraph_node.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/boundary_nodes.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

SubGraphNode::SubGraphNode(const std::string                &label,
                           std::weak_ptr<GraphConfig>        parent_config,
                           const std::shared_ptr<GraphNode> &subgraph)
    : BaseNode(label, parent_config), hmap::CoordFrame(), subgraph(subgraph)
{
  Logger::log()->trace("SubGraphNode::SubGraphNode: label: {}", label);

  if (!this->subgraph)
  {
    auto cfg_ptr = parent_config.lock();
    this->subgraph = std::make_shared<GraphNode>(label + "_graph", cfg_ptr);
  }

  this->auto_bind_ports();
}

std::shared_ptr<GraphNode> SubGraphNode::get_subgraph() const { return this->subgraph; }

void SubGraphNode::set_subgraph(const std::shared_ptr<GraphNode> &new_subgraph)
{
  this->subgraph = new_subgraph;
  this->auto_bind_ports();
}

void SubGraphNode::auto_bind_ports()
{
  if (!this->subgraph)
    return;

  // 1. Collect desired slots from child graph boundary nodes using their node ID
  std::vector<std::pair<std::string, std::shared_ptr<gnode::ISubGraphInputNode>>> desired_inputs;
  std::vector<std::pair<std::string, std::shared_ptr<gnode::ISubGraphOutputNode>>> desired_outputs;
  std::unordered_set<std::string> desired_slot_names;

  for (const auto &[inner_node_id, node_ptr] : this->subgraph->get_nodes())
  {
    auto input_node = std::dynamic_pointer_cast<gnode::ISubGraphInputNode>(node_ptr);
    if (input_node)
    {
      const std::string port_name = inner_node_id;
      desired_inputs.push_back({port_name, input_node});
      desired_slot_names.insert(port_name);
    }

    auto output_node = std::dynamic_pointer_cast<gnode::ISubGraphOutputNode>(node_ptr);
    if (output_node)
    {
      const std::string port_name = inner_node_id;
      desired_outputs.push_back({port_name, output_node});
      desired_slot_names.insert(port_name);
    }
  }

  // 2. Remove ports that no longer exist in the child graph
  // If this node is part of a parent graph, also remove parent links connected to removed ports
  gnode::Graph *parent_graph = this->get_p_graph();

  for (int i = this->get_nports() - 1; i >= 0; --i)
  {
    const std::string port_label = this->get_port_label(i);
    if (desired_slot_names.find(port_label) == desired_slot_names.end())
    {
      if (parent_graph)
      {
        for (const auto &link : parent_graph->get_link_views(this->get_id()))
        {
          if ((link.from == this->get_id() && link.port_label_from == port_label) ||
              (link.to == this->get_id() && link.port_label_to == port_label))
          {
            parent_graph->remove_link(link.from,
                                      link.port_label_from,
                                      link.to,
                                      link.port_label_to);
          }
        }
      }

      this->remove_port(i);
    }
  }

  // 3. Update bindings and add newly created boundary ports
  this->inputs.clear();
  this->outputs.clear();

  for (auto &[port_name, input_node] : desired_inputs)
  {
    if (!this->has_port(port_name))
      this->expose_input<hmap::VirtualArray>(input_node, port_name);
    else
      this->inputs.push_back({port_name, input_node});
  }

  for (auto &[port_name, output_node] : desired_outputs)
  {
    if (!this->has_port(port_name))
      this->expose_output<hmap::VirtualArray>(output_node, port_name);
    else
      this->outputs.push_back({port_name, output_node});
  }
}

void SubGraphNode::compute()
{
  Logger::log()->trace("SubGraphNode::compute: label: {}", this->get_label());

  // 1. Pull values from parent graph into child NodeIn boundary nodes
  for (auto &[port_name, in_node] : this->inputs)
  {
    if (in_node)
      in_node->pull_from_parent(this, port_name);
  }

  // 2. Evaluate child graph synchronously inside parent worker thread pass
  if (this->subgraph)
  {
    // Save worker and set to nullptr to ensure synchronous evaluation
    gnode::GraphWorker *saved_worker = this->subgraph->get_p_worker();
    this->subgraph->set_p_worker(nullptr);

    try
    {
      this->subgraph->update();
    }
    catch (...)
    {
      this->subgraph->set_p_worker(saved_worker);
      throw;
    }

    this->subgraph->set_p_worker(saved_worker);
  }

  // 3. Push values from child NodeOut boundary nodes to parent graph
  this->sync_outputs();
}

void SubGraphNode::sync_outputs()
{
  for (auto &[port_name, out_node] : this->outputs)
  {
    if (out_node)
      out_node->push_to_parent(this, port_name);
  }
}

void SubGraphNode::json_from(nlohmann::json const &json)
{
  BaseNode::json_from(json);

  // CoordFrame
  std::vector<float> vo = {};
  std::vector<float> vs = {};

  json_safe_get(json, "origin", vo);
  json_safe_get(json, "size", vs);

  if (vo.size() >= 2)
    this->set_origin(glm::vec2(vo[0], vo[1]));
  if (vs.size() >= 2)
    this->set_size(glm::vec2(vs[0], vs[1]));

  float rotation_angle = 0.f;
  json_safe_get(json, "rotation_angle", rotation_angle);
  this->set_rotation_angle(rotation_angle);

  // Subgraph
  if (json.contains("subgraph"))
  {
    if (!this->subgraph)
    {
      auto cfg = std::make_shared<GraphConfig>();
      this->subgraph = std::make_shared<GraphNode>(this->get_id() + "_inner", cfg);
    }
    this->subgraph->json_from(json["subgraph"]);
    this->auto_bind_ports();
  }
}

nlohmann::json SubGraphNode::json_to() const
{
  nlohmann::json json = BaseNode::json_to();

  // CoordFrame
  json["origin"] = {this->get_origin().x, this->get_origin().y};
  json["size"] = {this->get_size().x, this->get_size().y};
  json["rotation_angle"] = this->get_rotation_angle();

  // Subgraph
  if (this->subgraph)
    json["subgraph"] = this->subgraph->json_to();

  return json;
}

} // namespace hesiod
