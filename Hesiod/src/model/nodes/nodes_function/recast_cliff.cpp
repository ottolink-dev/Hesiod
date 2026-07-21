/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_recast_cliff_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("talus_global", "talus_global", 1.f, 0.f, 5.f);
  node.add_attr<FloatAttribute>("radius", "radius", 0.1f, 0.01f, 0.5f);
  node.add_attr<FloatAttribute>("amplitude", "amplitude", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("gain", "gain", 2.f, 0.01f, 10.f);

  // attribute(s) order
  node.set_attr_ordered_key({"talus_global", "radius", "amplitude", "gain"});
}

void compute_recast_cliff_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    float talus = node.get_attr<FloatAttribute>("talus_global") / (float)p_out->shape.x;
    int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, talus, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out = *pa_in;

          hmap::recast_cliff(*pa_out,
                             talus,
                             ir,
                             node.get_attr<FloatAttribute>("amplitude"),
                             pa_mask,
                             node.get_attr<FloatAttribute>("gain"));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
