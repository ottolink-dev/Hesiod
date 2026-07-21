/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/primitives.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/compat_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"
#include "hesiod/model/utils.hpp"

using namespace attr;

namespace hesiod
{

void setup_strata_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  glm::vec2 kw_default;
  node.add_attr<FloatAttribute>("angle", "angle", 0.f, -180.f, 180.f);
  node.add_attr<FloatAttribute>("slope", "slope", 2.f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("kz", "kz", 1.f, 0.f, FLT_MAX);
  node.add_attr<FloatAttribute>("gamma", "gamma", 0.5f, 0.01f, 2.f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<BoolAttribute>("linear_gamma", "linear_gamma", true);
  node.add_attr<IntAttribute>("octaves", "Octaves", 4, 0, 32);
  node.add_attr<FloatAttribute>("lacunarity", "Lacunarity", 2.f, 0.01f, 4.f);
  node.add_attr<FloatAttribute>("gamma_noise_ratio", "gamma_noise_ratio", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("noise_amp", "noise_amp", 0.4f, 0.f, 1.f);

  kw_default = {4.f, 4.f};
  node.add_attr<WaveNbAttribute>("noise_kw",
                                 "Spatial Frequency",
                                 kw_default,
                                 0.f,
                                 32.f,
                                 true);

  node.add_attr<BoolAttribute>("enable_ridge_noise", "enable_ridge_noise", true);

  kw_default = {4.f, 1.5f};
  node.add_attr<WaveNbAttribute>("ridge_noise_kw",
                                 "ridge_noise_kw",
                                 kw_default,
                                 0.f,
                                 32.f,
                                 false);
  node.add_attr<FloatAttribute>("ridge_angle_shift",
                                "ridge_angle_shift",
                                45.f,
                                -180.f,
                                180.f);
  node.add_attr<FloatAttribute>("ridge_noise_amp", "ridge_noise_amp", 0.4f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("ridge_clamp_vmin", "ridge_clamp_vmin", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("ridge_remap_vmin", "ridge_remap_vmin", 0.6f, 0.f, 1.f);
  node.add_attr<BoolAttribute>("apply_elevation_mask", "apply_elevation_mask", true);
  node.add_attr<BoolAttribute>("apply_ridge_mask", "apply_ridge_mask", true);
  node.add_attr<FloatAttribute>("mask_gamma", "mask_gamma", 1.f, 0.01f, 4.f);

  // attribute(s) order
  node.set_attr_ordered_key({"_TEXT_Base paramaters",
                             "angle",
                             "slope",
                             "gamma",
                             "kz",
                             "seed",
                             "linear_gamma",
                             "_TEXT_Fractal strata",
                             "octaves",
                             "lacunarity",
                             "gamma_noise_ratio",
                             "_TEXT_Background noise",
                             "noise_amp",
                             "noise_kw",
                             "_TEXT_Ridges",
                             "enable_ridge_noise",
                             "ridge_noise_kw",
                             "ridge_angle_shift",
                             "ridge_noise_amp",
                             "ridge_clamp_vmin",
                             "ridge_remap_vmin",
                             "_TEXT_Masking",
                             "apply_elevation_mask",
                             "apply_ridge_mask",
                             "mask_gamma"});

  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_strata_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("output");

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_mask},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays,
                            const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in, pa_mask] = unpack<3>(p_arrays);

          *pa_out = *pa_in;

          // remap to [0, 1] as required by this filter
          hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);

          hmap::gpu::strata(*pa_out,
                            node.get_attr<FloatAttribute>("angle"),
                            node.get_attr<FloatAttribute>("slope"),
                            node.get_attr<FloatAttribute>("gamma"),
                            node.get_attr<SeedAttribute>("seed"),
                            node.get_attr<BoolAttribute>("linear_gamma"),
                            node.get_attr<FloatAttribute>("kz"),
                            node.get_attr<IntAttribute>("octaves"),
                            node.get_attr<FloatAttribute>("lacunarity"),
                            node.get_attr<FloatAttribute>("gamma_noise_ratio"),
                            node.get_attr<FloatAttribute>("noise_amp"),
                            node.get_attr<WaveNbAttribute>("noise_kw"),
                            node.get_attr<BoolAttribute>("enable_ridge_noise"),
                            node.get_attr<WaveNbAttribute>("ridge_noise_kw"),
                            node.get_attr<FloatAttribute>("ridge_angle_shift"),
                            node.get_attr<FloatAttribute>("ridge_noise_amp"),
                            node.get_attr<FloatAttribute>("ridge_clamp_vmin"),
                            node.get_attr<FloatAttribute>("ridge_remap_vmin"),
                            node.get_attr<BoolAttribute>("apply_elevation_mask"),
                            node.get_attr<BoolAttribute>("apply_ridge_mask"),
                            node.get_attr<FloatAttribute>("mask_gamma"),
                            pa_mask,
                            region.bbox);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();
    p_out->remap(hmin, hmax, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
