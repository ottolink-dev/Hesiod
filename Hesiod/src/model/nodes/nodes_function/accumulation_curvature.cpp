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

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIUS    = "radius";
constexpr const char *A_CLAMP_MAX = "clamp_max";
constexpr const char *A_VC_MAX    = "vc_max";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_accumulation_curvature_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Main Parameters");
  add_float(node, A_RADIUS, "Radius", 0.02f, 0.f, 0.2f);
  add_bool(node, A_CLAMP_MAX, "Clamp Maximum", false);
  add_float(node, A_VC_MAX, "Max Curvature Value", 0.05f, 0.f, 0.2f);

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

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  const auto radius    = node.val<float>(A_RADIUS);
  const auto clamp_max = node.val<bool>(A_CLAMP_MAX);
  const auto vc_max    = node.val<float>(A_VC_MAX);

  const int ir = std::max(1, (int)(radius * p_out->shape.x));
  const int nx = p_out->shape.x; // for gradient scaling

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in},
      [ir, nx, clamp_max, vc_max](std::vector<hmap::Array *> p_arrays,
                                  const hmap::TileRegion &)
      {
        auto [pa_out, pa_in] = unpack<2>(p_arrays);
        // nx^2 is gradient scaling...
        *pa_out = nx * nx *
                  hmap::gpu::curvature_quadric(*pa_in,
                                               ir,
                                               hmap::CurvatureType::CT_ACCUMULATION);

        // truncate high values if requested
        if (clamp_max)
          hmap::clamp_max(*pa_out, vc_max);
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
