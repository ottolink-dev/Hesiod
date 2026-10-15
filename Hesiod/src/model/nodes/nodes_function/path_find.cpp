/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"
#include "highmap/shortest_path.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_HEIGHTMAP = "heightmap";
constexpr const char *P_PATH      = "path";
constexpr const char *P_WAYPOINTS = "waypoints";

constexpr const char *A_CONTROL_POINTS_COUNT = "control_points_count";
constexpr const char *A_FAVOR_DOMAIN_CENTER  = "favor_domain_center";
constexpr const char *A_SEED                 = "seed";
constexpr const char *A_SMOOTH_PATH          = "smooth_path";
constexpr const char *A_SMOOTH_SAMPLING      = "smooth_sampling";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_path_find_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, P_WAYPOINTS);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_HEIGHTMAP);
  node.add_port<hmap::Path>(gnode::PortType::OUT, P_PATH);

  // attribute(s)
  // clang-format off
  node.set_current_category("Shortest Path");
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_CONTROL_POINTS_COUNT, "Control Points Count", 4096, 100, 30000);
  add_bool(node, A_FAVOR_DOMAIN_CENTER, "Favor Domain Center", false);

  node.set_current_category("Smoothing");
  add_bool(node, A_SMOOTH_PATH, "Enable Path Smoothing", false);
  add_int(node, A_SMOOTH_SAMPLING, "Samples", 32, 2, INT_MAX);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_path_find_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path         *p_waypoints = node.get_value_ref<hmap::Path>(P_WAYPOINTS);
  hmap::VirtualArray *p_hmap      = node.get_value_ref<hmap::VirtualArray>(P_HEIGHTMAP);
  hmap::Path         *p_out       = node.get_value_ref<hmap::Path>(P_PATH);

  if (!p_waypoints || !p_hmap || !p_out)
  {
    return;
  }

  if (p_waypoints->size() < 2)
  {
    *p_out = *p_waypoints;
    return;
  }

  // --- Params

  const auto seed                 = static_cast<std::uint32_t>(node.val<int>(A_SEED));
  const auto control_points_count = static_cast<size_t>(
      node.val<int>(A_CONTROL_POINTS_COUNT));
  const auto   favor_domain_center = node.val<bool>(A_FAVOR_DOMAIN_CENTER);
  const auto   smooth_path         = node.val<bool>(A_SMOOTH_PATH);
  const auto   smooth_sampling     = node.val<int>(A_SMOOTH_SAMPLING);
  const size_t n_waypoints         = p_waypoints->size();
  const size_t n_segments = p_waypoints->is_closed() ? n_waypoints : n_waypoints - 1;

  std::vector<hmap::Point> accumulated_points;

  for (size_t i = 0; i < n_segments; ++i)
  {
    size_t next_i = (i + 1) % n_waypoints;

    glm::vec2 start = {(*p_waypoints)[i].x, (*p_waypoints)[i].y};
    glm::vec2 end   = {(*p_waypoints)[next_i].x, (*p_waypoints)[next_i].y};

    std::uint32_t segment_seed = seed + static_cast<std::uint32_t>(i);

    hmap::Path segment_path = hmap::va::find_path_dijkstra(node.cfg().cm_cpu,
                                                           *p_hmap,
                                                           start,
                                                           end,
                                                           segment_seed,
                                                           control_points_count,
                                                           favor_domain_center);

    if (segment_path.empty())
    {
      if (accumulated_points.empty())
      {
        accumulated_points.push_back((*p_waypoints)[i]);
      }

      accumulated_points.push_back((*p_waypoints)[next_i]);
      continue;
    }

    size_t start_idx = accumulated_points.empty() ? 0 : 1;

    for (size_t p = start_idx; p < segment_path.size(); ++p)
    {
      accumulated_points.push_back(segment_path[p]);
    }
  }

  *p_out = hmap::Path(accumulated_points);
  p_out->set_closed(p_waypoints->is_closed());

  if (smooth_path && p_out->size() > 1)
  {
    *p_out = hmap::decimate_vw(*p_out, smooth_sampling);
    *p_out = hmap::bspline(*p_out);

    hmap::for_each_tile(
        {p_hmap},
        {},
        [&](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>,
            const hmap::TileRegion &region)
        {
          auto [pa_in] = unpack<1>(p_arrays_in);
          p_out->set_values_from_array(*pa_in, region.bbox);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
