/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_reverse_above_theshold_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "threshold");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("threshold_value",
                                "threshold_value",
                                0.5f,
                                -FLT_MAX,
                                FLT_MAX);
  node.add_attr<FloatAttribute>("scaling", "scaling", 0.5f, 0.f, 2.f);
  node.add_attr<FloatAttribute>("transition_extent", "transition_extent", 0.1f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"threshold_value", "scaling", "transition_extent"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_reverse_above_theshold_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_th = node.get_value_ref<hmap::VirtualArray>("threshold");
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in, p_th, p_mask},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];
          hmap::Array *pa_th = p_arrays[2];
          hmap::Array *pa_mask = p_arrays[3];

          *pa_out = *pa_in;

          if (pa_th)
          {
            hmap::reverse_above_theshold(
                *pa_out,
                *pa_th,
                pa_mask,
                node.get_attr<FloatAttribute>("scaling"),
                node.get_attr<FloatAttribute>("transition_extent"));
          }
          else
          {
            hmap::reverse_above_theshold(
                *pa_out,
                node.get_attr<FloatAttribute>("threshold_value"),
                pa_mask,
                node.get_attr<FloatAttribute>("scaling"),
                node.get_attr<FloatAttribute>("transition_extent"));
          }
        },
        node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
