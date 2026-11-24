/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <string>

#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// =====================================
// NodeInNode
// =====================================
class NodeInNode : public BaseNode
{
public:
  NodeInNode(const std::string &label, std::weak_ptr<GraphConfig> config);

  std::string get_slot_name() const;
  std::string get_data_type_name() const;
  void        on_settings_changed() override;
  void        rebuild_port();
  void        json_from(nlohmann::json const &json) override;

  std::function<void()> ports_updated;
  std::function<void()> boundary_changed;
};

void setup_node_in_node(BaseNode &node);
void compute_node_in_node(BaseNode &node);

} // namespace hesiod
