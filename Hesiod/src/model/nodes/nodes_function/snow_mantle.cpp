/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION_IN  = "elevation_in";
constexpr const char *P_SNOW_DEPTH_IN = "snow_depth_in";
constexpr const char *P_MASK          = "mask";
constexpr const char *P_NOISE         = "noise";
constexpr const char *P_ELEVATION_OUT = "elevation";
constexpr const char *P_SNOW_DEPTH    = "snow_depth";

constexpr const char *A_STRENGTH         = "strength";
constexpr const char *A_SNOWLINE         = "snowline";
constexpr const char *A_FALLOFF          = "falloff";
constexpr const char *A_TALUS_COVERAGE   = "talus_coverage";
constexpr const char *A_TALUS            = "talus";
constexpr const char *A_DIRECTIONAL      = "directional";
constexpr const char *A_WIND_DIRECTION   = "wind_direction";
constexpr const char *A_PEAKS            = "peaks";
constexpr const char *A_MELT             = "melt";
constexpr const char *A_SUN_AZIMUTH      = "sun_azimuth";
constexpr const char *A_SUN_ZENITH       = "sun_zenith";
constexpr const char *A_THERMAL_DURATION = "thermal_duration";
constexpr const char *A_THERMAL_TALUS    = "thermal_talus";
constexpr const char *A_COVERAGE         = "coverage";
constexpr const char *A_RADIUS           = "radius";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_snow_mantle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_SNOW_DEPTH_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_NOISE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_ELEVATION_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_SNOW_DEPTH, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.set_current_category("Snow Setup");
  add_float(node, A_STRENGTH, "Snow Amount", 0.1f, 0.001f, 0.5f, "{:.2e}", /* log */ true);
  add_float(node, A_SNOWLINE, "Snowline Altitude", 0.5f, 0.f, 1.f);
  add_float(node, A_FALLOFF, "Snowline Falloff", 0.1f, 0.f, 0.5f);
  add_float(node, A_COVERAGE, "Coverage Expansion", 0.5f, 0.f, 1.f);

  node.set_current_category("Slope & Peaks");
  add_float(node, A_TALUS_COVERAGE, "Slope Shedding Strength", 0.8f, 0.f, 1.f);
  add_float(node, A_TALUS, "Slope", 1.f, 0.f, FLT_MAX);
  add_float(node, A_PEAKS, "Peaks & Ridges Bonus", 0.2f, 0.f, 1.f);
  add_float(node, A_RADIUS, "Prefilter Radius", 0.05f, 0.f, 0.2f);
 
  node.set_current_category("Wind");
  add_float(node, A_DIRECTIONAL, "Wind Directional Bias", 0.f, 0.f, 1.f);
  add_float(node, A_WIND_DIRECTION, "Wind Azimuth", 0.f, 0.f, 360.f, "{:.0f}°");

  node.set_current_category("Melting");
  add_float(node, A_MELT, "Solar Melt Strength", 0.f, 0.f, 1.f);
  add_float(node, A_SUN_AZIMUTH, "Sun Azimuth", 180.f, -180.f, 180.f, "{:.0f}°");
  add_float(node, A_SUN_ZENITH, "Sun Zenith", 45.f, 0.f, 90.f, "{:.0f}°");

  node.set_current_category("Downhill Flow");
  add_float(node, A_THERMAL_DURATION, "Thermal Duration", 0.1f, 0.f, 6.f);
  add_float(node, A_THERMAL_TALUS, "Thermal Slope", 1.f, 0.f, FLT_MAX);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_snow_mantle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_z = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION_IN);
  if (!p_z)
    return;

  auto *p_snow_in = node.get_value_ref<hmap::VirtualArray>(P_SNOW_DEPTH_IN);
  auto *p_mask    = node.get_value_ref<hmap::VirtualArray>(P_MASK);
  auto *p_noise   = node.get_value_ref<hmap::VirtualArray>(P_NOISE);
  auto *p_z_out   = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION_OUT);
  auto *p_snow    = node.get_value_ref<hmap::VirtualArray>(P_SNOW_DEPTH);

  // --- Parameters

  const float strength         = node.val<float>(A_STRENGTH);
  const float snowline         = node.val<float>(A_SNOWLINE);
  const float falloff          = node.val<float>(A_FALLOFF);
  const float talus_coverage   = node.val<float>(A_TALUS_COVERAGE);
  const float slope            = node.val<float>(A_TALUS);
  const float directional      = node.val<float>(A_DIRECTIONAL);
  const float wind_direction   = node.val<float>(A_WIND_DIRECTION);
  const float peaks            = node.val<float>(A_PEAKS);
  const float melt             = node.val<float>(A_MELT);
  const float sun_azimuth      = node.val<float>(A_SUN_AZIMUTH);
  const float sun_zenith       = node.val<float>(A_SUN_ZENITH);
  const float thermal_duration = node.val<float>(A_THERMAL_DURATION);
  const float thermal_slope    = node.val<float>(A_THERMAL_TALUS);
  const float coverage         = node.val<float>(A_COVERAGE);
  const int   ir               = node.val_pixel_radius(A_RADIUS);

  // --- Tile-based computation

  const float nx                 = float(p_z->shape.x);
  const float talus              = slope / nx;
  const float thermal_talus      = thermal_slope / nx;
  const int   thermal_iterations = int(thermal_duration * nx);

  hmap::for_each_tile(
      {p_z, p_snow_in, p_mask, p_noise},
      {p_z_out, p_snow},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_snow_in, pa_mask, pa_noise] = unpack<4>(p_arrays_in);
        auto [pa_z_out, pa_snow]                   = unpack<2>(p_arrays_out);

        *pa_snow = hmap::gpu::snow_mantle(*pa_z,
                                          strength,
                                          snowline,
                                          falloff,
                                          ir,
                                          talus_coverage,
                                          talus,
                                          directional,
                                          wind_direction,
                                          peaks,
                                          melt,
                                          sun_azimuth,
                                          sun_zenith,
                                          thermal_iterations,
                                          talus, // same as snow limit
                                          coverage,
                                          pa_snow_in,
                                          pa_mask,
                                          pa_noise);

        *pa_z_out = *pa_z + *pa_snow;
      },
      node.cfg().cm_gpu);
}

} // namespace hesiod
