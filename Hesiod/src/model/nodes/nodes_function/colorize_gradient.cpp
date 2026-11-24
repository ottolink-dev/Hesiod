/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/range.hpp"

#include "meta/metadata/keys.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/constants/color_gradient.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_LEVEL   = "level";
constexpr const char *P_ALPHA   = "alpha";
constexpr const char *P_NOISE   = "noise";
constexpr const char *P_TEXTURE = "texture";

constexpr const char *A_GRADIENT         = "gradient";
constexpr const char *A_REVERSE_COLORMAP = "reverse_colormap";
constexpr const char *A_REVERSE_ALPHA    = "reverse_alpha";
constexpr const char *A_CLAMP_ALPHA      = "clamp_alpha";
constexpr const char *A_ENABLE_SHADE     = "enable_shading";
constexpr const char *A_SHADE_RADIUS     = "radius";
constexpr const char *A_SHADE_STRENGTH   = "strength";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_colorize_gradient_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_LEVEL);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ALPHA);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Gradient");
  add_color_gradient(node, A_GRADIENT, "gradient");

  node.set_metadata(
      A_GRADIENT,
      meta::keys::ui::presets,
      meta::GradientPresets{ColorGradientManager::get_instance().get_as_attr_presets()});

  add_bool(node, A_REVERSE_COLORMAP, "reverse_colormap", false);
  add_bool(node, A_REVERSE_ALPHA, "reverse_alpha", false);
  add_bool(node, A_CLAMP_ALPHA, "clamp_alpha", true);

  node.set_current_category("Shading");
  add_bool(node, A_ENABLE_SHADE, "Enable Shading", true);
  add_float(node, A_SHADE_RADIUS, "Radius", 0.05f, 0.001f, 0.2f);
  add_float(node, A_SHADE_STRENGTH, "Strength", 1.f, 0.f, 4.f);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_colorize_gradient_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_level = node.get_value_ref<hmap::VirtualArray>(P_LEVEL);
  auto *p_alpha = node.get_value_ref<hmap::VirtualArray>(P_ALPHA);
  auto *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto *p_tex   = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_level)
    return;

  // --- Params

  // define colormap based on color gradient
  const auto &gradient         = node.val<meta::ColorGradient>(A_GRADIENT).value();
  const auto  reverse_colormap = node.val<bool>(A_REVERSE_COLORMAP);
  const auto  reverse_alpha    = node.val<bool>(A_REVERSE_ALPHA);
  const auto  clamp_alpha      = node.val<bool>(A_CLAMP_ALPHA);

  std::vector<float>     positions       = {};
  std::vector<glm::vec3> colormap_colors = {};

  for (const auto &data : gradient)
  {
    positions.push_back(data.position);
    colormap_colors.push_back({data.color[0], data.color[1], data.color[2]});
  }

  // --- Compute

  // reverse alpha
  hmap::VirtualArray  alpha_copy;
  hmap::VirtualArray *p_alpha_copy = nullptr;

  if (!p_alpha)
  {
    p_alpha_copy = p_alpha;
  }
  else
  {
    alpha_copy.copy_from(*p_alpha, node.cfg().cm_cpu);
    p_alpha_copy = &alpha_copy;

    hmap::for_each_tile(
        {},
        {p_alpha_copy},
        [clamp_alpha, reverse_alpha](std::vector<const hmap::Array *>,
                                     std::vector<hmap::Array *> p_arrays_out,
                                     const hmap::TileRegion &)
        {
          auto [pa_alpha_copy] = unpack<1>(p_arrays_out);
          hmap::Array &alpha   = *pa_alpha_copy;

          if (clamp_alpha)
            hmap::clamp(alpha, 0.f, 1.f);

          if (reverse_alpha)
            alpha = 1.f - alpha;
        },
        node.cfg().cm_cpu);
  }

  // colorize
  float cmin = p_level->min(node.cfg().cm_cpu);
  float cmax = p_level->max(node.cfg().cm_cpu);

  if (p_noise)
  {
    cmin += p_noise->min(node.cfg().cm_cpu);
    cmax += p_noise->max(node.cfg().cm_cpu);
  }

  hmap::colorize(*p_tex,
                 *p_level,
                 node.cfg().cm_cpu,
                 cmin,
                 cmax,
                 positions,
                 colormap_colors,
                 p_alpha_copy,
                 reverse_colormap,
                 p_noise);

  // --- Topographic residual shading (optional)

  const bool enable_shading = node.val<bool>(A_ENABLE_SHADE);

  if (enable_shading)
  {
    const int   ir             = node.val_pixel_radius(A_SHADE_RADIUS);
    const float shade_strength = node.val<float>(A_SHADE_STRENGTH);

    std::vector<const hmap::VirtualArray *> vas_in  = {p_level};
    std::vector<hmap::VirtualArray *>       vas_out = p_tex->channels_ptr();

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
