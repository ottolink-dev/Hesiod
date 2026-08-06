/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/selector.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_select_soil_weathered_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  node.add_attr<FloatAttribute>("radius_curvature", "Curvature Radius", 0.f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("radius_gradient", "Gradient Radius", 0.005f, 0.f, 0.1f);
  node.add_attr<FloatAttribute>("gradient_gain", "Gradient Gain", 1.f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("curvature_weight", "Curvature Weight", 1.f, -1.f, 1.f);
  node.add_attr<FloatAttribute>("gradient_weight", "Gradient Weight", 0.2f, -1.f, 1.f);
  node.add_attr<EnumAttribute>("curvature_clamp_mode",
                               "Curvature Clamp Mode",
                               enum_mappings.clamping_mode_map,
                               "Keep positive & clamp");
  node.add_attr<FloatAttribute>("curvature_clamping",
                                "Curvature Clamp Limit",
                                1.f,
                                0.f,
                                FLT_MAX,
                                "{:.4f}");

  // attribute(s) order
  node.set_attr_ordered_key({"_GROUPBOX_BEGIN_Main Parameters",
                             "_TEXT_Weights",
                             "curvature_weight",
                             "gradient_weight",
                             //
                             "_TEXT_Curvature",
                             "radius_curvature",
                             "curvature_clamp_mode",
                             "curvature_clamping",
                             //
                             "_TEXT_Slope",
                             "radius_gradient",
                             "gradient_gain",
                             "_GROUPBOX_END_"});

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = false, .remap_active_state = true});
}

void compute_select_soil_weathered_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    int nx = p_out->shape.x; // for gradient scaling
    int ir_curv = (int)(node.get_attr<FloatAttribute>("radius_curvature") * nx);
    int ir_grad = std::max(1,
                           (int)(node.get_attr<FloatAttribute>("radius_gradient") * nx));

    // --- compute gradient norm

    hmap::VirtualArray grad_norm(CONFIG(node));

    hmap::for_each_tile(
        {&grad_norm, p_in},
        [&node, ir_grad](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);
          *pa_out = hmap::gpu::morphological_gradient(*pa_in, ir_grad);
        },
        node.cfg().cm_gpu);

    grad_norm.remap(0.f, 1.f, node.cfg().cm_cpu);

    // gain
    hmap::for_each_tile(
        {&grad_norm},
        [&node](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          hmap::gain(*pa_out, node.get_attr<FloatAttribute>("gradient_gain"));
        },
        node.cfg().cm_cpu);

    // --- compute mask

    hmap::for_each_tile(
        {p_out, p_in, &grad_norm},
        [&node, nx, ir_curv, ir_grad](std::vector<hmap::Array *> p_arrays,
                                      const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_in = p_arrays[1];
          hmap::Array *pa_grad_norm = p_arrays[2];

          auto mode = static_cast<hmap::ClampMode>(
              node.get_attr<EnumAttribute>("curvature_clamp_mode"));

          *pa_out = hmap::gpu::select_soil_weathered(
              *pa_in,
              *pa_grad_norm,
              ir_curv,
              mode,
              node.get_attr<FloatAttribute>("curvature_clamping"),
              node.get_attr<FloatAttribute>("curvature_weight"),
              node.get_attr<FloatAttribute>("gradient_weight"),
              (float)nx);
        },
        node.cfg().cm_gpu);

    // post-process
    p_out->smooth_overlap_buffers();
    post_process_heightmap(node, *p_out);
  }
}

} // namespace hesiod
