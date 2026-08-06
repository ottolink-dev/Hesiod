/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * p_node software. */
#include "highmap/boundary.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_falloff_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dr");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("strength", "strength", 1.f, 0.f, 4.f);
  node.add_attr<EnumAttribute>("distance_function",
                               "distance_function",
                               enum_mappings.distance_function_map,
                               "Euclidian");

  // attribute(s) order
  node.set_attr_ordered_key({"strength", "distance_function"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_falloff_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_dr = node.get_value_ref<hmap::VirtualArray>("dr");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    float strength = node.get_attr<FloatAttribute>("strength");

    if (!p_dr)
      hmap::for_each_tile(
          {p_out, p_in, p_dr},
          [&node, &strength](std::vector<hmap::Array *> p_arrays,
                             const hmap::TileRegion    &region)
          {
            hmap::Array *pa_out = p_arrays[0];
            hmap::Array *pa_in = p_arrays[1];
            hmap::Array *pa_dr = p_arrays[2];

            *pa_out = *pa_in;

            hmap::falloff(
                *pa_out,
                strength,
                (hmap::DistanceFunction)node.get_attr<EnumAttribute>("distance_function"),
                pa_dr,
                region.bbox);
          },
          node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
