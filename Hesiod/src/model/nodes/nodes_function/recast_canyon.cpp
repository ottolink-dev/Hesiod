/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_recast_canyon_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "noise");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("vcut", "vcut", 0.5f, -1.f, 2.f);
  node.add_attr<FloatAttribute>("gamma", "gamma", 4.f, 0.01f, 10.f);

  // attribute(s) order
  node.set_attr_ordered_key({"vcut", "gamma"});
}

void compute_recast_canyon_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_noise = node.get_value_ref<hmap::VirtualArray>("noise");
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in, p_noise, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];
          hmap::Array *pa_noise = p_arrays[2];
          hmap::Array *pa_mask = p_arrays[3];

          *pa_out = *pa_in;

          hmap::recast_canyon(*pa_out,
                              node.get_attr<FloatAttribute>("vcut"),
                              pa_mask,
                              node.get_attr<FloatAttribute>("gamma"),
                              pa_noise);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
