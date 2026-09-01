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

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT      = "input";
constexpr const char *P_BEDROCK    = "bedrock";
constexpr const char *P_MOISTURE   = "moisture";
constexpr const char *P_MASK       = "mask";
constexpr const char *P_OUTPUT     = "output";
constexpr const char *P_EROSION    = "erosion";
constexpr const char *P_DEPOSITION = "deposition";

constexpr const char *A_SEED                      = "seed";
constexpr const char *A_LEVELS                    = "levels";
constexpr const char *A_MIX                       = "mix";
constexpr const char *A_PARTICLE_DENSITY          = "particle_density";
constexpr const char *A_C_CAPACITY                = "c_capacity";
constexpr const char *A_C_EROSION                 = "c_erosion";
constexpr const char *A_C_DEPOSITION              = "c_deposition";
constexpr const char *A_C_INERTIA                 = "c_inertia";
constexpr const char *A_DRAG_RATE                 = "drag_rate";
constexpr const char *A_EVAP_RATE                 = "evap_rate";
constexpr const char *A_TALUS_SLOPE               = "talus_slope";
constexpr const char *A_COLLAPSE_RATE             = "collapse_rate";
constexpr const char *A_ENABLE_DEFAULT_BEDROCK    = "enable_default_bedrock";
constexpr const char *A_BD_ELEVATION_STRENGTH     = "bd_elevation_strength";
constexpr const char *A_BD_SLOPE_STRENGTH         = "bd_slope_strength";
constexpr const char *A_BD_SLOPE                  = "bd_slope";
constexpr const char *A_ENABLE_RIDGE_FORCING      = "enable_ridge_forcing";
constexpr const char *A_RIDGE_SPATIAL_FREQUENCY   = "ridge_spatial_frequency";
constexpr const char *A_RIDGE_ELEVATION_AMPLITUDE = "ridge_elevation_amplitude";

constexpr const char *G_SINGLE_SCALE = "Single-Scale";
constexpr const char *G_MULTISCALE   = "Multiscale";

// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

static void setup_common_particle_attributes(BaseNode &node)
{
  // clang-format off
  node.set_current_category("Erosion & Deposition");
  add_float(node, A_C_CAPACITY, "Sediment Capacity", 5.f, 0.1f, 40.f);
  add_float(node, A_C_EROSION, "Erosion Rate", 0.05f, 0.f, 0.3f);
  add_float(node, A_C_DEPOSITION, "Deposition Rate", 0.05f, 0.f, 0.3f);
  add_float(node, A_C_INERTIA, "Particle Inertia Factor", 0.f, 0.f, 0.9f);
  add_float(node, A_DRAG_RATE, "Velocity Drag Rate", 1e-3f, 1e-6f, 1e-1f, "{:.2e}", true);
  add_float(node, A_EVAP_RATE, "Evaporation Rate", 1e-3f, 1e-6f, 1e-1f, "{:.2e}", true);

  node.set_current_category("Bank Collapse");
  add_float(node, A_TALUS_SLOPE, "Talus Slope", 2.f, 0.f, 10.f);
  add_float(node, A_COLLAPSE_RATE, "Collapse Rate", 0.25f, 0.f, 0.25f);

  node.set_current_category("Bedrock");
  add_bool(node, A_ENABLE_DEFAULT_BEDROCK, "Enable Bedrock Resistance", true);
  add_float(node, A_BD_ELEVATION_STRENGTH, "Bedrock Elevation Gap", 0.2f, 0.f, 1.f);
  add_float(node, A_BD_SLOPE_STRENGTH, "Bedrock Slope Gap", 0.f, 0.f, 1.f);
  add_float(node, A_BD_SLOPE, "Bedrock Slope Limit", 2.f, 0.f, FLT_MAX);

  node.set_current_category("Ridge Forcing");
  add_bool(node, A_ENABLE_RIDGE_FORCING, "Enable Ridge Forcing", true);
  add_float(node, A_RIDGE_SPATIAL_FREQUENCY, "Ridge Spatial Frequency", 32.f, 0.f, FLT_MAX);
  add_float(node, A_RIDGE_ELEVATION_AMPLITUDE, "Ridge Height", 0.1f, 0.f, 1.f);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

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

  // Group: Single-Scale
  {
    node.set_current_group(G_SINGLE_SCALE);

    node.set_current_category("Simulation");
    add_seed(node, A_SEED, "Seed");
    add_float(node, A_PARTICLE_DENSITY, "Particle Density", 0.5f, 0.f, 4.f);

    setup_common_particle_attributes(node);
  }

  // Group: Multiscale
  {
    node.set_current_group(G_MULTISCALE);

    node.set_current_category("Simulation");
    add_seed(node, A_SEED, "Seed");
    add_int(node, A_LEVELS, "Levels", 3, 1, 6);
    add_float(node, A_MIX, "Mix", 0.8f, 0.f, 1.f);
    add_float(node, A_PARTICLE_DENSITY, "Particle Density", 0.5f, 0.f, 4.f);

    setup_common_particle_attributes(node);
  }

  // Reset active group to first
  node.set_current_group(G_SINGLE_SCALE);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_particle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in         = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_bedrock    = node.get_value_ref<hmap::VirtualArray>(P_BEDROCK);
  auto *p_moisture   = node.get_value_ref<hmap::VirtualArray>(P_MOISTURE);
  auto *p_mask       = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out        = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto *p_erosion    = node.get_value_ref<hmap::VirtualArray>(P_EROSION);
  auto *p_deposition = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);

  if (!p_in)
    return;

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_SINGLE_SCALE);

  const uint  seed             = uint(node.val<int>(A_SEED));
  const float particle_density = node.val<float>(A_PARTICLE_DENSITY);

  const float c_capacity    = node.val<float>(A_C_CAPACITY);
  const float c_erosion     = node.val<float>(A_C_EROSION);
  const float c_deposition  = node.val<float>(A_C_DEPOSITION);
  const float c_inertia     = node.val<float>(A_C_INERTIA);
  const float c_gravity     = 1.f;
  const float drag_rate     = node.val<float>(A_DRAG_RATE);
  const float evap_rate     = node.val<float>(A_EVAP_RATE);
  const float talus_slope   = node.val<float>(A_TALUS_SLOPE);
  const float collapse_rate = node.val<float>(A_COLLAPSE_RATE);

  const bool  enable_default_bedrock = node.val<bool>(A_ENABLE_DEFAULT_BEDROCK);
  const float bd_elevation_strength  = node.val<float>(A_BD_ELEVATION_STRENGTH);
  const float bd_slope_strength      = node.val<float>(A_BD_SLOPE_STRENGTH);
  const float bd_talus               = node.val<float>(A_BD_SLOPE) / p_out->shape.x;

  const bool  enable_ridge_forcing      = node.val<bool>(A_ENABLE_RIDGE_FORCING);
  const float ridge_spatial_frequency   = node.val<float>(A_RIDGE_SPATIAL_FREQUENCY);
  const float ridge_elevation_amplitude = node.val<float>(A_RIDGE_ELEVATION_AMPLITUDE);

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
        "HydraulicParticle [{}]: input is flat/near-constant (value range ~ "
        "0). "
        "Erosion is gradient-driven and will produce little or no change. "
        "Erode full-domain relief first, then multiply by the land mask; "
        "or enable ridge forcing.",
        node.get_id());
    return;
  }

  hmap::for_each_tile(
      {p_in, p_bedrock, p_moisture, p_mask},
      {p_out, p_erosion, p_deposition},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region)
      {
        auto [pa_in, pa_bedrock, pa_moisture, pa_mask] = unpack<4>(p_arrays_in);
        auto [pa_out, pa_erosion, pa_deposition]       = unpack<3>(p_arrays_out);

        *pa_out = *pa_in;

        // add ridges
        if (enable_ridge_forcing)
        {
          hmap::Array angle       = hmap::gradient_angle(*pa_out);
          int         octaves     = 4;
          float       weight      = 0.7f;
          float       persistence = 0.5f;
          float       lacunarity  = 2.f;

          glm::vec2 kr = {ridge_spatial_frequency, ridge_spatial_frequency};

          hmap::Array ridges = hmap::gpu::gabor_wave_fbm(region.shape,
                                                         kr,
                                                         seed + 1,
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

          const float talus_ref = 4.f / region.shape.x;
          const int   ir = std::max(1, int(region.shape.x / ridge_spatial_frequency));
          const float gradient_exp = 2.f;
          hmap::Array gn           = hmap::gradient_norm(*pa_out) / talus_ref;
          hmap::gpu::smooth_cpulse(gn, ir);
          hmap::clamp_max(gn, 1.f);
          gn = hmap::pow(gn, gradient_exp);

          *pa_out += ridge_elevation_amplitude * ridges * gn;
        }

        // define bedrock field
        hmap::Array bedrock;

        if (enable_default_bedrock && !pa_bedrock)
        {
          bedrock    = hmap::generate_bedrock(*pa_out,
                                           bd_elevation_strength,
                                           bd_slope_strength,
                                           bd_talus,
                                           hmin,
                                           hmax);
          pa_bedrock = &bedrock;
        }

        // eventually erode...
        if (current_group == G_MULTISCALE)
        {
          const int        nlevels = std::clamp(node.val<int>(A_LEVELS), 1, 6);
          const float      mix     = node.val<float>(A_MIX);
          std::vector<int> steps_per_level(nlevels);
          for (int i = 0; i < nlevels; ++i)
            steps_per_level[i] = 1 << (nlevels - 1 - i);

          hmap::gpu::hydraulic_particle_multiscale(*pa_out,
                                                   seed,
                                                   steps_per_level,
                                                   pa_bedrock,
                                                   pa_moisture,
                                                   nullptr,
                                                   pa_erosion,
                                                   pa_deposition,
                                                   particle_density,
                                                   c_capacity,
                                                   c_erosion,
                                                   c_deposition,
                                                   c_inertia,
                                                   c_gravity,
                                                   drag_rate,
                                                   evap_rate,
                                                   talus_slope,
                                                   collapse_rate,
                                                   mix);
        }
        else
        {
          const int nparticles = int(particle_density * region.shape.x * region.shape.y);

          hmap::gpu::hydraulic_particle(*pa_out,
                                        pa_mask,
                                        nparticles,
                                        seed,
                                        pa_bedrock,
                                        pa_moisture,
                                        nullptr,
                                        pa_erosion,
                                        pa_deposition,
                                        c_capacity,
                                        c_erosion,
                                        c_deposition,
                                        c_inertia,
                                        c_gravity,
                                        drag_rate,
                                        evap_rate,
                                        talus_slope,
                                        collapse_rate,
                                        /* iterations */ 1);
        }
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
