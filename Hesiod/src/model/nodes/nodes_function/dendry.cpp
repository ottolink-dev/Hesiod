/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_dendry_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "control");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  node.add_attr<IntAttribute>("subsampling", "subsampling", 4, 1, 8);

  node.add_attr<WaveNbAttribute>("kw",
                                 "Spatial Frequency",
                                 glm::vec2(8.f, 8.f),
                                 0.f,
                                 FLT_MAX);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("eps", "eps", 0.2f, 0.f, 1.f);
  node.add_attr<IntAttribute>("resolution", "resolution", 1, 1, 8);
  node.add_attr<FloatAttribute>("displacement", "displacement", 0.075f, 0.f, 0.2f);
  node.add_attr<IntAttribute>("primitives_resolution_steps",
                              "primitives_resolution_steps",
                              3,
                              1,
                              8);
  node.add_attr<FloatAttribute>("slope_power", "slope_power", 1.f, 0.f, 2.f);
  node.add_attr<FloatAttribute>("noise_amplitude_proportion",
                                "noise_amplitude_proportion",
                                0.01f,
                                0.f,
                                1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"subsampling",
                             "_SEPARATOR_",
                             "kw",
                             "seed",
                             "eps",
                             "slope_power",
                             "resolution",
                             "displacement",
                             "primitives_resolution_steps",
                             "noise_amplitude_proportion"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_dendry_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  // base noise function
  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>("control");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

  if (p_ctrl)
  {
    // TODO shape
    hmap::Array ctrl_array = p_ctrl->to_array(glm::ivec2(128, 128), node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_dx, p_dy},
        [&node, &ctrl_array](std::vector<hmap::Array *> p_arrays,
                             const hmap::TileRegion    &region)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_dx = p_arrays[1];
          hmap::Array *pa_dy = p_arrays[2];

          *pa_out = hmap::dendry(
              region.shape,
              node.get_attr<WaveNbAttribute>("kw"),
              node.get_attr<SeedAttribute>("seed"),
              ctrl_array,
              node.get_attr<FloatAttribute>("eps"),
              node.get_attr<IntAttribute>("resolution"),
              node.get_attr<FloatAttribute>("displacement"),
              node.get_attr<IntAttribute>("primitives_resolution_steps"),
              node.get_attr<FloatAttribute>("slope_power"),
              node.get_attr<FloatAttribute>("noise_amplitude_proportion"),
              true, // add noise
              0.5f, // overlap
              pa_dx,
              pa_dy,
              nullptr,
              region.bbox,
              node.get_attr<IntAttribute>("subsampling"));
        },
        node.cfg().cm_cpu);

    p_out->smooth_overlap_buffers();

    // post-process
    post_apply_enveloppe(node, *p_out, p_env);
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
