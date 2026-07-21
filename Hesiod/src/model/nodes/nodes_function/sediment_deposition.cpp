/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_sediment_deposition_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "deposition", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("talus_global", "talus_global", 0.2f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>("max_deposition", "max_deposition", 0.001f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("duration", "Duration", 0.3f, 0.05f, 6.f);
  node.add_attr<BoolAttribute>("scale_talus_with_elevation",
                               "scale_talus_with_elevation",
                               true);
  node.add_attr<IntAttribute>("subiterations", "subiterations", 10, 1, 50);

  // attribute(s) order
  node.set_attr_ordered_key({"talus_global",
                             "max_deposition",
                             "duration",
                             "scale_talus_with_elevation",
                             "subiterations"});
}

void compute_sediment_deposition_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_deposition_map = node.get_value_ref<hmap::VirtualArray>(
        "deposition");

    float talus = node.get_attr<FloatAttribute>("talus_global") / (float)p_out->shape.x;

    hmap::VirtualArray talus_map = hmap::VirtualArray(CONFIG(node));
    talus_map.fill(talus, node.cfg().cm_cpu);

    if (node.get_attr<BoolAttribute>("scale_talus_with_elevation"))
    {
      talus_map.copy_from(*p_in, node.cfg().cm_cpu);
      talus_map.remap(talus / 100.f, talus, node.cfg().cm_cpu);
    }

    int iterations = int(node.get_attr<FloatAttribute>("duration") * p_out->shape.x /
                         node.get_attr<IntAttribute>("subiterations"));

    hmap::for_each_tile(
        {p_out, p_in, p_mask, &talus_map, p_deposition_map},
        [&node, &talus, iterations](std::vector<hmap::Array *> p_arrays,
                                    const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];
          hmap::Array *pa_mask = p_arrays[2];
          hmap::Array *pa_talus = p_arrays[3];
          hmap::Array *pa_deposition_map = p_arrays[4];

          *pa_out = *pa_in;

          hmap::gpu::sediment_deposition(*pa_out,
                                         pa_mask,
                                         *pa_talus,
                                         pa_deposition_map,
                                         node.get_attr<FloatAttribute>("max_deposition"),
                                         node.get_attr<IntAttribute>("subiterations"),
                                         iterations);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    if (p_deposition_map)
    {
      p_deposition_map->smooth_overlap_buffers();
      p_deposition_map->remap(0.f, 1.f, node.cfg().cm_cpu);
    }
  }
}

} // namespace hesiod
