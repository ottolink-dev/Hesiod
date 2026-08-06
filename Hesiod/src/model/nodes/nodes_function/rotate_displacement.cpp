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

void setup_rotate_displacement_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "delta");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "dx", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "dy", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("angle", "angle", 0.f, -180.f, 180.f);

  // attribute(s) order
  node.set_attr_ordered_key({"angle"});
}

void compute_rotate_displacement_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("delta");

  if (p_in)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");

    hmap::for_each_tile(
        {p_in, p_dx, p_dy},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_in = p_arrays[0];
          hmap::Array *pa_dx = p_arrays[1];
          hmap::Array *pa_dy = p_arrays[2];

          hmap::rotate_displacement(*pa_in,
                                    node.get_attr<FloatAttribute>("angle"),
                                    *pa_dx,
                                    *pa_dy);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
