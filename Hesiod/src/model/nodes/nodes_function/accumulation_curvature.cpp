/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/curvature.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIUS = "radius";
constexpr const char *A_CLAMP_MAX = "clamp_max";
constexpr const char *A_VC_MAX = "vc_max";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_accumulation_curvature_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  node.set_current_category("Main Parameters");
  add_float(node, A_RADIUS, "radius", 0.02f, 0.f, 0.2f);
  add_bool(node, A_CLAMP_MAX, "clamp_max", false);
  add_float(node, A_VC_MAX, "vc_max", 0.05f, 0.f, 0.2f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_accumulation_curvature_node(BaseNode &node)
{
  Logger::log()->error("AccumulationCurvature node is deprecated, use Curvatures node");

  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = std::max(1, (int)(node.val<float>(A_RADIUS) * p_out->shape.x));
    int nx = p_out->shape.x; // for gradient scaling

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir, nx](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          // nx^2 is gradient scaling...
          *pa_out = nx * nx *
                    hmap::gpu::curvature_quadric(*pa_in,
                                                 ir,
                                                 hmap::CurvatureType::CT_ACCUMULATION);

          // truncate high values if requested
          if (node.val<bool>(A_CLAMP_MAX))
            hmap::clamp_max(*pa_out, node.val<float>(A_VC_MAX));
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
