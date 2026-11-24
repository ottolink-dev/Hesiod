/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

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

constexpr const char *P_IN   = "input";
constexpr const char *P_DX   = "dx";
constexpr const char *P_DY   = "dy";
constexpr const char *P_MASK = "mask";
constexpr const char *P_OUT  = "output";

constexpr const char *A_SLOPE          = "slope";
constexpr const char *A_DIRECTIONS     = "directions";
constexpr const char *A_SKEW           = "skew";
constexpr const char *A_ANGLE          = "angle";
constexpr const char *A_MODE           = "mode";
constexpr const char *A_ANGULAR_JITTER = "angular_jitter";
constexpr const char *A_FALLOFF_JITTER = "falloff_jitter";
constexpr const char *A_THRESHOLD      = "threshold";
constexpr const char *A_INVERT_INPUT   = "invert_input";
constexpr const char *A_SEED           = "seed";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_distance_shaper_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DX);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DY);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Pattern");
  add_enum(node, A_MODE, "Mode", enum_mappings.distance_mode_map, "Pyramid");
  add_float(node, A_SLOPE, "Slope", 10.f, 0.f, FLT_MAX);
  add_int(node, A_DIRECTIONS, "Directions", 8, 1, 32);
  add_angle(node, A_ANGLE, "Angle", 0.f, -180.f, 180.f);
  add_float(node, A_SKEW, "Skew", 0.f, -1.f, 1.f);
  add_float(node, A_THRESHOLD, "Threshold", 0.f, -1.f, 2.f);
  add_bool(node, A_INVERT_INPUT, "Invert Input", false);

  node.set_current_category("Jitter");
  add_float(node, A_ANGULAR_JITTER, "Angular Jitter", 0.f, 0.f, 1.f);
  add_float(node, A_FALLOFF_JITTER, "Falloff Jitter", 0.f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");
  // clang-format on

  setup_default_noise(node, {.noise_amp = 0.05f, .kw = 4.f, .smoothness = 0.5f});
  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_distance_shaper_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);
  const auto *p_dx   = node.get_value_ref<hmap::VirtualArray>(P_DX);
  const auto *p_dy   = node.get_value_ref<hmap::VirtualArray>(P_DY);
  const auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto       *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Parameters

  const auto slope          = node.val<float>(A_SLOPE);
  const auto directions     = node.val<int>(A_DIRECTIONS);
  const auto skew           = node.val<float>(A_SKEW);
  const auto angle          = node.val<float>(A_ANGLE);
  const auto mode           = node.val_enum<hmap::DistanceMode>(A_MODE);
  const auto angular_jitter = node.val<float>(A_ANGULAR_JITTER);
  const auto falloff_jitter = node.val<float>(A_FALLOFF_JITTER);
  const auto threshold      = node.val<float>(A_THRESHOLD);
  const auto invert_input   = node.val<bool>(A_INVERT_INPUT);
  const auto seed           = static_cast<std::uint32_t>(node.val<int>(A_SEED));

  constexpr auto  method        = hmap::DistanceMethod::RT;
  constexpr bool  invert_output = false;
  constexpr float mix_ratio     = 1.f;

  const float falloff = (p_in->shape.x > 0) ? (slope / static_cast<float>(p_in->shape.x))
                                            : slope;

  // --- Prepare mask

  hmap::VirtualArray mask_default = pre_process_mask(node, p_mask, *p_in);
  if (!mask_default.empty())
    p_mask = &mask_default;

  // --- Resolve default noise for domain warp

  hmap::VirtualArray noise_default_x(CONFIG(node));
  hmap::VirtualArray noise_default_y(CONFIG(node));

  generate_noise(node, p_dx, noise_default_x, 0);
  generate_noise(node, p_dy, noise_default_y, 1);

  // --- Compute

  hmap::for_each_tile(
      {p_in, p_dx, p_dy, p_mask},
      {p_out},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_in, pa_dx, pa_dy, pa_mask] = unpack<4>(p_arrays_in);
        auto [pa_out]                       = unpack<1>(p_arrays_out);

        *pa_out = hmap::gpu::distance_shaper(*pa_in,
                                             falloff,
                                             directions,
                                             skew,
                                             angle,
                                             mode,
                                             method,
                                             angular_jitter,
                                             falloff_jitter,
                                             threshold,
                                             invert_input,
                                             invert_output,
                                             mix_ratio,
                                             seed,
                                             pa_mask,
                                             pa_dx,
                                             pa_dy);
      },
      node.cfg().cm_gpu);

  // --- Post-process

  p_out->sync_overlap_buffers();
  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
