/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_diffusion_limited_aggregation_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("scale", "scale", 0.01f, 0.005f, 0.1f);
  node.add_attr<FloatAttribute>("seeding_radius", "seeding_radius", 0.4f, 0.1f, 0.5f);
  node.add_attr<FloatAttribute>("seeding_outer_radius_ratio",
                                "seeding_outer_radius_ratio",
                                0.2f,
                                0.01f,
                                0.5f);
  node.add_attr<FloatAttribute>("slope", "slope", 8.f, 0.1f, FLT_MAX);
  node.add_attr<FloatAttribute>("noise_ratio", "noise_ratio", 0.2f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"seed",
                             "scale",
                             "seeding_radius",
                             "seeding_outer_radius_ratio",
                             "slope",
                             "noise_ratio"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_diffusion_limited_aggregation_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  hmap::Array array = hmap::diffusion_limited_aggregation(
      node.cfg().shape,
      node.get_attr<FloatAttribute>("scale"),
      node.get_attr<SeedAttribute>("seed"),
      node.get_attr<FloatAttribute>("seeding_radius"),
      node.get_attr<FloatAttribute>("seeding_outer_radius_ratio"),
      node.get_attr<FloatAttribute>("slope"),
      node.get_attr<FloatAttribute>("noise_ratio"));

  p_out->from_array(array, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
