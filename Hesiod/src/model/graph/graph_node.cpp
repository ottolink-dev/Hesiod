/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/broadcast_node.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/receive_node.hpp"
#include "hesiod/model/utils.hpp"

#include <iostream>

namespace hesiod
{

GraphNode::GraphNode(const std::string &id, const std::shared_ptr<GraphConfig> &config)
    : gnode::Graph(id), hmap::CoordFrame(), config(config)
{
  Logger::log()->trace("GraphNode::GraphNode");

  // setup graph update callback
  auto lambda = [this](const std::string              &current_id,
                       const std::vector<std::string> &sorted_ids,
                       bool                            before_update)
  {
    // this should not happen...
    if (sorted_ids.empty())
      return;

    // compute progress
    int idx = find_index(sorted_ids, current_id);
    int nids = static_cast<int>(sorted_ids.size());

    // in percentage
    float r = static_cast<float>(idx);
    if (!before_update)
      r += 0.5f;
    float progress = 100.f * r / (static_cast<float>(nids) - 0.5f);

    if (this->update_progress)
      this->update_progress(current_id, progress);
  };

  this->set_update_callback(lambda);
}

std::string GraphNode::add_node(const std::string &node_type)
{
  Logger::log()->trace("GraphNode::add_node: node_type = {}", node_type);

  std::shared_ptr<gnode::Node> node = node_factory(node_type, this->config);
  node->compute();

  std::string node_id = this->add_node(node);

  return node_id;
}

std::string GraphNode::add_node(const std::shared_ptr<gnode::Node> &node,
                                const std::string                  &id)
{
  Logger::log()->trace("GraphNode::add_node: id = {}", id);

  // basic GNode adding...
  std::string node_id = gnode::Graph::add_node(node, id);

  // connections
  BaseNode *p_basenode = dynamic_cast<BaseNode *>(node.get());

  p_basenode->compute_started = [this](const std::string &node_id)
  {
    if (this->compute_started)
      this->compute_started(node_id);
  };

  p_basenode->compute_finished = [this](const std::string &node_id)
  {
    if (this->compute_finished)
      this->compute_finished(node_id);
  };

  // "special" nodes treatmentxs
  std::string node_type = p_basenode->get_node_type();

  if (node_type == "Broadcast")
    this->setup_new_broadcast_node(p_basenode);

  if (node_type == "Receive")
    this->setup_new_receive_node(p_basenode);

  return node_id;
}

void GraphNode::change_config_values(const GraphConfig &new_config)
{
  Logger::log()->trace("GraphNode::change_config_values");

  *this->config = new_config;

  for (auto &[id, p_node] : this->get_nodes())
    if (BaseNode *p_basenode = dynamic_cast<BaseNode *>(p_node.get()))
      p_basenode->propagate_config_change();

  this->update();
}

GraphConfig *GraphNode::get_config_ref() { return this->config.get(); }

std::shared_ptr<GraphNode> GraphNode::get_shared()
{
  try
  {
    return shared_from_this();
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

  // override the current config if one is provided
  if (p_input_config)
  {
    this->config = std::make_shared<GraphConfig>(*p_input_config);

    Logger::log()->trace("GraphNode::json_from: GraphConfig override");
  }
  else
  {
    if (json.contains("model_config"))
    {
      this->config->json_from(json["model_config"]);
    }
    else
    {
      Logger::log()->error("Missing key \"model_config\" in json");
    }
  }

  std::string id = "";
  uint        id_count = 0;

  json_safe_get(json, "id", id);
  json_safe_get(json, "id_count", id_count);

  this->set_id(id);
  this->set_id_count(id_count);

  // hmap::CoordFrame
  std::vector<float> vo = {};
  std::vector<float> vs = {};

  json_safe_get(json, "origin", vo);
  json_safe_get(json, "size", vs);

  this->set_origin(glm::vec2(vo[0], vo[1]));
  this->set_size(glm::vec2(vs[0], vs[1]));

  float rotation_angle = 0.f;
  json_safe_get(json, "rotation_angle", rotation_angle);

  this->set_rotation_angle(rotation_angle);

  // populate nodes
  if (json.contains("nodes"))
  {
    for (auto &json_node : json["nodes"])
    {
      std::string node_type = "";

      json_safe_get(json_node, "label", node_type);

      Logger::log()->trace("GraphNode::json_from, node type: {}", node_type);

      // instanciate the node
      std::shared_ptr<gnode::Node> node = node_factory(node_type, this->config);

      std::string id = "";
      json_safe_get(json_node, "id", id);

      this->add_node(node, id);

      // set its parameters
      dynamic_cast<BaseNode *>(node.get())->json_from(json_node);
    }
  }
  else
  {
    Logger::log()->error("Missing key \"nodes\" in json");
  }

  // links
  if (json.contains("links"))
  {
    for (auto &json_link : json["links"])
    {
      std::string node_id_from = "", port_id_from = "", node_id_to = "", port_id_to = "";

      json_safe_get(json_link, "node_id_from", node_id_from);
      json_safe_get(json_link, "port_id_from", port_id_from);
      json_safe_get(json_link, "node_id_to", node_id_to);
      json_safe_get(json_link, "port_id_to", port_id_to);

      Logger::log()->trace("GraphNode::json_from, new link request: {}/{} => {}/{}",
                           node_id_from,
                           port_id_from,
                           node_id_to,
                           port_id_to);

      this->new_link(node_id_from, port_id_from, node_id_to, port_id_to);
    }
  }
  else
  {
    Logger::log()->error("Missing key \"links\" in json");
  }
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
    nlohmann::json json_node;
    json_node = dynamic_cast<BaseNode *>(p_node.get())->json_to();
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

void GraphNode::on_broadcast_node_updated(const std::string &tag)
{
  Logger::log()->trace("GraphNode::on_broadcast_node_updated: tag: {}", tag);

  // loop over the nodes and update those with Receive type
  for (auto &[node_id, p_gnode] : this->nodes)
  {
    BaseNode *p_node = dynamic_cast<BaseNode *>(p_gnode.get());

    if (p_node)
      if (p_node->get_label() == "Receive")
      {
        ReceiveNode *p_receive_node = dynamic_cast<ReceiveNode *>(p_node);

        if (!p_receive_node)
        {
          Logger::log()->critical(
              "GraphNode::on_broadcast_node_updated: could not properly recast "
              "the node as a receiver. Tag {}",
              tag);
          throw std::runtime_error("could not properly recast the node as a receiver");
        }
        else
        {
          // only update Receive nodes with the proper tag
          if (p_receive_node->get_current_tag() == tag)
            this->update(node_id);
        }
      }
  }
}

void GraphNode::remove_node(const std::string &id)
{
  Logger::log()->trace("GraphNode::remove_node: id = {}", id);

  // "special" nodes treatment
  BaseNode *p_basenode = this->get_node_ref_by_id<BaseNode>(id);

  if (!p_basenode)
  {
    Logger::log()->error("GraphNode::remove_node: p_basenode is nullptr");
    return;
  }

  std::string node_type = p_basenode->get_node_type();

  if (node_type == "Broadcast")
  {
    std::string tag = this->get_node_ref_by_id<BroadcastNode>(id)->get_broadcast_tag();

    Logger::log()->trace("GraphNode::remove_node: removing broadcast tag {}", tag);

    if (this->remove_broadcast_tag)
      this->remove_broadcast_tag(tag);
  }

  // basic GNode removing...
  gnode::Graph::remove_node(id);
}

void GraphNode::reseed(bool backward)
{
  Logger::log()->trace("GraphNode::reseed: {}", this->get_id());

  // loop over nodes and increment seed number if any
  for (auto &[key, p_node] : this->nodes)
  {
    if (p_node)
      dynamic_cast<BaseNode *>(p_node.get())->reseed(backward);
  }

  this->update();
}

void GraphNode::set_p_broadcast_params(BroadcastMap *new_p_broadcast_params)
{
  Logger::log()->trace("GraphNode::set_p_broadcast_params: ptr = {}",
                       new_p_broadcast_params ? "OK" : "nullptr");

  this->p_broadcast_params = new_p_broadcast_params;
}

void GraphNode::setup_new_broadcast_node(BaseNode *p_node)
{
  Logger::log()->trace("GraphNode::setup_new_broadcast_node");

  BroadcastNode *p_broadcast_node = dynamic_cast<BroadcastNode *>(p_node);

  if (!p_broadcast_node)
  {
    Logger::log()->error("GraphNode::setup_new_broadcast_node: Invalid node type");
    return;
  }

  // connect
  p_broadcast_node->broadcast_node_updated =
      [this](const std::string &graph_id, const std::string &tag)
  {
    // pass through, re-emit the signals by the graph
    // editor (to be handled by the graph manager above)
    Logger::log()->trace("graph_node broadcasting, tag: {}", tag);

    if (this->broadcast_node_updated)
      this->broadcast_node_updated(graph_id, tag);
  };

  // broadcast infos: // store broadcast parameters (to be handled to the graph manage
  // above). Use an output port to store the data, to ensure it is
  // always available, full of zeros in worst case scenario
  const std::string tag = this->get_node_ref_by_id<BroadcastNode>(p_node->get_id())
                              ->get_broadcast_tag();
  const hmap::CoordFrame   *t_source = dynamic_cast<hmap::CoordFrame *>(this);
  const hmap::VirtualArray *h_source = this->get_node_ref_by_id(p_node->get_id())
                                           ->get_value_ref<hmap::VirtualArray>("thru");

  if (this->new_broadcast_tag)
    this->new_broadcast_tag(tag, t_source, h_source);
}

void GraphNode::setup_new_receive_node(BaseNode *p_node)
{
  Logger::log()->trace("GraphNode::setup_new_receive_node");

  ReceiveNode *p_receive_node = dynamic_cast<ReceiveNode *>(p_node);

  if (!p_receive_node)
  {
    Logger::log()->error("GraphNode::setup_new_receive_node: Invalid node type");
    return;
  }

  p_receive_node->set_p_broadcast_params(this->p_broadcast_params);
  p_receive_node->set_p_coord_frame(dynamic_cast<hmap::CoordFrame *>(this));
}

void GraphNode::update()
{
  Logger::log()->trace("GraphNode::update");

  if (this->update_started)
    this->update_started();

  gnode::Graph::update();

  if (this->update_finished)
    this->update_finished();
}

void GraphNode::update(const std::vector<std::string> &node_ids)
{
  Logger::log()->trace("GraphNode::update");

  // log
  {
    std::string node_ids_str = "";
    for (const auto &s : node_ids)
      node_ids_str += s + ", ";
    Logger::log()->trace("GraphNode::update: node_ids: {}", node_ids_str);
  }

  if (this->update_started)
    this->update_started();

  gnode::Graph::update(node_ids);

  if (this->update_finished)
    this->update_finished();
}

void GraphNode::update(const std::string &node_id)
{
  Logger::log()->trace("GraphNode::update: id = {}", node_id);
  this->update(std::vector<std::string>{node_id});
}

} // namespace hesiod
