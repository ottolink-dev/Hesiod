/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/range.hpp"

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

  auto &gradient_attr = add_color_gradient(node, A_GRADIENT, "gradient");
  gradient_attr.metadata().add(
      meta::keys::ui::presets,
      meta::GradientPresets{ColorGradientManager::get_instance().get_as_attr_presets()});

  add_bool(node, A_REVERSE_COLORMAP, "reverse_colormap", false);
  add_bool(node, A_REVERSE_ALPHA, "reverse_alpha", false);
  add_bool(node, A_CLAMP_ALPHA, "clamp_alpha", true);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_colorize_gradient_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_level = node.get_value_ref<hmap::VirtualArray>(P_LEVEL);

  if (p_level)
  {
    auto *p_alpha = node.get_value_ref<hmap::VirtualArray>(P_ALPHA);
    auto *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
    auto *p_tex   = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

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
          {p_alpha_copy},
          [clamp_alpha, reverse_alpha](std::vector<hmap::Array *> p_arrays,
                                       const hmap::TileRegion &)
          {
            hmap::Array &alpha = *p_arrays[0];

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
  }
}

} // namespace hesiod
