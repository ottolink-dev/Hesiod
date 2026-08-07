/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/colorize.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "texture_in";
constexpr const char *P_OUT = "texture_out";

constexpr const char *A_LEVELS      = "levels";
constexpr const char *A_EXPOSURE    = "exposure";
constexpr const char *A_CONTRAST    = "contrast";
constexpr const char *A_SATURATION  = "saturation";
constexpr const char *A_TEMPERATURE = "temperature";
constexpr const char *A_GAMMA       = "gamma";
constexpr const char *A_DITHER      = "dither_amp";
constexpr const char *A_FILMIC      = "filmic_tonemap";
constexpr const char *A_ACES        = "aces_tonemap";
constexpr const char *A_AGX         = "agx_tonemap";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_color_adjust_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualTexture>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_OUT, CONFIG_TEX(node));

  // --- Attributes

  // clang-format off
  node.add_attr<RangeAttribute>(A_LEVELS, "Levels");
  node.add_attr<FloatAttribute>(A_EXPOSURE, "Exposure", 0.f, -10.f, 10.f);
  node.add_attr<FloatAttribute>(A_CONTRAST, "Contrast", 1.f, 0.f, 4.f);
  node.add_attr<FloatAttribute>(A_SATURATION, "Saturation", 1.f, 0.f, 4.f);
  node.add_attr<FloatAttribute>(A_TEMPERATURE, "Temperature", 0.f, -1.f, 1.f);
  node.add_attr<FloatAttribute>(A_GAMMA, "Gamma", 1.f, 0.1f, 4.f);
  node.add_attr<FloatAttribute>(A_DITHER, "Dither Amplitude", 0.f, 0.f, 0.5f);
  node.add_attr<BoolAttribute>(A_FILMIC, "Filmic Tonemap", false);
  node.add_attr<BoolAttribute>(A_ACES, "ACES Tonemap", false);
  node.add_attr<BoolAttribute>(A_AGX, "AGX Tonemap", false);
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Color",
                             A_LEVELS,
                             A_EXPOSURE,
                             A_CONTRAST,
                             A_SATURATION,
                             A_TEMPERATURE,
                             A_GAMMA,
                             A_DITHER,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Tonemapping",
                             A_FILMIC,
                             A_ACES,
                             A_AGX,
                             "_GROUPBOX_END_"});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_color_adjust_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualTexture>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualTexture>(P_OUT);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto levels      = node.get_attr<RangeAttribute>(A_LEVELS);
  const auto exposure    = node.get_attr<FloatAttribute>(A_EXPOSURE);
  const auto contrast    = node.get_attr<FloatAttribute>(A_CONTRAST);
  const auto saturation  = node.get_attr<FloatAttribute>(A_SATURATION);
  const auto temperature = node.get_attr<FloatAttribute>(A_TEMPERATURE);
  const auto gamma       = node.get_attr<FloatAttribute>(A_GAMMA);
  const auto dither      = node.get_attr<FloatAttribute>(A_DITHER);
  const auto filmic      = node.get_attr<BoolAttribute>(A_FILMIC);
  const auto aces        = node.get_attr<BoolAttribute>(A_ACES);
  const auto agx         = node.get_attr<BoolAttribute>(A_AGX);
  // clang-format on

  hmap::ColorAdjust param = {.in_min         = levels[0],
                             .in_max         = levels[1],
                             .exposure       = exposure,
                             .contrast       = contrast,
                             .saturation     = saturation,
                             .temperature    = temperature,
                             .gamma          = gamma,
                             .dither_amp     = dither,
                             .filmic_tonemap = filmic,
                             .aces_tonemap   = aces,
                             .agx_tonemap    = agx};

  // --- Compute

  p_out->copy_from(*p_in, node.cfg().cm_cpu);

  for_each_tile(
      *p_out,
      [&](std::vector<hmap::Array *> &p, const hmap::TileRegion &region)
      {
        hmap::Array &r = *p[0];
        hmap::Array &g = *p[1];
        hmap::Array &b = *p[2];

        color_adjust(r, g, b, param, {region.key.tx, region.key.ty});
      },
      node.cfg().cm_cpu);
}

} // namespace hesiod
