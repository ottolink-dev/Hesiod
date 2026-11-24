/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/array.hpp"
#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"
#include "highmap/world/flora/forest.hpp"
#include "highmap/world/rocks/rock_field.hpp"

#include "gnode/graph.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/boundary_types.hpp"
#include "hesiod/model/nodes/node_in_node.hpp"

namespace hesiod
{

NodeInNode::NodeInNode(const std::string &label, std::weak_ptr<GraphConfig> config)
    : BaseNode(label, config)
{
}

std::string NodeInNode::get_slot_name() const
{
  return this->val<std::string>("slot_name");
}

std::string NodeInNode::get_data_type_name() const
{
  return this->val<std::string>("data_type");
}

void NodeInNode::json_from(nlohmann::json const &json)
{
  BaseNode::json_from(json);
  this->rebuild_port();
}

void NodeInNode::rebuild_port()
{
  if (gnode::Graph *g = this->get_p_graph())
  {
    std::vector<gnode::Link> to_remove;
    for (const auto &l : g->get_links())
      if (l.from == this->get_id())
        to_remove.push_back(l);
    for (const auto &l : to_remove)
      g->remove_link(l.from, l.port_from, l.to, l.port_to);
  }

  if (this->has_port("value"))
    this->remove_port("value");
  if (this->has_port("mask"))
    this->remove_port("mask");

  visit_boundary_type(
      this->get_data_type_name(),
      [this]<typename T>()
      {
        if constexpr (std::is_same_v<T, hmap::VirtualArray>)
          this->add_port<T>(gnode::PortType::OUT, "value", CONFIG((*this)));
        else if constexpr (std::is_same_v<T, hmap::VirtualTexture>)
          this->add_port<T>(gnode::PortType::OUT, "value", CONFIG_TEX((*this)));
        else if constexpr (std::is_same_v<T, hmap::Array>)
          this->add_port<T>(gnode::PortType::OUT, "value", this->cfg().shape);
        else
          this->add_port<T>(gnode::PortType::OUT, "value");
      });

  this->add_port<hmap::VirtualArray>(gnode::PortType::OUT, "mask", CONFIG((*this)));
}

void NodeInNode::on_settings_changed()
{
  if (this->get_nports() == 0 ||
      map_type_name(this->get_data_type(0)) != this->get_data_type_name())
  {
    this->rebuild_port();
    if (this->ports_updated)
      this->ports_updated();
  }

  if (this->boundary_changed)
    this->boundary_changed();
}

void setup_node_in_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  node.set_current_category("Boundary");
  add_string(node, "slot_name", "Port Name", "input_0");
  add_choice(node, "data_type", "Data Type", boundary_data_types(), "VirtualArray");

  if (auto *p_in = dynamic_cast<NodeInNode *>(&node))
    p_in->rebuild_port();
  else
    node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "value", CONFIG(node));
}

void compute_node_in_node(BaseNode & /* node */)
{
  // Data is populated by GraphNode::pull_inputs_from_parent()
}

} // namespace hesiod
