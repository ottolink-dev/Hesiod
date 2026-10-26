/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "gnode/subgraph.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "highmap/coord_frame.hpp"

namespace hesiod
{

class GraphNode; // forward

// =====================================
// SubGraphNode
// =====================================
class SubGraphNode : public BaseNode, public hmap::CoordFrame
{
public:
  SubGraphNode(const std::string                &label,
               std::weak_ptr<GraphConfig>        parent_config,
               const std::shared_ptr<GraphNode> &subgraph = nullptr);

  virtual ~SubGraphNode() = default;

  // --- Subgraph accessors ---
  std::shared_ptr<GraphNode> get_subgraph() const;
  void                       set_subgraph(const std::shared_ptr<GraphNode> &new_subgraph);

  // --- External port binding ---
  void auto_bind_ports();

  template <typename T>
  void expose_input(const std::shared_ptr<gnode::ISubGraphInputNode> &node_in)
  {
    const std::string &name = node_in->get_slot_name();
    if (!this->has_port(name))
      this->add_port<T>(gnode::PortType::IN, name, CONFIG((*this)));
    this->inputs.push_back({name, node_in});
  }

  template <typename T>
  void expose_output(const std::shared_ptr<gnode::ISubGraphOutputNode> &node_out)
  {
    const std::string &name = node_out->get_slot_name();
    if (!this->has_port(name))
      this->add_port<T>(gnode::PortType::OUT, name, CONFIG((*this)));
    this->outputs.push_back({name, node_out});
  }

  // --- Compute override ---
  void compute() override;

  // --- Serialization ---
  void           json_from(nlohmann::json const &json) override;
  nlohmann::json json_to() const override;

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

  std::shared_ptr<GraphNode> subgraph;
  std::vector<InputBinding>  inputs;
  std::vector<OutputBinding> outputs;
};

} // namespace hesiod
