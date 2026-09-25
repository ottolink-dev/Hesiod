/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/boundary.hpp"
#include "highmap/filters.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *G_RADIAL_PROFILE = "Radial Profile";
constexpr const char *G_BULK           = "Bulk";
constexpr const char *G_DISTANCE       = "Distance";

// Group: Radial Profile
constexpr const char *A_RADIAL_PROFILE = "radial_profile";
constexpr const char *A_PROFILE_PARAM  = "profile_param";
constexpr const char *A_AMOUNT         = "amount";
constexpr const char *A_RADIUS         = "radius";
constexpr const char *A_CENTER         = "center";
constexpr const char *A_DISTANCE_AXIS  = "distance_axis";

// Group: Bulk
constexpr const char *A_AMPLITUDE = "amplitude";
constexpr const char *A_BULK_TYPE = "bulk_type";

// Group: Distance
constexpr const char *A_STRENGTH          = "strength";
constexpr const char *A_DISTANCE_FUNCTION = "distance_function";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_falloff_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // --- Group 1: Radial Profile
  {
    node.set_current_group(G_RADIAL_PROFILE);

    add_enum(node,
             A_RADIAL_PROFILE,
             "radial_profile",
             enum_mappings.radial_profile_map,
             "Smoothstep");
    add_float(node, A_PROFILE_PARAM, "profile_param", 2.f, 0.f, 16.f);
    add_float(node, A_AMOUNT, "amount", 1.f, 0.f, 1.f);
    add_float(node, A_RADIUS, "radius", 0.5f, 0.f, 1.f);
    add_xy(node, A_CENTER, "center");
    add_enum(node,
             A_DISTANCE_FUNCTION,
             "distance_function",
             enum_mappings.distance_function_map,
             "Euclidian");
    add_enum(node,
             A_DISTANCE_AXIS,
             "distance_axis",
             enum_mappings.distance_function_axis_map,
             "XY");

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = false});
  }

  // --- Group 2: Bulk
  {
    node.set_current_group(G_BULK);

    node.set_current_category("Bulk Shape");
    add_float(node, A_AMPLITUDE, "amplitude", 1.f, -1.f, 4.f);
    add_enum(node,
             A_BULK_TYPE,
             "bulk_type",
             enum_mappings.primitive_type_map,
             "Cubic Pulse");

    node.set_current_category("Position");
    add_xy(node, A_CENTER, "center");

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = false});
  }

  // --- Group 3: Distance
  {
    node.set_current_group(G_DISTANCE);

    add_float(node, A_STRENGTH, "strength", 1.f, 0.f, 4.f);
    add_enum(node,
             A_DISTANCE_FUNCTION,
             "distance_function",
             enum_mappings.distance_function_map,
             "Euclidian");

    setup_post_process_heightmap_attributes(
        node,
        {.add_mix = true, .remap_active_state = false});
  }

  node.set_current_group(G_RADIAL_PROFILE);
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_falloff_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_in  = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

  if (!p_in || !p_out)
    return;

  // --- Current group

  const std::optional<std::string> current_group_name = node.get_meta_group()
                                                            .current_container_name();

  if (!current_group_name)
  {
    Logger::log()->error("compute_falloff_node: no group selected");
    return;
  }

  const std::string current_group = *current_group_name;

  Logger::log()->trace("compute_falloff_node: current_group {}", current_group);

  // --- Compute

  if (current_group == G_RADIAL_PROFILE)
  {
    const auto radial_profile = node.val_enum<hmap::RadialProfile>(A_RADIAL_PROFILE);
    const auto profile_param  = node.val<float>(A_PROFILE_PARAM);
    const auto amount         = node.val<float>(A_AMOUNT);
    const auto radius         = node.val<float>(A_RADIUS);
    const auto center         = node.val<glm::vec2>(A_CENTER);
    const auto distance      = node.val_enum<hmap::DistanceFunction>(A_DISTANCE_FUNCTION);
    const auto distance_axis = node.val_enum<hmap::DistanceFunctionAxis>(A_DISTANCE_AXIS);

    hmap::for_each_tile(
        {p_out, p_in},
        [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = *pa_in;

          hmap::zeroed_edges(*pa_out,
                             radial_profile,
                             profile_param,
                             amount,
                             distance,
                             distance_axis,
                             center,
                             radius,
                             nullptr,
                             region.bbox);
        },
        node.cfg().cm_cpu);
  }
  else if (current_group == G_BULK)
  {
    const auto amplitude = node.val<float>(A_AMPLITUDE);
    const auto bulk_type = node.val_enum<hmap::PrimitiveType>(A_BULK_TYPE);
    const auto center    = node.val<glm::vec2>(A_CENTER);

    hmap::for_each_tile(
        {p_out, p_in},
        [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = hmap::bulkify(*pa_in,
                                  bulk_type,
                                  amplitude,
                                  nullptr,
                                  nullptr,
                                  center,
                                  region.bbox);
        },
        node.cfg().cm_cpu);
  }
  else if (current_group == G_DISTANCE)
  {
    const float strength = node.val<float>(A_STRENGTH);
    const auto  dist_fn  = node.val_enum<hmap::DistanceFunction>(A_DISTANCE_FUNCTION);

    hmap::for_each_tile(
        {p_out, p_in},
        [&strength, dist_fn](std::vector<hmap::Array *> p_arrays,
                             const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = *pa_in;

          hmap::falloff(*pa_out, strength, dist_fn, nullptr, region.bbox);
        },
        node.cfg().cm_cpu);
  }

  // --- Post-process

  p_out->remap(p_in->min(node.cfg().cm_cpu),
               p_in->max(node.cfg().cm_cpu),
               node.cfg().cm_cpu);

  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
