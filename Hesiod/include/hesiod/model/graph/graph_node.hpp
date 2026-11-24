/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "nlohmann/json.hpp"

#include "gnode/graph.hpp"
#include "gnode/node.hpp"
#include "highmap/coord_frame.hpp"

#include "hesiod/model/graph/graph_config.hpp"

namespace gnode
{
class GraphWorker;
}

namespace hesiod
{

class BaseNode; // forward
class NodeInNode;
class NodeOutNode;
class TerrainMergeNode;

// =====================================
// GraphNode
// =====================================
class GraphNode : public gnode::Graph, public gnode::Node, public hmap::CoordFrame
{
public:
  GraphNode() = delete;
  GraphNode(const std::string &id, const std::shared_ptr<GraphConfig> &config);

  std::shared_ptr<GraphNode> get_shared();

  // --- Identity (disambiguate gnode::Graph and gnode::Node) ---
  std::string get_id() const { return gnode::Graph::get_id(); }
  void        set_id(const std::string &new_id);

  // --- Serialization ---
  void json_from(nlohmann::json const &json, GraphConfig *p_input_config = nullptr);
  nlohmann::json json_to() const;

  // --- Config ---
  GraphConfig *get_config_ref();
  void         change_config_values(const GraphConfig &new_config);

  // --- Node Factory ---
  std::string add_node(const std::string &node_type, const std::string &id = "");

  // --- gnode::Graph overrides ---
  std::string  add_node(const std::shared_ptr<gnode::Node> &p_node,
                        const std::string                  &id = "") override;
  virtual void remove_node(const std::string &id) override;
  void         update() override;
  void         update(const std::vector<std::string> &node_ids) override;
  void         update(const std::string &node_id) override;

  // --- gnode::Node override (global graph execution) ---
  void compute() override;

  // --- Dynamic Boundary Ports (NodeIn / NodeOut) ---
  void auto_bind_ports();
  void pull_inputs_from_parent();
  void push_outputs_to_parent();

  std::unordered_map<std::string, std::string> consume_port_renames();

  // --- Background Update (GraphWorker of the GraphManager, if any) ---
  void set_p_worker(gnode::GraphWorker *new_p_worker) { this->p_worker = new_p_worker; }
  void stop_update();

  // --- Others ---
  void        reseed(bool backward);
  std::string runtime_info_to_string(const char separator = ',') const;

  // --- Compute Callbacks ---
  std::function<void(const std::string &node_id)> compute_started;
  std::function<void(const std::string &node_id)> compute_finished;

  // --- Update Callbacks ---
  std::function<void()>                                           update_started;
  std::function<void()>                                           graph_update_finished;
  std::function<void(const std::string &node_id, float progress)> update_progress;
  std::function<void(const std::vector<std::string> &pass, int done)> update_steps;
  std::function<void(bool held)>                                      update_held;

  // --- Global Graph Callbacks ---
  std::function<void()>                    ports_updated;
  std::function<void(const std::string &)> global_output_updated;

private:
  struct InputBinding
  {
    std::string                       name;
    std::shared_ptr<NodeInNode>       node;
    std::shared_ptr<TerrainMergeNode> merge_node;
    int                               merge_slot = -1;
  };

  struct OutputBinding
  {
    std::string                  name;
    std::shared_ptr<NodeOutNode> node;
  };

  void setup_boundary_node(BaseNode *p_node);
  void handle_inner_update_finished();

  // --- Members ---
  std::shared_ptr<GraphConfig> config;
  gnode::EventConnection       graph_progress_conn;
  gnode::GraphWorker          *p_worker = nullptr; // owned by GraphManager
  std::vector<InputBinding>    inputs;
  std::vector<OutputBinding>   outputs;
  std::unordered_map<std::string, std::string> pending_port_renames;
};

} // namespace hesiod
