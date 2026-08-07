/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_scan_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("contrast", "contrast", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("brightness", "brightness", 0.5f, 0.f, 1.f);
  node.add_attr<BoolAttribute>("remap", "remap", false);
}

void compute_scan_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = hmap::scan_mask(*pa_in,
                                    node.get_attr<FloatAttribute>("brightness"),
                                    node.get_attr<FloatAttribute>("contrast"));
        },
        node.cfg().cm_cpu);

    if (node.get_attr<BoolAttribute>("remap"))
      p_out->remap(0.f, 1.f, node.cfg().cm_cpu);
  }
}

} // namespace hesiod
