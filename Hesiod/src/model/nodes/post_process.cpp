/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/math.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "hesiod/model/nodes/legacy/legacy_attributes.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

using namespace attr;

namespace hesiod
{

void post_apply_enveloppe(BaseNode           &node,
                          hmap::VirtualArray &h,
                          hmap::VirtualArray *p_env)
{
  Logger::log()->trace("post_apply_enveloppe: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  if (p_env)
  {
    float hmin = h.min(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {&h, p_env},
        [hmin](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          hmap::Array *pa_out = p_arrays[0];
          hmap::Array *pa_env = p_arrays[1];

          *pa_out -= hmin;
          *pa_out *= *pa_env;
          *pa_out += hmin;
        },
        node.cfg().cm_cpu);
  }
}

void post_apply_saturate_percentile(BaseNode           &node,
                                    hmap::VirtualArray &h,
                                    float               satmin,
                                    float               satmax)
{
  if (satmin == 0.f && satmax == 1.f)
    return;

  glm::vec2 range_sat = h.range_percentile(satmin, satmax, node.cfg().cm_cpu);
  glm::vec2 range = h.range(node.cfg().cm_cpu);

  hmap::for_each_tile(
      {&h},
      [range, range_sat](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
      {
        auto [pa_h] = unpack<1>(p_arrays);

        float k_smoothing = 0.1f * (range_sat.y - range_sat.x);

        hmap::saturate(*pa_h, range_sat.x, range_sat.y, range.x, range.y, k_smoothing);
      },
      node.cfg().cm_cpu);
}

void post_process_heightmap(BaseNode           &node,
                            hmap::VirtualArray &h,
                            hmap::VirtualArray *p_in)
{
  Logger::log()->trace("post_process_heightmap: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  // mix
  if (p_in)
  {
    // mix
    float k = 0.1f; // TODO hardcoded?
    int   ir = 0;
    int   method = node.get_attr<EnumAttribute>("post_mix_method");
    blend_heightmaps(node, h, *p_in, h, static_cast<BlendingMethod>(method), k, ir);

    // lerp between input and output
    float t = node.get_attr<FloatAttribute>("post_mix");

    hmap::for_each_tile(
        {&h, p_in},
        [t](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out, pa_in] = unpack<2>(p_arrays);

          *pa_out = hmap::lerp(*pa_in, *pa_out, t);
        },
        node.cfg().cm_cpu);
  }

  // inverse
  if (node.get_attr<BoolAttribute>("post_inverse"))
    h.inverse(node.cfg().cm_cpu);

  // gamma
  float post_gamma = node.get_attr<FloatAttribute>("post_gamma");

  if (post_gamma != 1.f)
  {
    float hmin = h.min(node.cfg().cm_cpu);
    float hmax = h.max(node.cfg().cm_cpu);
    h.remap(0.f, 1.f, hmin, hmax, node.cfg().cm_cpu);

    hmap::for_each_tile(
        {&h},
        [post_gamma](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa] = unpack<1>(p_arrays);
          hmap::gamma_correction(*pa, post_gamma);
        },
        node.cfg().cm_cpu);

    h.remap(hmin, hmax, 0.f, 1.f, node.cfg().cm_cpu);
  }

  // gain
  float post_gain = node.get_attr<FloatAttribute>("post_gain");

  if (post_gain != 1.f)
  {
    float hmin = h.min(node.cfg().cm_cpu);
    float hmax = h.max(node.cfg().cm_cpu);
    h.remap(0.f, 1.f, hmin, hmax, node.cfg().cm_cpu);

    hmap::for_each_tile(
        {&h},
        [post_gain](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa] = unpack<1>(p_arrays);
          hmap::gain(*pa, post_gain);
        },
        node.cfg().cm_cpu);

    h.remap(hmin, hmax, 0.f, 1.f, node.cfg().cm_cpu);
  }

  // smoothing
  const int ir = (int)(node.get_attr<FloatAttribute>("post_smoothing_radius") *
                       h.shape.x);

  if (ir)
  {
    hmap::for_each_tile(
        {&h},
        [&ir](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);
          return hmap::gpu::smooth_cpulse(*pa_out, ir);
        },
        node.cfg().cm_gpu);

    h.smooth_overlap_buffers();
  }

  // remap
  if (node.get_attr_ref<RangeAttribute>("post_remap")->get_is_active())
    h.remap(node.get_attr<RangeAttribute>("post_remap")[0],
            node.get_attr<RangeAttribute>("post_remap")[1],
            node.cfg().cm_cpu);

  // saturate
  if (node.get_attr_ref<RangeAttribute>("post_saturate")->get_is_active())
  {
    float hmin = h.min(node.cfg().cm_cpu);
    float hmax = h.max(node.cfg().cm_cpu);

    hmap::for_each_tile(
        {&h},
        [&node, &hmin, &hmax](std::vector<hmap::Array *> p_arrays,
                              const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          float k = 0.1f; // TODO hardcoded?

          hmap::saturate(*pa_out,
                         node.get_attr<RangeAttribute>("post_saturate")[0],
                         node.get_attr<RangeAttribute>("post_saturate")[1],
                         hmin,
                         hmax,
                         k);
        },
        node.cfg().cm_cpu);
  }
}

void setup_post_process_heightmap_attributes(BaseNode                   &node,
                                             PostProcessHeightmapOptions options)
{
  Logger::log()->trace("setup_post_process_heightmap_attributes: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  if (options.add_mix)
  {
    node.add_attr<EnumAttribute>("post_mix_method",
                                 "Mix Method",
                                 enum_mappings.blending_method_map,
                                 "replace");
    node.add_attr<FloatAttribute>("post_mix", "Mix Factor", 1.f, 0.f, 1.f);
  }

  node.add_attr<BoolAttribute>("post_inverse", "Invert Output", false);
  node.add_attr<FloatAttribute>("post_gamma", "Gamma", 1.f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("post_gain", "Gain", 1.f, 0.01f, 10.f);
  node.add_attr<FloatAttribute>("post_smoothing_radius",
                                "Smoothing Radius",
                                0.f,
                                0.f,
                                0.05f);
  node.add_attr<RangeAttribute>("post_remap", "Remap Range", options.remap_active_state);
  node.add_attr<RangeAttribute>("post_saturate", "Saturation Range", false);

  std::vector<std::string> *p_keys = node.get_attr_ordered_key_ref();

  p_keys->push_back("_GROUPBOX_BEGIN_Post-Processing");

  if (options.add_mix)
  {
    p_keys->push_back("post_mix_method");
    p_keys->push_back("post_mix");
  }

  p_keys->push_back("post_inverse");
  p_keys->push_back("post_gamma");
  p_keys->push_back("post_gain");
  p_keys->push_back("post_smoothing_radius");
  p_keys->push_back("post_remap");
  p_keys->push_back("post_saturate");
  p_keys->push_back("_GROUPBOX_END_");
}

} // namespace hesiod
