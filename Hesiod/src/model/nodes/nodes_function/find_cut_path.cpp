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

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_PATH = "path";
constexpr const char *P_IN   = "input";

constexpr const char *A_SEED                  = "seed";
constexpr const char *A_START                 = "start";
constexpr const char *A_END                   = "end";
constexpr const char *A_OFFSET_RATIO          = "offset_ratio";
constexpr const char *A_STEPS                 = "steps";
constexpr const char *A_FAVOR_BOUNDARY_CENTER = "favor_boundary_center";
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
  add_enum(node, A_START, "Start Boundary", enum_mappings.domain_boundary_map, "West");
  add_enum(node, A_END, "End Boundary", enum_mappings.domain_boundary_map, "East");
  add_float(node, A_OFFSET_RATIO, "Midpoint Radius", 0.2f, 0.f, 1.f);
  add_int(node, A_STEPS, "Search Steps", 16, 0, 32);
  add_seed(node, A_SEED, "Seed");
  add_bool(node, A_FAVOR_BOUNDARY_CENTER, "Favor Boundary Center", true);
  add_bool(node, A_FAVOR_LOWER_ELEVATION, "Favor Lower Elevation", true);
  add_bool(node, A_FAVOR_SINKS, "Favor Sinks (Local Minima)", true);
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
  const auto start                 = hmap::DomainBoundary(node.val<int>(A_START));
  const auto end                   = hmap::DomainBoundary(node.val<int>(A_END));
  const auto seed                  = node.val<int>(A_SEED);
  const auto offset_ratio          = node.val<float>(A_OFFSET_RATIO);
  const auto steps                 = node.val<int>(A_STEPS);
  const auto favor_boundary_center = node.val<bool>(A_FAVOR_BOUNDARY_CENTER);
  const auto favor_lower_elevation = node.val<bool>(A_FAVOR_LOWER_ELEVATION);
  const auto favor_sinks           = node.val<bool>(A_FAVOR_SINKS);
  const auto smooth_path           = node.val<bool>(A_SMOOTH_PATH);
  const auto smooth_sampling       = node.val<int>(A_SMOOTH_SAMPLING);
  // clang-format on

  // --- Compute

  hmap::for_each_tile(
      {p_in},
      [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_in] = unpack<1>(p_arrays);

        *p_path = hmap::find_cut_path_midpoint(*pa_in,
                                               start,
                                               end,
                                               seed,
                                               offset_ratio,
                                               steps,
                                               favor_boundary_center,
                                               favor_lower_elevation,
                                               favor_sinks);
        if (smooth_path)
        {
          *p_path = hmap::decimate_vw(*p_path, smooth_sampling);
          *p_path = hmap::bspline(*p_path);
          p_path->set_values_from_array(*pa_in);
        }
      },
      node.cfg().cm_single_array); // forced, not tileable
}

// void compute_find_cut_path_node(BaseNode &node)
// {
//   Logger::log()->trace("computing node [{}]/[{}]", node.get_label(),
//   node.get_id());

//   auto *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);
//   auto *p_path = node.get_value_ref<hmap::Path>(P_PATH);

//   if (!p_in)
//     return;

//   // --- Params lambda

//   const auto params = [&node]()
//   {
//     struct P
//     {
//       hmap::DomainBoundary start;
//       hmap::DomainBoundary end;
//       uint                 seed;
//       float                offset_ratio;
//       int                  steps;
//       bool                 favor_boundary_center;
//       bool                 favor_lower_elevation;
//       bool                 favor_sinks;
//       bool                 smooth_path;
//       int                  smooth_sampling;
//     };

//     // clang-format off
//     return P{
//       .start = hmap::DomainBoundary(node.val<int>(A_START)),
//       .end = hmap::DomainBoundary(node.val<int>(A_END)),
//       .seed = node.val<int>(A_SEED),
//       .offset_ratio = node.val<float>(A_OFFSET_RATIO),
//       .steps = node.val<int>(A_STEPS),
//       .favor_boundary_center =
//       node.val<bool>(A_FAVOR_BOUNDARY_CENTER),
//       .favor_lower_elevation =
//       node.val<bool>(A_FAVOR_LOWER_ELEVATION), .favor_sinks =
//       node.val<bool>(A_FAVOR_SINKS), .smooth_path =
//       node.val<bool>(A_SMOOTH_PATH), .smooth_sampling =
//       node.val<int>(A_SMOOTH_SAMPLING)
//     };
//     // clang-format on
//   }();

//   // --- Compute

//   hmap::for_each_tile(
//       {p_in},
//       [&node, p_path, &params](std::vector<hmap::Array *> p_arrays,
//                                const hmap::TileRegion &)
//       {
//         auto [pa_in] = unpack<1>(p_arrays);

//         *p_path = hmap::find_cut_path_midpoint(*pa_in,
//                                                params.start,
//                                                params.end,
//                                                params.seed,
//                                                params.offset_ratio,
//                                                params.steps,
//                                                params.favor_boundary_center,
//                                                params.favor_lower_elevation,
//                                                params.favor_sinks);

//         if (params.smooth_path)
//         {
//           *p_path = hmap::decimate_vw(*p_path, params.smooth_sampling);
//           *p_path = hmap::bspline(*p_path);
//           p_path->set_values_from_array(*pa_in);
//         }
//       },
//       node.cfg().cm_single_array); // forced, not tileable
// }

} // namespace hesiod
