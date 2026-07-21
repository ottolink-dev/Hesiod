/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/boundary.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN = "input";
constexpr const char *P_DR = "dr";
constexpr const char *P_OUT = "output";

constexpr const char *A_RADIAL_PROFILE = "radial_profile";
constexpr const char *A_PROFILE_PARAM = "profile_param";
constexpr const char *A_AMOUNT = "amount";
constexpr const char *A_RADIUS = "radius";
constexpr const char *A_CENTER = "center";
constexpr const char *A_DISTANCE = "distance_function";
constexpr const char *A_DISTANCE_AXIS = "distance_axis";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_zeroed_edges_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DR);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.add_attr<EnumAttribute>(A_RADIAL_PROFILE, "Radial Profile", enum_mappings.radial_profile_map, "Smoothstep");
  node.add_attr<FloatAttribute>(A_PROFILE_PARAM, "Profile Parameter", 2.f, 0.f, 16.f);
  node.add_attr<FloatAttribute>(A_AMOUNT, "Amount", 1.f, 0.f, 1.f);
  node.add_attr<FloatAttribute>(A_RADIUS, "Radius", 0.5f, 0.f, 1.f);
  node.add_attr<Vec2FloatAttribute>(A_CENTER, "Center");
  node.add_attr<EnumAttribute>(A_DISTANCE, "Distance Function", enum_mappings.distance_function_map, "Euclidian");
  node.add_attr<EnumAttribute>(A_DISTANCE_AXIS, "Distance Axis", enum_mappings.distance_function_axis_map, "XY");
  // clang-format on

  // --- Attribute(s) order

  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             A_AMOUNT,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Profile",
                             A_RADIAL_PROFILE,
                             A_PROFILE_PARAM,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Falloff",
                             A_RADIUS,
                             A_DISTANCE,
                             A_DISTANCE_AXIS,
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Position",
                             A_CENTER,
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_zeroed_edges_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_dr = node.get_value_ref<hmap::VirtualArray>(P_DR);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Params

  // clang-format off
  const auto radial_profile = hmap::RadialProfile(node.get_attr<EnumAttribute>(A_RADIAL_PROFILE));
  const auto profile_param  = node.get_attr<FloatAttribute>(A_PROFILE_PARAM);
  const auto amount         = node.get_attr<FloatAttribute>(A_AMOUNT);
  const auto radius         = node.get_attr<FloatAttribute>(A_RADIUS);
  const auto center         = node.get_attr<Vec2FloatAttribute>(A_CENTER);
  const auto distance       = hmap::DistanceFunction(node.get_attr<EnumAttribute>(A_DISTANCE));
  const auto distance_axis  = hmap::DistanceFunctionAxis(node.get_attr<EnumAttribute>(A_DISTANCE_AXIS));
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_out, p_in, p_dr},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        auto [pa_out, pa_in, pa_dr] = unpack<3>(p_arrays);

        *pa_out = *pa_in;

        hmap::zeroed_edges(*pa_out,
                           radial_profile,
                           profile_param,
                           amount,
                           distance,
                           distance_axis,
                           center,
                           radius,
                           pa_dr,
                           region.bbox);
      },
      node.cfg().cm_cpu);

  // --- Post-process

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
