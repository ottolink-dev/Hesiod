/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/gradient.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_DX = "dx";
constexpr const char *P_DY = "dy";
constexpr const char *P_IN = "input";

void setup_gradient_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DX, CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_DY, CONFIG(node));

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_gradient_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>(P_DX);
    hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>(P_DY);

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
