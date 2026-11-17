/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>

#include "gnode/subgraph.hpp"

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// =====================================
// NodeOutNode
// =====================================
class NodeOutNode : public BaseNode, public gnode::ISubGraphOutputNode
{
public:
  NodeOutNode(const std::string &label, std::weak_ptr<GraphConfig> config);

  std::string get_slot_name() const override;
  std::string get_data_type_name() const;
  void        rebuild_port();
  void        json_from(nlohmann::json const &json) override;

  void register_on_parent(gnode::Node       *parent_node,
                          const std::string &port_label) const override;

  void push_to_parent(gnode::Node *parent_node, const std::string &port_label) override;

  std::function<void()> ports_updated;
};

void setup_node_out_node(BaseNode &node);
void compute_node_out_node(BaseNode &node);

} // namespace hesiod
