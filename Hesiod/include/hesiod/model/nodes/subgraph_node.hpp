/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <string>

#include "gnode/subgraph.hpp"
#include "highmap/coord_frame.hpp"
#include "hesiod/model/graph/graph_config.hpp"
#include "hesiod/model/nodes/base_node.hpp"

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
  void expose_input(const std::shared_ptr<gnode::NodeIn<T>> &node_in)
  {
    if (this->subgraph_impl)
      this->subgraph_impl->expose_input<T>(node_in);
  }

  template <typename T>
  void expose_output(const std::shared_ptr<gnode::NodeOut<T>> &node_out)
  {
    if (this->subgraph_impl)
      this->subgraph_impl->expose_output<T>(node_out);
  }

  // --- Compute override ---
  void compute() override;

  // --- Serialization ---
  void           json_from(nlohmann::json const &json) override;
  nlohmann::json json_to() const override;

private:
  std::shared_ptr<GraphNode>           subgraph;
  std::shared_ptr<gnode::SubGraphNode> subgraph_impl;
};

} // namespace hesiod
