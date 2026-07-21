/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/gradient.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_gradient_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "dx", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "dy", CONFIG(node));

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_gradient_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");

    hmap::for_each_tile(
        {p_dx, p_in},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_dx = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];

          hmap::gradient_x(*pa_in, *pa_dx);
        },
        node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_dy, p_in},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_dy = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];

          hmap::gradient_y(*pa_in, *pa_dy);
        },
        node.cfg().cm_cpu);

    p_dx->smooth_overlap_buffers();
    p_dy->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_dx);
    post_process_heightmap(node, *p_dy);
  }
}

} // namespace hesiod
