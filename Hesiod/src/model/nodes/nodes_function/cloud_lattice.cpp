/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/grids.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_cloud_lattice_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");

  // attribute(s)
  glm::vec2 default_value = {0.1f, 0.1f};
  node.add_attr<IntAttribute>("npoints", "npoints", 50, 1, INT_MAX);
  node.add_attr<WaveNbAttribute>("stagger_ratio",
                                 "stagger_ratio",
                                 default_value,
                                 0.f,
                                 1.f,
                                 true);
  node.add_attr<WaveNbAttribute>("jitter_ratio",
                                 "jitter_ratio",
                                 default_value,
                                 0.f,
                                 1.f,
                                 true);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<RangeAttribute>("remap", "remap");

  // attribute(s) order
  node.set_attr_ordered_key(
      {"npoints", "stagger_ratio", "jitter_ratio", "seed", "_SEPARATOR_", "remap"});
}

void compute_cloud_lattice_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>("cloud");

  *p_out = hmap::random_cloud_jittered(node.get_attr<IntAttribute>("npoints"),
                                       node.get_attr<WaveNbAttribute>("jitter_ratio"),
                                       node.get_attr<WaveNbAttribute>("stagger_ratio"),
                                       node.get_attr<SeedAttribute>("seed"));

  if (node.get_attr_ref<RangeAttribute>("remap")->get_is_active())
    p_out->remap_values(node.get_attr<RangeAttribute>("remap")[0],
                        node.get_attr<RangeAttribute>("remap")[1]);
}

} // namespace hesiod
