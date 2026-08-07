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

void setup_smooth_fill_holes_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.05f, 0.001f, 0.2f);

  // attribute(s) order
  node.set_attr_ordered_key({"radius"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_smooth_fill_holes_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>("output");

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out                       = *pa_in;

          hmap::gpu::smooth_fill_holes(*pa_out, ir, pa_mask);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
