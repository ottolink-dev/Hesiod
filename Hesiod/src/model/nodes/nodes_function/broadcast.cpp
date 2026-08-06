/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/broadcast_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_broadcast_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "thru", CONFIG(node));

  // attribute(s)

  bool read_only = true; // tag is indeed set automatically
  node.add_attr<StringAttribute>("tag", "tag", "UNDEFINED", read_only);
}

void compute_broadcast_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_thru = node.get_value_ref<hmap::VirtualArray>("thru");
    p_thru->copy_from(*p_in, node.get_config_ref()->cm_cpu);

    BroadcastNode *p_broadcast_node = dynamic_cast<BroadcastNode *>(&node);

    if (!p_broadcast_node)
    {
      Logger::log()->error("compute_receive_node: Failed to cast to BroadcastNode");
      return;
    }

    std::string broadcast_tag = p_broadcast_node->get_broadcast_tag();
    std::string graph_id = node.get_graph_id();
    std::string node_id = node.get_id();

    Logger::log()->trace("compute_broadcast_node: broadcasting graph: {}, node: {}",
                         graph_id,
                         node_id);

    Logger::log()->trace("broadcast_tag: {}", broadcast_tag);

    // this goes to the graph editor
    if (p_broadcast_node->broadcast_node_updated)
      p_broadcast_node->broadcast_node_updated(graph_id, broadcast_tag);
  }
}

} // namespace hesiod
