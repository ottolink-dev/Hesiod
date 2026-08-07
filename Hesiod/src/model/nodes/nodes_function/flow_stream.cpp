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

constexpr const char *A_DEPTH               = "depth";
constexpr const char *A_DISTANCE_EXPONENT   = "distance_exponent";
constexpr const char *A_ELEVATION_RATIO     = "elevation_ratio";
constexpr const char *A_MERGING_RADIUS      = "merging_radius";
constexpr const char *A_NOISE_RATIO         = "noise_ratio";
constexpr const char *A_RIVER_RADIUS        = "river_radius";
constexpr const char *A_RIVERBANK_SLOPE     = "riverbank_slope";
constexpr const char *A_RIVERBED_SLOPE      = "riverbed_slope";
constexpr const char *A_SEED                = "seed";
constexpr const char *A_UPWARD_PENALIZATION = "upward_penalization";

void setup_flow_stream_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_SOURCES);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_RIVER_MASK, CONFIG(node));

  // attribute(s)
  add_float(node, A_ELEVATION_RATIO, "elevation_ratio", 0.5f, 0.f, 0.95f);
  add_float(node, A_DISTANCE_EXPONENT, "distance_exponent", 2.f, 0.1f, 4.f);
  add_float(node, A_UPWARD_PENALIZATION, "upward_penalization", 100.f, 1.f, FLT_MAX);
  add_float(node, A_RIVERBANK_SLOPE, "riverbank_slope", 1.f, 0.f, 16.f);
  add_float(node, A_RIVER_RADIUS, "river_radius", 0.001f, 0.001f, 0.1f);
  add_float(node, A_DEPTH, "depth", 0.01f, 0.f, 0.2f);
  add_float(node, A_MERGING_RADIUS, "merging_radius", 0.001f, 0.f, 0.1f);
  add_float(node, A_RIVERBED_SLOPE, "riverbed_slope", 0.01f, 0.f, 0.1f);
  add_float(node, A_NOISE_RATIO, "noise_ratio", 0.9f, 0.f, 1.f);
  add_seed(node, A_SEED, "Seed");
}

void compute_flow_stream_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in    = node.get_value_ref<hmap::VirtualArray>(P_IN);
  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>(P_SOURCES);

  if (p_in && p_cloud)
  {
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_RIVER_MASK);

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, p_cloud](std::vector<hmap::Array *> p_arrays,
                         const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);
          *pa_out                       = *pa_in;

          // find a flow stream for each source
          std::vector<hmap::Path> path_list;

          for (auto p : p_cloud->points)
          {
            int        i = (int)(p.x * (region.shape.x - 1.f));
            int        j = (int)(p.y * (region.shape.y - 1.f));
            glm::ivec2 ij_start(i, j);

            hmap::Path path = hmap::flow_stream(*pa_out,
                                                ij_start,
                                                node.val<float>(A_ELEVATION_RATIO),
                                                node.val<float>(A_DISTANCE_EXPONENT),
                                                node.val<float>(A_UPWARD_PENALIZATION));

            path_list.push_back(path);
          }

          // dig the rivers
          int merging_ir = int(node.val<float>(A_MERGING_RADIUS) *
                               (region.shape.x - 1.f));
          merging_ir     = std::max(1, merging_ir);

          int river_ir = int(node.val<float>(A_RIVER_RADIUS) * (region.shape.x - 1.f));

          hmap::dig_river(*pa_out,
                          path_list,
                          node.val<float>(A_RIVERBANK_SLOPE) / (region.shape.x - 1.f),
                          river_ir,
                          merging_ir,
                          node.val<float>(A_DEPTH),
                          node.val<float>(A_RIVERBED_SLOPE),
                          node.val<float>(A_NOISE_RATIO),
                          node.val<int>(A_SEED),
                          pa_mask);
        },
        node.cfg().cm_single_array);
  }
}

} // namespace hesiod
