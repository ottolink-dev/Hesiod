/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_hydraulic_stream_log_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "erosion", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "deposition", CONFIG(node));
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "flow_map", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("c_erosion", "Erosion Strength", 0.1f, 0.01f, 1.f);
  node.add_attr<FloatAttribute>("talus_ref", "Slope Threshold", 0.1f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("saturation_ratio",
                                "Water Saturation Threshold",
                                1.f,
                                0.01f,
                                1.f);
  node.add_attr<FloatAttribute>("deposition_radius",
                                "Deposition Radius",
                                0.1f,
                                0.f,
                                0.2f);
  node.add_attr<FloatAttribute>("deposition_scale_ratio",
                                "Sediment Amount Scale",
                                1.f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("gradient_power", "Influence Power", 0.8f, 0.1f, 2.f);
  node.add_attr<FloatAttribute>("gradient_scaling_ratio",
                                "Influence Scale",
                                1.f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("gradient_prefilter_radius",
                                "Prefilter Radius",
                                0.1f,
                                0.f,
                                0.2f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Core Erosion Controls",
                             "c_erosion",
                             "talus_ref",
                             "saturation_ratio",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Slope-Based Controls",
                             "gradient_power",
                             "gradient_scaling_ratio",
                             "gradient_prefilter_radius",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Deposition Controls",
                             "deposition_radius",
                             "deposition_scale_ratio",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_hydraulic_stream_log_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_erosion_map = node.get_value_ref<hmap::VirtualArray>("erosion");
    hmap::VirtualArray *p_deposition_map = node.get_value_ref<hmap::VirtualArray>(
        "deposition");
    hmap::VirtualArray *p_flow_map = node.get_value_ref<hmap::VirtualArray>("flow_map");

    int deposition_ir = (int)(node.get_attr<FloatAttribute>("deposition_radius") *
                              p_out->shape.x);
    int gradient_ir = (int)(node.get_attr<FloatAttribute>("gradient_prefilter_radius") *
                            p_out->shape.x);

    deposition_ir = std::max(1, deposition_ir);
    gradient_ir = std::max(1, gradient_ir);

    hmap::for_each_tile(
        {p_out, p_in, p_mask, p_erosion_map, p_deposition_map, p_flow_map},
        [&node, deposition_ir, gradient_ir](std::vector<hmap::Array *> p_arrays,
                                            const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];
          hmap::Array *pa_mask = p_arrays[2];
          hmap::Array *pa_erosion_map = p_arrays[3];
          hmap::Array *pa_deposition_map = p_arrays[4];
          hmap::Array *pa_flow_map = p_arrays[5];

          *pa_out = *pa_in;

          hmap::gpu::hydraulic_stream_log(
              *pa_out,
              node.get_attr<FloatAttribute>("c_erosion"),
              node.get_attr<FloatAttribute>("talus_ref"),
              pa_mask,
              deposition_ir,
              node.get_attr<FloatAttribute>("deposition_scale_ratio"),
              node.get_attr<FloatAttribute>("gradient_power"),
              node.get_attr<FloatAttribute>("gradient_scaling_ratio"),
              gradient_ir,
              node.get_attr<FloatAttribute>("saturation_ratio"),
              /* p_bedrock */ nullptr,
              /* p_moisture_map */ nullptr,
              pa_erosion_map,
              pa_deposition_map,
              pa_flow_map);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    p_erosion_map->smooth_overlap_buffers();
    p_erosion_map->remap(0.f, 1.f, node.cfg().cm_cpu);

    p_deposition_map->smooth_overlap_buffers();
    p_deposition_map->remap(0.f, 1.f, node.cfg().cm_cpu);

    p_flow_map->smooth_overlap_buffers();
    p_flow_map->remap(0.f, 1.f, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
