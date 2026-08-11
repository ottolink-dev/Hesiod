/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

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

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_OUTPUT  = "phasor_fbm";
constexpr const char *P_ANGLE   = "angle";
constexpr const char *P_NOISE_X = "noise_x";
constexpr const char *P_NOISE_Y = "noise_y";

constexpr const char *A_SEED            = "seed";
constexpr const char *A_KP_GLOBAL       = "kp_global";
constexpr const char *A_ANGLE_SHIFT     = "angle_shift";
constexpr const char *A_OCTAVES         = "octaves";
constexpr const char *A_WEIGHT          = "weight";
constexpr const char *A_PERSISTENCE     = "persistence";
constexpr const char *A_LACUNARITY      = "lacunarity";
constexpr const char *A_PROFILE         = "profile";
constexpr const char *A_DELTA           = "delta";
constexpr const char *A_PHASE_SMOOTHING = "phase_smoothing";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_phasor_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ANGLE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_X);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_Y);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_KP_GLOBAL, "Spatial Frequency", 8.f, 0.f, FLT_MAX);
  add_float(node, A_ANGLE_SHIFT, "Angle Offset", 0.f, -180.f, 180.f, "{:.0f}°");
  add_seed(node, A_SEED, "Seed");
  add_enum(node, A_PROFILE, "Waveform Profile", enum_mappings.phasor_profile_map, "Cosine Square");
  add_int(node, A_OCTAVES, "Octaves", 4, 1, 8);
  add_float(node, A_WEIGHT, "Weight", 0.7f, 0.f, 1.f);
  add_float(node, A_PERSISTENCE, "Persistence", 0.5f, 0.f, 1.f);
  add_float(node, A_LACUNARITY, "Lacunarity", 2.f, 0.01f, 4.f);
  add_float(node, A_DELTA, "Profile Sharpness", 3.f, 0.001f, 100.f, "{:.2e}", true);
  add_float(node, A_PHASE_SMOOTHING, "Phase Transition Smoothing", 10.f, 0.001f, 100.f, "{:.2e}", true);
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_phasor_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_angle   = node.get_value_ref<hmap::VirtualArray>(P_ANGLE);
  auto *p_noise_x = node.get_value_ref<hmap::VirtualArray>(P_NOISE_X);
  auto *p_noise_y = node.get_value_ref<hmap::VirtualArray>(P_NOISE_Y);
  auto *p_out     = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto seed             = node.val<int>(A_SEED);
  const auto kp_global        = node.val<float>(A_KP_GLOBAL);
  const auto angle_shift_rads = float(M_PI) / 180.f * node.val<float>(A_ANGLE_SHIFT);
  const auto profile          = hmap::PhasorProfile(node.val<int>(A_PROFILE));
  const auto octaves          = node.val<int>(A_OCTAVES);
  const auto weight           = node.val<float>(A_WEIGHT);
  const auto persistence      = node.val<float>(A_PERSISTENCE);
  const auto lacunarity       = node.val<float>(A_LACUNARITY);
  const auto delta            = node.val<float>(A_DELTA);
  const auto phase_smoothing  = node.val<float>(A_PHASE_SMOOTHING);
  // clang-format on

  constexpr int n_kernel_samples = 8;

  // --- Compute

  hmap::for_each_tile(
      {p_noise_x, p_noise_y, p_angle},
      {p_out},
      [=](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region)
      {
        auto [pa_noise_x, pa_noise_y, pa_angle] = unpack<3>(p_arrays_in);
        auto [pa_out]                           = unpack<1>(p_arrays_out);

        *pa_out = hmap::gpu::phasor_fbm(profile,
                                        region.shape,
                                        kp_global,
                                        seed,
                                        angle_shift_rads,
                                        octaves,
                                        weight,
                                        persistence,
                                        lacunarity,
                                        n_kernel_samples,
                                        {1.f, 1.f},
                                        delta,
                                        phase_smoothing,
                                        pa_angle,
                                        pa_noise_x,
                                        pa_noise_y,
                                        region.bbox);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
