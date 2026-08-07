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

void setup_zoom_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("zoom_factor", "zoom_factor", 2.f, 1.f, 10.f);
  node.add_attr<BoolAttribute>("periodic", "periodic", false);
  node.add_attr<Vec2FloatAttribute>("center", "center");
  node.add_attr<BoolAttribute>("remap", "remap", false);

  // attribute(s) order
  node.set_attr_ordered_key({"zoom_factor", "periodic", "center", "remap"});
}

void compute_zoom_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_dx  = node.get_value_ref<hmap::VirtualArray>("dx");
    hmap::VirtualArray *p_dy  = node.get_value_ref<hmap::VirtualArray>("dy");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in, p_dx, p_dy},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_dx, pa_dy] = unpack<4>(p_arrays);

          *pa_out = hmap::zoom(*pa_in,
                               node.get_attr<FloatAttribute>("zoom_factor"),
                               node.get_attr<BoolAttribute>("periodic"),
                               node.get_attr<Vec2FloatAttribute>("center"),
                               pa_dx,
                               pa_dy);
        },
        node.cfg().cm_single_array);

    if (node.get_attr<BoolAttribute>("remap"))
      p_out->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
