/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <unordered_set>
#include <utility>

#include "gnode/graph_worker.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/boundary_types.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/node_in_node.hpp"
#include "hesiod/model/nodes/node_out_node.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

GraphNode::GraphNode(const std::string &id, const std::shared_ptr<GraphConfig> &config)
    : gnode::Graph(id), gnode::Node("GraphNode", id), hmap::CoordFrame(), config(config)
{
  Logger::log()->trace("GraphNode::GraphNode: {}", id);

  auto lambda = [this](const std::string              &current_id,
                       const std::vector<std::string> &sorted_ids,
                       bool                            before_update)
  {
    if (sorted_ids.empty())
      return;

    int idx = find_index(sorted_ids, current_id);
    int nids = static_cast<int>(sorted_ids.size());

    float r = static_cast<float>(idx);
    if (!before_update)
      r += 0.5f;
    float progress = 100.f * r / (static_cast<float>(nids) - 0.5f);

    if (this->update_progress)
      this->update_progress(current_id, progress);

    if (this->update_steps)
      this->update_steps(sorted_ids, before_update ? idx : idx + 1);
  };

  this->graph_progress_conn = this->graph_progress_event.subscribe(lambda);

  gnode::Graph::update_finished = [this]() { this->handle_inner_update_finished(); };
}

void GraphNode::set_id(const std::string &new_id)
{
  gnode::Graph::set_id(new_id);
  gnode::Node::set_id(new_id);
}

std::string GraphNode::add_node(const std::string &node_type, const std::string &id)
{
  Logger::log()->trace("GraphNode::add_node: node_type = {}, id = {}", node_type, id);

  std::shared_ptr<gnode::Node> node = node_factory(node_type, this->config);
  return this->add_node(node, id);
}

std::string GraphNode::add_node(const std::shared_ptr<gnode::Node> &node,
                                const std::string                  &id)
{
  Logger::log()->trace("GraphNode::add_node: id = {}", id);

  this->stop_update();

  std::string node_id = gnode::Graph::add_node(node, id);

  BaseNode *p_basenode = dynamic_cast<BaseNode *>(node.get());
  if (p_basenode)
  {
    p_basenode->compute_started = [this](const std::string &nid)
    {
      if (this->compute_started)
        this->compute_started(nid);
    };

    p_basenode->compute_finished = [this](const std::string &nid)
    {
      if (this->compute_finished)
        this->compute_finished(nid);
    };

    const std::string node_type = p_basenode->get_node_type();
    if (node_type == "NodeIn" || node_type == "NodeOut")
    {
      this->setup_boundary_node(p_basenode);
      this->auto_bind_ports();
    }
  }

  return node_id;
}

void GraphNode::setup_boundary_node(BaseNode *p_node)
{
  if (auto *p_in = dynamic_cast<NodeInNode *>(p_node))
  {
    p_in->boundary_changed = [this]() { this->auto_bind_ports(); };
  }
  else if (auto *p_out = dynamic_cast<NodeOutNode *>(p_node))
  {
    p_out->boundary_changed = [this]() { this->auto_bind_ports(); };
  }
}

void GraphNode::auto_bind_ports()
{
  // 1. Snapshot incident links in parent GraphManager by port label
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

  // 2. Collect active boundary nodes and update GraphNode external ports
  std::unordered_set<std::string>              active_inputs;
  std::unordered_set<std::string>              active_outputs;
  std::unordered_set<std::string>              used_labels;
  std::unordered_map<std::string, std::string> renames;

  auto find_old_input_name = [this](const NodeInNode *ptr) -> std::string
  {
    for (const auto &b : this->inputs)
      if (b.node.get() == ptr)
        return b.name;
    return {};
  };

  auto find_old_output_name = [this](const NodeOutNode *ptr) -> std::string
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

  for (const auto &[_, node_ptr] : this->nodes)
  {
    if (auto input_node = std::dynamic_pointer_cast<NodeInNode>(node_ptr))
    {
      if (input_node->get_nports() > 0)
      {
        const std::string slot_name = make_unique_label(input_node->get_slot_name());
        active_inputs.insert(slot_name);

        const std::string old_name = find_old_input_name(input_node.get());
        if (!old_name.empty() && old_name != slot_name)
          renames[old_name] = slot_name;

        if (this->has_port(slot_name) && this->get_data_type(this->get_port_index(
                                             slot_name)) != input_node->get_data_type(0))
        {
          this->remove_port(slot_name);
          std::erase_if(this->inputs,
                        [&slot_name](const InputBinding &b)
                        { return b.name == slot_name; });
        }

        if (!this->has_port(slot_name))
        {
          visit_boundary_type(input_node->get_data_type_name(),
                              [this, &slot_name]<typename T>()
                              { this->add_port<T>(gnode::PortType::IN, slot_name); });
          this->inputs.push_back({slot_name, input_node});
        }
      }
    }
    else if (auto output_node = std::dynamic_pointer_cast<NodeOutNode>(node_ptr))
    {
      if (output_node->get_nports() > 0)
      {
        const std::string slot_name = make_unique_label(output_node->get_slot_name());
        active_outputs.insert(slot_name);

        const std::string old_name = find_old_output_name(output_node.get());
        if (!old_name.empty() && old_name != slot_name)
          renames[old_name] = slot_name;

        if (this->has_port(slot_name) && this->get_data_type(this->get_port_index(
                                             slot_name)) != output_node->get_data_type(0))
        {
          this->remove_port(slot_name);
          std::erase_if(this->outputs,
                        [&slot_name](const OutputBinding &b)
                        { return b.name == slot_name; });
        }

        if (!this->has_port(slot_name))
        {
          visit_boundary_type(
              output_node->get_data_type_name(),
              [this, &slot_name]<typename T>()
              {
                if constexpr (std::is_same_v<T, hmap::VirtualArray>)
                  this->add_port<T>(gnode::PortType::OUT,
                                    slot_name,
                                    this->config->shape,
                                    this->config->tile_shape,
                                    this->config->halo,
                                    this->config->storage_mode);
                else if constexpr (std::is_same_v<T, hmap::VirtualTexture>)
                  this->add_port<T>(gnode::PortType::OUT,
                                    slot_name,
                                    this->config->shape,
                                    this->config->tile_shape,
                                    this->config->halo,
                                    4,
                                    this->config->storage_mode);
                else if constexpr (std::is_same_v<T, hmap::Array>)
                  this->add_port<T>(gnode::PortType::OUT, slot_name, this->config->shape);
                else
                  this->add_port<T>(gnode::PortType::OUT, slot_name);
              });
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

  // 3. Reconcile snapshotted links in parent GraphManager
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

      const bool can_rebind = (out_idx >= 0 && in_idx >= 0 &&
                               p_from->get_port_type(port_from) == gnode::PortType::OUT &&
                               p_to->get_port_type(port_to) == gnode::PortType::IN &&
                               p_from->get_data_type(out_idx) ==
                                   p_to->get_data_type(in_idx));

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

std::unordered_map<std::string, std::string> GraphNode::consume_port_renames()
{
  return std::exchange(this->pending_port_renames, {});
}

void GraphNode::pull_inputs_from_parent()
{
  gnode::Graph *parent_graph = this->get_p_graph();
  const auto   *dst_frame = static_cast<const hmap::CoordFrame *>(this);

  for (auto &[port_name, in_node] : this->inputs)
  {
    if (!in_node)
      continue;

    const GraphConfig      *src_cfg = this->config.get();
    const hmap::CoordFrame *src_frame = nullptr;

    if (parent_graph)
    {
      const int in_idx = this->get_port_index(port_name);
      for (const auto &link : parent_graph->get_links())
      {
        if (link.to == this->get_id() && link.port_to == in_idx)
        {
          if (auto *src_graph = dynamic_cast<GraphNode *>(
                  parent_graph->get_node(link.from)))
          {
            src_cfg = src_graph->get_config_ref();
            src_frame = static_cast<const hmap::CoordFrame *>(src_graph);
          }
          break;
        }
      }
    }

    Logger::log()->trace("GraphNode::pull_inputs_from_parent: {} port={} src_frame={}",
                         this->get_id(),
                         port_name,
                         src_frame ? "ok" : "null");

    visit_boundary_type(
        in_node->get_data_type_name(),
        [this, &port_name, &in_node, src_cfg, src_frame, dst_frame]<typename T>()
        {
          if (!this->has_port<T>(port_name) || !in_node->has_port<T>("value"))
            return;
          auto *p_src = this->get_value_ref<T>(port_name);
          auto *p_dst = in_node->get_value_ref<T>("value");
          auto *p_mask = in_node->has_port<hmap::VirtualArray>("mask")
                             ? in_node->get_value_ref<hmap::VirtualArray>("mask")
                             : nullptr;
          if (p_dst)
          {
            transfer_boundary_value(p_src,
                                    *src_cfg,
                                    src_frame,
                                    *p_dst,
                                    *this->config,
                                    dst_frame,
                                    p_mask);
            if constexpr (std::is_same_v<T, hmap::VirtualArray>)
            {
              Logger::log()->trace(
                  "pull_inputs_from_parent[VA]: {} port={} p_src={} ({}, {}), "
                  "src_frame=({},{}; {},{}; {}), dst_frame=({},{}; {},{}; {}), "
                  "p_dst=({}, {})",
                  this->get_id(),
                  port_name,
                  p_src ? "ok" : "null",
                  p_src ? p_src->min(src_cfg->cm_cpu) : 0.f,
                  p_src ? p_src->max(src_cfg->cm_cpu) : 0.f,
                  src_frame ? src_frame->get_origin().x : 0.f,
                  src_frame ? src_frame->get_origin().y : 0.f,
                  src_frame ? src_frame->get_size().x : 0.f,
                  src_frame ? src_frame->get_size().y : 0.f,
                  src_frame ? src_frame->get_rotation_angle() : 0.f,
                  dst_frame ? dst_frame->get_origin().x : 0.f,
                  dst_frame ? dst_frame->get_origin().y : 0.f,
                  dst_frame ? dst_frame->get_size().x : 0.f,
                  dst_frame ? dst_frame->get_size().y : 0.f,
                  dst_frame ? dst_frame->get_rotation_angle() : 0.f,
                  p_dst->min(this->config->cm_cpu),
                  p_dst->max(this->config->cm_cpu));
            }
          }
        });

    in_node->is_dirty = true;
  }
}

void GraphNode::push_outputs_to_parent()
{
  Logger::log()->trace("GraphNode::push_outputs_to_parent: {} ({} outputs)",
                       this->get_id(),
                       this->outputs.size());

  for (auto &[port_name, out_node] : this->outputs)
  {
    if (!out_node)
      continue;

    visit_boundary_type(
        out_node->get_data_type_name(),
        [this, &port_name, &out_node]<typename T>()
        {
          if (!this->has_port<T>(port_name) || !out_node->has_port<T>("value"))
          {
            Logger::log()->warn("push_outputs_to_parent: type mismatch port={} dtype={} "
                                "has_graph_port={} has_node_port={}",
                                port_name,
                                out_node->get_data_type_name(),
                                this->has_port<T>(port_name),
                                out_node->has_port<T>("value"));
            return;
          }
          auto *p_src = out_node->get_value_ref<T>("value");
          auto *p_dst = this->get_value_ref<T>(port_name);
          if (p_dst)
          {
            transfer_boundary_value(p_src,
                                    *this->config,
                                    nullptr,
                                    *p_dst,
                                    *this->config,
                                    nullptr);
            if constexpr (std::is_same_v<T, hmap::VirtualArray>)
            {
              Logger::log()->trace("push_outputs_to_parent[VA]: {} port={} p_src={} "
                                   "({}, {}), p_dst=({}, {})",
                                   this->get_id(),
                                   port_name,
                                   p_src ? "ok" : "null",
                                   p_src ? p_src->min(this->config->cm_cpu) : 0.f,
                                   p_src ? p_src->max(this->config->cm_cpu) : 0.f,
                                   p_dst->min(this->config->cm_cpu),
                                   p_dst->max(this->config->cm_cpu));
            }
          }
        });
  }
}

void GraphNode::compute()
{
  Logger::log()->trace("GraphNode::compute: {}", this->get_id());

  this->pull_inputs_from_parent();

  if (this->p_worker && this->p_worker->request(*this))
    return;

  gnode::Graph::update();
  this->handle_inner_update_finished();
}

void GraphNode::handle_inner_update_finished()
{
  this->push_outputs_to_parent();

  if (this->graph_update_finished)
    this->graph_update_finished();

  if (this->global_output_updated)
    this->global_output_updated(this->get_id());
}

void GraphNode::change_config_values(const GraphConfig &new_config)
{
  Logger::log()->trace("GraphNode::change_config_values");

  this->stop_update();

  *this->config = new_config;

  for (auto &[id, p_node] : this->get_nodes())
    if (BaseNode *p_basenode = dynamic_cast<BaseNode *>(p_node.get()))
      p_basenode->propagate_config_change();

  this->pull_inputs_from_parent();
  this->update();
}

GraphConfig *GraphNode::get_config_ref() { return this->config.get(); }

std::shared_ptr<GraphNode> GraphNode::get_shared()
{
  try
  {
    return std::dynamic_pointer_cast<GraphNode>(gnode::Graph::shared_from_this());
  }
  catch (...)
  {
    Logger::log()->critical("GraphNode::get_shared: object is not managed by shared_ptr");
    return nullptr;
  }
}

void GraphNode::json_from(nlohmann::json const &json, GraphConfig *p_input_config)
{
  Logger::log()->trace("GraphNode::json_from, graph {}", this->get_id());

  nlohmann::json converted_json = convert_legacy_graph_json(json);

  if (p_input_config)
  {
    this->config = std::make_shared<GraphConfig>(*p_input_config);
    Logger::log()->trace("GraphNode::json_from: GraphConfig override");
  }
  else
  {
    if (converted_json.contains("model_config"))
      this->config->json_from(converted_json["model_config"]);
    else
      Logger::log()->error("Missing key \"model_config\" in json");
  }

  std::string id = "";
  uint        id_count = 0;

  json_safe_get(converted_json, "id", id);
  json_safe_get(converted_json, "id_count", id_count);

  this->set_id(id);
  this->set_id_count(id_count);

  // hmap::CoordFrame
  std::vector<float> vo = {};
  std::vector<float> vs = {};

  json_safe_get(converted_json, "origin", vo);
  json_safe_get(converted_json, "size", vs);

  if (vo.size() >= 2)
    this->set_origin(glm::vec2(vo[0], vo[1]));
  if (vs.size() >= 2)
    this->set_size(glm::vec2(vs[0], vs[1]));

  float rotation_angle = 0.f;
  json_safe_get(converted_json, "rotation_angle", rotation_angle);
  this->set_rotation_angle(rotation_angle);

  // populate nodes
  if (converted_json.contains("nodes"))
  {
    for (auto &json_node : converted_json["nodes"])
    {
      std::string node_type = "";
      json_safe_get(json_node, "label", node_type);

      if (node_type == "Broadcast" || node_type == "Receive")
      {
        Logger::log()->warn("GraphNode::json_from: skipping legacy '{}' node", node_type);
        continue;
      }

      std::string node_id = "";
      json_safe_get(json_node, "id", node_id);

      try
      {
        std::shared_ptr<gnode::Node> node = node_factory(node_type, this->config);
        if (!node)
          throw std::runtime_error("Unknown or invalid node type: " + node_type);

        this->add_node(node, node_id);

        if (auto *p_base = dynamic_cast<BaseNode *>(node.get()))
          p_base->json_from(json_node);
        else
          throw std::runtime_error("Node is not a BaseNode: " + node_type);
      }
      catch (const std::exception &e)
      {
        HSD_CTX.get_error_manager().push_error(
            ErrorCategory::NodeCreation,
            std::format("Graph '{}': Failed to create node '{}' (type '{}'): {}",
                        this->get_id(),
                        node_id.empty() ? "?" : node_id,
                        node_type,
                        e.what()));
      }
      catch (...)
      {
        HSD_CTX.get_error_manager().push_error(
            ErrorCategory::NodeCreation,
            std::format(
                "Graph '{}': Failed to create node '{}' (type '{}'): unknown error",
                this->get_id(),
                node_id.empty() ? "?" : node_id,
                node_type));
      }
    }
  }
  else
  {
    Logger::log()->error("Missing key \"nodes\" in json");
  }

  // links
  if (converted_json.contains("links"))
  {
    for (auto &json_link : converted_json["links"])
    {
      std::string node_id_from = "", port_id_from = "", node_id_to = "", port_id_to = "";

      json_safe_get(json_link, "node_id_from", node_id_from);
      json_safe_get(json_link, "port_id_from", port_id_from);
      json_safe_get(json_link, "node_id_to", node_id_to);
      json_safe_get(json_link, "port_id_to", port_id_to);

      try
      {
        this->new_link(node_id_from, port_id_from, node_id_to, port_id_to);
      }
      catch (const std::exception &e)
      {
        HSD_CTX.get_error_manager().push_error(
            ErrorCategory::LinkCreation,
            std::format("Graph '{}': Failed to create link {}/{} => {}/{}: {}",
                        this->get_id(),
                        node_id_from,
                        port_id_from,
                        node_id_to,
                        port_id_to,
                        e.what()));
      }
      catch (...)
      {
        HSD_CTX.get_error_manager().push_error(
            ErrorCategory::LinkCreation,
            std::format("Graph '{}': Failed to create link {}/{} => {}/{}: unknown error",
                        this->get_id(),
                        node_id_from,
                        port_id_from,
                        node_id_to,
                        port_id_to));
      }
    }
  }
  else
  {
    Logger::log()->error("Missing key \"links\" in json");
  }

  this->auto_bind_ports();
}

nlohmann::json GraphNode::json_to() const
{
  Logger::log()->trace("GraphNode::json_to, graph {}", this->get_id());

  nlohmann::json json;
  json["id"] = this->get_id();
  json["id_count"] = this->get_id_count();
  json["model_config"] = this->config->json_to();

  // hmap::CoordFrame
  json["origin"] = {this->get_origin().x, this->get_origin().y};
  json["size"] = {this->get_size().x, this->get_size().y};
  json["rotation_angle"] = this->get_rotation_angle();

  // nodes
  json["nodes"] = nlohmann::json::array();

  for (auto &[key, p_node] : this->nodes)
  {
    nlohmann::json json_node = dynamic_cast<BaseNode *>(p_node.get())->json_to();
    json["nodes"].push_back(json_node);
  }

  // links
  json["links"] = nlohmann::json::array();

  for (auto &link : this->links)
  {
    nlohmann::json json_link;

    gnode::Node *p_from = this->get_node_ref_by_id(link.from);
    gnode::Node *p_to = this->get_node_ref_by_id(link.to);

    json_link["node_id_from"] = link.from;
    json_link["node_id_to"] = link.to;
    json_link["port_id_from"] = p_from->get_port_label(link.port_from);
    json_link["port_id_to"] = p_to->get_port_label(link.port_to);

    json["links"].push_back(json_link);
  }

  return json;
}

void GraphNode::remove_node(const std::string &id)
{
  Logger::log()->trace("GraphNode::remove_node: id = {}", id);

  this->stop_update();

  BaseNode *p_basenode = this->get_node_ref_by_id<BaseNode>(id);
  if (!p_basenode)
  {
    Logger::log()->error("GraphNode::remove_node: p_basenode is nullptr");
    return;
  }

  const std::string node_type = p_basenode->get_node_type();

  gnode::Graph::remove_node(id);

  if (node_type == "NodeIn" || node_type == "NodeOut")
    this->auto_bind_ports();
}

std::string GraphNode::runtime_info_to_string(const char separator) const
{
  std::string result;

  for (const auto &[node_id, p_gnode] : this->nodes)
  {
    const BaseNode *p_node = dynamic_cast<const BaseNode *>(p_gnode.get());
    if (p_node)
    {
      if (!result.empty())
        result += '\n';
      result += p_node->runtime_info_to_string(separator);
    }
  }

  return result;
}

void GraphNode::reseed(bool backward)
{
  Logger::log()->trace("GraphNode::reseed: {}", this->get_id());

  for (auto &[key, p_node] : this->nodes)
  {
    if (p_node)
      dynamic_cast<BaseNode *>(p_node.get())->reseed(backward);
  }

  this->update();
}

void GraphNode::stop_update()
{
  if (this->p_worker)
    this->p_worker->stop();
}

void GraphNode::update()
{
  Logger::log()->trace("GraphNode::update");

  if (this->update_started)
    this->update_started();

  if (this->p_worker && this->p_worker->request(*this))
    return;

  gnode::Graph::update();
  this->handle_inner_update_finished();
}

void GraphNode::update(const std::vector<std::string> &node_ids)
{
  Logger::log()->trace("GraphNode::update (node_ids)");

  if (this->update_started)
    this->update_started();

  if (this->p_worker && this->p_worker->request(*this, &node_ids))
    return;

  gnode::Graph::update(node_ids);
  this->handle_inner_update_finished();
}

void GraphNode::update(const std::string &node_id)
{
  Logger::log()->trace("GraphNode::update: id = {}", node_id);
  this->update(std::vector<std::string>{node_id});
}

} // namespace hesiod
