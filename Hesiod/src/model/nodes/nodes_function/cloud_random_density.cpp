/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_CLOUD   = "cloud";
constexpr const char *P_DENSITY = "density";

constexpr const char *A_NPOINTS = "npoints";
constexpr const char *A_REMAP   = "remap";
constexpr const char *A_SEED    = "seed";

void setup_cloud_random_density_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_DENSITY);
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, P_CLOUD);

  // attribute(s)
  add_int(node, A_NPOINTS, "npoints", 50, 1, INT_MAX);
  add_seed(node, A_SEED, "Seed");
  add_range(node, A_REMAP, "remap", {0.f, 1.f}, -1.f, 2.f, true);
}

void compute_cloud_random_density_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_density = node.get_value_ref<hmap::VirtualArray>(P_DENSITY);

  if (p_density)
  {
    hmap::Cloud *p_cloud = node.get_value_ref<hmap::Cloud>(P_CLOUD);

    int npoints          = node.val<int>(A_NPOINTS);
    int ntiles           = p_density->get_ntiles();
    int npoints_per_tile = std::max(1, int(float(npoints / ntiles)));

    std::vector<hmap::Cloud> clouds;
    std::mutex               mtx;

    hmap::for_each_tile(
        {p_density},
        [&node, &clouds, npoints_per_tile, &mtx](std::vector<hmap::Array *> p_arrays,
                                                 const hmap::TileRegion    &region)
        {
          auto [pa_density] = unpack<1>(p_arrays);

          uint tile_seed = node.val<int>(A_SEED) + region.key.hash();

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

    if ((node.attr<glm::vec2>(A_REMAP) &&
                 node.attr<glm::vec2>(A_REMAP)->metadata().try_value<bool>(
                     meta::keys::ui::active)
             ? *node.attr<glm::vec2>(A_REMAP)->metadata().try_value<bool>(
                   meta::keys::ui::active)
             : true) &&
        p_cloud->size() > 0)
    {
      p_cloud->remap_values(node.val<glm::vec2>(A_REMAP)[0],
                            node.val<glm::vec2>(A_REMAP)[1]);
    }
  }
}

} // namespace hesiod
