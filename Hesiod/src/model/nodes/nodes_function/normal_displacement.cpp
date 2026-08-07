/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_normal_displacement_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.05f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("amount", "amount", 5.f, 0.f, 20.f);
  node.add_attr<BoolAttribute>("reverse", "reverse", false);
  node.add_attr<IntAttribute>("iterations", "iterations", 3, 1, 10);

  // attribute(s) order
  node.set_attr_ordered_key({"radius", "amount", "reverse", "iterations"});
}

void compute_normal_displacement_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(0, (int)(node.get_attr<FloatAttribute>("radius") * p_in->shape.x));

    for (int it = 0; it < node.get_attr<IntAttribute>("iterations"); it++)
    {
      hmap::for_each_tile(
          {p_out, p_in, p_mask},
          [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
          {
            auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

            *pa_out = *pa_in;

            hmap::gpu::normal_displacement(*pa_out,
                                           pa_mask,
                                           node.get_attr<FloatAttribute>("amount"),
                                           ir,
                                           node.get_attr<BoolAttribute>("reverse"));
          },
          node.cfg().cm_gpu);

      p_out->smooth_overlap_buffers();
    }
  }
}

} // namespace hesiod
