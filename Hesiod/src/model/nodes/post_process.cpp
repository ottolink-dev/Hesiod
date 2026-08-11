/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "highmap/filters.hpp"
#include "highmap/math.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "meta/metadata/keys.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void post_apply_enveloppe(BaseNode           &node,
                          hmap::VirtualArray &h,
                          hmap::VirtualArray *p_env)
{
  Logger::log()->trace("post_apply_enveloppe: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  if (!p_env)
    return;

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
    int   method = node.val<int>("post_mix_method");
    blend_heightmaps(node, h, *p_in, h, static_cast<BlendingMethod>(method), k, ir);

    // lerp between input and output
    float t = node.val<float>("post_mix");

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
  if (node.val<bool>("post_inverse"))
    h.inverse(node.cfg().cm_cpu);

  // gamma
  float post_gamma = node.val<float>("post_gamma");

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
  float post_gain = node.val<float>("post_gain");

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
  const int ir = (int)(node.val<float>("post_smoothing_radius") * h.shape.x);

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
  if (node.metadata_val<bool>("post_remap", meta::keys::ui::active))
  {
    glm::vec2 range = node.val<glm::vec2>("post_remap");

    h.remap(range.x, range.y, node.cfg().cm_cpu);
  }

  // saturate
  if (node.metadata_val<bool>("post_saturate", meta::keys::ui::active))
  {
    float     hmin = h.min(node.cfg().cm_cpu);
    float     hmax = h.max(node.cfg().cm_cpu);
    glm::vec2 range = node.val<glm::vec2>("post_saturate");

    hmap::for_each_tile(
        {&h},
        [&](std::vector<hmap::Array *> p_arrays, const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays);

          float k = 0.1f; // TODO hardcoded?

          hmap::saturate(*pa_out, range.x, range.y, hmin, hmax, k);
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

  node.set_current_category("Post-Processing");

  // clang-format off
  if (options.add_mix)
  {
    add_enum(node, "post_mix_method", "Mix Method", enum_mappings.blending_method_map, "replace");
    add_float(node, "post_mix", "Mix Factor", 1.f, 0.f, 1.f); 
  }

  add_bool(node, "post_inverse", "Invert Output", false);
  add_float(node, "post_gamma", "Gamma", 1.f, 0.01f, 10.f);
  add_float(node, "post_gain", "Gain", 1.f, 0.01f, 10.f);
  add_float(node, "post_smoothing_radius", "Smoothing Radius", 0.f, 0.f, 0.05f);
  add_range(node, "post_remap", "Remap Range", {0.f, 1.f}, -1.f, 2.f, options.remap_active_state);
  add_range(node, "post_saturate", "Saturation Range", {0.f, 1.f}, -1.f, 2.f, false);
  // clang-format on
}

} // namespace hesiod
