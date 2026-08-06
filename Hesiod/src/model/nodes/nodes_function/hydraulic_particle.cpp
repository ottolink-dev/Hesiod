/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/filters.hpp"
#include "highmap/gradient.hpp"
#include "highmap/math.hpp"
#include "highmap/morphology.hpp"
#include "highmap/multiscale/downscaling.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT = "input";
constexpr const char *P_BEDROCK = "bedrock";
constexpr const char *P_MOISTURE = "moisture";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUTPUT = "output";
constexpr const char *P_EROSION = "erosion";
constexpr const char *P_DEPOSITION = "deposition";

constexpr const char *A_SCALE = "scale";
constexpr const char *A_SEED = "seed";
constexpr const char *A_PARTICLE_DENSITY = "particle_density";
constexpr const char *A_C_CAPACITY = "c_capacity";
constexpr const char *A_C_EROSION = "c_erosion";
constexpr const char *A_C_DEPOSITION = "c_deposition";
constexpr const char *A_C_INERTIA = "c_inertia";
constexpr const char *A_DRAG_RATE = "drag_rate";
constexpr const char *A_EVAP_RATE = "evap_rate";
constexpr const char *A_ENABLE_DIRECTIONAL_BIAS = "enable_directional_bias";
constexpr const char *A_ANGLE_BIAS = "angle_bias";
constexpr const char *A_DEPOSITION_ONLY = "deposition_only";
constexpr const char *A_ENABLE_DEFAULT_BEDROCK = "enable_default_bedrock";
constexpr const char *A_BD_ELEVATION_STRENGTH = "bd_elevation_strength";
constexpr const char *A_BD_SLOPE_STRENGTH = "bd_slope_strength";
constexpr const char *A_BD_SLOPE = "bd_slope";
constexpr const char *A_ENABLE_RIDGE_FORCING = "enable_ridge_forcing";
constexpr const char *A_RIDGE_SPATIAL_FREQUENCY = "ridge_spatial_frequency";
constexpr const char *A_RIDGE_ELEVATION_AMPLITUDE = "ridge_elevation_amplitude";

// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

void setup_hydraulic_particle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BEDROCK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MOISTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_EROSION, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DEPOSITION, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<FloatAttribute>(A_PARTICLE_DENSITY, "Particle Density", 0.5f, 0.f, 4.f);
  node.add_attr<FloatAttribute>(A_C_CAPACITY, "Sediment Capacity", 5.f, 0.1f, 40.f);
  node.add_attr<FloatAttribute>(A_C_EROSION, "Erosion Rate", 0.05f, 0.f, 0.3f);
  node.add_attr<FloatAttribute>(A_C_DEPOSITION, "Deposition Rate", 0.2f, 0.f, 0.3f);
  node.add_attr<FloatAttribute>(A_C_INERTIA, "Particle Inertia Factor", 0.2f, 0.f, 0.9f);
  node.add_attr<FloatAttribute>(A_DRAG_RATE, "Velocity Drag Rate", 0.001f, 0.f, 0.02f);
  node.add_attr<FloatAttribute>(A_EVAP_RATE, "Evaporation Rate", 0.001f, 0.f, 0.02f);
  node.add_attr<BoolAttribute>(A_ENABLE_DIRECTIONAL_BIAS, "Enable Directional Bias", false);
  node.add_attr<FloatAttribute>(A_ANGLE_BIAS, "Directional Bias Angle", 0.f, -180.f, 180.f, "{:.0f}°");
  node.add_attr<BoolAttribute>(A_DEPOSITION_ONLY, "Deposition Only Mode", false);
  node.add_attr<BoolAttribute>(A_ENABLE_DEFAULT_BEDROCK, "Enable Bedrock Resistance", true);
  node.add_attr<FloatAttribute>(A_BD_ELEVATION_STRENGTH, "Bedrock Elevation Gap", 0.05f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_BD_SLOPE_STRENGTH, "Bedrock Slope Gap", 0.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_BD_SLOPE, "Bedrock Slope Limit", 2.f, 0.f, FLT_MAX);
  node.add_attr<BoolAttribute>(A_ENABLE_RIDGE_FORCING, "Enable Ridge Forcing", true);
  node.add_attr<FloatAttribute>(A_RIDGE_SPATIAL_FREQUENCY, "Ridge Spatial Frequency", 32.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_RIDGE_ELEVATION_AMPLITUDE, "Ridge Height", 0.1f, 0.f, 1.f);
  // clang-format on

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Simulation",
                             A_PARTICLE_DENSITY,
                             A_SEED,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Sediment Dynamics",
                             A_C_CAPACITY,
                             A_C_EROSION,
                             A_C_DEPOSITION,
                             A_C_INERTIA,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Pre-Erosion Ridge Forcing",
                             A_ENABLE_RIDGE_FORCING,
                             A_RIDGE_SPATIAL_FREQUENCY,
                             A_RIDGE_ELEVATION_AMPLITUDE,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Bedrock Resistance",
                             A_ENABLE_DEFAULT_BEDROCK,
                             A_BD_ELEVATION_STRENGTH,
                             A_BD_SLOPE_STRENGTH,
                             A_BD_SLOPE,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Deposition Mode",
                             A_DEPOSITION_ONLY,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Particle Behavior",
                             A_DRAG_RATE,
                             A_EVAP_RATE,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Directional Control",
                             A_ENABLE_DIRECTIONAL_BIAS,
                             A_ANGLE_BIAS,
                             "_GROUPBOX_END_"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_particle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_bedrock = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK);
  auto *p_moisture = node.get_value_ref<hmap::VirtualArray>(P_MOISTURE);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto *p_erosion = node.get_value_ref<hmap::VirtualArray>(P_EROSION);
  auto *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);

  if (!p_in)
    return;

  // --- Params wrapper

  const auto params = [&node, p_out]()
  {
    struct P
    {
      uint  seed;
      int   nparticles;
      float c_capacity;
      float c_erosion;
      float c_deposition;
      float c_inertia;
      float c_gravity;
      float drag_rate;
      float evap_rate;
      bool  enable_directional_bias;
      float angle_bias;
      bool  deposition_only;
      bool  enable_default_bedrock;
      float bd_elevation_strength;
      float bd_slope_strength;
      float bd_talus;
      bool  enable_ridge_forcing;
      float ridge_spatial_frequency;
      float ridge_elevation_amplitude;
    };

    const int   ncells = p_out->shape.x * p_out->shape.y;
    const float density = node.get_attr<FloatAttribute>(A_PARTICLE_DENSITY);
    const int   nparticles = (int)(density * ncells);
    const float bd_talus = node.get_attr<FloatAttribute>(A_BD_SLOPE) / p_out->shape.x;

    // clang-format off
    return P{
        .seed = node.get_attr<SeedAttribute>(A_SEED),
        .nparticles = nparticles,
        .c_capacity = node.get_attr<FloatAttribute>(A_C_CAPACITY),
        .c_erosion = node.get_attr<FloatAttribute>(A_C_EROSION),
        .c_deposition = node.get_attr<FloatAttribute>(A_C_DEPOSITION),
        .c_inertia = node.get_attr<FloatAttribute>(A_C_INERTIA),
        .c_gravity = 1.f,
        .drag_rate = node.get_attr<FloatAttribute>(A_DRAG_RATE),
        .evap_rate = node.get_attr<FloatAttribute>(A_EVAP_RATE),
        .enable_directional_bias = node.get_attr<BoolAttribute>(A_ENABLE_DIRECTIONAL_BIAS),
        .angle_bias = node.get_attr<FloatAttribute>(A_ANGLE_BIAS),
        .deposition_only = node.get_attr<BoolAttribute>(A_DEPOSITION_ONLY),
        .enable_default_bedrock = node.get_attr<BoolAttribute>(A_ENABLE_DEFAULT_BEDROCK),
        .bd_elevation_strength = node.get_attr<FloatAttribute>(A_BD_ELEVATION_STRENGTH),
        .bd_slope_strength = node.get_attr<FloatAttribute>(A_BD_SLOPE_STRENGTH),
        .bd_talus = bd_talus,
        .enable_ridge_forcing = node.get_attr<BoolAttribute>(A_ENABLE_RIDGE_FORCING),
        .ridge_spatial_frequency = node.get_attr<FloatAttribute>(A_RIDGE_SPATIAL_FREQUENCY),
        .ridge_elevation_amplitude = node.get_attr<FloatAttribute>(A_RIDGE_ELEVATION_AMPLITUDE)};
    // clang-format on
  }();

  // --- Resolve bedrock

  // set the bedrock at the input heightmap to prevent any erosion, if
  // requested
  if (params.deposition_only)
    p_bedrock = p_in;

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Compute

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  // Particle erosion is gradient-driven: flat or masked-to-zero input has no
  // slope, so particles carry no sediment and the output is unchanged. Warn
  // rather than silently returning the input untouched.
  if (hmax - hmin < 1.0e-6f)
  {
    Logger::log()->warn(
        "HydraulicParticle [{}]: input is flat/near-constant (value range ~ 0). "
        "Erosion is gradient-driven and will produce little or no change. "
        "Erode full-domain relief first, then multiply by the land mask; "
        "or enable ridge forcing.",
        node.get_id());
    return;
  }

  hmap::for_each_tile(
      {p_in, p_bedrock, p_moisture, p_mask},
      {p_out, p_erosion, p_deposition},
      [&node, &params, hmin, hmax](std::vector<const hmap::Array *> p_arrays_in,
                                   std::vector<hmap::Array *>       p_arrays_out,
                                   const hmap::TileRegion          &region)
      {
        auto [pa_in, pa_bedrock, pa_moisture, pa_mask] = unpack<4>(p_arrays_in);
        auto [pa_out, pa_erosion, pa_deposition] = unpack<3>(p_arrays_out);

        *pa_out = *pa_in;

        // add ridges
        if (params.enable_ridge_forcing)
        {
          hmap::Array angle = hmap::gradient_angle(*pa_out);
          int         octaves = 4;
          float       weight = 0.7f;
          float       persistence = 0.5f;
          float       lacunarity = 2.f;

          glm::vec2 kr = {params.ridge_spatial_frequency, params.ridge_spatial_frequency};

          hmap::Array ridges = hmap::gpu::gabor_wave_fbm(region.shape,
                                                         kr,
                                                         params.seed + 1,
                                                         angle * 180.f / M_PI + 90.f,
                                                         0.f,
                                                         octaves,
                                                         weight,
                                                         persistence,
                                                         lacunarity,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         region.bbox);

          ridges = 0.5f * ridges - 0.5f; // in [-1..0]

          // TODO move this to params lambda
          const float talus_ref = 4.f / region.shape.x;
          const int   ir = std::max(1,
                                  int(region.shape.x / params.ridge_spatial_frequency));
          const float gradient_exp = 2.f;
          hmap::Array gn = hmap::gradient_norm(*pa_out) / talus_ref;
          hmap::gpu::smooth_cpulse(gn, ir);
          hmap::clamp_max(gn, 1.f);
          gn = hmap::pow(gn, gradient_exp);

          *pa_out += params.ridge_elevation_amplitude * ridges * gn;
        }

        // define bedrock field
        hmap::Array bedrock;

        if (params.enable_default_bedrock && !pa_bedrock)
        {
          bedrock = hmap::generate_bedrock(*pa_out,
                                           params.bd_elevation_strength,
                                           params.bd_slope_strength,
                                           params.bd_talus,
                                           hmin,
                                           hmax);
          pa_bedrock = &bedrock;
        }

        // eventually erode...
        hmap::gpu::hydraulic_particle(*pa_out,
                                      pa_mask,
                                      params.nparticles,
                                      params.seed,
                                      pa_bedrock,
                                      pa_moisture,
                                      nullptr,
                                      pa_erosion,
                                      pa_deposition,
                                      params.c_capacity,
                                      params.c_erosion,
                                      params.c_deposition,
                                      params.c_inertia,
                                      params.c_gravity,
                                      params.drag_rate,
                                      params.evap_rate,
                                      params.enable_directional_bias,
                                      params.angle_bias);
      },
      node.cfg().cm_gpu);

  // --- post-treatments

  p_out->smooth_overlap_buffers();
  p_out->remap(hmin, hmax, node.cfg().cm_cpu);

  p_erosion->smooth_overlap_buffers();
  p_erosion->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_deposition->smooth_overlap_buffers();
  p_deposition->remap(0.f, 1.f, node.cfg().cm_cpu);

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
