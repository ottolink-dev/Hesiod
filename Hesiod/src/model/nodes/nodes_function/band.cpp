/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_DR = "dr";
constexpr const char *P_DS = "offset";
constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_ANGLE = "angle";
constexpr const char *A_LENGTH = "length";
constexpr const char *A_WIDTH = "width";
constexpr const char *A_PROFILE = "profile";
constexpr const char *A_PROFILE_PARAM = "profile_param";
constexpr const char *A_CENTER = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_band_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DS);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  node.set_current_category("Global");
  add_float(node, A_LENGTH, "Length", 0.5f, 0.f, 4.f);
  add_float(node, A_WIDTH, "Width", 0.1f, 0.f, 1.f);
  add_angle(node, A_ANGLE, "Angle", 0.f, -180.f, 180.f);
  add_xy(node, A_CENTER, "Center");

  node.set_current_category("Profile");
  add_enum(node, A_PROFILE, "Profile", enum_mappings.radial_profile_map, "Smoothstep");
  add_float(node, A_PROFILE_PARAM, "Profile Sharpness", 0.5f, 0.f, 10.f);

  setup_default_noise(
      node,
      {.noise_amp = 1.8f, .kw = 4.f, .smoothness = 0.5f, .noise_type = "Perlin"});
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_band_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dr = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_ds = node.get_value_ref<hmap::VirtualArray>(P_DS);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  const auto angle = node.val<float>(A_ANGLE);
  const auto length = node.val<float>(A_LENGTH);
  const auto width = node.val<float>(A_WIDTH);
  const auto profile = hmap::RadialProfile(node.val<int>(A_PROFILE));
  const auto profile_param = node.val<float>(A_PROFILE_PARAM);
  const auto center = node.val<glm::vec2>(A_CENTER);

  // --- Resolve default noise

  hmap::VirtualArray noise_default(CONFIG(node));
  generate_noise(node, p_ds, noise_default);

  // --- Compute

  hmap::for_each_tile(
      {p_dr, p_ds},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dr, pa_ds] = unpack<2>(in);
        auto [pa_out] = unpack<1>(out);

        *pa_out = hmap::band(region.shape,
                             angle,
                             length,
                             width,
                             profile,
                             profile_param,
                             pa_dr,
                             pa_ds,
                             center,
                             region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
