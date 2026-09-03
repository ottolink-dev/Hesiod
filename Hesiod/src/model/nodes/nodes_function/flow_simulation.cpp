/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"
#include "highmap/morphology.hpp"
#include "highmap/primitives.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_ELEVATION = "elevation";
constexpr const char *P_DEPTH_MAP = "depth_map";
constexpr const char *P_WATER_IN  = "water_depth_in";
constexpr const char *P_WATER_OUT = "water_depth";

constexpr const char *A_WATER_DEPTH             = "water_depth";
constexpr const char *A_DURATION                = "duration";
constexpr const char *A_DRY_OUT_RATIO           = "dry_out_ratio";
constexpr const char *A_FLUX_DIFFUSION          = "flux_diffusion";
constexpr const char *A_FLUX_DIFFUSION_STRENGTH = "flux_diffusion_strength";
constexpr const char *A_SOLVER_STRIDE           = "solver_stride";
constexpr const char *A_DEPTH_MAP_TYPE          = "depth_map_type";
constexpr const char *A_POST_FILTER             = "post_filter";
constexpr const char *A_RADIUS                  = "radius";
constexpr const char *A_AREA_FILTER             = "area_filter";
constexpr const char *A_RADIUS_LIMIT            = "radius_limit";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flow_simulation_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // ports
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DEPTH_MAP);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_WATER_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_OUT, CONFIG(node));

  // attributes
  // clang-format off
  add_float(node, A_WATER_DEPTH, "Initial Water Depth", 0.01f, 0.001f, 0.5f, "{:.2e}", true);
  add_float(node, A_DURATION, "Simulation Duration", 0.2f, 0.f, 8.f);
  add_float(node, A_DRY_OUT_RATIO, "Dry-Out Threshold Ratio", 0.01f, 0.f, 1.f);
  add_bool(node, A_FLUX_DIFFUSION, "Enable Flux Diffusion", true);
  add_float(node, A_FLUX_DIFFUSION_STRENGTH, "Flux Diffusion Strength", 0.01f, 0.f, 0.1f);
  add_int(node, A_SOLVER_STRIDE, "Solver Iteration Stride", 8, 1, 32);
  add_enum(node, A_DEPTH_MAP_TYPE, "Predefined Depth Map", DefaultMapOptions::type_map());
  add_bool(node, A_POST_FILTER, "Enable Filtering", false);
  add_float(node, A_RADIUS, "Filter Radius", 0.05f, 0.f, 0.2f);
  add_bool(node, A_AREA_FILTER, "Remove Small Flow Regions", true);
  add_float(node, A_RADIUS_LIMIT, "Minimum Lake Radius", 0.01f, 1e-3f, 0.5f, "{:.2e}", true);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flow_simulation_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // --- Inputs / Outputs

  auto *p_z         = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  auto *p_depth_map = node.get_value_ref<hmap::VirtualArray>(P_DEPTH_MAP);
  auto *p_water_in  = node.get_value_ref<hmap::VirtualArray>(P_WATER_IN);
  auto *p_water_out = node.get_value_ref<hmap::VirtualArray>(P_WATER_OUT);

  if (!p_z)
    return;

  // --- Params

  // clang-format off
  const auto water_depth_init = node.val<float>(A_WATER_DEPTH);
  const auto duration         = node.val<float>(A_DURATION);
  const auto dry_out_ratio    = node.val<float>(A_DRY_OUT_RATIO);
  const auto flux_diffusion   = node.val<bool>(A_FLUX_DIFFUSION);
  const auto flux_strength    = node.val<float>(A_FLUX_DIFFUSION_STRENGTH);
  const auto solver_stride    = node.val<int>(A_SOLVER_STRIDE);
  const auto post_filter      = node.val<bool>(A_POST_FILTER);
  const auto radius           = node.val<float>(A_RADIUS);
  const auto area_filter      = node.val<bool>(A_AREA_FILTER);
  const auto radius_limit     = node.val<float>(A_RADIUS_LIMIT);
  // clang-format on

  // --- Compute mode

  hmap::ComputeMode cm = node.cfg().cm_gpu;
  cm.stride            = solver_stride;

  int   iterations  = int(duration * p_z->shape.x / cm.stride);
  float water_depth = water_depth_init;

  // --- Resolve depth map

  hmap::VirtualArray dmap(CONFIG(node));

  if (p_water_in)
  {
    p_depth_map = &dmap;
    copy_data(*p_water_in, dmap, node.cfg().cm_cpu);
    water_depth = p_depth_map->max(node.cfg().cm_cpu);
  }
  else if (!p_depth_map)
  {
    auto map_type = DefaultMapOptions::Type(node.val<int>(A_DEPTH_MAP_TYPE));

    generate_map(node, p_depth_map, dmap, DefaultMapOptions{.map_type = map_type});
  }

  float dmin = p_depth_map->min(node.cfg().cm_cpu);
  float dmax = p_depth_map->max(node.cfg().cm_cpu);

  if (dmin == dmax)
  {
    dmin = 0.f;
    dmax = 1.f;
  }

  // --- Compute

  hmap::for_each_tile(
      {p_z, p_depth_map},
      {p_water_out},
      [&](std::vector<const hmap::Array *> in,
          std::vector<hmap::Array *>       out,
          const hmap::TileRegion &)
      {
        auto [pa_z, pa_depth] = unpack<2>(in);
        auto [pa_water]       = unpack<1>(out);

        hmap::Array depth_scaled = *pa_depth;
        hmap::remap(depth_scaled, 0.f, 1.f, dmin, dmax);

        *pa_water = hmap::gpu::flow_simulation(*pa_z,
                                               water_depth,
                                               depth_scaled,
                                               iterations,
                                               0.5f,
                                               flux_diffusion,
                                               flux_strength,
                                               dry_out_ratio);
      },
      cm);

  // --- Post-filter

  if (area_filter)
  {
    float area_pixels = M_PI * std::pow(radius_limit * p_z->shape.x, 2);

    hmap::for_each_tile(
        {p_water_out},
        [&](std::vector<hmap::Array *> arrays, const hmap::TileRegion &)
        {
          auto [pa_water] = unpack<1>(arrays);

          *pa_water = hmap::area_remove(*pa_water, area_pixels);
        },
        node.cfg().cm_single_array); // mandatory
  }

  if (post_filter)
  {
    int ir = std::max(1, int(radius * p_z->shape.x));

    hmap::for_each_tile(
        {p_z, p_water_out},
        [&](std::vector<hmap::Array *> arrays, const hmap::TileRegion &)
        {
          auto [pa_z, pa_water] = unpack<2>(arrays);

          hmap::gpu::water_depth_filter(*pa_water, *pa_z, ir, nullptr, true, 0.2f);
        },
        node.cfg().cm_gpu);
  }
}

} // namespace hesiod
