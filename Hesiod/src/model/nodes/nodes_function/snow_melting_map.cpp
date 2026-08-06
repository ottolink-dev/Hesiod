/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION = "elevation";
constexpr const char *P_MELTING_MAP = "melting_map";

constexpr const char *A_MELT_RANGE = "melt_range";
constexpr const char *A_ELEVATION_EXP = "elevation_exp";
constexpr const char *A_ELEVATION_STRENGTH = "elevation_strength";
constexpr const char *A_SUN_AZIMUTH = "sun_azimuth";
constexpr const char *A_SUN_ZENITH = "sun_zenith";
constexpr const char *A_ASPECT_STRENGTH = "aspect_strength";
constexpr const char *A_SLOPE_EXP = "slope_exp";
constexpr const char *A_SLOPE_STRENGTH = "slope_strength";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_snow_melting_map_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MELTING_MAP, CONFIG(node));

  // attribute(s)
  // clang-format off
  node.add_attr<RangeAttribute>(A_MELT_RANGE, "Melt Elevation Range", glm::vec2(0.f, 0.3f), -1.f, 2.f);
  node.add_attr<FloatAttribute>(A_ELEVATION_EXP, "Elevation Exponent", 1.f, 0.01f, 4.f);
  node.add_attr<FloatAttribute>(A_ELEVATION_STRENGTH, "Elevation Strength", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SUN_AZIMUTH, "Sun Azimuth", 0.f, -180.f, 180.f, "{:.0f}°");
  node.add_attr<FloatAttribute>(A_SUN_ZENITH, "Sun Zenith", 60.f, 0.f, 90.f, "{:.0f}°");
  node.add_attr<FloatAttribute>(A_ASPECT_STRENGTH, "Aspect Strength", 0.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_SLOPE_EXP, "Slope Exponent", 1.f, 0.01f, 4.f);
  node.add_attr<FloatAttribute>(A_SLOPE_STRENGTH, "Slope Strength", 0.f, 0.f, 1.f);
  // clang-format on

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Elevation Control",
                             A_MELT_RANGE,
                             A_ELEVATION_EXP,
                             A_ELEVATION_STRENGTH,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Solar Exposure",
                             A_SUN_AZIMUTH,
                             A_SUN_ZENITH,
                             A_ASPECT_STRENGTH,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Terrain Influence",
                             A_SLOPE_EXP,
                             A_SLOPE_STRENGTH,
                             "_GROUPBOX_END_"});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_snow_melting_map_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_z = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  if (!p_z)
    return;

  auto *p_melting_map = node.get_value_ref<hmap::VirtualArray>(P_MELTING_MAP);

  // --- Parameters wrapper

  const auto params = [&node]()
  {
    struct P
    {
      float melt_start_elevation;
      float melt_end_elevation;
      float elevation_exp;
      float elevation_strength;
      float sun_azimuth;
      float sun_zenith;
      float aspect_strength;
      float slope_exp;
      float slope_strength;
    };

    const glm::vec2 range = node.get_attr<RangeAttribute>(A_MELT_RANGE);

    return P{.melt_start_elevation = range.x,
             .melt_end_elevation = range.y,
             .elevation_exp = node.get_attr<FloatAttribute>(A_ELEVATION_EXP),
             .elevation_strength = node.get_attr<FloatAttribute>(A_ELEVATION_STRENGTH),
             .sun_azimuth = node.get_attr<FloatAttribute>(A_SUN_AZIMUTH),
             .sun_zenith = node.get_attr<FloatAttribute>(A_SUN_ZENITH),
             .aspect_strength = node.get_attr<FloatAttribute>(A_ASPECT_STRENGTH),
             .slope_exp = node.get_attr<FloatAttribute>(A_SLOPE_EXP),
             .slope_strength = node.get_attr<FloatAttribute>(A_SLOPE_STRENGTH)};
  }();

  // --- Compute

  hmap::for_each_tile(
      {p_z},
      {p_melting_map},
      [&node, &params](std::vector<const hmap::Array *> p_arrays_in,
                       std::vector<hmap::Array *>       p_arrays_out,
                       const hmap::TileRegion &)
      {
        const auto [pa_z] = unpack<1>(p_arrays_in);
        auto [pa_melting_map] = unpack<1>(p_arrays_out);

        *pa_melting_map = hmap::snow_melting_map(*pa_z,
                                                 params.melt_start_elevation,
                                                 params.melt_end_elevation,
                                                 params.elevation_exp,
                                                 params.elevation_strength,
                                                 params.sun_azimuth,
                                                 params.sun_zenith,
                                                 params.aspect_strength,
                                                 params.slope_exp,
                                                 params.slope_strength);
      },
      node.cfg().cm_cpu);
}

} // namespace hesiod
