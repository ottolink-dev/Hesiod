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

void setup_cloud_random_power_law_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");

  // attribute(s)
  node.add_attr<FloatAttribute>("distance_min", "distance_min", 0.01f, 0.001f, 0.2f);
  node.add_attr<FloatAttribute>("distance_max", "distance_max", 0.15f, 0.001f, 1.f);
  node.add_attr<FloatAttribute>("alpha", "alpha", 0.5f, 0.01f, 4.f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<RangeAttribute>("remap", "remap");

  // attribute(s) order
  node.set_attr_ordered_key(
      {"distance_min", "distance_max", "alpha", "seed", "_SEPARATOR_", "remap"});
}

void compute_cloud_random_power_law_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>("cloud");

  *p_out = hmap::random_cloud_distance_power_law(
      node.get_attr<FloatAttribute>("distance_min"),
      node.get_attr<FloatAttribute>("distance_max"),
      node.get_attr<FloatAttribute>("alpha"),
      node.get_attr<SeedAttribute>("seed"));

  if (node.get_attr_ref<RangeAttribute>("remap")->get_is_active())
    p_out->remap_values(node.get_attr<RangeAttribute>("remap")[0],
                        node.get_attr<RangeAttribute>("remap")[1]);
}

} // namespace hesiod
