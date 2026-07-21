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

void setup_cloud_random_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");

  // attribute(s)
  node.add_attr<IntAttribute>("npoints", "npoints", 50, 1, INT_MAX);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<EnumAttribute>("method",
                               "method",
                               hmap::point_sampling_method_as_string,
                               "Latin Hypercube Sampling");
  node.add_attr<RangeAttribute>("remap", "remap");

  // attribute(s) order
  node.set_attr_ordered_key({"npoints", "seed", "method", "_SEPARATOR_", "remap"});
}

void compute_cloud_random_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>("cloud");

  *p_out = hmap::random_cloud(
      node.get_attr<IntAttribute>("npoints"),
      node.get_attr<SeedAttribute>("seed"),
      (hmap::PointSamplingMethod)node.get_attr<EnumAttribute>("method"));

  if (node.get_attr_ref<RangeAttribute>("remap")->get_is_active())
    p_out->remap_values(node.get_attr<RangeAttribute>("remap")[0],
                        node.get_attr<RangeAttribute>("remap")[1]);
}

} // namespace hesiod
