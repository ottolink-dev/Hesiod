/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "highmap/filters.hpp"
#include "highmap/math.hpp"
#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/range.hpp"
#include "highmap/virtual_array/virtual_array.hpp"

#include "meta/core/attribute.hpp"
#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/attributes.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/post_effects.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void post_apply_enveloppe(BaseNode                 &node,
                          hmap::VirtualArray       &h,
                          const hmap::VirtualArray *p_env)
{
  Logger::log()->trace("post_apply_enveloppe: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  if (!p_env)
    return;

  float hmin = h.min(node.cfg().cm_cpu);

  hmap::for_each_tile(
      {p_env},
      {&h},
      [hmin](std::vector<const hmap::Array *> p_arrays_in,
             std::vector<hmap::Array *>       p_arrays_out,
             const hmap::TileRegion &)
      {
        auto [pa_env] = unpack<1>(p_arrays_in);
        auto [pa_out] = unpack<1>(p_arrays_out);

        *pa_out -= hmin;
        *pa_out *= *pa_env;
        *pa_out += hmin;
      },
      node.cfg().cm_cpu);
}

void post_apply_normalize_and_clip(BaseNode           &node,
                                   hmap::VirtualArray &h,
                                   float               multiplier,
                                   bool                smooth_transition,
                                   bool                range_01)
{
  Logger::log()->trace("post_apply_normalize_and_clip: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  hmap::for_each_tile(
      {},
      {&h},
      [multiplier, smooth_transition, range_01](std::vector<const hmap::Array *>,
                                                std::vector<hmap::Array *> p_arrays_out,
                                                const hmap::TileRegion &,
                                                hmap::ICommunicator &comm)
      {
        auto [pa_out] = unpack<1>(p_arrays_out);

        hmap::normalize_and_clip(*pa_out, multiplier, smooth_transition, range_01, comm);
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
      {},
      {&h},
      [range, range_sat](std::vector<const hmap::Array *>,
                         std::vector<hmap::Array *> p_arrays_out,
                         const hmap::TileRegion &)
      {
        auto [pa_h] = unpack<1>(p_arrays_out);

        float k_smoothing = 0.1f * (range_sat.y - range_sat.x);

        hmap::saturate(*pa_h, range_sat.x, range_sat.y, range.x, range.y, k_smoothing);
      },
      node.cfg().cm_cpu);
}

void post_process_heightmap(BaseNode                 &node,
                            hmap::VirtualArray       &h,
                            const hmap::VirtualArray *p_in)
{
  Logger::log()->trace("post_process_heightmap: [{}]/[{}]",
                       node.get_node_type(),
                       node.get_id());

  // mix
  if (p_in)
  {
    // mix
    float      k = 0.1f; // TODO hardcoded?
    int        ir = 0;
    const auto method = node.val_enum<BlendingMethod>(post_keys::MIX_METHOD);
    blend_heightmaps(node, h, *p_in, h, method, k, ir);

    // lerp between input and output
    float t = node.val<float>(post_keys::MIX);

    hmap::for_each_tile(
        {p_in},
        {&h},
        [t](std::vector<const hmap::Array *> p_arrays_in,
            std::vector<hmap::Array *>       p_arrays_out,
            const hmap::TileRegion &)
        {
          auto [pa_in] = unpack<1>(p_arrays_in);
          auto [pa_out] = unpack<1>(p_arrays_out);

          *pa_out = hmap::lerp(*pa_in, *pa_out, t);
        },
        node.cfg().cm_cpu);
  }

  // inverse
  if (node.val<bool>(post_keys::INVERSE))
    h.inverse(node.cfg().cm_cpu);

  // gamma
  float post_gamma = node.val<float>(post_keys::GAMMA);

  if (post_gamma != 1.f)
  {
    float hmin = h.min(node.cfg().cm_cpu);
    float hmax = h.max(node.cfg().cm_cpu);
    h.remap(0.f, 1.f, hmin, hmax, node.cfg().cm_cpu);

    hmap::for_each_tile(
        {},
        {&h},
        [post_gamma](std::vector<const hmap::Array *>,
                     std::vector<hmap::Array *> p_arrays_out,
                     const hmap::TileRegion &)
        {
          auto [pa] = unpack<1>(p_arrays_out);
          hmap::gamma_correction(*pa, post_gamma);
        },
        node.cfg().cm_cpu);

    h.remap(hmin, hmax, 0.f, 1.f, node.cfg().cm_cpu);
  }

  // gain
  float post_gain = node.val<float>(post_keys::GAIN);

  if (post_gain != 1.f)
  {
    float hmin = h.min(node.cfg().cm_cpu);
    float hmax = h.max(node.cfg().cm_cpu);
    h.remap(0.f, 1.f, hmin, hmax, node.cfg().cm_cpu);

    hmap::for_each_tile(
        {},
        {&h},
        [post_gain](std::vector<const hmap::Array *>,
                    std::vector<hmap::Array *> p_arrays_out,
                    const hmap::TileRegion &)
        {
          auto [pa] = unpack<1>(p_arrays_out);
          hmap::gain(*pa, post_gain);
        },
        node.cfg().cm_cpu);

    h.remap(hmin, hmax, 0.f, 1.f, node.cfg().cm_cpu);
  }

  // smoothing
  const int ir = (int)(node.val<float>(post_keys::SMOOTHING_RADIUS) * h.shape.x);

  if (ir)
  {
    hmap::for_each_tile(
        {},
        {&h},
        [&ir](std::vector<const hmap::Array *>,
              std::vector<hmap::Array *> p_arrays_out,
              const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays_out);
          return hmap::gpu::smooth_cpulse(*pa_out, ir);
        },
        node.cfg().cm_gpu);

    h.sync_overlap_buffers();
  }

  // remap
  if (node.state_val<bool>(post_keys::REMAP, meta::keys::state::active))
  {
    glm::vec2 range = node.val<glm::vec2>(post_keys::REMAP);

    h.remap(range.x, range.y, node.cfg().cm_cpu);
  }

  // saturate
  if (node.state_val<bool>(post_keys::SATURATE, meta::keys::state::active))
  {
    float     hmin = h.min(node.cfg().cm_cpu);
    float     hmax = h.max(node.cfg().cm_cpu);
    glm::vec2 range = node.val<glm::vec2>(post_keys::SATURATE);

    hmap::for_each_tile(
        {},
        {&h},
        [&](std::vector<const hmap::Array *>,
            std::vector<hmap::Array *> p_arrays_out,
            const hmap::TileRegion &)
        {
          auto [pa_out] = unpack<1>(p_arrays_out);

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

  // The slider-driven effects carry the same `state.active` the two ranges
  // already have, so the properties panel can keep one in the output chain
  // while it still sits on its neutral value. Declared here rather than on
  // first use: an undo snapshot only restores the state keys it contains.
  const auto declare_stage = [](meta::AbstractAttribute &attr)
  { attr.state().try_add(meta::keys::state::active, false); };

  // clang-format off
  if (options.add_mix)
  {
    add_enum(node, post_keys::MIX_METHOD, "Mix Method", enum_mappings.blending_method_map, "replace");
    declare_stage(add_float(node, post_keys::MIX, "Mix Factor", 1.f, 0.f, 1.f));
  }

  add_bool(node, post_keys::INVERSE, "Invert Output", false);
  declare_stage(add_float(node, post_keys::GAMMA, "Gamma", 1.f, 0.01f, 4.f));
  declare_stage(add_float(node, post_keys::GAIN, "Gain", 1.f, 0.01f, 4.f));
  declare_stage(add_float(node, post_keys::SMOOTHING_RADIUS, "Smoothing Radius", 0.f, 0.f, 0.05f));
  add_range(node, post_keys::REMAP, "Remap Range", {0.f, 1.f}, -1.f, 2.f, options.remap_active_state);
  add_range(node, post_keys::SATURATE, "Saturation Range", {0.f, 1.f}, -1.f, 2.f, false);
  // clang-format on
}

// --- effect catalogue

namespace
{

bool stage_flag(const meta::AttributeContainer &container, const char *key)
{
  const meta::AbstractAttribute *p_attr = container.find(key);
  if (!p_attr)
    return false;

  const bool *p_flag = p_attr->state().try_value<bool>(meta::keys::state::active);
  return p_flag && *p_flag;
}

void set_stage_flag(meta::AttributeContainer &container, const char *key, bool value)
{
  meta::AbstractAttribute *p_attr = container.find(key);
  if (!p_attr)
    return;

  if (bool *p_flag = p_attr->state().try_value<bool>(meta::keys::state::active))
    *p_flag = value;
  else
    p_attr->state().try_add(meta::keys::state::active, bool(value));
}

template <typename T>
void set_attribute(meta::AttributeContainer &container, const char *key, const T &value)
{
  if (meta::AbstractAttribute *p_attr = container.find(key))
    if (auto *p_typed = p_attr->try_cast<meta::Attribute<T>>())
      p_typed->set_value(value);
}

bool is_range_active(const meta::AttributeContainer &container, const char *key)
{
  const meta::AbstractAttribute *p_attr = container.find(key);
  if (!p_attr)
    return false;

  // a range that never declared the flag is on, as its editor has it
  const bool *p_flag = p_attr->state().try_value<bool>(meta::keys::state::active);
  return p_flag ? *p_flag : true;
}

void set_range_active(meta::AttributeContainer &container, const char *key, bool active)
{
  meta::AbstractAttribute *p_attr = container.find(key);
  auto *p_range = p_attr ? p_attr->try_cast<meta::Attribute<glm::vec2>>() : nullptr;

  if (!p_range || is_range_active(container, key) == active)
    return;

  meta::AttributeContainer &state = p_range->state();

  const auto remember = [&state](const glm::vec2 &range)
  {
    if (auto *p_last = state.try_value<glm::vec2>(meta::keys::state::last_active_value))
      *p_last = range;
    else
      state.try_add(meta::keys::state::last_active_value, glm::vec2(range));
  };

  // The flag goes first: writing the value notifies whoever shows the range,
  // and they read the flag to decide how.
  if (bool *p_flag = state.try_value<bool>(meta::keys::state::active))
    *p_flag = active;
  else
    state.try_add(meta::keys::state::active, bool(active));

  if (!active)
  {
    // what the range editor leaves behind when it is switched off
    remember(p_range->value());
    p_range->set_value(glm::vec2(-1.f, 0.f));
    return;
  }

  float vmin = -1.f;
  float vmax = 2.f;
  if (const float *p = p_range->metadata().try_value<float>(meta::keys::constraints::min))
    vmin = *p;
  if (const float *p = p_range->metadata().try_value<float>(meta::keys::constraints::max))
    vmax = *p;

  const auto usable = [vmin, vmax](const glm::vec2 &range)
  {
    // The whole domain is what the range editor seeds a range with before it
    // has ever been on. Nobody picked it, and for a remap it is a poor place
    // to start: the output jumps to three times its height.
    return range.x < range.y && !(range.x <= vmin && range.y >= vmax);
  };

  const glm::vec2  current = p_range->value();
  const glm::vec2 *p_last = state.try_value<glm::vec2>(
      meta::keys::state::last_active_value);

  glm::vec2 range(std::clamp(0.f, vmin, vmax), std::clamp(1.f, vmin, vmax));

  if (p_last && usable(*p_last))
    range = *p_last;
  else if (usable(current) && current != glm::vec2(-1.f, 0.f))
    range = current;

  remember(range);
  p_range->set_value(range);
}

} // namespace

const std::vector<PostEffectInfo> &post_effect_catalog()
{
  // clang-format off
  static const std::vector<PostEffectInfo> catalog = {
      {PostEffect::MIX, "Mix", "Blend the result with the incoming heightmap", {post_keys::MIX_METHOD, post_keys::MIX}},
      {PostEffect::INVERT, "Invert", "Turn the output upside down", {post_keys::INVERSE}},
      {PostEffect::GAMMA, "Gamma", "Bend the mid-heights, both ends stay put", {post_keys::GAMMA}},
      {PostEffect::GAIN, "Gain", "Sharpen or soften the contrast around mid-height", {post_keys::GAIN}},
      {PostEffect::SMOOTHING, "Smooth", "Blur the output over a radius", {post_keys::SMOOTHING_RADIUS}},
      {PostEffect::REMAP, "Remap", "Stretch the output to fit a range", {post_keys::REMAP}},
      {PostEffect::SATURATE, "Saturate", "Clip to a range, then stretch back to full height", {post_keys::SATURATE}},
  };
  // clang-format on

  return catalog;
}

const PostEffectInfo &post_effect_info(PostEffect effect)
{
  for (const PostEffectInfo &info : post_effect_catalog())
    if (info.effect == effect)
      return info;

  return post_effect_catalog().front(); // unreachable: every effect is listed
}

bool is_post_effect_attribute(const std::string &key)
{
  for (const PostEffectInfo &info : post_effect_catalog())
    for (const std::string &owned : info.keys)
      if (owned == key)
        return true;

  return false;
}

bool has_post_effect(const meta::AttributeContainer &container, PostEffect effect)
{
  for (const std::string &key : post_effect_info(effect).keys)
    if (!container.contains(key))
      return false;

  return true;
}

bool has_post_effects(const meta::AttributeContainer &container)
{
  for (const PostEffectInfo &info : post_effect_catalog())
    if (has_post_effect(container, info.effect))
      return true;

  return false;
}

bool is_post_effect_active(const meta::AttributeContainer &container, PostEffect effect)
{
  if (!has_post_effect(container, effect))
    return false;

  // The comparisons are the pass's own, exact ones included: whatever it would
  // apply has to show as active.
  const auto off_neutral = [&container](const char *key, float neutral)
  {
    const float *p_value = container.try_value<float>(key);
    return p_value && *p_value != neutral;
  };

  switch (effect)
  {
  case PostEffect::MIX:
  {
    const int *p_method = container.try_value<int>(post_keys::MIX_METHOD);
    return (p_method && *p_method != BlendingMethod::REPLACE) ||
           off_neutral(post_keys::MIX, 1.f) || stage_flag(container, post_keys::MIX);
  }

  case PostEffect::INVERT:
  {
    const bool *p_value = container.try_value<bool>(post_keys::INVERSE);
    return p_value && *p_value;
  }

  case PostEffect::GAMMA:
    return off_neutral(post_keys::GAMMA, 1.f) || stage_flag(container, post_keys::GAMMA);

  case PostEffect::GAIN:
    return off_neutral(post_keys::GAIN, 1.f) || stage_flag(container, post_keys::GAIN);

  case PostEffect::SMOOTHING:
    return off_neutral(post_keys::SMOOTHING_RADIUS, 0.f) ||
           stage_flag(container, post_keys::SMOOTHING_RADIUS);

  case PostEffect::REMAP:
    return is_range_active(container, post_keys::REMAP);

  case PostEffect::SATURATE:
    return is_range_active(container, post_keys::SATURATE);
  }

  return false;
}

void set_post_effect_active(meta::AttributeContainer &container,
                            PostEffect                effect,
                            bool                      active)
{
  if (!has_post_effect(container, effect))
    return;

  // Flags before values throughout: a value write notifies the panel, which
  // then asks whether the effect is active.
  switch (effect)
  {
  case PostEffect::MIX:
    set_stage_flag(container, post_keys::MIX, active);
    if (!active)
    {
      set_attribute<int>(container, post_keys::MIX_METHOD, BlendingMethod::REPLACE);
      set_attribute<float>(container, post_keys::MIX, 1.f);
    }
    break;

  case PostEffect::INVERT:
    set_attribute<bool>(container, post_keys::INVERSE, active);
    break;

  case PostEffect::GAMMA:
    set_stage_flag(container, post_keys::GAMMA, active);
    if (!active)
      set_attribute<float>(container, post_keys::GAMMA, 1.f);
    break;

  case PostEffect::GAIN:
    set_stage_flag(container, post_keys::GAIN, active);
    if (!active)
      set_attribute<float>(container, post_keys::GAIN, 1.f);
    break;

  case PostEffect::SMOOTHING:
    set_stage_flag(container, post_keys::SMOOTHING_RADIUS, active);
    if (!active)
      set_attribute<float>(container, post_keys::SMOOTHING_RADIUS, 0.f);
    break;

  case PostEffect::REMAP:
    set_range_active(container, post_keys::REMAP, active);
    break;

  case PostEffect::SATURATE:
    set_range_active(container, post_keys::SATURATE, active);
    break;
  }
}

} // namespace hesiod
