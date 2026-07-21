/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_blend_poisson_bf_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 1");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input 2");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<IntAttribute>("iterations", "iterations", 500, 1, INT_MAX);

  // attribute(s) order
  node.set_attr_ordered_key({"iterations"});

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_blend_poisson_bf_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1 = node.get_value_ref<hmap::VirtualArray>("input 1");
  hmap::VirtualArray *p_in2 = node.get_value_ref<hmap::VirtualArray>("input 2");

  if (p_in1 && p_in2)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");

    hmap::for_each_tile(
        {p_out, p_in1, p_in2, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in1 = p_arrays[1];
          hmap::Array *pa_in2 = p_arrays[2];
          hmap::Array *pa_mask = p_arrays[3];

          *pa_out = hmap::gpu::blend_poisson_bf(*pa_in1,
                                                *pa_in2,
                                                node.get_attr<IntAttribute>("iterations"),
                                                pa_mask);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
