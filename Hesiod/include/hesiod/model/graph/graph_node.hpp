/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>

#include "nlohmann/json.hpp"

#include "gnode/graph.hpp"
#include "highmap/coord_frame.hpp"

#include "hesiod/model/graph/broadcast_param.hpp"
#include "hesiod/model/graph/graph_config.hpp"

namespace hesiod
{

class BaseNode; // forward

// =====================================
// GraphNode
// =====================================
class GraphNode : public gnode::Graph,
                  public hmap::CoordFrame,
                  public std::enable_shared_from_this<GraphNode>
{
public:
  GraphNode() = delete;
  GraphNode(const std::string &id, const std::shared_ptr<GraphConfig> &config);

  std::shared_ptr<GraphNode> get_shared();

  // --- Serialization ---
  void json_from(nlohmann::json const &json, GraphConfig *p_input_config = nullptr);
  nlohmann::json json_to() const;

  // --- Config. ---
  GraphConfig *get_config_ref();
  void         change_config_values(const GraphConfig &new_config);

  // --- Node Factory (create nodes from their type) ---
  std::string add_node(const std::string &node_type);

  // --- GNode::Graph override ---
  std::string  add_node(const std::shared_ptr<gnode::Node> &p_node,
                        const std::string                  &id = "") override;
  virtual void remove_node(const std::string &id) override;
  void         update() override;
  void         update(const std::vector<std::string> &node_ids) override;
  void         update(const std::string &node_id) override;

  // --- Inter-graph Broadcasting ---
  BroadcastMap *get_p_broadcast_params() { return this->p_broadcast_params; }
  void          set_p_broadcast_params(BroadcastMap *new_p_broadcast_params);
  void          on_broadcast_node_updated(const std::string &tag);

  // --- Others... ---
  void reseed(bool backward);

  // --- Compute Callbacks
  std::function<void(const std::string &node_id)> compute_started;
  std::function<void(const std::string &node_id)> compute_finished;

  // --- Update Callbacks
  std::function<void()>                                           update_started;
  std::function<void()>                                           update_finished;
  std::function<void(const std::string &node_id, float progress)> update_progress;

  // --- Broadcast Callbacks
  std::function<void(const std::string &graph_id, const std::string &tag)>
                                              broadcast_node_updated;
  std::function<void(const std::string &tag)> remove_broadcast_tag;
  std::function<void(const std::string        &tag,
                     const hmap::CoordFrame   *t_source,
                     const hmap::VirtualArray *h_source)>
      new_broadcast_tag;

private:
  // --- Helpers ---
  void setup_new_broadcast_node(BaseNode *p_node);
  void setup_new_receive_node(BaseNode *p_node);

  // --- Members ---
  std::shared_ptr<GraphConfig> config;
  BroadcastMap                *p_broadcast_params = nullptr; // own by GraphManager
  gnode::EventConnection       graph_progress_conn;
};

} // namespace hesiod
