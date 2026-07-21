/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_OUT = "output";
constexpr const char *P_IN = "input";
constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";
constexpr const char *P_MASK = "mask";

constexpr const char *A_SEED = "seed";
constexpr const char *A_CONTROL_POINTS_COUNT = "control_points_count";
constexpr const char *A_M_EXP = "m_exp";
constexpr const char *A_DRAINAGE_NOISE_STRENGTH = "drainage_noise_strength";
constexpr const char *A_UPLIFT_RATE = "uplift_rate";
constexpr const char *A_TOLERANCE = "tolerance";
constexpr const char *A_MAX_ITERATIONS = "max_iterations";
constexpr const char *A_SMIN = "smin";
constexpr const char *A_SMAX = "smax";
constexpr const char *A_UNIFORM_SMAX = "uniform_smax";
constexpr const char *A_STRENGTH = "strength";
constexpr const char *A_SCALE_ERODIBILITY_WITH_Z = "scale_erodibility_with_z";
constexpr const char *A_ERODIBILITY_DISTRIB_EXP = "erodibility_distrib_exp";
constexpr const char *A_DEPOSITION_RADIUS = "deposition_radius";
constexpr const char *A_DEPOSITION_STRENGTH = "eposition_strength";
constexpr const char *A_STREAM_STRENGTH = "stream_strength";
constexpr const char *A_STREAM_EXP = "stream_exp";
constexpr const char *A_ITP_METHOD = "itp_method";
constexpr const char *A_ENABLE_POST_SMOOTHING = "enable_post_smoothing";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_hydraulic_saleve_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<FloatAttribute>(A_M_EXP, "Drainage Exponent", 0.15f, 0.01f, 0.8f);
  node.add_attr<FloatAttribute>(A_DRAINAGE_NOISE_STRENGTH, "Drainage Noise Strength", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_UPLIFT_RATE, "Uplift Rate", 1.f, 0.1f, 1.f);
  node.add_attr<BoolAttribute>(A_UNIFORM_SMAX, "Uniform Slope Limiter", false);
  node.add_attr<FloatAttribute>(A_SMAX, "Maximum Slope (Domain Center)", 6.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_SMIN, "Minimum Slope (Domain Border)", 0.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_STRENGTH, "Erosion Strength", 0.7f, 0.f, 1.f);
  node.add_attr<BoolAttribute>(A_SCALE_ERODIBILITY_WITH_Z, "Terrain Shape Preservation", true);
  node.add_attr<FloatAttribute>(A_ERODIBILITY_DISTRIB_EXP, "Shape Preservation Strength", 2.f, 0.1f, 4.f);
  node.add_attr<FloatAttribute>(A_DEPOSITION_RADIUS, "Deposition Radius", 0.1f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>(A_DEPOSITION_STRENGTH, "Sediment Deposition Strength", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_STREAM_EXP, "Influence Exponent", 0.8f, 0.01f, 1.f);
  node.add_attr<FloatAttribute>(A_STREAM_STRENGTH, "Fine Erosion Strength", 0.02f, 0.f, 1.f);
  node.add_attr<IntAttribute>(A_CONTROL_POINTS_COUNT, "Control Points Count", 15000, 500, 100000);
  node.add_attr<FloatAttribute>(A_TOLERANCE, "Convergence Tolerance", 1e-3f, 1e-5f, 1e-1f, "{:.2e}", /* log */ true);
  node.add_attr<IntAttribute>(A_MAX_ITERATIONS, "Max. Iterations", 500, 1, 1000);

  std::vector<std::string> choices = {"Natural Neighbors", "Delaunay + Gradients"};
  node.add_attr<ChoiceAttribute>(A_ITP_METHOD, "Interpolation Method", choices, "Delaunay + Gradients");
  node.add_attr<BoolAttribute>(A_ENABLE_POST_SMOOTHING, "Enable Smoothing", false);
  // clang-format on

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Large-Scale Drainage Erosion",
                             A_STRENGTH,
                             A_M_EXP,
                             A_UPLIFT_RATE,
                             A_DRAINAGE_NOISE_STRENGTH,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Slope Constraints",
                             A_UNIFORM_SMAX,
                             A_SMAX,
                             A_SMIN,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Sediment Deposition",
                             A_DEPOSITION_RADIUS,
                             A_DEPOSITION_STRENGTH,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Secondary Flow Erosion",
                             A_STREAM_STRENGTH,
                             A_STREAM_EXP,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Terrain Shape Preservation",
                             A_SCALE_ERODIBILITY_WITH_Z,
                             A_ERODIBILITY_DISTRIB_EXP,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Simulation & Solver",
                             A_CONTROL_POINTS_COUNT,
                             A_TOLERANCE,
                             A_MAX_ITERATIONS,
                             A_SEED,
                             A_ENABLE_POST_SMOOTHING,
                             A_ITP_METHOD,
                             "_GROUPBOX_END_"});

  setup_default_noise(node, {.noise_amp = 0.05f, .kw = 4.f, .smoothness = 0.15f});
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_saleve_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto  p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto  p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto  p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto  p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in)
    return;

  // --- Params lambda

  const auto params = [&node, p_out]()
  {
    struct P
    {
      uint                        seed;
      size_t                      count;
      float                       m_exp;
      float                       drainage_noise_strength;
      float                       uplift_rate;
      float                       tolerance;
      int                         max_iterations;
      bool                        uniform_smax;
      float                       smin;
      float                       smax;
      float                       strength;
      bool                        scale_erodibility_with_z;
      float                       erodibility_distrib_exp;
      int                         deposition_ir;
      float                       deposition_strength;
      float                       stream_strength;
      float                       stream_exp;
      hmap::InterpolationMethod2D itp_method;
      bool                        enable_post_smoothing;
    };

    int   nx = p_out->shape.x;
    int   deposition_ir = (int)(node.get_attr<FloatAttribute>(A_DEPOSITION_RADIUS) * nx);
    bool  uniform_smax = node.get_attr<BoolAttribute>(A_UNIFORM_SMAX);
    float smax = node.get_attr<FloatAttribute>(A_SMAX);
    float smin = uniform_smax ? smax : node.get_attr<FloatAttribute>(A_SMIN);

    std::string                 itp_choice = node.get_attr<ChoiceAttribute>(A_ITP_METHOD);
    hmap::InterpolationMethod2D itp_method;
    if (itp_choice == "Natural Neighbors")
      itp_method = hmap::InterpolationMethod2D::ITP2D_NNI;
    else
      itp_method = hmap::InterpolationMethod2D::ITP2D_DELAUNAY_GRADIENT;

    // clang-format off
    return P{.seed = node.get_attr<SeedAttribute>(A_SEED),
             .count = size_t(node.get_attr<IntAttribute>(A_CONTROL_POINTS_COUNT)),
             .m_exp = node.get_attr<FloatAttribute>(A_M_EXP),
	     .drainage_noise_strength = node.get_attr<FloatAttribute>(A_DRAINAGE_NOISE_STRENGTH),
             .uplift_rate = node.get_attr<FloatAttribute>(A_UPLIFT_RATE),
             .tolerance = node.get_attr<FloatAttribute>(A_TOLERANCE),
             .max_iterations = node.get_attr<IntAttribute>(A_MAX_ITERATIONS),
             .uniform_smax = uniform_smax,
             .smin = smin,
             .smax = smax,
             .strength = node.get_attr<FloatAttribute>(A_STRENGTH),
             .scale_erodibility_with_z = node.get_attr<BoolAttribute>(A_SCALE_ERODIBILITY_WITH_Z),
             .erodibility_distrib_exp = node.get_attr<FloatAttribute>(A_ERODIBILITY_DISTRIB_EXP),
             .deposition_ir = deposition_ir,
             .deposition_strength = node.get_attr<FloatAttribute>(A_DEPOSITION_STRENGTH),
	     .stream_strength = node.get_attr<FloatAttribute>(A_STREAM_STRENGTH),
	     .stream_exp = node.get_attr<FloatAttribute>(A_STREAM_EXP),
	     .itp_method = itp_method,
	     .enable_post_smoothing = node.get_attr<BoolAttribute>(A_ENABLE_POST_SMOOTHING)
    };
    // clang-format on
  }();

  // --- Prepare default noise

  hmap::VirtualArray noise_default_x(CONFIG(node));
  hmap::VirtualArray noise_default_y(CONFIG(node));
  generate_noise(node, p_dx, noise_default_x, 0);
  generate_noise(node, p_dy, noise_default_y, 1);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_dx, p_dy, p_mask},
      {p_out},
      [&node, &params](std::vector<const hmap::Array *> p_arrays_in,
                       std::vector<hmap::Array *>       p_arrays_out,
                       const hmap::TileRegion          &region)
      {
        const auto [pa_in, pa_dx, pa_dy, pa_mask] = unpack<4>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        // add a small background noise to avoid numerical artefacts
        // due to perfectly flat surfaces
        hmap::Array base = *pa_in + 1e-3f * hmap::gpu::noise(hmap::NoiseType::PERLIN,
                                                             region.shape,
                                                             {2.f, 2.f},
                                                             params.seed,
                                                             nullptr,
                                                             nullptr,
                                                             nullptr,
                                                             region.bbox);

        *pa_out = hmap::hydraulic_saleve(base,
                                         pa_mask,
                                         params.seed,
                                         params.count,
                                         params.m_exp,
                                         params.uplift_rate,
                                         params.tolerance,
                                         params.max_iterations,
                                         params.smin,
                                         params.smax,
                                         params.strength,
                                         params.scale_erodibility_with_z,
                                         params.erodibility_distrib_exp,
                                         params.drainage_noise_strength,
                                         /* enable_post_slope_limiter */ false,
                                         /* post_slope_limit */ 0.f,
                                         params.enable_post_smoothing,
                                         params.itp_method,
                                         pa_dx,
                                         pa_dy);
      },
      node.cfg().cm_single_array); // forced, not tileable

  // add 1st layer of deposition
  if (params.deposition_ir > 0 && params.deposition_strength > 0.f)
  {
    hmap::for_each_tile(
        {p_out},
        [&params](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          hmap::gpu::deposition_fill_holes(*pa_out,
                                           params.deposition_ir,
                                           params.deposition_strength,
                                           /* iterations */ 1);
        },
        node.cfg().cm_gpu);
  }

  // add fine river erosion
  if (params.stream_strength > 0.f)
  {
    hmap::for_each_tile(
        {p_out, p_mask},
        [&params](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_mask] = unpack<2>(p_arrays);

          hmap::gpu::hydraulic_stream_log(*pa_out,
                                          params.stream_strength,
                                          /* talus_ref */ 0.1f,
                                          pa_mask,
                                          params.deposition_ir,
                                          params.deposition_strength,
                                          params.stream_exp,
                                          /* gradient_scaling_ratio */ 1.f,
                                          params.deposition_ir,
                                          /* saturation_ratio */ 1.f,
                                          /* p_bedrock */ nullptr,
                                          /* p_moisture_map */ nullptr,
                                          /* pa_erosion_map */ nullptr,
                                          /* pa_deposition_map */ nullptr,
                                          /* pa_flow_map */ nullptr);
        },
        node.cfg().cm_gpu);
  }

  // post-process
  p_out->smooth_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
