/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/hydrology/hydrology.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_flooding_from_point_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "elevation");
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("depth_min", "depth_min", 0.01f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"depth_min"});
}

void compute_flooding_from_point_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("elevation");
  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>("cloud");

  if (p_in && p_cloud)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("water_depth");

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

          *pa_out = hmap::flooding_from_point(*pa_in,
                                              i,
                                              j,
                                              node.get_attr<FloatAttribute>("depth_min"));
        },
        node.cfg().cm_single_array); // forced, not tileable
  }
}

} // namespace hesiod
