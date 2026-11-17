/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <unordered_set>

#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/subgraph_node.hpp"

namespace hesiod
{

SubGraphNode::SubGraphNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
  auto child_cfg = std::make_shared<GraphConfig>();
  if (auto parent_cfg = config.lock())
    *child_cfg = *parent_cfg;

  this->subgraph = std::make_shared<GraphNode>("subgraph", child_cfg);
  this->subgraph->update_finished = [this]()
  {
    if (auto *parent_gno = dynamic_cast<GraphNode *>(this->get_p_graph()))
      parent_gno->update(this->get_id());
  };
}

GraphNode *SubGraphNode::get_subgraph() const { return this->subgraph.get(); }

std::shared_ptr<GraphNode> SubGraphNode::get_subgraph_shared() const
{
  return this->subgraph;
}

void SubGraphNode::set_subgraph(std::shared_ptr<GraphNode> new_subgraph)
{
  this->inputs.clear();
  this->outputs.clear();
  this->subgraph = std::move(new_subgraph);
  if (this->subgraph)
  {
    this->subgraph->update_finished = [this]()
    {
      if (auto *parent_gno = dynamic_cast<GraphNode *>(this->get_p_graph()))
        parent_gno->update(this->get_id());
    };
  }
  this->auto_bind_ports();
}

void SubGraphNode::auto_bind_ports()
{
  if (!this->subgraph)
    return;

  // 1. Snapshot incident links in parent graph by port label before mutating ports
  struct LinkSnapshot
  {
    std::string from;
    std::string port_label_from;
    std::string to;
    std::string port_label_to;
  };

  std::vector<LinkSnapshot> incident_links;
  gnode::Graph             *parent_graph = this->get_p_graph();

  if (parent_graph)
  {
    const std::string my_id = this->get_id();
    for (const auto &link : parent_graph->get_links())
    {
      if (link.from == my_id || link.to == my_id)
      {
        gnode::Node *p_from = parent_graph->get_node(link.from);
        gnode::Node *p_to = parent_graph->get_node(link.to);
        if (p_from && p_to)
        {
          incident_links.push_back({link.from,
                                    p_from->get_port_label(link.port_from),
                                    link.to,
                                    p_to->get_port_label(link.port_to)});
        }
      }
    }

    for (const auto &snap : incident_links)
      parent_graph->remove_link(snap.from,
                                snap.port_label_from,
                                snap.to,
                                snap.port_label_to);
  }

  // 2. Collect active boundary nodes and update ports
  std::unordered_set<std::string>              active_inputs;
  std::unordered_set<std::string>              active_outputs;
  std::unordered_set<std::string>              used_labels;
  std::unordered_map<std::string, std::string> renames;

  auto find_old_input_name = [this](const gnode::ISubGraphInputNode *ptr) -> std::string
  {
    for (const auto &b : this->inputs)
      if (b.node.get() == ptr)
        return b.name;
    return {};
  };

  auto find_old_output_name = [this](const gnode::ISubGraphOutputNode *ptr) -> std::string
  {
    for (const auto &b : this->outputs)
      if (b.node.get() == ptr)
        return b.name;
    return {};
  };

  auto make_unique_label = [&used_labels](const std::string &base)
  {
    std::string candidate = base.empty() ? "port" : base;
    if (!used_labels.contains(candidate))
    {
      used_labels.insert(candidate);
      return candidate;
    }
    int idx = 1;
    while (used_labels.contains(candidate + "_" + std::to_string(idx)))
      ++idx;
    candidate += "_" + std::to_string(idx);
    used_labels.insert(candidate);
    return candidate;
  };

  for (const auto &[_, node_ptr] : this->subgraph->get_nodes())
  {
    if (auto input_node = std::dynamic_pointer_cast<gnode::ISubGraphInputNode>(node_ptr))
    {
      auto *as_node = dynamic_cast<gnode::Node *>(input_node.get());
      if (as_node && as_node->get_nports() > 0)
      {
        const std::string slot_name = make_unique_label(input_node->get_slot_name());
        active_inputs.insert(slot_name);

        const std::string old_name = find_old_input_name(input_node.get());
        if (!old_name.empty() && old_name != slot_name)
          renames[old_name] = slot_name;

        if (!this->has_port(slot_name))
        {
          input_node->register_on_parent(this, slot_name);
          this->inputs.push_back({slot_name, input_node});
        }
      }
    }

    if (auto output_node = std::dynamic_pointer_cast<gnode::ISubGraphOutputNode>(
            node_ptr))
    {
      auto *as_node = dynamic_cast<gnode::Node *>(output_node.get());
      if (as_node && as_node->get_nports() > 0)
      {
        const std::string slot_name = make_unique_label(output_node->get_slot_name());
        active_outputs.insert(slot_name);

        const std::string old_name = find_old_output_name(output_node.get());
        if (!old_name.empty() && old_name != slot_name)
          renames[old_name] = slot_name;

        if (!this->has_port(slot_name))
        {
          output_node->register_on_parent(this, slot_name);
          this->outputs.push_back({slot_name, output_node});
        }
      }
    }
  }

  std::erase_if(this->inputs,
                [this, &active_inputs](const InputBinding &binding)
                {
                  if (!active_inputs.contains(binding.name))
                  {
                    this->remove_port(binding.name);
                    return true;
                  }
                  return false;
                });

  std::erase_if(this->outputs,
                [this, &active_outputs](const OutputBinding &binding)
                {
                  if (!active_outputs.contains(binding.name))
                  {
                    this->remove_port(binding.name);
                    return true;
                  }
                  return false;
                });

  // 3. Reconcile snapshotted links in parent graph
  if (parent_graph)
  {
    const std::string my_id = this->get_id();
    for (const auto &snap : incident_links)
    {
      gnode::Node *p_from = parent_graph->get_node(snap.from);
      gnode::Node *p_to = parent_graph->get_node(snap.to);
      if (!p_from || !p_to)
        continue;

      std::string port_from = snap.port_label_from;
      if (snap.from == my_id)
        if (auto it = renames.find(port_from); it != renames.end())
          port_from = it->second;

      std::string port_to = snap.port_label_to;
      if (snap.to == my_id)
        if (auto it = renames.find(port_to); it != renames.end())
          port_to = it->second;

      const int out_idx = p_from->get_port_index(port_from);
      const int in_idx = p_to->get_port_index(port_to);

      bool can_rebind = (out_idx >= 0 && in_idx >= 0 &&
                         p_from->get_port_type(port_from) == gnode::PortType::OUT &&
                         p_to->get_port_type(port_to) == gnode::PortType::IN &&
                         p_from->get_data_type(out_idx) == p_to->get_data_type(in_idx));

      if (can_rebind)
        parent_graph->new_link(snap.from, out_idx, snap.to, in_idx);
      else
        p_to->is_dirty = true;
    }
  }

  this->pending_port_renames = std::move(renames);

  if (this->ports_updated)
    this->ports_updated();
}

void SubGraphNode::compute_subgraph()
{
  if (!this->subgraph)
    return;

  for (auto &[port_name, in_node] : this->inputs)
  {
    if (in_node)
      in_node->pull_from_parent(this, port_name);
  }

  // Evaluate child graph synchronously on current thread (GraphWorker manages
  // the outer pass)
  this->subgraph->gnode::Graph::update();

  for (auto &[port_name, out_node] : this->outputs)
  {
    if (out_node)
      out_node->push_to_parent(this, port_name);
  }
}

void SubGraphNode::propagate_config_change()
{
  if (!this->syncing_config && this->val<bool>("inherit_config") && this->subgraph)
  {
    this->syncing_config = true;
    this->subgraph->change_config_values(*this->get_config_ref());
    this->syncing_config = false;
  }

  BaseNode::propagate_config_change();
}

void SubGraphNode::on_pre_async_compute()
{
  BaseNode::on_pre_async_compute();

  if (!this->subgraph)
    return;

  for (const auto &[_, node_ptr] : this->subgraph->get_nodes())
  {
    if (node_ptr)
      node_ptr->on_pre_async_compute();
  }
}

void SubGraphNode::on_async_compute_cancelled()
{
  BaseNode::on_async_compute_cancelled();

  if (!this->subgraph)
    return;

  for (const auto &[_, node_ptr] : this->subgraph->get_nodes())
  {
    if (node_ptr)
      node_ptr->on_async_compute_cancelled();
  }
}

void SubGraphNode::json_from(nlohmann::json const &json)
{
  BaseNode::json_from(json);

  if (json.contains("subgraph") && json["subgraph"].is_object())
  {
    if (!this->subgraph)
      this->subgraph = std::make_shared<GraphNode>("subgraph",
                                                   std::make_shared<GraphConfig>());
    this->subgraph->json_from(json["subgraph"]);
  }

  if (json.contains("subgraph_ui") && json["subgraph_ui"].is_object())
    this->pending_subgraph_ui_json = json["subgraph_ui"];

  this->auto_bind_ports();
}

nlohmann::json SubGraphNode::json_to() const
{
  nlohmann::json json = BaseNode::json_to();

  if (this->subgraph)
    json["subgraph"] = this->subgraph->json_to();

  if (this->subgraph_ui_provider)
  {
    nlohmann::json ui_json = this->subgraph_ui_provider();
    if (!ui_json.is_null() && !ui_json.empty())
      json["subgraph_ui"] = ui_json;
  }
  else if (!this->pending_subgraph_ui_json.is_null() &&
           !this->pending_subgraph_ui_json.empty())
  {
    json["subgraph_ui"] = this->pending_subgraph_ui_json;
  }

  return json;
}

void setup_subgraph_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  node.set_current_category("SubGraph");
  add_bool(node, "inherit_config", "Sync Config with Parent", true);
}

void compute_subgraph_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  if (auto *p_sg = dynamic_cast<SubGraphNode *>(&node))
    p_sg->compute_subgraph();
}

} // namespace hesiod
