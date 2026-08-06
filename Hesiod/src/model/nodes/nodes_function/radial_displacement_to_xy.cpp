/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/transform.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_radial_displacement_to_xy_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dr");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "dx", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "dy", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("smoothing", "smoothing", 1.f, 0.f, 10.f);
  node.add_attr<Vec2FloatAttribute>("center", "center");

  // attribute(s) order
  node.set_attr_ordered_key({"smoothing", "center"});
}

void compute_radial_displacement_to_xy_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_dr = node.get_value_ref<hmap::VirtualArray>("dr");

  if (p_dr)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");

    hmap::for_each_tile(
        {p_dr, p_dx, p_dy},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          hmap::Array *pa_dr = p_arrays[0];
          hmap::Array *pa_dx = p_arrays[1];
          hmap::Array *pa_dy = p_arrays[2];

          hmap::radial_displacement_to_xy(*pa_dr,
                                          *pa_dx,
                                          *pa_dy,
                                          node.get_attr<FloatAttribute>("smoothing"),
                                          node.get_attr<Vec2FloatAttribute>("center"),
                                          region.bbox);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
