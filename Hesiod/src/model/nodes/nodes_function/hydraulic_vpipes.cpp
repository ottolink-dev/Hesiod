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

void setup_hydraulic_vpipes_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "bedrock");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "moisture");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "water_depth", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "sediment", CONFIG(node));

  // attribute(s)
  // node.add_attr<FloatAttribute>("duration", "Duration", 0.01f, 0.01f, 0.5f);
  node.add_attr<IntAttribute>("iterations", "iterations", 30, 1, 200);
  node.add_attr<FloatAttribute>("water_height",
                                "water_height",
                                0.01f,
                                0.001f,
                                0.1f,
                                "{:.2e}",
                                /* log */ true);
  node.add_attr<BoolAttribute>("maintain_water_volume", "maintain_water_volume", true);
  node.add_attr<FloatAttribute>("evap_rate", "evap_rate", 0.01f, 0.f, 0.5f);

  node.add_attr<FloatAttribute>("k_capacity", "k_capacity", 0.5f, 0.01f, 2.f);
  node.add_attr<FloatAttribute>("k_erode", "k_erode", 0.002f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("k_depose", "k_depose", 0.01f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("k_discharge_exp", "k_discharge_exp", 1.f, 0.1f, 2.f);

  node.add_attr<FloatAttribute>("downcutting_max_depth_ratio",
                                "downcutting_max_depth_ratio",
                                1.5f,
                                0.01f,
                                3.f);

  node.add_attr<BoolAttribute>("flux_diffusion", "flux_diffusion", true);
  node.add_attr<FloatAttribute>("flux_diffusion_strength",
                                "flux_diffusion_strength",
                                0.01f,
                                1e-4f,
                                1e-1f,
                                "{:.2e}",
                                true);

  // attribute(s) order
  node.set_attr_ordered_key({// "duration",
                             "iterations",
                             "water_height",
                             "maintain_water_volume",
                             "evap_rate",
                             "k_capacity",
                             "k_erode",
                             "k_depose",
                             "k_discharge_exp",
                             "downcutting_max_depth_ratio",
                             "flux_diffusion",
                             "flux_diffusion_strength"});
}

void compute_hydraulic_vpipes_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_bedrock = node.get_value_ref<hmap::VirtualArray>("bedrock");
    hmap::VirtualArray *p_moisture_map = node.get_value_ref<hmap::VirtualArray>(
        "moisture");
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");

    hmap::VirtualArray *p_out         = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_water_depth = node.get_value_ref<hmap::VirtualArray>(
        "water_depth");
    hmap::VirtualArray *p_sediment = node.get_value_ref<hmap::VirtualArray>("sediment");

    // int iterations = int(node.get_attr<FloatAttribute>("duration") *
    // p_out->shape.x);
    int iterations = node.get_attr<IntAttribute>("iterations");

    hmap::for_each_tile(
        {p_in, p_bedrock, p_moisture_map, p_mask},
        {p_out, p_water_depth, p_sediment},
        [&node, iterations](std::vector<const hmap::Array *> p_arrays_in,
                            std::vector<hmap::Array *>       p_arrays_out,
                            const hmap::TileRegion &)
        {
          const auto [pa_in, pa_bedrock, pa_moisture_map, pa_mask] = unpack<4>(
              p_arrays_in);
          auto [pa_out, pa_water_depth, pa_sediment] = unpack<3>(p_arrays_out);

          *pa_out = *pa_in;

          hmap::gpu::hydraulic_vpipes(
              *pa_out,
              node.get_attr<FloatAttribute>("water_height"),
              node.get_attr<BoolAttribute>("maintain_water_volume"),
              node.get_attr<FloatAttribute>("evap_rate"),
              iterations,
              /* dt */ 0.5f,
              node.get_attr<FloatAttribute>("k_capacity"),
              node.get_attr<FloatAttribute>("k_erode"),
              node.get_attr<FloatAttribute>("k_depose"),
              node.get_attr<FloatAttribute>("k_discharge_exp"),
              node.get_attr<FloatAttribute>("downcutting_max_depth_ratio"),
              node.get_attr<BoolAttribute>("flux_diffusion"),
              node.get_attr<FloatAttribute>("flux_diffusion_strength")
              //  *p_rain_map = nullptr,
              // Array *p_water_depth = nullptr,
              // Array *p_sediment = nullptr,
              // Array *p_vel_u = nullptr,
              // Array *p_vel_v = nullptr
          );
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // if (p_erosion_map)
    // {
    //   p_erosion_map->smooth_overlap_buffers();
    //   p_erosion_map->remap(0.f, 1.f, node.cfg().cm_cpu);
    // }

    // if (p_deposition_map)
    // {
    //   p_deposition_map->smooth_overlap_buffers();
    //   p_deposition_map->remap(0.f, 1.f, node.cfg().cm_cpu);
    // }
  }
}

} // namespace hesiod
