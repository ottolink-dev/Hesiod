/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/statistics.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_detrend_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
}

void compute_detrend_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    float               min = p_in->min(node.cfg().cm_cpu);
    float               max = p_in->max(node.cfg().cm_cpu);

    // work on a single array
    hmap::Array z_array = p_in->to_array(node.cfg().cm_cpu);
    z_array = hmap::detrend_reg(z_array);
    p_out->from_array(z_array, node.cfg().cm_cpu);

    if (min != max)
      p_out->remap(min, max, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
