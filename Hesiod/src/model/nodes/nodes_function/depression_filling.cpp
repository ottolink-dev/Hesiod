/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_depression_filling_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "fill map", CONFIG(node));

  // attribute(s)
  node.add_attr<BoolAttribute>("remap fill map", "remap fill map", true);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Main Parameters", "remap fill map", "_GROUPBOX_END_"});
}

void compute_depression_filling_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_fill_map = node.get_value_ref<hmap::VirtualArray>("fill map");

    hmap::for_each_tile(
        {p_out, p_in, p_fill_map},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_fill_map] = unpack<3>(p_arrays);

          *pa_out = *pa_in;

          hmap::depression_filling_priority_flood(*pa_out);

          *pa_fill_map = *pa_out - *pa_in;
        },
        node.cfg().cm_single_array); // forced, not tileable

    if (node.get_attr<BoolAttribute>("remap fill map"))
      p_fill_map->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
