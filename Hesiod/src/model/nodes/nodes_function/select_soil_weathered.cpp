/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
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

constexpr const char *A_CURVATURE_CLAMP_MODE = "curvature_clamp_mode";
constexpr const char *A_CURVATURE_CLAMPING   = "curvature_clamping";
constexpr const char *A_CURVATURE_WEIGHT     = "curvature_weight";
constexpr const char *A_GRADIENT_GAIN        = "gradient_gain";
constexpr const char *A_GRADIENT_WEIGHT      = "gradient_weight";
constexpr const char *A_RADIUS_CURVATURE     = "radius_curvature";
constexpr const char *A_RADIUS_GRADIENT      = "radius_gradient";

void setup_select_soil_weathered_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_RADIUS_CURVATURE, "Curvature Radius", 0.f, 0.f, 0.1f);
  add_float(node, A_RADIUS_GRADIENT, "Gradient Radius", 0.005f, 0.f, 0.1f);
  add_float(node, A_GRADIENT_GAIN, "Gradient Gain", 1.f, 0.01f, 10.f);
  add_float(node, A_CURVATURE_WEIGHT, "Curvature Weight", 1.f, -1.f, 1.f);
  add_float(node, A_GRADIENT_WEIGHT, "Gradient Weight", 0.2f, -1.f, 1.f);
  add_enum(node,
           A_CURVATURE_CLAMP_MODE,
           "Curvature Clamp Mode",
           enum_mappings.clamping_mode_map,
           "Keep positive & clamp");
  add_float(node,
            A_CURVATURE_CLAMPING,
            "Curvature Clamp Limit",
            1.f,
            0.f,
            FLT_MAX,
            "{:.4f}");

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_soil_weathered_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int nx      = p_out->shape.x; // for gradient scaling
    int ir_curv = (int)(node.val<float>(A_RADIUS_CURVATURE) * nx);
    int ir_grad = std::max(1, (int)(node.val<float>(A_RADIUS_GRADIENT) * nx));

    // --- compute gradient norm

    hmap::VirtualArray grad_norm(CONFIG(node));

    hmap::for_each_tile(
        {&grad_norm, p_in},
        [&node, ir_grad](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::gpu::morphological_gradient(*pa_in, ir_grad);
        },
        node.cfg().cm_gpu);

    grad_norm.remap(0.f, 1.f, node.cfg().cm_cpu);

    // gain
    hmap::for_each_tile(
        {&grad_norm},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          hmap::gain(*pa_out, node.val<float>(A_GRADIENT_GAIN));
        },
        node.cfg().cm_cpu);

    // --- compute mask

    hmap::for_each_tile(
        {p_out, p_in, &grad_norm},
        [&node, nx, ir_curv, ir_grad](std::vector<hmap::Array *> p_arrays,
                                      const hmap::TileRegion &)
        {
          hmap::Array *pa_out       = p_arrays[0];
          hmap::Array *pa_in        = p_arrays[1];
          hmap::Array *pa_grad_norm = p_arrays[2];

          auto mode = static_cast<hmap::ClampMode>(node.val<int>(A_CURVATURE_CLAMP_MODE));

          *pa_out = hmap::gpu::select_soil_weathered(
              *pa_in,
              *pa_grad_norm,
              ir_curv,
              mode,
              node.val<float>(A_CURVATURE_CLAMPING),
              node.val<float>(A_CURVATURE_WEIGHT),
              node.val<float>(A_GRADIENT_WEIGHT),
              (float)nx);
        },
        node.cfg().cm_gpu);

    // post-process
    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
