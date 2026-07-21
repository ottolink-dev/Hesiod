/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_select_soil_rocks_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("rmax", "Max Radius", 0.1f, 0.f, 0.2f);
  node.add_attr<FloatAttribute>("rmin", "Min Radius", 0.f, 0.f, 0.2f);
  node.add_attr<IntAttribute>("steps", "Sampling Steps", 4, 2, 8);

  node.add_attr<FloatAttribute>("smaller_scales_weight",
                                "Smaller-Scale Influence",
                                1.f,
                                0.f,
                                2.f);
  node.add_attr<EnumAttribute>("curvature_clamp_mode",
                               "Clamp Mode",
                               enum_mappings.clamping_mode_map,
                               "Keep positive & clamp");
  node.add_attr<FloatAttribute>("curvature_clamping",
                                "Clamp Limit",
                                1.f,
                                0.f,
                                FLT_MAX,
                                "{:.4f}");
  node.add_attr<FloatAttribute>("saturation_limit", "Saturation Limit", 0.3f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("k_saturation", "Saturation Smoothing", 0.1f, 0.f, 1.f);

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "smaller_scales_weight",
                             "rmax",
                             "rmin",
                             "steps",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Saturation Controls",
                             "saturation_limit",
                             "k_saturation",
                             "_GROUPBOX_END_",
                             //
                             "_GROUPBOX_BEGIN_Curvature Clamping",
                             "curvature_clamp_mode",
                             "curvature_clamping",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_soil_rocks_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // --- selector

    int nx = p_out->shape.x;
    int ir_min = (int)(node.get_attr<FloatAttribute>("rmin") * nx);
    int ir_max = std::max(1, (int)(node.get_attr<FloatAttribute>("rmax") * nx));

    hmap::for_each_tile(
        {p_out, p_in},
        [&node, ir_min, ir_max](std::vector<hmap::Array *> p_arrays,
                                const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          auto mode = static_cast<hmap::ClampMode>(
              node.get_attr<EnumAttribute>("curvature_clamp_mode"));

          *pa_out = hmap::gpu::select_soil_rocks(
              *pa_in,
              ir_max,
              ir_min,
              node.get_attr<IntAttribute>("steps"),
              node.get_attr<FloatAttribute>("smaller_scales_weight"),
              mode,
              node.get_attr<FloatAttribute>("curvature_clamping"));
        },
        node.cfg().cm_gpu);

    // post-process
    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);

    // --- saturate

    float hmin = p_out->min(node.cfg().cm_cpu);
    float hmax = p_out->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          hmap::saturate(*pa_out,
                         0.f,
                         node.get_attr<FloatAttribute>("saturation_limit"),
                         hmin,
                         hmax,
                         node.get_attr<FloatAttribute>("k_saturation"));
        },
        node.cfg().cm_cpu);
  }
}

} // namespace hesiod
