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

void setup_constant_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("value", "value", 0.f, -4.f, 4.f);

  // attribute(s) order
  node.set_attr_ordered_key({"value"});
}

void compute_constant_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

  hmap::for_each_tile(
      {p_out},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        hmap::Array *pa_out = p_arrays[0];
        *pa_out = node.get_attr<FloatAttribute>("value");
      },
      node.cfg().cm_cpu);
}

} // namespace hesiod
