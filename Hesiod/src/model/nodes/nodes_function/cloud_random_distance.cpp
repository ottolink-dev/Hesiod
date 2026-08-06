/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_cloud_random_distance_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "density");
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");

  // attribute(s)
  node.add_attr<FloatAttribute>("distance_min", "distance_min", 0.05f, 0.001f, 0.2f);
  node.add_attr<FloatAttribute>("distance_max", "distance_max", 0.1f, 0.001f, 1.f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<RangeAttribute>("remap", "remap");

  // attribute(s) order
  node.set_attr_ordered_key(
      {"distance_min", "distance_max", "seed", "_SEPARATOR_", "remap"});
}

void compute_cloud_random_distance_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_density = node.get_value_ref<hmap::VirtualArray>("density");
  hmap::Cloud        *p_cloud = node.get_value_ref<hmap::Cloud>("cloud");

  if (p_density)
  {
    // TODO distribute
    hmap::Array density_array = p_density->to_array(node.cfg().cm_cpu);

    *p_cloud = hmap::random_cloud_distance(node.get_attr<FloatAttribute>("distance_min"),
                                           node.get_attr<FloatAttribute>("distance_max"),
                                           density_array,
                                           node.get_attr<SeedAttribute>("seed"));

    std::vector<hmap::Cloud> clouds;
    std::mutex               mtx;

    hmap::for_each_tile(
        {p_density},
        [&node, &clouds, &mtx](std::vector<hmap::Array *> p_arrays,
                               const hmap::TileRegion    &region)
        {
          auto [pa_density] = unpack<1>(p_arrays);

          uint tile_seed = node.get_attr<SeedAttribute>("seed") + region.key.hash();

          float dmin = node.get_attr<FloatAttribute>("distance_min");
          float dmax = node.get_attr<FloatAttribute>("distance_max");

          hmap::Cloud c = hmap::random_cloud_distance(dmin,
                                                      dmax,
                                                      *pa_density,
                                                      tile_seed,
                                                      region.bbox);

          std::lock_guard<std::mutex> lock(mtx);
          clouds.emplace_back(c);
        },
        node.cfg().cm_cpu);

    // merge per tile clouds
    *p_cloud = merge_clouds(clouds);
  }
  else
  {
    *p_cloud = hmap::random_cloud_distance(node.get_attr<FloatAttribute>("distance_min"),
                                           node.get_attr<SeedAttribute>("seed"));
  }

  if (node.get_attr_ref<RangeAttribute>("remap")->get_is_active() && p_cloud->size() > 0)
    p_cloud->remap_values(node.get_attr<RangeAttribute>("remap")[0],
                          node.get_attr<RangeAttribute>("remap")[1]);
}

} // namespace hesiod
