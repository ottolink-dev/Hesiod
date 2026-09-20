/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_INPUT  = "input";
constexpr const char *P_DX     = "dx";
constexpr const char *P_DY     = "dy";
constexpr const char *P_MASK   = "mask";
constexpr const char *P_OUTPUT = "output";

constexpr const char *A_AMP      = "amp";
constexpr const char *A_ANGLE    = "angle";
constexpr const char *A_GAMMA    = "gamma";
constexpr const char *A_JITTER_X = "jitter.x";
constexpr const char *A_JITTER_Y = "jitter.y";
constexpr const char *A_KW       = "kw";
constexpr const char *A_SEED     = "seed";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_jagged_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUTPUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Main Parameters");
  add_wavenumber(node, A_KW, "Spatial Frequency", glm::vec2(32.f, 32.f), 0.f, FLT_MAX, true);
  add_seed(node, A_SEED, "Seed");
  add_angle(node, A_ANGLE, "Angle", 0.f, -180.f, 180.f);
  add_float(node, A_AMP, "Amplitude", 0.01f, 0.001f, 0.1f);
  add_float(node, A_GAMMA, "Gamma", 1.f, 0.01f, 4.f);
  add_float(node, A_JITTER_X, "jitter.x", 1.f, 0.f, 1.f);
  add_float(node, A_JITTER_Y, "jitter.y", 1.f, 0.f, 1.f);
  // clang-format on

  setup_default_noise(node, {.noise_amp = 0.025f, .kw = 8.f, .smoothness = 0.f});
  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_jagged_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_INPUT);
  auto *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  auto *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUTPUT);

  if (!p_in || !p_out)
    return;

  // --- Parameters

  const auto     kw   = node.val<glm::vec2>(A_KW);
  const auto     amp  = node.val<float>(A_AMP);
  const uint32_t seed = static_cast<uint32_t>(node.val<int>(A_SEED));
  const auto jitter = glm::vec2(node.val<float>(A_JITTER_X), node.val<float>(A_JITTER_Y));
  const auto gamma  = node.val<float>(A_GAMMA);
  const auto angle  = node.val<float>(A_ANGLE);

  // --- Prepare mask

  std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

  // --- Resolve default noise

  hmap::VirtualArray noise_default_x(CONFIG(node));
  hmap::VirtualArray noise_default_y(CONFIG(node));
  uint               seed_increment = 0;
  generate_noise(node, p_dx, noise_default_x, ++seed_increment);
  generate_noise(node, p_dy, noise_default_y, ++seed_increment);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_dx, p_dy, p_mask},
      {p_out},
      [kw, amp, seed, jitter, gamma, angle](std::vector<const hmap::Array *> p_arrays_in,
                                            std::vector<hmap::Array *>       p_arrays_out,
                                            const hmap::TileRegion          &region)
      {
        const auto [pa_in, pa_dx, pa_dy, pa_mask] = unpack<4>(p_arrays_in);
        auto [pa_out]                             = unpack<1>(p_arrays_out);

        *pa_out = hmap::gpu::jagged(*pa_in,
                                    kw,
                                    amp,
                                    seed,
                                    jitter,
                                    gamma,
                                    angle,
                                    pa_mask,
                                    pa_dx,
                                    pa_dy,
                                    region.bbox);
      },
      node.cfg().cm_gpu);

  p_out->smooth_overlap_buffers();

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
