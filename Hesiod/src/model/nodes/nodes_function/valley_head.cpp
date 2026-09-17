/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DR     = "dr";
constexpr const char *P_ENV    = "envelope";
constexpr const char *P_MASK   = "mask";
constexpr const char *P_OFFSET = "offset";
constexpr const char *P_OUT    = "output";

constexpr const char *A_ANGLE             = "angle";
constexpr const char *A_CENTER            = "center";
constexpr const char *A_DEPTH_LENGTH      = "depth_length";
constexpr const char *A_DEVELOPMENT_POWER = "development_power";
constexpr const char *A_MAX_DEPTH         = "max_depth";
constexpr const char *A_MAX_WIDTH         = "max_width";
constexpr const char *A_MIN_WIDTH         = "min_width";
constexpr const char *A_PROFILE_POWER     = "profile_power";
constexpr const char *A_WIDTH_LENGTH      = "width_length";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_valley_head_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_OFFSET);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // --- Attributes

  node.set_current_category("Geometry");
  add_angle(node, A_ANGLE, "Angle", 145.f, -180.f, 180.f);
  add_xy(node, A_CENTER, "Center", {0.2f, 0.8f});
  add_float(node, A_MAX_DEPTH, "Max Depth", 0.3f, 0.f, 1.f);
  add_float(node, A_MIN_WIDTH, "Min Width", 0.02f, 0.f, 1.f);
  add_float(node, A_MAX_WIDTH, "Max Width", 0.4f, 0.f, 1.f);

  node.set_current_category("Evolution");
  add_float(node, A_DEPTH_LENGTH, "Depth Length", 0.5f, 0.001f, 1.f);
  add_float(node, A_WIDTH_LENGTH, "Width Length", 0.5f, 0.001f, 1.f);
  add_float(node, A_DEVELOPMENT_POWER, "Development Power", 1.f, 0.01f, 4.f);
  add_float(node, A_PROFILE_POWER, "Profile Power", 1.3f, 0.01f, 4.f);

  // --- Noise and post process

  setup_default_noise(
      node,
      {.noise_amp = 0.3f, .kw = 4.f, .smoothness = 0.5f, .noise_type = "Perlin"});
  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_valley_head_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_offset = node.get_value_ref<hmap::VirtualArray>(P_OFFSET);
  auto *p_dr     = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_env    = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out    = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_mask   = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_out)
    return;

  // --- Params

  const auto angle             = node.val<float>(A_ANGLE);
  const auto max_depth         = node.val<float>(A_MAX_DEPTH);
  const auto min_width         = node.val<float>(A_MIN_WIDTH);
  const auto max_width         = node.val<float>(A_MAX_WIDTH);
  const auto depth_length      = node.val<float>(A_DEPTH_LENGTH);
  const auto width_length      = node.val<float>(A_WIDTH_LENGTH);
  const auto development_power = node.val<float>(A_DEVELOPMENT_POWER);
  const auto profile_power     = node.val<float>(A_PROFILE_POWER);
  const auto center            = node.val<glm::vec2>(A_CENTER);

  // --- Resolve default noise

  hmap::VirtualArray noise_default(CONFIG(node));
  generate_noise(node, p_offset, noise_default);

  // --- Compute

  hmap::for_each_tile(
      {p_offset, p_dr},
      {p_out, p_mask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_offset, pa_dr] = unpack<2>(in);
        auto [pa_out, pa_mask]  = unpack<2>(out);

        *pa_out = hmap::valley_head(region.shape,
                                    angle,
                                    max_depth,
                                    min_width,
                                    max_width,
                                    depth_length,
                                    width_length,
                                    development_power,
                                    profile_power,
                                    pa_offset,
                                    pa_dr,
                                    center,
                                    region.bbox,
                                    pa_mask);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
