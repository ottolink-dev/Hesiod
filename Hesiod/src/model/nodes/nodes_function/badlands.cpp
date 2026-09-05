/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DX  = "dx";
constexpr const char *P_DY  = "dy";
constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_ELEVATION      = "elevation";
constexpr const char *A_KW             = "kw";
constexpr const char *A_SEED           = "seed";
constexpr const char *A_OCTAVES        = "octaves";
constexpr const char *A_RUGOSITY       = "rugosity";
constexpr const char *A_ANGLE          = "angle";
constexpr const char *A_K_SMOOTHING    = "k_smoothing";
constexpr const char *A_BASE_NOISE_AMP = "base_noise_amp";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_badlands_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_ELEVATION, "Elevation", 0.4f, 0.f, 1.f);
  add_wavenumber(node, A_KW, "Spatial Frequency");
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_OCTAVES, "Octaves", 8, 0, 32);
  add_float(node, A_RUGOSITY, "Smoothness", 0.2f, 0.f, 1.f);
  add_float(node, A_ANGLE, "Noise Angle", 30.f, -180.f, 180.f);
  add_float(node, A_K_SMOOTHING, "Transition Smoothness", 0.1f, 0.f, 1.f);
  add_float(node, A_BASE_NOISE_AMP, "Noise Amplitude", 0.2f, 0.f, 1.f);
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_badlands_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dx  = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy  = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto elevation      = node.val<float>(A_ELEVATION);
  const auto kw             = node.val<glm::vec2>(A_KW);
  const auto seed           = node.val<int>(A_SEED);
  const auto octaves        = node.val<int>(A_OCTAVES);
  const auto rugosity       = node.val<float>(A_RUGOSITY);
  const auto angle          = node.val<float>(A_ANGLE);
  const auto k_smoothing    = node.val<float>(A_K_SMOOTHING);
  const auto base_noise_amp = node.val<float>(A_BASE_NOISE_AMP);
  // clang-format on

  const float noise_amp_scaled = base_noise_amp * 2.f / std::max(kw.x, kw.y);

  // --- Compute

  hmap::for_each_tile(
      {p_dx, p_dy},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dx, pa_dy] = unpack<2>(in);
        auto [pa_out]       = unpack<1>(out);

        *pa_out = hmap::gpu::badlands(region.shape,
                                      kw,
                                      seed,
                                      octaves,
                                      rugosity,
                                      angle,
                                      k_smoothing,
                                      noise_amp_scaled,
                                      pa_dx,
                                      pa_dy,
                                      region.bbox);
      },
      node.cfg().cm_gpu);

  p_out->remap(0.f, elevation, node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
