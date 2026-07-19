/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/compat_attributes.hpp"

#include "highmap/interpolate/interpolate_array.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/nodes/receive_node.hpp"

using namespace attr;

namespace hesiod
{

void setup_receive_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)

  // nothing by default, this is going to be updated and populated
  // automatically by the graph manager
  std::vector<std::string> choices = {"NO TAG AVAILABLE"};
  node.add_attr<ChoiceAttribute>("tag", "tag", choices, choices.front());

  // adjust corresponding widget
  node.get_attr_ref<ChoiceAttribute>("tag")->set_use_combo_list(true);
}

void compute_receive_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
  std::string         tag = node.get_attr<ChoiceAttribute>("tag");

  // cast to specialized node
  ReceiveNode *p_receive_node = dynamic_cast<ReceiveNode *>(&node);

  if (!p_receive_node)
  {
    Logger::log()->error("compute_receive_node: Failed to cast to ReceiveNode");
    return;
  }

  if (!p_receive_node->get_p_broadcast_params())
  {
    // at node construction, node may be computed without the graph
    // infos being set
    Logger::log()->trace("compute_receive_node: p_broadcast_params {}",
                         p_receive_node->get_p_broadcast_params() ? "ok" : "NOK");
    return;
  }

  if (p_receive_node->get_p_broadcast_params()->empty())
  {
    Logger::log()->trace("compute_receive_node: empty map");
    return;
  }

  if (p_receive_node->get_p_broadcast_params()->contains(tag))
  {
    BroadcastParam broadcast_param = p_receive_node->get_p_broadcast_params()->at(tag);

    // retrieve various pointers for this broadcast
    const hmap::CoordFrame   *t_source = broadcast_param.t_source;
    const hmap::VirtualArray *p_va = broadcast_param.p_va;
    hmap::CoordFrame         *t_target = p_receive_node->get_p_coord_frame();

    if (t_source && p_va && t_target)
    {
      hmap::interpolate_heightmap(*p_va, *p_out, *t_source, *t_target, node.cfg().cm_cpu);
    }
    else
    {
      Logger::log()->warn("broadcast inputs not available (nullptr)");
    }
  }
  else
  {
    Logger::log()->trace("tag {} not available in broadcast_param", tag);
  }
}

} // namespace hesiod
