/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_cloud_random_density_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "density");
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");

  // attribute(s)
  node.add_attr<IntAttribute>("npoints", "npoints", 50, 1, INT_MAX);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<RangeAttribute>("remap", "remap");

  // attribute(s) order
  node.set_attr_ordered_key({"npoints", "seed", "_SEPARATOR_", "remap"});
}

void compute_cloud_random_density_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_density = node.get_value_ref<hmap::VirtualArray>("density");

  if (p_density)
  {
    hmap::Cloud *p_cloud = node.get_value_ref<hmap::Cloud>("cloud");

    int npoints = node.get_attr<IntAttribute>("npoints");
    int ntiles = p_density->get_ntiles();
    int npoints_per_tile = std::max(1, int(float(npoints / ntiles)));

    std::vector<hmap::Cloud> clouds;
    std::mutex               mtx;

    hmap::for_each_tile(
        {p_density},
        [&node, &clouds, npoints_per_tile, &mtx](std::vector<hmap::Array *> p_arrays,
                                                 const hmap::TileRegion    &region)
        {
          auto [pa_density] = unpack<1>(p_arrays);

          uint tile_seed = node.get_attr<SeedAttribute>("seed") + region.key.hash();

          hmap::Cloud c = hmap::random_cloud_density(npoints_per_tile,
                                                     *pa_density,
                                                     tile_seed,
                                                     region.bbox);

          std::lock_guard<std::mutex> lock(mtx);
          clouds.emplace_back(c);
        },
        node.cfg().cm_cpu);

    // merge per tile clouds
    *p_cloud = merge_clouds(clouds);

    if (node.get_attr_ref<RangeAttribute>("remap")->get_is_active() &&
        p_cloud->size() > 0)
    {
      p_cloud->remap_values(node.get_attr<RangeAttribute>("remap")[0],
                            node.get_attr<RangeAttribute>("remap")[1]);
    }
  }
}

} // namespace hesiod
