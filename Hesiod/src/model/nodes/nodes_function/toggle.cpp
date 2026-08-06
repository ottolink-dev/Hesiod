/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{

void setup_toggle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input A");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input B");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<BoolAttribute>("toggle", "toggle", "A", "B", true);

  // attribute(s) order
  node.set_attr_ordered_key({"toggle"});
}

void compute_toggle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in_a = node.get_value_ref<hmap::VirtualArray>("input A");
  hmap::VirtualArray *p_in_b = node.get_value_ref<hmap::VirtualArray>("input B");

  if (p_in_a && p_in_b)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // copy the either A or B input heightmap based on the toggle state
    hmap::for_each_tile(
        {p_out, p_in_a, p_in_b},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in_a = p_arrays[1];
          hmap::Array *pa_in_b = p_arrays[2];

          if (node.get_attr<BoolAttribute>("toggle"))
            *pa_out = *pa_in_a;
          else
            *pa_out = *pa_in_b;
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
