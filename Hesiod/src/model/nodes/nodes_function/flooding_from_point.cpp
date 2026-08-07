/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD       = "cloud";
constexpr const char *P_ELEVATION   = "elevation";
constexpr const char *P_WATER_DEPTH = "water_depth";

constexpr const char *A_DEPTH_MIN = "depth_min";

void setup_flooding_from_point_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_ELEVATION);
  node.add_port<hmap::Cloud>(gnode::PortType::IN, P_CLOUD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_WATER_DEPTH, CONFIG(node));

  // attribute(s)
  add_float(node, A_DEPTH_MIN, "depth_min", 0.01f, 0.f, 1.f);
}

void compute_flooding_from_point_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in    = node.get_value_ref<hmap::VirtualArray>(P_ELEVATION);
  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);

  if (p_in && p_cloud)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_WATER_DEPTH);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, p_cloud](std::vector<hmap::Array *> p_arrays,
                         const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          // convert point positions to cell indices
          std::vector<int> i, j;

          for (const auto &p : p_cloud->points)
          {
            int ip = static_cast<int>((p.x - region.bbox.x) /
                                      (region.bbox.y - region.bbox.x) *
                                      (region.shape.x - 1));
            int jp = static_cast<int>((p.y - region.bbox.z) /
                                      (region.bbox.w - region.bbox.z) *
                                      (region.shape.y - 1));

            if (ip >= 0 && ip <= region.shape.x - 1 && jp >= 0 &&
                jp <= region.shape.y - 1)
            {
              i.push_back(ip);
              j.push_back(jp);
            }
          }

          *pa_out = hmap::flooding_from_point(*pa_in, i, j, node.val<float>(A_DEPTH_MIN));
        },
        node.cfg().cm_single_array); // forced, not tileable
  }
}

} // namespace hesiod
