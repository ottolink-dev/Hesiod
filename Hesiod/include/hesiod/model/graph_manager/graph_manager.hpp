/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "gnode/graph.hpp"

namespace gnode
{
class GraphWorker;
}

namespace hesiod
{

class GraphNode; // forward
class GraphConfig;

// =====================================
// GraphManager
// =====================================
using GraphNodeMap = std::map<std::string, std::shared_ptr<GraphNode>>;

class GraphManager : public gnode::Graph
{
public:
  explicit GraphManager(const std::string &id = "");
  ~GraphManager() override;

  std::shared_ptr<GraphManager> get_shared();
  void                          clear();

  // --- Accessors ---
  GraphNodeMap             get_graph_nodes() const;
  std::vector<std::string> get_graph_order() const;
  int                      get_graph_order_index(const std::string &graph_id) const;
  GraphNode               *get_graph_ref_by_id(const std::string &graph_id) const;
  gnode::GraphWorker      &get_worker();

  // --- GraphNode management ---
  std::string add_graph_node(const std::shared_ptr<GraphNode> &p_graph_node,
                             const std::string                &graph_id = "");
  bool        is_graph_id_available(const std::string &graph_id) const;
  void        remove_graph_node(const std::string &graph_id);
  void        reseed(bool backward);
  std::string runtime_info_to_string(const char separator = ',') const;

  // --- Global Graph Update ---
  void update() override;
  void update(const std::string &graph_id) override;
  void update(const std::vector<std::string> &graph_ids) override;

  // --- Serialization ---
  void           json_from(nlohmann::json const &json, GraphConfig *p_config);
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;
  void load_from_file(const std::string &fname, GraphConfig *p_config = nullptr);
  void save_to_file(const std::string &fname) const;

  // --- Callbacks ---
  std::function<void(float progress)>         update_progress;
  std::function<void(const std::string &msg)> update_failed;
  std::function<void(bool held)>              update_held;

private:
  void on_graph_node_output_updated(const std::string &graph_id);
  void on_update_progress(const std::string &node_id, float progress);
  void on_update_failed(std::exception_ptr error);
  void advance_global_queue();

  std::deque<std::string> pending_global_queue;
  std::string             active_global_graph_id;

  // last: it has to stop before the graphs go
  std::unique_ptr<gnode::GraphWorker> worker;
};

} // namespace hesiod
