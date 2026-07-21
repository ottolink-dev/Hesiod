/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/geometry/cloud.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_cloud_shuffle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "input");
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "output");

  // attribute(s)
  node.add_attr<FloatAttribute>("dx", "dx", 0.f, -0.5f, 0.5f);
  node.add_attr<FloatAttribute>("dy", "dy", 0.f, -0.5f, 0.5f);
  node.add_attr<FloatAttribute>("dv", "dv", 0.f, -0.5f, 0.5f);
  node.add_attr<SeedAttribute>("seed", "Seed");

  // attribute(s) order
  node.set_attr_ordered_key({"dx", "dy", "dv", "seed"});
}

void compute_cloud_shuffle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in = node.get_value_ref<hmap::Cloud>("input");

  if (p_in)
  {
    hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>("output");

    // copy the input heightmap
    *p_out = *p_in;

    if (p_in->size() > 0)
      p_out->shuffle(node.get_attr<FloatAttribute>("dx"),
                     node.get_attr<FloatAttribute>("dy"),
                     node.get_attr<SeedAttribute>("seed"),
                     node.get_attr<FloatAttribute>("dv"));
  }
}

} // namespace hesiod
