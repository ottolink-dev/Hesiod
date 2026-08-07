/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_hydraulic_stream_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "erosion", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("c_erosion", "c_erosion", 0.05f, 0.01f, 0.1f);
  node.add_attr<FloatAttribute>("talus_ref", "talus_ref", 0.1f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("radius", "radius", 0.f, 0.f, 0.05f);
  node.add_attr<FloatAttribute>("clipping_ratio", "clipping_ratio", 10.f, 0.1f, 100.f);

  // attribute(s) order
  node.set_attr_ordered_key({"c_erosion", "talus_ref", "radius", "clipping_ratio"});
}

void compute_hydraulic_stream_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out         = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_mask        = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_erosion_map = node.get_value_ref<hmap::VirtualArray>("erosion");

    int ir = (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in, p_mask, p_erosion_map},
        [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out         = p_arrays[0];
          hmap::Array *pa_in          = p_arrays[1];
          hmap::Array *pa_mask        = p_arrays[2];
          hmap::Array *pa_erosion_map = p_arrays[3];

          *pa_out = *pa_in;

          hmap::hydraulic_stream(*pa_out,
                                 pa_mask,
                                 node.get_attr<FloatAttribute>("c_erosion"),
                                 node.get_attr<FloatAttribute>("talus_ref"),
                                 nullptr,
                                 nullptr,
                                 pa_erosion_map,
                                 ir,
                                 node.get_attr<FloatAttribute>("clipping_ratio"));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    p_erosion_map->smooth_overlap_buffers();
    p_erosion_map->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
