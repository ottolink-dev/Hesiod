/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/blending.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_OUT    = "output";
constexpr const char *P_SOURCE = "source";
constexpr const char *P_TARGET = "target";

constexpr const char *A_AMPLITUDE           = "amplitude";
constexpr const char *A_RADIUS              = "radius";
constexpr const char *A_TARGET_PREFILTERING = "target_prefiltering";

void setup_transfer_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SOURCE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_TARGET);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.05f, 0.f, 0.2f);
  add_float(node, A_AMPLITUDE, "amplitude", 0.5f, -2.f, 4.f);
  add_bool(node, A_TARGET_PREFILTERING, "target_prefiltering", false);

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

void compute_transfer_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_s = node.get_value_ref<hmap::VirtualArray>(P_SOURCE);
  hmap::VirtualArray *p_t = node.get_value_ref<hmap::VirtualArray>(P_TARGET);

  if (p_s && p_t)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = std::max(1, (int)(node.val<float>(A_RADIUS) * p_out->shape.x));

    hmap::for_each_tile(
        {p_out, p_s, p_t},
        [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_s   = p_arrays[1];
          hmap::Array *pa_t   = p_arrays[2];

          *pa_out = hmap::gpu::transfer(*pa_s,
                                        *pa_t,
                                        ir,
                                        node.val<float>(A_AMPLITUDE),
                                        node.val<bool>(A_TARGET_PREFILTERING));
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
