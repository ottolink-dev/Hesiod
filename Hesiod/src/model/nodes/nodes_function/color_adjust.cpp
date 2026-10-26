/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN        = "texture_in";
constexpr const char *P_SHADING = "shading";
constexpr const char *P_OUT       = "texture_out";

constexpr const char *A_LEVELS         = "levels";
constexpr const char *A_EXPOSURE       = "exposure";
constexpr const char *A_CONTRAST       = "contrast";
constexpr const char *A_SATURATION     = "saturation";
constexpr const char *A_TEMPERATURE    = "temperature";
constexpr const char *A_GAMMA          = "gamma";
constexpr const char *A_DITHER         = "dither_amp";
constexpr const char *A_FILMIC         = "filmic_tonemap";
constexpr const char *A_ACES           = "aces_tonemap";
constexpr const char *A_AGX            = "agx_tonemap";
constexpr const char *A_ENABLE_SHADE   = "enable_shading";
constexpr const char *A_SHADE_RADIUS   = "radius";
constexpr const char *A_SHADE_STRENGTH = "strength";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_color_adjust_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SHADING);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_OUT, CONFIG_TEX(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Adjustments");
  add_range(node, A_LEVELS, "Levels", {0.f, 1.f}, -1.f, 2.f, true);
  add_float(node, A_EXPOSURE, "Exposure", 0.f, -10.f, 10.f);
  add_float(node, A_CONTRAST, "Contrast", 1.f, 0.f, 4.f);
  add_float(node, A_SATURATION, "Saturation", 1.f, 0.f, 4.f);
  add_float(node, A_TEMPERATURE, "Temperature", 0.f, -1.f, 1.f);
  add_float(node, A_GAMMA, "Gamma", 1.f, 0.1f, 4.f);

  node.set_current_category("Tonemapping");
  add_bool(node, A_FILMIC, "Filmic Tonemap", false);
  add_bool(node, A_ACES, "ACES Tonemap", false);
  add_bool(node, A_AGX, "AGX Tonemap", false);

  node.set_current_category("Dithering");
  add_float(node, A_DITHER, "Dither Amplitude", 0.f, 0.f, 0.5f);

  node.set_current_category("Shading");
  add_bool(node, A_ENABLE_SHADE, "Enable Shading", true);
  add_float(node, A_SHADE_RADIUS, "Radius", 0.05f, 0.001f, 0.2f);
  add_float(node, A_SHADE_STRENGTH, "Strength", 1.f, 0.f, 4.f);
  // clang-format on

  // --- Attribute(s) order
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_color_adjust_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in        = node.get_value_ref<hmap::VirtualTexture>(P_IN);
  auto *p_shading = node.get_value_ref<hmap::VirtualArray>(P_SHADING);
  auto *p_out       = node.get_value_ref<hmap::VirtualTexture>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto levels      = node.val<glm::vec2>(A_LEVELS);
  const auto exposure    = node.val<float>(A_EXPOSURE);
  const auto contrast    = node.val<float>(A_CONTRAST);
  const auto saturation  = node.val<float>(A_SATURATION);
  const auto temperature = node.val<float>(A_TEMPERATURE);
  const auto gamma       = node.val<float>(A_GAMMA);
  const auto dither      = node.val<float>(A_DITHER);
  const auto filmic      = node.val<bool>(A_FILMIC);
  const auto aces        = node.val<bool>(A_ACES);
  const auto agx         = node.val<bool>(A_AGX);
  // clang-format on

  hmap::ColorAdjust param = {.in_min         = levels[0],
                             .in_max         = levels[1],
                             .exposure       = exposure,
                             .contrast       = contrast,
                             .saturation     = saturation,
                             .temperature    = temperature,
                             .gamma          = gamma,
                             .dither_amp     = dither,
                             .shade          = 0.f,
                             .filmic_tonemap = filmic,
                             .aces_tonemap   = aces,
                             .agx_tonemap    = agx};

  // --- Compute

  p_out->copy_from(*p_in, node.cfg().cm_cpu);

  hmap::for_each_tile(
      {},
      p_out->channels_ptr(),
      [&](std::vector<const hmap::Array *>,
          std::vector<hmap::Array *> p_arrays_out,
          const hmap::TileRegion    &region)
      {
        auto [r, g, b] = unpack<3>(p_arrays_out);

        color_adjust(*r, *g, *b, param, {region.key.tx, region.key.ty});
      },
      node.cfg().cm_cpu);

  // --- Topographic residual shading (optional)

  const bool enable_shading = node.val<bool>(A_ENABLE_SHADE);

  if (p_shading && enable_shading)
  {
    const int   ir             = node.val_pixel_radius(A_SHADE_RADIUS);
    const float shade_strength = node.val<float>(A_SHADE_STRENGTH);

    std::vector<const hmap::VirtualArray *> vas_in  = {p_shading};
    std::vector<hmap::VirtualArray *>       vas_out = p_out->channels_ptr();

    hmap::for_each_tile(
        vas_in,
        vas_out,
        [ir, shade_strength](std::vector<const hmap::Array *> p_arrays_in,
                             std::vector<hmap::Array *>       p_arrays_out,
                             const hmap::TileRegion &)
        {
          auto [pa_z]    = unpack<1>(p_arrays_in);
          auto [r, g, b] = unpack<3>(p_arrays_out);

          hmap::Texture tile_tex(*r, *g, *b);
          hmap::shade_topographic_residual(tile_tex, *pa_z, ir, shade_strength);

          *r = std::move(tile_tex[0]);
          *g = std::move(tile_tex[1]);
          *b = std::move(tile_tex[2]);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
