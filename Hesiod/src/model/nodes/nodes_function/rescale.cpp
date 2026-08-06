/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_rescale_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("scaling", "scaling", 1.f, 0.0001f, FLT_MAX, "{:.4f}");
  node.add_attr<BoolAttribute>("centered", "centered", false);

  // attribute(s) order
  node.set_attr_ordered_key({"scaling", "centered"});
}

void compute_rescale_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    float vref = 0.f;

    if (node.get_attr<BoolAttribute>("centered"))
      vref = p_out->mean(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, vref](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = *pa_in;

          hmap::rescale(*pa_out, node.get_attr<FloatAttribute>("scaling"), vref);
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
