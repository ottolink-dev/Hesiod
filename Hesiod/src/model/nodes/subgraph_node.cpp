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

  this->inputs.clear();
  this->outputs.clear();

  for (const auto &[_, node_ptr] : this->subgraph->get_nodes())
  {
    auto input_node = std::dynamic_pointer_cast<gnode::ISubGraphInputNode>(node_ptr);
    if (input_node)
    {
      std::string slot_name = input_node->get_slot_name();
      if (!this->has_port(slot_name))
      {
        this->expose_input<hmap::VirtualArray>(input_node);
      }
      else
      {
        this->inputs.push_back({slot_name, input_node});
      }
    }

    auto output_node = std::dynamic_pointer_cast<gnode::ISubGraphOutputNode>(node_ptr);
    if (output_node)
    {
      std::string slot_name = output_node->get_slot_name();
      if (!this->has_port(slot_name))
      {
        this->expose_output<hmap::VirtualArray>(output_node);
      }
      else
      {
        this->outputs.push_back({slot_name, output_node});
      }
    }
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
