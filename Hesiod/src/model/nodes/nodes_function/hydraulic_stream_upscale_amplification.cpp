/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{

void setup_hydraulic_stream_upscale_amplification_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("c_erosion", "c_erosion", 0.01f, 0.001f, 0.1f);
  node.add_attr<FloatAttribute>("talus_ref", "talus_ref", 0.1f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("radius", "radius", 0.f, 0.f, 0.05f);
  node.add_attr<FloatAttribute>("clipping_ratio", "clipping_ratio", 10.f, 0.1f, 100.f);
  node.add_attr<IntAttribute>("upscaling_levels", "upscaling_levels", 1, 0, 4);
  node.add_attr<FloatAttribute>("persistence", "Persistence", 0.5f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"c_erosion",
                             "talus_ref",
                             "radius",
                             "clipping_ratio",
                             "_SEPARATOR_",
                             "upscaling_levels",
                             "persistence"});
}

void compute_hydraulic_stream_upscale_amplification_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out = *pa_in;

          hmap::hydraulic_stream_upscale_amplification(
              *pa_out,
              pa_mask,
              node.get_attr<FloatAttribute>("c_erosion"),
              node.get_attr<FloatAttribute>("talus_ref"),
              node.get_attr<IntAttribute>("upscaling_levels"),
              node.get_attr<FloatAttribute>("persistence"),
              ir,
              node.get_attr<FloatAttribute>("clipping_ratio"));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();
  }
}

} // namespace hesiod
