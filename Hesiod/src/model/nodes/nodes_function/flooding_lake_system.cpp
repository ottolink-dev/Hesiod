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

void setup_flooding_lake_system_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("mininal_radius", "mininal_radius", 0.05f, 0.f, 0.5f);

  // attribute(s) order
  node.set_attr_ordered_key(
      {"_GROUPBOX_BEGIN_Exclusion filter", "mininal_radius", "_GROUPBOX_END_"});
}

void compute_flooding_lake_system_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("elevation");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("water_depth");

    int   ir = node.get_attr<FloatAttribute>("mininal_radius") * (float)p_in->shape.x;
    float surface_threshold = M_PI * ir * ir;

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, surface_threshold](std::vector<hmap::Array *> p_arrays,
                                   const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::flooding_lake_system(*pa_in, surface_threshold);
        },
        node.cfg().cm_single_array); // forced, not tileable
  }
}

} // namespace hesiod
