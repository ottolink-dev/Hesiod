/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>
#include <numbers>

#include "highmap/colorize.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION  = "elevation";
constexpr const char *P_DEPOSITION = "deposition";
constexpr const char *P_NOISE      = "noise";
constexpr const char *P_TEXTURE    = "texture";

constexpr const char *A_GRASS    = "grass";
constexpr const char *A_DRY      = "dry_grass";
constexpr const char *A_SOIL     = "soil";
constexpr const char *A_SEDIMENT = "sediment";
constexpr const char *A_ROCK     = "rock";
constexpr const char *A_RIDGE    = "ridge";
constexpr const char *A_SNOW     = "snow";

constexpr const char *A_ROCK_SLOPE    = "rock_slope";
constexpr const char *A_ROCK_EDGE     = "rock_edge";
constexpr const char *A_STRATA        = "strata";
constexpr const char *A_STREAKS       = "streaks";
constexpr const char *A_STREAK_LENGTH = "streak_length";
constexpr const char *A_FLOW          = "flow";
constexpr const char *A_FLOW_WIDTH    = "flow_width";
constexpr const char *A_DEPOSITION    = "deposition_amount";
constexpr const char *A_SNOW_LINE     = "snow_line";
constexpr const char *A_SNOW_AMOUNT   = "snow_amount";
constexpr const char *A_SEED          = "seed";
constexpr const char *A_NOISE_SCALE   = "noise_scale";
constexpr const char *A_COLOR_NOISE   = "color_noise";
constexpr const char *A_DETAIL        = "detail";
constexpr const char *A_CAVITY        = "cavity";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_terrain_texture_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPOSITION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualTexture>(gnode::PortType::OUT, P_TEXTURE, CONFIG_TEX(node));

  // --- Attributes

  node.set_current_category("Materials");
  add_color(node, A_GRASS, "Grass", {0.38f, 0.44f, 0.29f, 1.f});
  add_color(node, A_DRY, "Dry Grass", {0.60f, 0.58f, 0.42f, 1.f});
  add_color(node, A_SOIL, "Soil", {0.64f, 0.55f, 0.43f, 1.f});
  add_color(node, A_SEDIMENT, "Sediment", {0.80f, 0.76f, 0.68f, 1.f});
  add_color(node, A_ROCK, "Rock", {0.53f, 0.50f, 0.46f, 1.f});
  add_color(node, A_RIDGE, "Ridge", {0.71f, 0.70f, 0.68f, 1.f});
  add_color(node, A_SNOW, "Snow", {0.94f, 0.95f, 0.97f, 1.f});

  node.set_current_category("Rock & Ridges");
  add_float(node, A_ROCK_SLOPE, "Rock Slope", 3.f, 0.f, FLT_MAX);
  add_float(node, A_ROCK_EDGE, "Edge Softness", 0.15f, 0.01f, 1.f);
  add_float(node, A_STRATA, "Strata", 0.35f, 0.f, 1.f);
  add_float(node, A_STREAKS, "Ridge Streaks", 0.8f, 0.f, 1.f);
  add_float(node, A_STREAK_LENGTH, "Streak Length", 0.06f, 0.f, 0.25f);

  node.set_current_category("Water & Snow");
  add_float(node, A_FLOW, "Flow Channels", 0.8f, 0.f, 1.f);
  add_float(node, A_FLOW_WIDTH, "Channel Width", 0.5f, 0.f, 1.f);
  add_float(node, A_DEPOSITION, "Deposition", 0.5f, 0.f, 1.f);
  add_float(node, A_SNOW_LINE, "Snow Line", 0.85f, 0.f, 1.f);
  add_float(node, A_SNOW_AMOUNT, "Snow", 1.f, 0.f, 1.f);

  node.set_current_category("Variation");
  add_seed(node, A_SEED);
  add_float(node, A_NOISE_SCALE, "Noise Scale", 0.2f, 0.1f, 2.f);
  add_float(node, A_COLOR_NOISE, "Color Noise", 1.f, 0.f, 2.f);
  add_float(node, A_DETAIL, "Relief Detail", 1.f, 0.f, 2.f);
  add_float(node, A_CAVITY, "Cavity Shading", 0.6f, 0.f, 1.f);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

namespace
{

glm::vec3 rgb(const glm::vec4 &c) { return {c.r, c.g, c.b}; }

} // namespace

void compute_terrain_texture_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  const auto *p_z     = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  const auto *p_dep   = node.get_value_ref<hmap::VirtualArray>(P_DEPOSITION);
  const auto *p_noise = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto       *p_tex   = node.get_value_ref<hmap::VirtualTexture>(P_TEXTURE);

  if (!p_z)
    return;

  // --- Params

  const hmap::TerrainTextureParams p = {
      .grass         = rgb(node.val<glm::vec4>(A_GRASS)),
      .dry           = rgb(node.val<glm::vec4>(A_DRY)),
      .soil          = rgb(node.val<glm::vec4>(A_SOIL)),
      .sediment      = rgb(node.val<glm::vec4>(A_SEDIMENT)),
      .rock          = rgb(node.val<glm::vec4>(A_ROCK)),
      .ridge         = rgb(node.val<glm::vec4>(A_RIDGE)),
      .snow          = rgb(node.val<glm::vec4>(A_SNOW)),
      .rock_slope    = node.val<float>(A_ROCK_SLOPE),
      .rock_edge     = node.val<float>(A_ROCK_EDGE),
      .strata        = node.val<float>(A_STRATA),
      .streaks       = node.val<float>(A_STREAKS),
      .streak_length = node.val<float>(A_STREAK_LENGTH),
      .flow          = node.val<float>(A_FLOW),
      .flow_width    = node.val<float>(A_FLOW_WIDTH),
      .deposition    = node.val<float>(A_DEPOSITION),
      .snow_line     = node.val<float>(A_SNOW_LINE),
      .snow_amount   = node.val<float>(A_SNOW_AMOUNT),
      .seed          = (uint32_t)node.val<int>(A_SEED),
      .noise_scale   = node.val<float>(A_NOISE_SCALE),
      .color_noise   = node.val<float>(A_COLOR_NOISE),
      .detail        = node.val<float>(A_DETAIL),
      .cavity        = node.val<float>(A_CAVITY),
  };

  // --- Compute

  // fall lines and drainage cross tile borders: work on the whole map at once
  hmap::for_each_tile(
      {p_z, p_dep, p_noise},
      p_tex->channels_ptr(),
      [&p](std::vector<const hmap::Array *> p_arrays_in,
           std::vector<hmap::Array *>       p_arrays_out,
           const hmap::TileRegion          &region)
      {
        auto [pa_z, pa_dep, pa_noise] = unpack<3>(p_arrays_in);
        hmap::Texture tex             = hmap::terrain_texture(*pa_z,
                                                  pa_dep,
                                                  p,
                                                  region.bbox,
                                                  0.f,
                                                  -1.f,
                                                  pa_noise);

        for (size_t ch = 0; ch < p_arrays_out.size() && ch < tex.channels.size(); ++ch)
          *p_arrays_out[ch] = std::move(tex.channels[ch]);
      },
      node.cfg().cm_single_array);
}

} // namespace hesiod
