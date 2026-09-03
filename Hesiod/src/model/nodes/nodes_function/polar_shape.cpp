/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_NOISE_R     = "noise_r";
constexpr const char *P_NOISE_THETA = "noise_theta";
constexpr const char *P_ENV         = "envelope";
constexpr const char *P_OUT         = "output";

constexpr const char *A_RMIN            = "rmin";
constexpr const char *A_RMAX            = "rmax";
constexpr const char *A_ASPECT_RATIO    = "aspect_ratio";
constexpr const char *A_SMOOTHING_WIDTH = "smoothing_width";
constexpr const char *A_SQUARE_BASE     = "square_base";
constexpr const char *A_ANGLE           = "angle";
constexpr const char *A_SECTOR_ANGLE    = "sector_angle";
constexpr const char *A_VMIN            = "vmin";
constexpr const char *A_KT_VALUE        = "kt_value";
constexpr const char *A_KR_BORDER       = "kr_border";
constexpr const char *A_KR_BORDER_RATIO = "kr_border_ratio";
constexpr const char *A_CENTER          = "center";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_polar_shape_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_R);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE_THETA);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ENV);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  add_float(node, A_RMIN, "Inner Radius", 0.1f, 0.f, 1.f);
  add_float(node, A_RMAX, "Outer Radius", 0.3f, 0.f, 1.f);
  add_float(node, A_ASPECT_RATIO, "Aspect Ratio", 1.f, 0.01f, 10.f);
  add_float(node, A_SMOOTHING_WIDTH, "Smoothing Width", 0.1f, 0.f, 1.f);
  add_bool(node, A_SQUARE_BASE, "Enable Square Base", false);
  add_float(node, A_ANGLE, "Angle", 15.f, -180.f, 180.f, "{:.1f}°");
  add_float(node, A_SECTOR_ANGLE, "Sector Angle", 90.f, 0.f, 360.f, "{:.1f}°");
  add_float(node, A_VMIN, "Lower Value", 0.5f, 0.f, 1.f);
  add_float(node, A_KT_VALUE, "Wavenumber (value)", 0.f, 0.f, FLT_MAX);
  add_float(node, A_KR_BORDER, "Wavenumber (border)", 0.f, 0.f, FLT_MAX);
  add_float(node, A_KR_BORDER_RATIO, "Border Variations", 0.1f, 0.f, 1.f);
  add_xy(node, A_CENTER, "Center");
  // clang-format on

  // --- Attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_polar_shape_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_noise_r     = node.get_value_ref<hmap::VirtualArray>(P_NOISE_R);
  auto *p_noise_theta = node.get_value_ref<hmap::VirtualArray>(P_NOISE_THETA);
  auto *p_env         = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out         = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto rmin            = node.val<float>(A_RMIN);
  const auto rmax            = node.val<float>(A_RMAX);
  const auto aspect_ratio    = node.val<float>(A_ASPECT_RATIO);
  const auto smoothing_width = node.val<float>(A_SMOOTHING_WIDTH);
  const auto square_base     = node.val<bool>(A_SQUARE_BASE);
  const auto angle           = node.val<float>(A_ANGLE);
  const auto sector_angle    = node.val<float>(A_SECTOR_ANGLE);
  const auto vmin            = node.val<float>(A_VMIN);
  const auto kt_value        = node.val<float>(A_KT_VALUE);
  const auto kr_border       = node.val<float>(A_KR_BORDER);
  const auto kr_border_ratio = node.val<float>(A_KR_BORDER_RATIO);
  const auto center          = node.val<glm::vec2>(A_CENTER);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_noise_r, p_noise_theta},
      {p_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion          &region)
      {
        auto [pa_noise_r, pa_noise_theta] = unpack<2>(in);
        auto [pa_out]                     = unpack<1>(out);

        *pa_out = hmap::polar_shape(region.shape,
                                    rmin,
                                    rmax,
                                    aspect_ratio,
                                    smoothing_width,
                                    square_base,
                                    angle,
                                    sector_angle,
                                    vmin,
                                    kt_value,
                                    kr_border,
                                    kr_border_ratio,
                                    pa_noise_r,
                                    pa_noise_theta,
                                    center,
                                    region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
