/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"

#include "hesiod/model/nodes/attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

// -----------------------------------------------------------------------------
// Ports & Attributes
// -----------------------------------------------------------------------------
constexpr const char *P_IN       = "input";
constexpr const char *P_MOISTURE = "moisture";
constexpr const char *P_OUT      = "output";

constexpr const char *A_C_CAPACITY   = "c_capacity";
constexpr const char *A_C_DEPOSITION = "c_deposition";
constexpr const char *A_C_EROSION    = "c_erosion";
constexpr const char *A_EVAP_RATE    = "evap_rate";
constexpr const char *A_ITERATIONS   = "iterations";
constexpr const char *A_WATER_LEVEL  = "water_level";

void setup_hydraulic_musgrave_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_IN);
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, P_MOISTURE);
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, P_OUT, CONFIG(node));

  // attribute(s)
  add_int(node, A_ITERATIONS, "iterations", 100, 1, INT_MAX);
  add_float(node, A_C_CAPACITY, "c_capacity", 2.f, 0.1f, 40.f);
  add_float(node, A_C_EROSION, "c_erosion", 0.01f, 0.001f, 0.1f);
  add_float(node, A_C_DEPOSITION, "c_deposition", 0.01f, 0.001f, 0.1f);
  add_float(node, A_WATER_LEVEL, "water_level", 0.001f, 0.f, 0.1f);
  add_float(node, A_EVAP_RATE, "evap_rate", 0.001f, 0.f, 0.1f);

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});
}

void compute_hydraulic_musgrave_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(P_IN);

  if (p_in)
  {
    hmap::VirtualArray *p_moisture_map = node.get_value_ref<hmap::VirtualArray>(
        P_MOISTURE);
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>(P_OUT);

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
                                   node.val<int>(A_ITERATIONS),
                                   node.val<float>(A_C_CAPACITY),
                                   node.val<float>(A_C_EROSION),
                                   node.val<float>(A_C_DEPOSITION),
                                   node.val<float>(A_WATER_LEVEL),
                                   node.val<float>(A_EVAP_RATE));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
