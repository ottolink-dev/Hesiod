/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <vector>

#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

constexpr const char *P_INPUT = "input";
constexpr const char *P_OUTPUT = "output";
constexpr const char *P_SEDIMENT = "sediment";
constexpr const char *P_DISCHARGE = "discharge";

constexpr const char *A_MULTISCALE = "multiscale";
constexpr const char *A_STEPS_PER_LEVEL = "steps_per_level";
constexpr const char *A_STEPS = "steps";
constexpr const char *A_SEED = "seed";
constexpr const char *A_WORLD_EXTENT_KM = "world_extent_km";
constexpr const char *A_Z_SCALE_KM = "z_scale_km";
constexpr const char *A_SAMPLES = "samples";
constexpr const char *A_MAXAGE = "maxage";
constexpr const char *A_LRATE = "lrate";
constexpr const char *A_TIME_STEP = "time_step";
constexpr const char *A_RAINFALL = "rainfall";
constexpr const char *A_EVAP_RATE = "evap_rate";
constexpr const char *A_GRAVITY = "gravity";
constexpr const char *A_VISCOSITY = "viscosity";
constexpr const char *A_BED_SHEAR = "bed_shear";
constexpr const char *A_EXIT_SLOPE = "exit_slope";
constexpr const char *A_CRIT_SLOPE = "crit_slope";
constexpr const char *A_SETTLE_RATE = "settle_rate";
constexpr const char *A_THERMAL_RATE = "thermal_rate";
constexpr const char *A_DEPOSITION_RATE = "deposition_rate";
constexpr const char *A_SUSPENSION_RATE = "suspension_rate";

void setup_hydraulic_mcdonald_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SEDIMENT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DISCHARGE, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<BoolAttribute>(A_MULTISCALE, "Multiscale", true);
  node.add_attr<VecIntAttribute>(A_STEPS_PER_LEVEL, "Steps Per Level", std::vector<int>({512, 256, 128}), 1, 4096);
  node.add_attr<IntAttribute>(A_STEPS, "Steps", 256, 1, 4096);
  node.add_attr<SeedAttribute>(A_SEED, "Seed");
  node.add_attr<FloatAttribute>(A_WORLD_EXTENT_KM, "World Extent [km]", 40.f, 1.f, 1000.f);
  node.add_attr<FloatAttribute>(A_Z_SCALE_KM, "Height Scale [km]", 4.f, 0.01f, 100.f);
  node.add_attr<IntAttribute>(A_SAMPLES, "Samples", 8192, 256, 65536);
  node.add_attr<IntAttribute>(A_MAXAGE, "Max Particle Age", 512, 1, 4096);
  node.add_attr<FloatAttribute>(A_LRATE, "Flow Filter Rate", 0.2f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_TIME_STEP, "Time Step [y]", 10.f, 0.01f, 1000.f);
  node.add_attr<FloatAttribute>(A_RAINFALL, "Rainfall [m/y]", 1.f, 0.f, 100.f);
  node.add_attr<FloatAttribute>(A_EVAP_RATE, "Evaporation Rate", 1e-4f, 0.f, 1.f, "{:.2e}");
  node.add_attr<FloatAttribute>(A_GRAVITY, "Gravity [m/s2]", 9.81f, 0.f, 100.f);
  node.add_attr<FloatAttribute>(A_VISCOSITY, "Viscosity", 0.025f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_BED_SHEAR, "Bed Shear", 0.01f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_EXIT_SLOPE, "Exit Slope", 0.01f, 0.f, 10.f);
  node.add_attr<FloatAttribute>(A_CRIT_SLOPE, "Critical Slope", 0.57f, 0.f, 10.f);
  node.add_attr<FloatAttribute>(A_SETTLE_RATE, "Settle Rate", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_THERMAL_RATE, "Thermal Rate", 2.5e-3f, 0.f, 1.f, "{:.2e}");
  node.add_attr<FloatAttribute>(A_DEPOSITION_RATE, "Deposition Rate", 5e-3f, 0.f, 1.f, "{:.2e}");
  node.add_attr<FloatAttribute>(A_SUSPENSION_RATE, "Suspension Rate", 2.5e-4f, 0.f, 1.f, "{:.2e}");
  // clang-format on

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Mode",
                             A_MULTISCALE, A_STEPS_PER_LEVEL, A_STEPS, A_SEED,
                             "_GROUPBOX_END_",
                             "_GROUPBOX_BEGIN_World Scale",
                             A_WORLD_EXTENT_KM, A_Z_SCALE_KM,
                             "_GROUPBOX_END_",
                             "_GROUPBOX_BEGIN_Simulation",
                             A_SAMPLES, A_MAXAGE, A_LRATE, A_TIME_STEP,
                             "_GROUPBOX_END_",
                             "_GROUPBOX_BEGIN_Hydrology",
                             A_RAINFALL, A_EVAP_RATE, A_GRAVITY, A_VISCOSITY,
                             A_BED_SHEAR, A_EXIT_SLOPE,
                             "_GROUPBOX_END_",
                             "_GROUPBOX_BEGIN_Sediment & Thermal",
                             A_CRIT_SLOPE, A_SETTLE_RATE, A_THERMAL_RATE,
                             A_DEPOSITION_RATE, A_SUSPENSION_RATE,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_hydraulic_mcdonald_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);
  auto *p_sediment = node.get_value_ref<hmap::VirtualArray>(P_SEDIMENT);
  auto *p_discharge = node.get_value_ref<hmap::VirtualArray>(P_DISCHARGE);

  if (!p_in)
    return;

  // --- Params
  const bool             multiscale = node.get_attr<BoolAttribute>(A_MULTISCALE);
  const std::vector<int> steps_per_level = node.get_attr<VecIntAttribute>(A_STEPS_PER_LEVEL);
  const int              steps = node.get_attr<IntAttribute>(A_STEPS);
  const uint             seed = node.get_attr<SeedAttribute>(A_SEED);
  const float            world_extent_km = node.get_attr<FloatAttribute>(A_WORLD_EXTENT_KM);
  const float            z_scale_km = node.get_attr<FloatAttribute>(A_Z_SCALE_KM);
  const int              samples = node.get_attr<IntAttribute>(A_SAMPLES);
  const int              maxage = node.get_attr<IntAttribute>(A_MAXAGE);
  const float            lrate = node.get_attr<FloatAttribute>(A_LRATE);
  const float            time_step = node.get_attr<FloatAttribute>(A_TIME_STEP);
  const float            rainfall = node.get_attr<FloatAttribute>(A_RAINFALL);
  const float            evap_rate = node.get_attr<FloatAttribute>(A_EVAP_RATE);
  const float            gravity = node.get_attr<FloatAttribute>(A_GRAVITY);
  const float            viscosity = node.get_attr<FloatAttribute>(A_VISCOSITY);
  const float            bed_shear = node.get_attr<FloatAttribute>(A_BED_SHEAR);
  const float            exit_slope = node.get_attr<FloatAttribute>(A_EXIT_SLOPE);
  const float            crit_slope = node.get_attr<FloatAttribute>(A_CRIT_SLOPE);
  const float            settle_rate = node.get_attr<FloatAttribute>(A_SETTLE_RATE);
  const float            thermal_rate = node.get_attr<FloatAttribute>(A_THERMAL_RATE);
  const float            deposition_rate = node.get_attr<FloatAttribute>(A_DEPOSITION_RATE);
  const float            suspension_rate = node.get_attr<FloatAttribute>(A_SUSPENSION_RATE);

  const int full_nx = p_out->shape.x;

  // --- Flat-input guard (erosion is gradient-driven)
  const float hmin = p_in->min(node.cfg().cm_cpu);
  const float hmax = p_in->max(node.cfg().cm_cpu);
  if (hmax - hmin < 1.0e-6f)
  {
    Logger::log()->warn(
        "HydraulicMcDonald [{}]: input is flat/near-constant. Erosion is "
        "gradient-driven and will produce little or no change.",
        node.get_id());
    return;
  }

  // --- Divergence guard for single-scale at high resolution
  if (!multiscale && full_nx >= 1024)
    Logger::log()->warn(
        "HydraulicMcDonald [{}]: single-scale erosion numerically diverges at "
        ">= 1024 px past a few hundred steps. Enable Multiscale for high-res terrain.",
        node.get_id());

  hmap::for_each_tile(
      {p_in},
      {p_out, p_sediment, p_discharge},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_in] = unpack<1>(in);
        auto [pa_out, pa_sediment, pa_discharge] = unpack<3>(out);

        *pa_out = *pa_in;

        // keep physical cell size invariant across tilings
        const float world_extent_eff = world_extent_km *
                                       (float)region.shape.x / (float)full_nx;

        if (multiscale)
          hmap::gpu::hydraulic_mcdonald_multiscale(*pa_out,
                                                   seed,
                                                   steps_per_level,
                                                   pa_sediment,
                                                   pa_discharge,
                                                   world_extent_eff,
                                                   z_scale_km,
                                                   samples,
                                                   maxage,
                                                   lrate,
                                                   time_step,
                                                   rainfall,
                                                   evap_rate,
                                                   gravity,
                                                   viscosity,
                                                   bed_shear,
                                                   crit_slope,
                                                   settle_rate,
                                                   thermal_rate,
                                                   deposition_rate,
                                                   suspension_rate,
                                                   exit_slope);
        else
          hmap::gpu::hydraulic_mcdonald(*pa_out,
                                        steps,
                                        seed,
                                        pa_sediment,
                                        pa_discharge,
                                        world_extent_eff,
                                        z_scale_km,
                                        samples,
                                        maxage,
                                        lrate,
                                        time_step,
                                        rainfall,
                                        evap_rate,
                                        gravity,
                                        viscosity,
                                        bed_shear,
                                        crit_slope,
                                        settle_rate,
                                        thermal_rate,
                                        deposition_rate,
                                        suspension_rate,
                                        exit_slope);
      },
      node.cfg().cm_gpu);

  // --- post-treatments (mirror hydraulic_particle)
  p_out->smooth_overlap_buffers();
  p_out->remap(hmin, hmax, node.cfg().cm_cpu);

  if (p_sediment)
  {
    p_sediment->smooth_overlap_buffers();
    p_sediment->remap(0.f, 1.f, node.cfg().cm_cpu);
  }

  if (p_discharge)
  {
    p_discharge->smooth_overlap_buffers();
    p_discharge->remap(0.f, 1.f, node.cfg().cm_cpu);
  }

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
