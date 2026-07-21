/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_NOISE_R = "noise_r";
constexpr const char *P_NOISE_THETA = "noise_theta";
constexpr const char *P_ENV = "envelope";
constexpr const char *P_OUT = "output";

constexpr const char *A_RMIN = "rmin";
constexpr const char *A_RMAX = "rmax";
constexpr const char *A_ASPECT_RATIO = "aspect_ratio";
constexpr const char *A_SMOOTHING_WIDTH = "smoothing_width";
constexpr const char *A_SQUARE_BASE = "square_base";
constexpr const char *A_ANGLE = "angle";
constexpr const char *A_SECTOR_ANGLE = "sector_angle";
constexpr const char *A_VMIN = "vmin";
constexpr const char *A_KT_VALUE = "kt_value";
constexpr const char *A_KR_BORDER = "kr_border";
constexpr const char *A_KR_BORDER_RATIO = "kr_border_ratio";
constexpr const char *A_CENTER = "center";

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
  node.add_attr<FloatAttribute>(A_RMIN, "Inner Radius", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_RMAX, "Outer Radius", 0.3f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_ASPECT_RATIO, "Aspect Ratio", 1.f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>(A_SMOOTHING_WIDTH, "Smoothing Width", 0.1f, 0.f, 1.f);
  node.add_attr<BoolAttribute>(A_SQUARE_BASE, "Enable Square Base", false);
  node.add_attr<FloatAttribute>(A_ANGLE, "Angle", 15.f, -180.f, 180.f, "{:.1f}°");
  node.add_attr<FloatAttribute>(A_SECTOR_ANGLE, "Sector Angle", 90.f, 0.f, 360.f, "{:.1f}°");
  node.add_attr<FloatAttribute>(A_VMIN, "Lower Value", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_KT_VALUE, "Wavenumber (value)", 0.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_KR_BORDER, "Wavenumber (border)", 0.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>(A_KR_BORDER_RATIO, "Border Variations", 0.1f, 0.f, 1.f);
  node.add_attr<Vec2FloatAttribute>(A_CENTER, "Center");
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({
      "_GROUPBOX_BEGIN_Shape",
      A_RMIN,
      A_RMAX,
      A_ASPECT_RATIO,
      A_SMOOTHING_WIDTH,
      A_SQUARE_BASE,
      A_CENTER,
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Angular Controls",
      A_ANGLE,
      A_SECTOR_ANGLE,
      A_VMIN,
      A_KT_VALUE,
      "_GROUPBOX_END_",
      //
      "_GROUPBOX_BEGIN_Border Controls",
      A_KR_BORDER,
      A_KR_BORDER_RATIO,
      "_GROUPBOX_END_",
  });

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

  auto *p_noise_r = node.get_value_ref<hmap::VirtualArray>(P_NOISE_R);
  auto *p_noise_theta = node.get_value_ref<hmap::VirtualArray>(P_NOISE_THETA);
  auto *p_env = node.get_value_ref<hmap::VirtualArray>(P_ENV);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_out)
    return;

  // --- Params

  // clang-format off
  const auto rmin            = node.get_attr<FloatAttribute>(A_RMIN);
  const auto rmax            = node.get_attr<FloatAttribute>(A_RMAX);
  const auto aspect_ratio    = node.get_attr<FloatAttribute>(A_ASPECT_RATIO);
  const auto smoothing_width = node.get_attr<FloatAttribute>(A_SMOOTHING_WIDTH);
  const auto square_base     = node.get_attr<BoolAttribute>(A_SQUARE_BASE);
  const auto angle           = node.get_attr<FloatAttribute>(A_ANGLE);
  const auto sector_angle    = node.get_attr<FloatAttribute>(A_SECTOR_ANGLE);
  const auto vmin            = node.get_attr<FloatAttribute>(A_VMIN);
  const auto kt_value        = node.get_attr<FloatAttribute>(A_KT_VALUE);
  const auto kr_border       = node.get_attr<FloatAttribute>(A_KR_BORDER);
  const auto kr_border_ratio = node.get_attr<FloatAttribute>(A_KR_BORDER_RATIO);
  const auto center          = node.get_attr<Vec2FloatAttribute>(A_CENTER);
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
        auto [pa_out] = unpack<1>(out);

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
