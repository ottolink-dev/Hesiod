/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
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

constexpr const char *A_CURVATURE_CLAMP_MODE  = "curvature_clamp_mode";
constexpr const char *A_CURVATURE_CLAMPING    = "curvature_clamping";
constexpr const char *A_K_SATURATION          = "k_saturation";
constexpr const char *A_RMAX                  = "rmax";
constexpr const char *A_RMIN                  = "rmin";
constexpr const char *A_SATURATION_LIMIT      = "saturation_limit";
constexpr const char *A_SMALLER_SCALES_WEIGHT = "smaller_scales_weight";
constexpr const char *A_STEPS                 = "steps";

void setup_select_soil_rocks_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RMAX, "Max Radius", 0.1f, 0.f, 0.2f);
  add_float(node, A_RMIN, "Min Radius", 0.f, 0.f, 0.2f);
  add_int(node, A_STEPS, "Sampling Steps", 4, 2, 8);

  add_float(node, A_SMALLER_SCALES_WEIGHT, "Smaller-Scale Influence", 1.f, 0.f, 2.f);
  add_enum(node,
           A_CURVATURE_CLAMP_MODE,
           "Clamp Mode",
           enum_mappings.clamping_mode_map,
           "Keep positive & clamp");
  add_float(node, A_CURVATURE_CLAMPING, "Clamp Limit", 1.f, 0.f, FLT_MAX, "{:.4f}");
  add_float(node, A_SATURATION_LIMIT, "Saturation Limit", 0.3f, 0.f, 1.f);
  add_float(node, A_K_SATURATION, "Saturation Smoothing", 0.1f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_soil_rocks_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    // --- selector

    int nx     = p_out->shape.x;
    int ir_min = (int)(node.val<float>(A_RMIN) * nx);
    int ir_max = std::max(1, (int)(node.val<float>(A_RMAX) * nx));

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir_min, ir_max](std::vector<hmap::Array *> p_arrays,
                                const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          auto mode = static_cast<hmap::ClampMode>(node.val<int>(A_CURVATURE_CLAMP_MODE));

          *pa_out = hmap::gpu::select_soil_rocks(*pa_in,
                                                 ir_max,
                                                 ir_min,
                                                 node.val<int>(A_STEPS),
                                                 node.val<float>(A_SMALLER_SCALES_WEIGHT),
                                                 mode,
                                                 node.val<float>(A_CURVATURE_CLAMPING));
        },
        node.cfg().cm_gpu);

    // post-process
    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);

    // --- saturate

    float hmin = p_out->min(node.cfg().cm_cpu);
    float hmax = p_out->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          hmap::saturate(*pa_out,
                         0.f,
                         node.val<float>(A_SATURATION_LIMIT),
                         hmin,
                         hmax,
                         node.val<float>(A_K_SATURATION));
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
