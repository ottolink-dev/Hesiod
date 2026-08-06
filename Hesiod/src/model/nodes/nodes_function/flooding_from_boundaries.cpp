/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_flooding_from_boundaries_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("elevation", "elevation", 0.2f, -1.f, 2.f);
  node.add_attr<BoolAttribute>("from_east", "from_east", true);
  node.add_attr<BoolAttribute>("from_west", "from_west", true);
  node.add_attr<BoolAttribute>("from_north", "from_north", true);
  node.add_attr<BoolAttribute>("from_south", "from_south", true);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"elevation", "from_east", "from_west", "from_north", "from_south"});
}

void compute_flooding_from_boundaries_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("water_depth");

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::flooding_from_boundaries(
              *pa_in,
              node.get_attr<FloatAttribute>("elevation"),
              node.get_attr<BoolAttribute>("from_east"),
              node.get_attr<BoolAttribute>("from_west"),
              node.get_attr<BoolAttribute>("from_north"),
              node.get_attr<BoolAttribute>("from_south"));
        },
        node.cfg().cm_single_array); // forced, not tileable
  }
}

} // namespace hesiod
