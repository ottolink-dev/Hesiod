/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
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

constexpr const char *P_INPUT     = "input";
constexpr const char *P_MOISTURE  = "moisture";
constexpr const char *P_MASK      = "mask";
constexpr const char *P_OUTPUT    = "output";
constexpr const char *P_SEDIMENT  = "sediment";
constexpr const char *P_DISCHARGE = "discharge";

constexpr const char *A_SEED         = "seed";
constexpr const char *A_STEPS        = "steps";
constexpr const char *A_LEVELS       = "levels";
constexpr const char *A_STRENGTH     = "strength";
constexpr const char *A_DEPOSITION   = "deposition";
constexpr const char *A_CRIT_SLOPE   = "crit_slope";
constexpr const char *A_MEANDERING   = "meandering";
constexpr const char *A_SCALE        = "scale";
constexpr const char *A_RELIEF_SCALE = "relief_scale";

// --- Advanced parameters

constexpr const char *A_USE_PHYSICAL_OVERRIDE = "use_physical_override";
constexpr const char *A_WORLD_EXTENT_KM       = "world_extent_km";
constexpr const char *A_Z_SCALE_KM            = "z_scale_km";
constexpr const char *A_SAMPLES               = "samples";
constexpr const char *A_MAXAGE                = "maxage";
constexpr const char *A_LRATE                 = "lrate";
constexpr const char *A_TIME_STEP             = "time_step";
constexpr const char *A_RAINFALL              = "rainfall";
constexpr const char *A_EVAP_RATE             = "evap_rate";
constexpr const char *A_GRAVITY               = "gravity";
constexpr const char *A_VISCOSITY             = "viscosity";
constexpr const char *A_BED_SHEAR             = "bed_shear";
constexpr const char *A_SETTLE_RATE           = "settle_rate";
constexpr const char *A_THERMAL_RATE          = "thermal_rate";
constexpr const char *A_DEPOSITION_RATE       = "deposition_rate";
constexpr const char *A_SUSPENSION_RATE       = "suspension_rate";
constexpr const char *A_EXIT_SLOPE            = "exit_slope";

constexpr const char *G_SINGLE_SCALE = "Single-Scale";
constexpr const char *G_MULTISCALE   = "Multiscale";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

static void setup_common_mcdonald_attributes(BaseNode &node)
{
  // clang-format off
  node.set_current_category("McDonald Parameters");
  add_float(node, A_STRENGTH, "Strength", 0.5f, 0.f, 1.f);
  add_float(node, A_DEPOSITION, "Deposition", 0.5f, 0.f, 1.f);
  add_float(node, A_CRIT_SLOPE, "Critical Slope", 0.57f, 0.01f, 2.f);
  add_float(node, A_MEANDERING, "Meandering", 0.5f, 0.f, 1.f);
  add_float(node, A_SCALE, "Scale", 1.0f, 0.01f, 10.f);
  add_float(node, A_RELIEF_SCALE, "Relief Scale", 1.0f, 0.01f, 2.f);

  node.set_current_category("Advanced");
  add_bool(node, A_USE_PHYSICAL_OVERRIDE, "Custom Physical Parameters", false);
  add_float(node, A_WORLD_EXTENT_KM, "World Extent (km)", 40.f, 0.1f, 1000.f);
  add_float(node, A_Z_SCALE_KM, "Elevation Range (km)", 4.f, 0.01f, 100.f);
  add_int(node, A_SAMPLES, "Samples", 8192, 256, 65536);
  add_int(node, A_MAXAGE, "Max Age", 512, 16, 4096);
  add_float(node, A_LRATE, "Filter Rate", 0.2f, 0.01f, 1.f);
  add_float(node, A_TIME_STEP, "Time Step", 10.f, 0.1f, 100.f);
  add_float(node, A_RAINFALL, "Rainfall", 1.f, 0.f, 10.f);
  add_float(node, A_EVAP_RATE, "Evaporation Rate", 1e-4f, 1e-9f, 1e-1f, "{:.2e}", true);
  add_float(node, A_GRAVITY, "Gravity", 9.81f, 0.1f, 50.f);
  add_float(node, A_VISCOSITY, "Viscosity", 0.025f, 0.f, 1.f);
  add_float(node, A_BED_SHEAR, "Bed Shear", 0.01f, 0.f, 1.f);
  add_float(node, A_SETTLE_RATE, "Settle Rate", 0.1f, 0.f, 1.f);
  add_float(node, A_THERMAL_RATE, "Thermal Rate", 2.5e-3f, 1e-6f, 1e-1f, "{:.2e}", true);
  add_float(node, A_DEPOSITION_RATE, "Deposition Rate", 5e-3f, 1e-6f, 1e-1f, "{:.2e}", true);
  add_float(node, A_SUSPENSION_RATE, "Suspension Rate", 2.5e-4f, 1e-6f, 1e-1f, "{:.2e}", true);
  add_float(node, A_EXIT_SLOPE, "Exit Slope", 0.01f, 0.f, 1.f);
  // clang-format on

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void setup_hydraulic_mcdonald_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MOISTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SEDIMENT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DISCHARGE, CONFIG(node));

  // --- Group: Single-Scale

  {
    node.set_current_group(G_SINGLE_SCALE);

    node.set_current_category("Simulation");
    add_seed(node, A_SEED, "Seed");
    add_int(node, A_STEPS, "Steps", 64, 1, 2000);

    setup_common_mcdonald_attributes(node);
  }

  // --- Group: Multiscale

  {
    node.set_current_group(G_MULTISCALE);

    node.set_current_category("Simulation");
    add_seed(node, A_SEED, "Seed");
    add_int(node, A_LEVELS, "Levels", 3, 1, 6);
    add_int(node, A_STEPS, "Steps", 64, 1, 2000);

    setup_common_mcdonald_attributes(node);
  }

  // reset active group to first
  node.set_current_group(G_SINGLE_SCALE);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_hydraulic_mcdonald_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in        = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  const auto *p_moisture  = node.get_value_ref<hmap::VirtualArray>(P_MOISTURE);
  const auto *p_mask      = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out       = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto       *p_sediment  = node.get_value_ref<hmap::VirtualArray>(P_SEDIMENT);
  auto       *p_discharge = node.get_value_ref<hmap::VirtualArray>(P_DISCHARGE);

  if (!p_in)
    return;

  const std::string current_group = node.get_meta_group()
                                        .current_container_name()
                                        .value_or(G_SINGLE_SCALE);

  const uint seed  = uint(node.val<int>(A_SEED));
  const int  steps = node.val<int>(A_STEPS);

  const bool use_physical_override = node.val<bool>(A_USE_PHYSICAL_OVERRIDE);

  hmap::gpu::McDonaldParams params = {
      .strength     = node.val<float>(A_STRENGTH),
      .deposition   = node.val<float>(A_DEPOSITION),
      .crit_slope   = node.val<float>(A_CRIT_SLOPE),
      .meandering   = node.val<float>(A_MEANDERING),
      .scale        = node.val<float>(A_SCALE),
      .relief_scale = node.val<float>(A_RELIEF_SCALE),
  };

  // --- Prepare mask

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  // --- Compute

  float hmin = p_in->min(node.cfg().cm_cpu);
  float hmax = p_in->max(node.cfg().cm_cpu);

  if (hmax - hmin < 1.0e-6f)
  {
    Logger::log()->warn(
        "HydraulicMcDonald [{}]: input is flat/near-constant (value range ~ 0). "
        "Erosion is gradient-driven and will produce little or no change.",
        node.get_id());
    return;
  }

  hmap::for_each_tile(
      {p_in, p_moisture},
      {p_out, p_sediment, p_discharge},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_moisture]     = unpack<2>(p_arrays_in);
        auto [pa_out, pa_sed, pa_dis] = unpack<3>(p_arrays_out);

        *pa_out = *pa_in;

        if (current_group == G_MULTISCALE)
        {
          const int        nlevels = std::clamp(node.val<int>(A_LEVELS), 1, 6);
          std::vector<int> steps_per_level(nlevels);
          for (int i = 0; i < nlevels; ++i)
            steps_per_level[i] = std::max(1, steps * (1 << (nlevels - 1 - i)));

          if (use_physical_override)
          {
            hmap::gpu::hydraulic_mcdonald_multiscale(*pa_out,
                                                     seed,
                                                     steps_per_level,
                                                     pa_moisture,
                                                     pa_sed,
                                                     pa_dis,
                                                     node.val<float>(A_WORLD_EXTENT_KM),
                                                     node.val<float>(A_Z_SCALE_KM),
                                                     node.val<int>(A_SAMPLES),
                                                     node.val<int>(A_MAXAGE),
                                                     node.val<float>(A_LRATE),
                                                     node.val<float>(A_TIME_STEP),
                                                     node.val<float>(A_RAINFALL),
                                                     node.val<float>(A_EVAP_RATE),
                                                     node.val<float>(A_GRAVITY),
                                                     node.val<float>(A_VISCOSITY),
                                                     node.val<float>(A_BED_SHEAR),
                                                     node.val<float>(A_CRIT_SLOPE),
                                                     node.val<float>(A_SETTLE_RATE),
                                                     node.val<float>(A_THERMAL_RATE),
                                                     node.val<float>(A_DEPOSITION_RATE),
                                                     node.val<float>(A_SUSPENSION_RATE),
                                                     node.val<float>(A_EXIT_SLOPE));
          }
          else
          {
            hmap::gpu::hydraulic_mcdonald_multiscale(*pa_out,
                                                     seed,
                                                     steps_per_level,
                                                     params,
                                                     pa_moisture,
                                                     pa_sed,
                                                     pa_dis);
          }
        }
        else
        {
          if (use_physical_override)
          {
            hmap::gpu::hydraulic_mcdonald(*pa_out,
                                          steps,
                                          seed,
                                          pa_moisture,
                                          pa_sed,
                                          pa_dis,
                                          node.val<float>(A_WORLD_EXTENT_KM),
                                          node.val<float>(A_Z_SCALE_KM),
                                          node.val<int>(A_SAMPLES),
                                          node.val<int>(A_MAXAGE),
                                          node.val<float>(A_LRATE),
                                          node.val<float>(A_TIME_STEP),
                                          node.val<float>(A_RAINFALL),
                                          node.val<float>(A_EVAP_RATE),
                                          node.val<float>(A_GRAVITY),
                                          node.val<float>(A_VISCOSITY),
                                          node.val<float>(A_BED_SHEAR),
                                          node.val<float>(A_CRIT_SLOPE),
                                          node.val<float>(A_SETTLE_RATE),
                                          node.val<float>(A_THERMAL_RATE),
                                          node.val<float>(A_DEPOSITION_RATE),
                                          node.val<float>(A_SUSPENSION_RATE),
                                          node.val<float>(A_EXIT_SLOPE));
          }
          else
          {
            hmap::gpu::hydraulic_mcdonald(*pa_out,
                                          steps,
                                          seed,
                                          params,
                                          pa_moisture,
                                          pa_sed,
                                          pa_dis);
          }
        }
      },
      node.cfg().cm_gpu);

  // --- Post-treatments

  p_out->sync_overlap_buffers();

  p_sediment->sync_overlap_buffers();
  p_sediment->remap(0.f, 1.f, node.cfg().cm_cpu);

  p_discharge->sync_overlap_buffers();
  p_discharge->remap(0.f, 1.f, node.cfg().cm_cpu);

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
