/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_hydraulic_musgrave_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "moisture");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<IntAttribute>("iterations", "iterations", 100, 1, INT_MAX);
  node.add_attr<FloatAttribute>("c_capacity", "c_capacity", 2.f, 0.1f, 40.f);
  node.add_attr<FloatAttribute>("c_erosion", "c_erosion", 0.01f, 0.001f, 0.1f);
  node.add_attr<FloatAttribute>("c_deposition", "c_deposition", 0.01f, 0.001f, 0.1f);
  node.add_attr<FloatAttribute>("water_level", "water_level", 0.001f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("evap_rate", "evap_rate", 0.001f, 0.f, 0.1f);

  // attribute(s) order
  node.set_attr_ordered_key({"iterations",
                             "c_capacity",
                             "c_erosion",
                             "c_deposition",
                             "water_level",
                             "evap_rate"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_hydraulic_musgrave_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_moisture_map = node.get_value_ref<hmap::VirtualArray>(
        "moisture");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    hmap::for_each_tile(
        {p_out, p_in, p_moisture_map},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
        {
          hmap::Array *pa_out          = p_arrays[0];
          hmap::Array *pa_in           = p_arrays[1];
          hmap::Array *pa_moisutre_map = p_arrays[2];

          *pa_out = *pa_in;

          hmap::Array moisture_map_array(region.shape, 1.f);
          if (pa_moisutre_map)
            moisture_map_array = *pa_moisutre_map;

          hmap::hydraulic_musgrave(*pa_out,
                                   moisture_map_array,
                                   node.get_attr<IntAttribute>("iterations"),
                                   node.get_attr<FloatAttribute>("c_capacity"),
                                   node.get_attr<FloatAttribute>("c_erosion"),
                                   node.get_attr<FloatAttribute>("c_deposition"),
                                   node.get_attr<FloatAttribute>("water_level"),
                                   node.get_attr<FloatAttribute>("evap_rate"));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
