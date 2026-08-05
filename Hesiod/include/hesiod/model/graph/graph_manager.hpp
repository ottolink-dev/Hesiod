/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include "nlohmann/json.hpp"

#include "hesiod/model/graph/broadcast_param.hpp"
#include "hesiod/model/graph/flatten_config.hpp"

#include <filesystem> // must be here,
                      // https://bugreports.qt.io/browse/QTBUG-73263

namespace hesiod
{

class GraphNode; // forward
class GraphConfig;

// =====================================
// GraphManager
// =====================================
using GraphNodeMap = std::map<std::string, std::shared_ptr<GraphNode>>;

class GraphManager : public std::enable_shared_from_this<GraphManager>
{
public:
  explicit GraphManager(const std::string &id = "");

  std::shared_ptr<GraphManager> get_shared();
  void                          clear();

  // --- Accessors ---
  const BroadcastMap             &get_broadcast_params();
  FlattenConfig                   get_export_param() const;
  const GraphNodeMap             &get_graph_nodes();
  const std::vector<std::string> &get_graph_order();
  int                             get_graph_order_index(const std::string &graph_id);
  GraphNode                      *get_graph_ref_by_id(const std::string &graph_id);
  std::string                     get_id() const;

  void set_export_param(const FlattenConfig &new_export_param);
  void set_graph_order(const std::vector<std::string> &new_graph_order);
  void set_id(const std::string &new_id);

  // --- GraphNode management ---
  std::string add_graph_node(const std::shared_ptr<GraphNode> &p_graph_node,
                             const std::string                &graph_id = "");
  void        export_flatten();
  bool is_graph_above(const std::string &graph_id, const std::string &ref_graph_id);
  bool is_graph_id_available(const std::string &graph_id);
  void remove_graph_node(const std::string &graph_id);
  void reseed(bool backward);
  void update();

  // --- Serialization ---
  void           json_from(nlohmann::json const &json, GraphConfig *p_config);
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;
  void load_from_file(const std::string &fname, GraphConfig *p_config = nullptr);
  void save_to_file(const std::string &fname) const;

  // --- Callbacks ---
  std::function<void(const std::string &tag)> new_broadcast_tag;
  std::function<void(const std::string &tag)> remove_broadcast_tag;
  std::function<void(float progress)>         update_progress;

  // reports a graph update that was abandoned because it threw, so the GUI can
  // tell the user instead of the failure being visible only in the log
  std::function<void(const std::string &message)> update_failed;

private:
  // --- Intergraph interactions ---
  void on_broadcast_node_updated(const std::string &graph_id, const std::string &tag);
  void on_new_broadcast_tag(const std::string        &tag,
                            const hmap::CoordFrame   *t_source,
                            const hmap::VirtualArray *h_source);
  void on_remove_broadcast_tag(const std::string &tag);
  void on_update_progress(const std::string &node_id, float progress);

  std::string              id;
  GraphNodeMap             graph_nodes;
  int                      id_count = 0;
  std::vector<std::string> graph_order;
  BroadcastMap             broadcast_params;
  FlattenConfig            export_param;
};

} // namespace hesiod
