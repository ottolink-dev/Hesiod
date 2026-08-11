/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN  = "input";
constexpr const char *P_OUT = "output";

constexpr const char *A_ANGLE  = "angle";
constexpr const char *A_RADIUS = "radius";
constexpr const char *A_SIGMA  = "sigma";

void setup_select_angle_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_ANGLE, "angle", 0.f, 0.f, 360.f);
  add_float(node, A_SIGMA, "sigma", 90.f, 0.f, 180.f);
  add_float(node, A_RADIUS, "radius", 0.f, 0.f, 0.2f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_select_angle_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    int ir = (int)(node.val<float>(A_RADIUS) * p_out->shape.x);

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, &ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out              = select_angle(*pa_in,
                                 node.val<float>(A_ANGLE),
                                 node.val<float>(A_SIGMA),
                                 ir);
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
