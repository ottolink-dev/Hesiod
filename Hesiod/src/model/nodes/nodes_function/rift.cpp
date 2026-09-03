/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

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

constexpr const char *P_DR          = "dr";
constexpr const char *P_DS          = "offset";
constexpr const char *P_ENV         = "envelope";
constexpr const char *P_OUT         = "output";
constexpr const char *P_RIFT_MASK   = "rift_mask";
constexpr const char *P_BOTTOM_MASK = "bottom_mask";

constexpr const char *A_ANGLE                = "angle";
constexpr const char *A_RADIUS               = "radius";
constexpr const char *A_AXIAL_SLOPE          = "axial_slope";
constexpr const char *A_DEPTH                = "depth";
constexpr const char *A_SCALE_WITH_DEPTH     = "scale_with_depth";
constexpr const char *A_PROFILE              = "profile";
constexpr const char *A_PROFILE_PARAM        = "profile_param";
constexpr const char *A_BOTTOM_EXTENT        = "bottom_extent";
constexpr const char *A_BOTTOM_DEPTH         = "bottom_depth";
constexpr const char *A_BOTTOM_PROFILE       = "bottom_profile";
constexpr const char *A_BOTTOM_PROFILE_PARAM = "bottom_profile_param";
constexpr const char *A_BOTTOM_MIN_DEPTH     = "bottom_force_minimum_depth";
constexpr const char *A_OUTER_SLOPE          = "outer_slope";
constexpr const char *A_CENTER               = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_rift_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DS);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_RIFT_MASK, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_BOTTOM_MASK, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_ANGLE, "Angle", 15.f, -180.f, 180.f);
  add_float(node, A_RADIUS, "Radius", 0.1f, 0.f, 1.f);
  add_float(node, A_AXIAL_SLOPE, "Axial Slope", 0.1f, -1.f, 1.f);
  add_float(node, A_DEPTH, "Depth", 0.1f, 0.f, 1.f);
  add_bool(node, A_SCALE_WITH_DEPTH, "Scale Radius with Depth", true);
  add_enum(node, A_PROFILE, "Profile", enum_mappings.radial_profile_map, "Smoothstep");
  add_float(node, A_PROFILE_PARAM, "Profile Sharpness", 0.5f, 0.f, 10.f);
  add_float(node, A_BOTTOM_EXTENT, "Bottom Extent", 0.2f, 0.f, 1.f);
  add_float(node, A_BOTTOM_DEPTH, "Bottom Depth", 0.02f, 0.f, 1.f);
  add_enum(node, A_BOTTOM_PROFILE, "Bottom Profile", enum_mappings.radial_profile_map, "Square Root");
  add_float(node, A_BOTTOM_PROFILE_PARAM, "Bottom Profile Sharpness", 0.5f, 0.f, 10.f);
  add_bool(node, A_BOTTOM_MIN_DEPTH, "Ensure Minimum Depth", true);
  add_float(node, A_OUTER_SLOPE, "Outer Slope", 0.2f, -4.f, 4.f);
  add_xy(node, A_CENTER, "Center");
  // clang-format on

  // --- Attribute(s) order

  setup_default_noise(
      node,
      {.noise_amp = 1.8f, .kw = 4.f, .smoothness = 0.5f, .noise_type = "Perlin"});
  setup_post_process_heightmap_attributes(
      node,
      {.add_mix = false, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_rift_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_dr    = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_ds    = node.get_value_ref<hmap::VirtualArray>(P_DS);
  auto *p_env   = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_rmask = node.get_value_ref<hmap::VirtualArray>(P_RIFT_MASK);
  auto *p_bmask = node.get_value_ref<hmap::VirtualArray>(P_BOTTOM_MASK);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto angle                  = node.val<float>(A_ANGLE);
  const auto radius                 = node.val<float>(A_RADIUS);
  const auto axial_slope            =  node.val<float>(A_AXIAL_SLOPE);
  const auto depth                  = node.val<float>(A_DEPTH);
  const auto scale_with_depth       = node.val<bool>(A_SCALE_WITH_DEPTH);
  const auto profile                = hmap::RadialProfile(node.val<int>(A_PROFILE));
  const auto profile_param          = node.val<float>(A_PROFILE_PARAM);
  const auto bottom_extent          = node.val<float>(A_BOTTOM_EXTENT);
  const auto bottom_depth           = node.val<float>(A_BOTTOM_DEPTH);
  const auto bottom_profile         = hmap::RadialProfile(node.val<int>(A_BOTTOM_PROFILE));
  const auto bottom_profile_param   = node.val<float>(A_BOTTOM_PROFILE_PARAM);
  const auto bottom_force_min_depth = node.val<bool>(A_BOTTOM_MIN_DEPTH); 
  const auto outer_slope            = node.val<float>(A_OUTER_SLOPE);
  const auto center                 = node.val<glm::vec2>(A_CENTER);
  // clang-format on

  // --- Resolve default noise

  hmap::VirtualArray noise_default(CONFIG(node));
  generate_noise(node, p_ds, noise_default);

  // --- Compute

  hmap::for_each_tile(
      {p_dr, p_ds},
      {p_out, p_rmask, p_bmask},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_dr, pa_ds]               = unpack<2>(in);
        auto [pa_out, pa_rmask, pa_bmask] = unpack<3>(out);

        *pa_out = hmap::rift(region.shape,
                             angle,
                             radius,
                             axial_slope,
                             depth,
                             scale_with_depth,
                             profile,
                             profile_param,
                             bottom_extent,
                             bottom_depth,
                             bottom_profile,
                             bottom_profile_param,
                             bottom_force_min_depth,
                             outer_slope,
                             pa_dr,
                             pa_ds,
                             center,
                             region.bbox,
                             pa_rmask,
                             pa_bmask);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
