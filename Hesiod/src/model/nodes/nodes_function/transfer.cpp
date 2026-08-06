/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_transfer_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "source");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "target");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius", "radius", 0.05f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("amplitude", "amplitude", 0.5f, -2.f, 4.f);
  node.add_attr<BoolAttribute>("target_prefiltering", "target_prefiltering", false);

  // attribute(s) order
  node.set_attr_ordered_key({"radius", "amplitude", "target_prefiltering"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_transfer_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_s = node.get_value_ref<hmap::VirtualArray>("source");
  hmap::VirtualArray *p_t = node.get_value_ref<hmap::VirtualArray>("target");

  if (p_s && p_t)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int ir = std::max(1, (int)(node.get_attr<FloatAttribute>("radius") * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_s, p_t},
        [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_s = p_arrays[1];
          hmap::Array *pa_t = p_arrays[2];

          *pa_out = hmap::gpu::transfer(
              *pa_s,
              *pa_t,
              ir,
              node.get_attr<FloatAttribute>("amplitude"),
              node.get_attr<BoolAttribute>("target_prefiltering"));
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
