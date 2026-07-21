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

void setup_cloud_merge_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud1");
  node.add_port<hmap::Cloud>(gnode::PortType::IN, "cloud2");
  node.add_port<hmap::Cloud>(gnode::PortType::OUT, "cloud");
}

void compute_cloud_merge_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::Cloud *p_in1 = node.get_value_ref<hmap::Cloud>("cloud1");
  hmap::Cloud *p_in2 = node.get_value_ref<hmap::Cloud>("cloud2");

  if (p_in1 && p_in2)
  {
    hmap::Cloud *p_out = node.get_value_ref<hmap::Cloud>("cloud");

    // convert the input
    *p_out = hmap::merge_cloud(*p_in1, *p_in2);
  }
}

} // namespace hesiod
