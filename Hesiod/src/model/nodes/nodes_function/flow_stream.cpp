/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/carving.hpp"
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------

constexpr const char *P_IN         = "input";
constexpr const char *P_OUT        = "output";
constexpr const char *P_RIVER_MASK = "river_mask";
constexpr const char *P_SOURCES    = "sources";

constexpr const char *A_CONTROL_POINTS_COUNT = "control_points_count";
constexpr const char *A_DEPTH                = "depth";
constexpr const char *A_DISTANCE_EXPONENT    = "distance_exponent";
constexpr const char *A_ELEVATION_RATIO      = "elevation_ratio";
constexpr const char *A_FAVOR_DOMAIN_CENTER  = "favor_domain_center";
constexpr const char *A_MERGING_RADIUS       = "merging_radius";
constexpr const char *A_NOISE_RATIO          = "noise_ratio";
constexpr const char *A_RIVER_RADIUS         = "river_radius";
constexpr const char *A_RIVERBANK_SLOPE      = "riverbank_slope";
constexpr const char *A_RIVERBED_SLOPE       = "riverbed_slope";
constexpr const char *A_SEED                 = "seed";
constexpr const char *A_UPWARD_PENALIZATION  = "upward_penalization";

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup_flow_stream_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // --- Ports

  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_SOURCES);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_RIVER_MASK, CONFIG(node));

  // --- Attributes

  // clang-format off
  node.set_current_category("Pathfinding");
  add_seed(node, A_SEED, "Seed");
  add_int(node, A_CONTROL_POINTS_COUNT, "Control Points Count", 4096, 100, 30000);
  add_float(node, A_ELEVATION_RATIO, "Elevation vs Slope Weight", 0.5f, 0.f, 0.99f);
  add_float(node, A_DISTANCE_EXPONENT, "Distance Exponent", 2.f, 0.1f, 4.f);
  add_float(node, A_UPWARD_PENALIZATION, "Upward Penalization", 100.f, 0.f, 1000.f);
  add_bool(node, A_FAVOR_DOMAIN_CENTER, "Favor Domain Center", true);

  node.set_current_category("Riverbed Slope");
  add_float(node, A_RIVERBED_SLOPE, "Riverbed Slope", 0.01f, 0.f, 0.1f);
  add_float(node, A_DEPTH, "Depth", 0.01f, 0.f, 0.2f);

  node.set_current_category("Riverbank Carving");
  add_float(node, A_RIVERBANK_SLOPE, "Riverbank Slope", 1.f, 0.f, 16.f);
  add_float(node, A_RIVER_RADIUS, "River Radius", 0.001f, 0.001f, 0.1f);
  add_float(node, A_MERGING_RADIUS, "Merging Radius", 0.001f, 0.f, 0.1f);
  add_float(node, A_NOISE_RATIO, "Noise Ratio", 0.9f, 0.f, 1.f);
  // clang-format on
}

// -----------------------------------------------------------------------------
// Compute
// -----------------------------------------------------------------------------

void compute_flow_stream_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  const auto *p_in    = node.get_value_ref<hmap::VirtualArray>(P_IN);
  const auto *p_cloud = node.get_value_ref<hmap::Cloud>(P_SOURCES);
  auto       *p_out   = node.get_value_ref<hmap::VirtualArray>(P_OUT);
  auto       *p_mask  = node.get_value_ref<hmap::VirtualArray>(P_RIVER_MASK);

  if (!p_in)
    return;

  if (!p_cloud || p_cloud->empty())
  {
    p_out->copy_from(*p_in, node.cfg().cm_cpu);
    p_mask->fill(0.f, node.cfg().cm_cpu);
    p_out->sync_overlap_buffers();
    p_mask->sync_overlap_buffers();
    return;
  }

  // --- Parameters

  // clang-format off
  const auto seed                 = static_cast<std::uint32_t>(node.val<int>(A_SEED));
  const auto control_points_count = static_cast<size_t>(node.val<int>(A_CONTROL_POINTS_COUNT));
  const auto elevation_ratio      = node.val<float>(A_ELEVATION_RATIO);
  const auto distance_exponent    = node.val<float>(A_DISTANCE_EXPONENT);
  const auto upward_penalization  = node.val<float>(A_UPWARD_PENALIZATION);
  const auto favor_domain_center  = node.val<bool>(A_FAVOR_DOMAIN_CENTER);

  const auto depth                = node.val<float>(A_DEPTH);
  const auto riverbed_slope       = node.val<float>(A_RIVERBED_SLOPE);
  const auto riverbank_slope      = node.val<float>(A_RIVERBANK_SLOPE);
  const auto river_radius         = node.val<float>(A_RIVER_RADIUS);
  const auto merging_radius       = node.val<float>(A_MERGING_RADIUS);
  const auto noise_ratio          = node.val<float>(A_NOISE_RATIO);
  // clang-format on

  // --- Flow stream paths

  std::vector<hmap::Path> path_list;
  path_list.reserve(p_cloud->size());

  for (const auto &p : *p_cloud)
  {
    hmap::Path path = hmap::va::flow_stream(node.cfg().cm_cpu,
                                            *p_in,
                                            glm::vec2(p.x, p.y),
                                            seed,
                                            control_points_count,
                                            elevation_ratio,
                                            distance_exponent,
                                            upward_penalization,
                                            favor_domain_center);

    if (path.size() > 1)
      path_list.push_back(std::move(path));
  }

  // --- Dig rivers

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

        if (path_list.empty())
        {
          *pa_mask = 0.f;
          return;
        }

        const auto  nx              = region.shape.x - 1.f;
        const int   merging_ir      = std::max(1, static_cast<int>(merging_radius * nx));
        const int   river_ir        = static_cast<int>(river_radius * nx);
        const float riverbank_talus = riverbank_slope / nx;

        hmap::dig_river(*pa_out,
                        path_list,
                        riverbank_talus,
                        river_ir,
                        merging_ir,
                        depth,
                        riverbed_slope,
                        noise_ratio,
                        seed,
                        pa_mask);
      },
      node.cfg().cm_single_array);

  p_out->sync_overlap_buffers();
  p_mask->sync_overlap_buffers();
}

} // namespace hesiod
