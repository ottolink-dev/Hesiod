/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN        = "input";
constexpr const char *P_MASK      = "mask";
constexpr const char *P_OUT       = "output";
constexpr const char *P_THRESHOLD = "threshold";

constexpr const char *A_SCALING           = "scaling";
constexpr const char *A_THRESHOLD_VALUE   = "threshold_value";
constexpr const char *A_TRANSITION_EXTENT = "transition_extent";

void setup_reverse_above_theshold_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_THRESHOLD);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MASK);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_float(node, A_THRESHOLD_VALUE, "threshold_value", 0.5f, -FLT_MAX, FLT_MAX);
  add_float(node, A_SCALING, "scaling", 0.5f, 0.f, 2.f);
  add_float(node, A_TRANSITION_EXTENT, "transition_extent", 0.1f, 0.f, 1.f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_reverse_above_theshold_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_th   = node.get_value_ref<hmap::VirtualArray>(P_THRESHOLD);
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>(P_MASK);
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>(P_OUT);

    hmap::for_each_tile(
        {p_out, p_in, p_th, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out  = p_arrays[0];
          hmap::Array *pa_in   = p_arrays[1];
          hmap::Array *pa_th   = p_arrays[2];
          hmap::Array *pa_mask = p_arrays[3];

          *pa_out = *pa_in;

          if (pa_th)
          {
            hmap::reverse_above_theshold(*pa_out,
                                         *pa_th,
                                         pa_mask,
                                         node.val<float>(A_SCALING),
                                         node.val<float>(A_TRANSITION_EXTENT));
          }
          else
          {
            hmap::reverse_above_theshold(*pa_out,
                                         node.val<float>(A_THRESHOLD_VALUE),
                                         pa_mask,
                                         node.val<float>(A_SCALING),
                                         node.val<float>(A_TRANSITION_EXTENT));
          }
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
