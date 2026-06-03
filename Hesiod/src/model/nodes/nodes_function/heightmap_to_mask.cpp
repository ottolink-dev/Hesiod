/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/range.hpp"

#include "attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_heightmap_to_mask_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "mask", CONFIG(node));

  // attribute(s)

  // attribute(s) order

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_heightmap_to_mask_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");

    // clamp to [0, 1]
    hmap::for_each_tile(
        {p_mask, p_in},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_mask, pa_in] = unpack<2>(p_arrays);

          *pa_mask = *pa_in;
          hmap::clamp(*pa_mask, 0.f, 1.f);
        },
        node.cfg().cm_gpu);

    // post-process
    post_process_heightmap(node, *p_mask);
  }
}

} // namespace hesiod
