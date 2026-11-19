/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "glm/vec2.hpp"

#include "meta/core/attribute.hpp"
#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/app/enum_mappings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/post_effects.hpp"

namespace hesiod
{

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

void set_active_flag(meta::AttributeContainer &state, bool value)
{
  if (bool *p_flag = state.try_value<bool>(meta::keys::state::active))
    *p_flag = value;
  else
    state.try_add(meta::keys::state::active, bool(value));
}

void set_stage_flag(meta::AttributeContainer &container, const char *key, bool value)
{
  if (meta::AbstractAttribute *p_attr = container.find(key))
    set_active_flag(p_attr->state(), value);
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

  if (!p_range)
    return;

  meta::AttributeContainer &state = p_range->state();
  const bool *p_flag = state.try_value<bool>(meta::keys::state::active);
  if (p_flag && *p_flag == active)
    return;

  const auto remember = [&state](const glm::vec2 &range)
  {
    if (auto *p_last = state.try_value<glm::vec2>(meta::keys::state::last_active_value))
      *p_last = range;
    else
      state.try_add(meta::keys::state::last_active_value, glm::vec2(range));
  };

  // The flag goes first: writing the value notifies whoever shows the range,
  // and they read the flag to decide how.
  set_active_flag(state, active);

  float vmin = -1.f;
  float vmax = 2.f;
  if (const float *p = p_range->metadata().try_value<float>(meta::keys::constraints::min))
    vmin = *p;
  if (const float *p = p_range->metadata().try_value<float>(meta::keys::constraints::max))
    vmax = *p;

  if (!active)
  {
    // what the range editor leaves behind when it is switched off
    remember(p_range->value());
    p_range->set_value(
        glm::vec2(std::clamp(-1.f, vmin, vmax), std::clamp(0.f, vmin, vmax)));
    return;
  }

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

struct FloatEffectSpec
{
  const char *key;
  float       neutral;
};

const FloatEffectSpec *float_effect_spec(PostEffect effect)
{
  static constexpr FloatEffectSpec gamma{post_keys::GAMMA, 1.f};
  static constexpr FloatEffectSpec gain{post_keys::GAIN, 1.f};
  static constexpr FloatEffectSpec smoothing{post_keys::SMOOTHING_RADIUS, 0.f};

  switch (effect)
  {
  case PostEffect::GAMMA:
    return &gamma;
  case PostEffect::GAIN:
    return &gain;
  case PostEffect::SMOOTHING:
    return &smoothing;
  default:
    return nullptr;
  }
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

  Logger::log()->error("post_effect_info: unknown PostEffect {}", int(effect));
  return post_effect_catalog().front();
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

  if (const FloatEffectSpec *p_spec = float_effect_spec(effect))
    return off_neutral(p_spec->key, p_spec->neutral) ||
           stage_flag(container, p_spec->key);

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

  case PostEffect::REMAP:
    return is_range_active(container, post_keys::REMAP);

  case PostEffect::SATURATE:
    return is_range_active(container, post_keys::SATURATE);

  default:
    return false;
  }

  return false;
}

void set_post_effect_active(meta::AttributeContainer &container,
                            PostEffect                effect,
                            bool                      active)
{
  if (!has_post_effect(container, effect))
    return;

  Logger::log()->trace("set_post_effect_active: effect={}, active={}",
                       post_effect_info(effect).name,
                       active);

  // Flags before values throughout: a value write notifies the panel, which
  // then asks whether the effect is active.
  if (const FloatEffectSpec *p_spec = float_effect_spec(effect))
  {
    set_stage_flag(container, p_spec->key, active);
    if (!active)
      set_attribute<float>(container, p_spec->key, p_spec->neutral);
    return;
  }

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

  case PostEffect::REMAP:
    set_range_active(container, post_keys::REMAP, active);
    break;

  case PostEffect::SATURATE:
    set_range_active(container, post_keys::SATURATE, active);
    break;

  default:
    break;
  }
}

} // namespace hesiod
