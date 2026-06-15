/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/path.hpp"
#include "highmap/primitives.hpp"

#include "attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_island_chain_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Path>(gnode::PortType::IN, "path");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<IntAttribute>("island_count", "Islands", 5, 1, 64);
  node.add_attr<FloatAttribute>("island_radius", "Radius", 0.1f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("size_falloff", "Size Falloff", 0.5f, -1.f, 1.f);
  node.add_attr<FloatAttribute>("size_jitter", "Size Jitter", 0.3f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("scatter", "Scatter", 0.f, 0.f, 0.5f);
  node.add_attr<FloatAttribute>("displacement", "Displacement", 0.2f, 0.f, FLT_MAX);
  node.add_attr<EnumAttribute>("noise_type", "Type", enum_mappings.noise_type_map);
  node.add_attr<FloatAttribute>("kw", "kw", 4.f, 0.f, FLT_MAX);
  node.add_attr<IntAttribute>("octaves", "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>("weight", "Weight", 0.7f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("persistence", "Persistence", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("lacunarity", "Lacunarity", 2.f, 0.01f, 4.f);

  // attribute(s) order
  node.set_attr_ordered_key({"seed",
                             "island_count",
                             "island_radius",
                             "size_falloff",
                             "size_jitter",
                             "scatter",
                             "displacement",
                             "noise_type",
                             "kw",
                             "octaves",
                             "weight",
                             "persistence",
                             "lacunarity"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_island_chain_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Path *p_path = node.get_value_ref<hmap::Path>("path");

  if (p_path && p_path->size() > 1)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

    hmap::for_each_tile(
        {p_out},
        [&node, p_path](std::vector<hmap::Array *> p_arrays,
                        const hmap::TileRegion    &region)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          *pa_out = hmap::island_chain_land_mask(
              region.shape,
              *p_path,
              node.get_attr<SeedAttribute>("seed"),
              node.get_attr<IntAttribute>("island_count"),
              node.get_attr<FloatAttribute>("island_radius"),
              node.get_attr<FloatAttribute>("size_falloff"),
              node.get_attr<FloatAttribute>("size_jitter"),
              node.get_attr<FloatAttribute>("scatter"),
              node.get_attr<FloatAttribute>("displacement"),
              (hmap::NoiseType)node.get_attr<EnumAttribute>("noise_type"),
              node.get_attr<FloatAttribute>("kw"),
              node.get_attr<IntAttribute>("octaves"),
              node.get_attr<FloatAttribute>("weight"),
              node.get_attr<FloatAttribute>("persistence"),
              node.get_attr<FloatAttribute>("lacunarity"),
              region.bbox);
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
