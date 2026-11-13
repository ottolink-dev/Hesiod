/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/carving.hpp"
#include "highmap/geometry/cloud.hpp"

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

constexpr const char *P_IN    = "input";
constexpr const char *P_CLOUD = "cloud";
constexpr const char *P_OUT   = "output";
constexpr const char *P_MASK  = "mask";

constexpr const char *A_RADIUS           = "radius";
constexpr const char *A_STRENGTH         = "strength";
constexpr const char *A_RADIAL_PROFILE   = "radial_profile";
constexpr const char *A_PROFILE_PARAM    = "profile_param";
constexpr const char *A_USE_CLOUD_VALUES = "use_cloud_values";
constexpr const char *A_ARRAY_SAMPLING   = "array_sampling";
constexpr const char *A_SAMPLING_RADIUS  = "sampling_radius";
constexpr const char *A_TERRAIN_AWARE    = "terrain_aware";
constexpr const char *A_SLOPE_WEIGHT     = "slope_weight";
constexpr const char *A_UPHILL_WEIGHT    = "uphill_weight";
constexpr const char *A_MAX_HEIGHT_ABOVE = "max_height_above";
constexpr const char *A_TERRAIN_FOLLOW   = "terrain_follow";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flatten_around_points_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);

  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_MASK, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Flattening");
  add_float(node, A_RADIUS, "Radius", 0.2f, 0.f, 0.5f);
  add_float(node, A_STRENGTH, "Strength", 1.f, 0.f, 1.f);
  add_enum(node, A_RADIAL_PROFILE, "Profile Type", enum_mappings.radial_profile_map, "Smoothstep");
  add_float(node, A_PROFILE_PARAM, "Profile Sharpness", 1.f, 0.f, 8.f);

  node.set_current_category("Elevation Sampling");
  add_bool(node, A_USE_CLOUD_VALUES, "Use Cloud Values", false);
  add_enum(node, A_ARRAY_SAMPLING, "Sampling Method", enum_mappings.array_sampling_map, "Minimum");
  add_float(node, A_SAMPLING_RADIUS, "Sampling Radius", 0.02f, 0.f, 0.5f);

  node.set_current_category("Terrain Aware");
  add_bool(node, A_TERRAIN_AWARE, "Terrain Aware", false);
  add_float(node, A_SLOPE_WEIGHT, "Slope Weight", 1.f, 0.f, 10.f);
  add_float(node, A_UPHILL_WEIGHT, "Uphill Weight", 2.f, 0.f, 10.f);
  add_float(node, A_MAX_HEIGHT_ABOVE, "Max Height Above", -1.f, -1.f, 2.f);
  add_float(node, A_TERRAIN_FOLLOW, "Terrain Follow", 0.5f, 0.f, 1.f);
  // clang-format on

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flatten_around_points_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  const auto *p_in    = node.get_value_ref<hmap::VirtualArray>(P_IN);
  const auto *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);

  auto *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);

  if (!p_in || !p_cloud || !p_out)
    return;

  if (p_cloud->empty())
    return;

  // --- Parameters

  const float radius         = node.val<float>(A_RADIUS);
  const float strength       = node.val<float>(A_STRENGTH);
  const auto  radial_profile = node.val_enum<hmap::RadialProfile>(A_RADIAL_PROFILE);
  const float profile_param  = node.val<float>(A_PROFILE_PARAM);
  const bool  use_cloud_vals = node.val<bool>(A_USE_CLOUD_VALUES);
  const auto  array_sampling = node.val_enum<hmap::ArraySampling>(A_ARRAY_SAMPLING);
  const float sampling_rad   = node.val<float>(A_SAMPLING_RADIUS);
  const bool  terrain_aware  = node.val<bool>(A_TERRAIN_AWARE);
  const float slope_weight   = node.val<float>(A_SLOPE_WEIGHT);
  const float uphill_weight  = node.val<float>(A_UPHILL_WEIGHT);
  const float max_height_abv = node.val<float>(A_MAX_HEIGHT_ABOVE);
  const float terrain_follow = node.val<float>(A_TERRAIN_FOLLOW);

  // --- Sample point elevations if requested

  hmap::Cloud cloud_work = *p_cloud;

  if (!use_cloud_vals)
  {
    hmap::for_each_tile(
        {p_in},
        {},
        [&](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>,
            const hmap::TileRegion &region)
        {
          auto [pa_in] = unpack<1>(p_arrays_in);
          cloud_work.set_values_from_array(*pa_in,
                                           array_sampling,
                                           sampling_rad,
                                           region.bbox);
        },
        node.cfg().cm_cpu);
  }

  const std::vector<float> xp = cloud_work.get_x();
  const std::vector<float> yp = cloud_work.get_y();
  const std::vector<float> zp = cloud_work.get_values();

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      {p_out, p_mask},
      [&](std::vector<const hmap::Array *> p_arrays_in,
          std::vector<hmap::Array *>       p_arrays_out,
          const hmap::TileRegion          &region)
      {
        auto [pa_in]           = unpack<1>(p_arrays_in);
        auto [pa_out, pa_mask] = unpack<2>(p_arrays_out);

        *pa_out = *pa_in;

        if (terrain_aware)
        {
          hmap::flatten_around_points_terrain_aware(*pa_out,
                                                    xp,
                                                    yp,
                                                    zp,
                                                    radius,
                                                    slope_weight,
                                                    uphill_weight,
                                                    max_height_abv,
                                                    terrain_follow,
                                                    strength,
                                                    radial_profile,
                                                    profile_param,
                                                    pa_mask,
                                                    region.bbox);
        }
        else
        {
          hmap::flatten_around_points(*pa_out,
                                      xp,
                                      yp,
                                      zp,
                                      radius,
                                      strength,
                                      radial_profile,
                                      profile_param,
                                      pa_mask,
                                      region.bbox);
        }
      },
      node.cfg().cm_cpu);

  p_out->sync_overlap_buffers();

  if (p_mask)
    p_mask->sync_overlap_buffers();

  // --- Post-process

  post_process_heightmap(node, *p_out, p_in);
}

} // namespace hesiod
