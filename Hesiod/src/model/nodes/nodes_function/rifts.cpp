/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/erosion.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void setup_rifts_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "input");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dx");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "dy");
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "mask");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "output", CONFIG(node));

  // attribute(s)
  glm::vec2 kw_default = {4.f, 1.2f};
  node.add_attr<WaveNbAttribute>("kw", "Spatial Frequency", kw_default, 0.f, 32.f, false);
  node.add_attr<FloatAttribute>("angle", "angle", 0.f, -180.f, 180.f);
  node.add_attr<FloatAttribute>("amplitude", "amplitude", 0.1f, 0.f, 1.f);
  node.add_attr<SeedAttribute>("seed", "Seed");
  node.add_attr<FloatAttribute>("k_smooth_bottom", "k_smooth_bottom", 0.05f, 0.f, 0.3f);
  node.add_attr<FloatAttribute>("k_smooth_top", "k_smooth_top", 0.05f, 0.f, 0.3f);
  node.add_attr<FloatAttribute>("radial_spread_amp",
                                "radial_spread_amp",
                                0.2f,
                                -1.f,
                                1.f);
  node.add_attr<FloatAttribute>("elevation_noise_amp",
                                "elevation_noise_amp",
                                0.1f,
                                0.f,
                                1.f);
  node.add_attr<FloatAttribute>("clamp_vmin", "clamp_vmin", 0.5f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("remap_vmin", "remap_vmin", 0.6f, 0.f, 1.f);
  node.add_attr<FloatAttribute>("elevation_noise_shift",
                                "elevation_noise_shift",
                                0.f,
                                -1.f,
                                1.f);
  node.add_attr<BoolAttribute>("apply_mask", "apply_mask", true);
  node.add_attr<BoolAttribute>("reverse_mask", "reverse_mask", false);
  node.add_attr<FloatAttribute>("mask_gamma", "mask_gamma", 1.f, 0.01f, 4.f);
  node.add_attr<Vec2FloatAttribute>("center", "center");

  // attribute(s) order
  node.set_attr_ordered_key({
      "_TEXT_Base paramaters",
      "kw",
      "angle",
      "amplitude",
      "seed",
      "_TEXT_Edge smoothing",
      "k_smooth_bottom",
      "k_smooth_top",
      "_TEXT_Ridge geometry",
      "radial_spread_amp",
      "elevation_noise_amp",
      "clamp_vmin",
      "remap_vmin",
      "elevation_noise_shift",
      "center",
      "_TEXT_Masking",
      "apply_mask",
      "reverse_mask",
      "mask_gamma",
  });

  setup_default_noise(node, {.noise_amp = 0.1f, .kw = 4.f});
  setup_pre_process_mask_attributes(node);
  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = false});
}

void compute_rifts_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>("input");

  if (p_in)
  {
    hmap::VirtualArray *p_dx   = node.get_value_ref<hmap::VirtualArray>("dx");
    hmap::VirtualArray *p_dy   = node.get_value_ref<hmap::VirtualArray>("dy");
    hmap::VirtualArray *p_mask = node.get_value_ref<hmap::VirtualArray>("mask");
    hmap::VirtualArray *p_out  = node.get_value_ref<hmap::VirtualArray>("output");

    // prepare mask
    std::shared_ptr<hmap::VirtualArray> sp_mask = pre_process_mask(node, p_mask, *p_in);

    // prepare default noise
    hmap::VirtualArray noise_default(CONFIG(node));
    generate_noise(node, p_dx, noise_default);

    // remap to [0, 1] as required by this filter
    float hmin = p_in->min(node.cfg().cm_cpu);
    float hmax = p_in->max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {p_out, p_in, p_dx, p_dy, p_mask},
        [&node, hmin, hmax](std::vector<hmap::Array *> p_arrays,
                            const hmap::TileRegion    &region)
        {
          auto [pa_out, pa_in, pa_dx, pa_dy, pa_mask] = unpack<5>(p_arrays);

          *pa_out = *pa_in;

          hmap::remap(*pa_out, 0.f, 1.f, hmin, hmax);

          hmap::gpu::rifts(*pa_out,
                           node.get_attr<WaveNbAttribute>("kw"),
                           node.get_attr<FloatAttribute>("angle"),
                           node.get_attr<FloatAttribute>("amplitude"),
                           node.get_attr<SeedAttribute>("seed"),
                           node.get_attr<FloatAttribute>("elevation_noise_shift"),
                           node.get_attr<FloatAttribute>("k_smooth_bottom"),
                           node.get_attr<FloatAttribute>("k_smooth_top"),
                           node.get_attr<FloatAttribute>("radial_spread_amp"),
                           node.get_attr<FloatAttribute>("elevation_noise_amp"),
                           node.get_attr<FloatAttribute>("clamp_vmin"),
                           node.get_attr<FloatAttribute>("remap_vmin"),
                           node.get_attr<BoolAttribute>("apply_mask"),
                           !node.get_attr<BoolAttribute>("reverse_mask") &&
                               node.get_attr<BoolAttribute>("apply_mask"),
                           node.get_attr<FloatAttribute>("mask_gamma"),
                           pa_dx,
                           pa_dy,
                           pa_mask,
                           node.get_attr<Vec2FloatAttribute>("center"),
                           region.bbox);
          hmap::remap(*pa_out, hmin, hmax, 0.f, 1.f);
        },
        node.cfg().cm_gpu);

    p_out->smooth_overlap_buffers();

    // remap to original range
    p_out->remap(hmin, hmax, node.cfg().cm_cpu);

    // post-process
    post_process_heightmap(node, *p_out, p_in);
  }
}

} // namespace hesiod
