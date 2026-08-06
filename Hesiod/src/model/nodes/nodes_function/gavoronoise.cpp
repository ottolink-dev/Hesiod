/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_gavoronoise_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "control");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "envelope");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "angle");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<WaveNbAttribute>("kw", "Spatial Frequency");
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("amplitude", "amplitude", 0.05f, 0.001f, 0.2f);

  node.add_attr<WaveNbAttribute>("kw_multiplier",
                                 "kw_multiplier",
                                 glm::vec2(8.f, 8.f),
                                 0.f,
                                 32.f,
                                 true);
  node.add_attr<FloatAttribute>("angle", "angle", 0.f, -180.f, 180.f);
  node.add_attr<FloatAttribute>("angle_spread_ratio",
                                "angle_spread_ratio",
                                1.f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("slope_strength", "slope_strength", 0.5f, 0.f, 8.f);
  node.add_attr<FloatAttribute>("branch_strength", "branch_strength", 2.f, 0.f, 8.f);
  node.add_attr<FloatAttribute>("z_cut_min", "z_cut_min", 0.2f, -1.f, 2.f);
  node.add_attr<FloatAttribute>("z_cut_max", "z_cut_max", 1.f, -1.f, 2.f);
  node.add_attr<IntAttribute>("octaves", "Octaves", 8, 0, 32);
  node.add_attr<FloatAttribute>("persistence", "Persistence", 0.4f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("lacunarity", "Lacunarity", 2.f, 0.01f, 4.f);

  // attribute(s) order
  node.set_attr_ordered_key({"kw",
                             "seed",
                             "amplitude",
                             "kw_multiplier",
                             "angle",
                             "angle_spread_ratio",
                             "slope_strength",
                             "branch_strength",
                             "z_cut_min",
                             "z_cut_max",
                             "octaves",
                             "persistence",
                             "lacunarity"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_gavoronoise_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_dx = node.get_value_ref<hmap::VirtualArray>("dx");
  hmap::VirtualArray *p_dy = node.get_value_ref<hmap::VirtualArray>("dy");
  hmap::VirtualArray *p_ctrl = node.get_value_ref<hmap::VirtualArray>("control");
  hmap::VirtualArray *p_env = node.get_value_ref<hmap::VirtualArray>("envelope");
  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
  hmap::VirtualArray *p_angle = node.get_value_ref<hmap::VirtualArray>("angle");

  hmap::for_each_tile(
      {p_out, p_ctrl, p_dx, p_dy, p_angle},
      [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &region)
      {
        hmap::Array *pa_out = p_arrays[0];
        hmap::Array *pa_ctrl = p_arrays[1];
        hmap::Array *pa_dx = p_arrays[2];
        hmap::Array *pa_dy = p_arrays[3];
        hmap::Array *pa_angle = p_arrays[4];

        hmap::Array angle_deg(region.shape, node.get_attr<FloatAttribute>("angle"));

        if (pa_angle)
          angle_deg += (*pa_angle) * 180.f / M_PI;

        *pa_out = hmap::gpu::gavoronoise(
            region.shape,
            node.get_attr<WaveNbAttribute>("kw"),
            node.get_attr<SeedAttribute>("seed"),
            angle_deg,
            node.get_attr<FloatAttribute>("amplitude"),
            node.get_attr<FloatAttribute>("angle_spread_ratio"),
            node.get_attr<WaveNbAttribute>("kw_multiplier"),
            node.get_attr<FloatAttribute>("slope_strength"),
            node.get_attr<FloatAttribute>("branch_strength"),
            node.get_attr<FloatAttribute>("z_cut_min"),
            node.get_attr<FloatAttribute>("z_cut_max"),
            node.get_attr<IntAttribute>("octaves"),
            node.get_attr<FloatAttribute>("persistence"),
            node.get_attr<FloatAttribute>("lacunarity"),
            pa_ctrl,
            pa_dx,
            pa_dy,
            region.bbox);
      },
      node.cfg().cm_gpu);

  // post-process
  post_apply_enveloppe(node, *p_out, p_env);
  post_process_heightmap(node, *p_out);
}

} // namespace hesiod
