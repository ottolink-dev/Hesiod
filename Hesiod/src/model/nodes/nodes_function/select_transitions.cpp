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
constexpr const char *P_BLEND   = "blend";
constexpr const char *P_INPUT_1 = "input 1";
constexpr const char *P_INPUT_2 = "input 2";
constexpr const char *P_OUT     = "output";

void setup_select_transitions_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_1);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_INPUT_2);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_BLEND);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_transitions_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in1   = node.get_value_ref<hmap::VirtualArray>(P_INPUT_1);
  hmap::VirtualArray *p_in2   = node.get_value_ref<hmap::VirtualArray>(P_INPUT_2);
  hmap::VirtualArray *p_blend = node.get_value_ref<hmap::VirtualArray>(P_BLEND);

  if (p_in1 && p_in2 && p_blend)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::for_each_tile(
        {p_out, p_in1, p_in2, p_blend},
        [](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array &m  = *p_arrays[0];
          hmap::Array &a1 = *p_arrays[1];
          hmap::Array &a2 = *p_arrays[2];
          hmap::Array &a3 = *p_arrays[3];

          m = hmap::select_transitions(a1, a2, a3);
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
