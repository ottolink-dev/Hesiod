/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/morphology.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIUS       = "radius";
constexpr const char *A_RIDGE_SELECT = "ridge_select";

void setup_select_valley_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS, "radius", 0.05f, 0.001f, 0.5f);
  add_bool(node, A_RIDGE_SELECT, "ridge_select", false);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_valley_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  int ir = std::max(1, (int)(node.val<float>(A_RADIUS) * p_out->shape.x));

  hmap::for_each_tile(
      {p_out, p_in},
      [&node, ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        *pa_out = hmap::gpu::select_valley(*pa_in, ir, node.val<bool>(A_RIDGE_SELECT));
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // post-process
  post_apply_saturate_percentile(node, *p_out, 0.f, 0.95f);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
