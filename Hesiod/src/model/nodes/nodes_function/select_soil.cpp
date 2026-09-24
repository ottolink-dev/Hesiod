/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
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

constexpr const char *G_FLOW      = "Flow";
constexpr const char *G_ROCKS     = "Rocks";
constexpr const char *G_WEATHERED = "Weathered";
constexpr const char *G_RIVERS    = "Rivers";

constexpr const char *A_CLIPPING_RATIO        = "clipping_ratio";
constexpr const char *A_CURVATURE_CLAMP_MODE  = "curvature_clamp_mode";
constexpr const char *A_CURVATURE_CLAMPING    = "curvature_clamping";
constexpr const char *A_CURVATURE_WEIGHT      = "curvature_weight";
constexpr const char *A_FLOW_GAMMA            = "flow_gamma";
constexpr const char *A_FLOW_WEIGHT           = "flow_weight";
constexpr const char *A_GRADIENT_GAIN         = "gradient_gain";
constexpr const char *A_GRADIENT_WEIGHT       = "gradient_weight";
constexpr const char *A_K_SATURATION          = "k_saturation";
constexpr const char *A_RADIUS_CURVATURE      = "radius_curvature";
constexpr const char *A_RADIUS_GRADIENT       = "radius_gradient";
constexpr const char *A_RMAX                  = "rmax";
constexpr const char *A_RMIN                  = "rmin";
constexpr const char *A_SATURATION_LIMIT      = "saturation_limit";
constexpr const char *A_SMALLER_SCALES_WEIGHT = "smaller_scales_weight";
constexpr const char *A_STEPS                 = "steps";
constexpr const char *A_TALUS_REF             = "talus_ref";
constexpr const char *A_MIN_MAX_KERNEL        = "min_max_kernel";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_select_soil_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Group 1: Flow

  {
    node.set_current_group(G_FLOW);

    node.set_current_category("Flow Parameters");
    add_float(node, A_RADIUS_GRADIENT, "Gradient Radius", 0.f, 0.f, 0.1f);
    add_float(node, A_GRADIENT_WEIGHT, "Gradient Weight", 1.f, 0.f, 1.f);
    add_float(node, A_FLOW_WEIGHT, "Flow Weight", 0.01f, 0.f, 1.f);
    add_float(node, A_TALUS_REF, "Ref. Talus", 10.f, 0.01f, 32.f);
    add_float(node, A_CLIPPING_RATIO, "Clipping Ratio", 50.f, 0.1f, 100.f);
    add_float(node, A_FLOW_GAMMA, "Flow Distrib. Exponent", 1.f, 0.01f, 4.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = true});
  }

  // --- Group 2: Rocks

  {
    node.set_current_group(G_ROCKS);

    node.set_current_category("Rocks Parameters");
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

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // --- Group 3: Weathered

  {
    node.set_current_group(G_WEATHERED);

    node.set_current_category("Weathered Parameters");
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

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});

    node.set_current_category("Advanced");
    add_enum(node,
             A_MIN_MAX_KERNEL,
             "Kernel Type",
             enum_mappings.min_max_kernel_map,
             "Octagon");
  }

  // --- Group 4: Rivers

  {
    node.set_current_group(G_RIVERS);

    node.set_current_category("Rivers Parameters");
    add_float(node, A_TALUS_REF, "talus_ref", 0.1f, 0.01f, 10.f);
    add_float(node, A_CLIPPING_RATIO, "clipping_ratio", 50.f, 0.1f, 100.f);

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = false, .remap_active_state = true});
  }

  // Reset active group to first
  node.set_current_group(G_FLOW);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_select_soil_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  const std::string group = node.get_meta_group().current_container_name().value_or(
      G_FLOW);

  // --- Group Dispatch

  if (group == G_FLOW)
  {
    int   nx    = p_out->shape.x;
    int   ir    = std::max(1, (int)(node.val<float>(A_RADIUS_GRADIENT) * nx));
    float talus = node.val<float>(A_TALUS_REF) / nx;

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, nx, ir, talus](std::vector<hmap::Array *> p_arrays,
                               const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          float k_smooth       = 0.01f;

          *pa_out = hmap::gpu::select_soil_flow(*pa_in,
                                                ir,
                                                node.val<float>(A_GRADIENT_WEIGHT),
                                                (float)nx,
                                                node.val<float>(A_FLOW_WEIGHT),
                                                talus,
                                                node.val<float>(A_CLIPPING_RATIO),
                                                node.val<float>(A_FLOW_GAMMA),
                                                k_smooth);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_ROCKS)
  {
    int nx     = p_out->shape.x;
    int ir_min = (int)(node.val<float>(A_RMIN) * nx);
    int ir_max = std::max(1, (int)(node.val<float>(A_RMAX) * nx));

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir_min, ir_max](std::vector<hmap::Array *> p_arrays,
                                const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          auto mode            = node.val_enum<hmap::ClampMode>(A_CURVATURE_CLAMP_MODE);

          *pa_out = hmap::gpu::select_soil_rocks(*pa_in,
                                                 ir_max,
                                                 ir_min,
                                                 node.val<int>(A_STEPS),
                                                 node.val<float>(A_SMALLER_SCALES_WEIGHT),
                                                 mode,
                                                 node.val<float>(A_CURVATURE_CLAMPING));
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);

    // saturate
    hmap::for_each_tile(
        {p_out},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          hmap::saturate(*pa_out,
                         0.f,
                         node.val<float>(A_SATURATION_LIMIT),
                         node.val<float>(A_K_SATURATION));
        },
        node.cfg().cm_cpu);
  }
  else if (group == G_WEATHERED)
  {
    const auto kernel_type = node.val_enum<hmap::MinMaxKernel>(A_MIN_MAX_KERNEL);
    int        nx          = p_out->shape.x;
    int        ir_curv     = (int)(node.val<float>(A_RADIUS_CURVATURE) * nx);
    int        ir_grad     = std::max(1, (int)(node.val<float>(A_RADIUS_GRADIENT) * nx));

    hmap::VirtualArray grad_norm(CONFIG(node));

    hmap::for_each_tile(
        {&grad_norm, p_in},
        [&node, ir_grad, kernel_type](std::vector<hmap::Array *> p_arrays,
                                      const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::gpu::morphological_gradient(*pa_in, ir_grad, kernel_type);
        },
        node.cfg().cm_gpu);

    grad_norm.remap(0.f, 1.f, node.cfg().cm_cpu);

    hmap::for_each_tile(
        {&grad_norm},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);
          hmap::gain(*pa_out, node.val<float>(A_GRADIENT_GAIN));
        },
        node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, &grad_norm},
        [&node, nx, ir_curv](std::vector<hmap::Array *> p_arrays,
                             const hmap::TileRegion &)
        {
          auto [pa_out, pa_in, pa_grad_norm] = unpack<3>(p_arrays);
          auto mode = node.val_enum<hmap::ClampMode>(A_CURVATURE_CLAMP_MODE);

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

    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
  else if (group == G_RIVERS)
  {
    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = hmap::select_rivers(*pa_in,
                                        node.val<float>(A_TALUS_REF),
                                        node.val<float>(A_CLIPPING_RATIO));
        },
        node.cfg().cm_single_array);

    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
