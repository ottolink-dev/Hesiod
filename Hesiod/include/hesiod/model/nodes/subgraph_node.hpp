/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gnode/subgraph.hpp"

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

class GraphNode; // forward

// =====================================
// SubGraphNode
// =====================================
class SubGraphNode : public BaseNode
{
public:
  SubGraphNode(const std::string &label, std::weak_ptr<GraphConfig> config);

  GraphNode                 *get_subgraph() const;
  std::shared_ptr<GraphNode> get_subgraph_shared() const;
  void                       set_subgraph(std::shared_ptr<GraphNode> new_subgraph);

  void auto_bind_ports();
  void compute_subgraph();
  void propagate_config_change() override;
  bool is_syncing_config() const { return this->syncing_config; }

  void on_pre_async_compute() override;
  void on_async_compute_cancelled() override;

  void           json_from(nlohmann::json const &json) override;
  nlohmann::json json_to() const override;

  const nlohmann::json &get_pending_subgraph_ui_json() const
  {
    return this->pending_subgraph_ui_json;
  }
  void set_pending_subgraph_ui_json(const nlohmann::json &json)
  {
    this->pending_subgraph_ui_json = json;
  }

  std::function<void()>           ports_updated;
  std::function<nlohmann::json()> subgraph_ui_provider;

  std::unordered_map<std::string, std::string> consume_port_renames()
  {
    return std::exchange(this->pending_port_renames, {});
  }

private:
  struct InputBinding
  {
    std::string                                name;
    std::shared_ptr<gnode::ISubGraphInputNode> node;
  };

  struct OutputBinding
  {
    std::string                                 name;
    std::shared_ptr<gnode::ISubGraphOutputNode> node;
  };

  std::shared_ptr<GraphNode>                   subgraph;
  std::vector<InputBinding>                    inputs;
  std::vector<OutputBinding>                   outputs;
  nlohmann::json                               pending_subgraph_ui_json;
  std::unordered_map<std::string, std::string> pending_port_renames;
  bool                                         syncing_config = false;
};

void setup_subgraph_node(BaseNode &node);
void compute_subgraph_node(BaseNode &node);

} // namespace hesiod
