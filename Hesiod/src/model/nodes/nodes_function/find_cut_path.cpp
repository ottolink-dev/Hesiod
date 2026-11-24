/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/shortest_path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_PATH = "path";
constexpr const char *P_IN   = "input";

constexpr const char *A_SEED                  = "seed";
constexpr const char *A_START                 = "start";
constexpr const char *A_END                   = "end";
constexpr const char *A_CONTROL_POINTS_COUNT  = "control_points_count";
constexpr const char *A_FAVOR_BOUNDARY_CENTER = "favor_boundary_center";
constexpr const char *A_FAVOR_CENTER          = "favor_center";
constexpr const char *A_FAVOR_LOWER_ELEVATION = "favor_lower_elevation";
constexpr const char *A_FAVOR_SINKS           = "favor_sinks";
constexpr const char *A_SMOOTH_PATH           = "smooth_path";
constexpr const char *A_SMOOTH_SAMPLING       = "smooth_sampling";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_find_cut_path_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_PATH);

  // attribute(s)
  // clang-format off
  node.set_current_category("Boundary");
  add_enum(node, A_START, "Start Boundary", enum_mappings.domain_boundary_map, "West");
  add_enum(node, A_END, "End Boundary", enum_mappings.domain_boundary_map, "East");
  add_bool(node, A_FAVOR_BOUNDARY_CENTER, "Favor Boundary Center", true);
  add_bool(node, A_FAVOR_LOWER_ELEVATION, "Favor Lower Elevation", true);
  add_bool(node, A_FAVOR_SINKS, "Favor Sinks (Local Minima)", true);

  node.set_current_category("Shortest Path");
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_CONTROL_POINTS_COUNT, "Control Points Count", 4096, 100, 30000);
  add_bool(node, A_FAVOR_CENTER, "Favor Domain Center", true);

  node.set_current_category("Smoothing");
  add_bool(node, A_SMOOTH_PATH, "Enable Path Smoothing", false);
  add_int(node, A_SMOOTH_SAMPLING, "Samples", 32, 2, INT_MAX);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_find_cut_path_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  auto *p_in   = node.get_value_ref<hmap::VirtualArray>(P_IN);
  auto *p_path = node.get_value_ref<hmap::Path>(P_PATH);

  if (!p_in)
    return;

  // --- Params

  // clang-format off
  const auto start                 = node.val_enum<hmap::DomainBoundary>(A_START);
  const auto end                   = node.val_enum<hmap::DomainBoundary>(A_END);
  const auto seed                  = static_cast<std::uint32_t>(node.val<int>(A_SEED));
  const auto control_points_count  = static_cast<size_t>(node.val<int>(A_CONTROL_POINTS_COUNT));
  const auto favor_boundary_center = node.val<bool>(A_FAVOR_BOUNDARY_CENTER);
  const auto favor_center          = node.val<bool>(A_FAVOR_CENTER);
  const auto favor_lower_elevation = node.val<bool>(A_FAVOR_LOWER_ELEVATION);
  const auto favor_sinks           = node.val<bool>(A_FAVOR_SINKS);
  const auto smooth_path           = node.val<bool>(A_SMOOTH_PATH);
  const auto smooth_sampling       = node.val<int>(A_SMOOTH_SAMPLING);
  // clang-format on

  // --- Compute

  *p_path = hmap::va::find_cut_path_dijkstra(node.cfg().cm_cpu,
                                             *p_in,
                                             start,
                                             end,
                                             seed,
                                             control_points_count,
                                             favor_boundary_center,
                                             favor_lower_elevation,
                                             favor_sinks,
                                             favor_center);

  if (smooth_path)
  {
    *p_path = hmap::decimate_vw(*p_path, smooth_sampling);
    *p_path = hmap::bspline(*p_path);

    hmap::for_each_tile(
        {p_in},
        {},
        [&](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>,
            const hmap::TileRegion &region)
        {
          auto [pa_in] = unpack<1>(p_arrays_in);
          p_path->set_values_from_array(*pa_in, region.bbox);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
