/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <string>
#include <vector>

namespace meta
{
class AttributeContainer;
}

namespace hesiod
{

/// Attributes set up by setup_post_process_heightmap_attributes() and read by
/// post_process_heightmap().
namespace post_keys
{

inline constexpr char MIX_METHOD[] = "post_mix_method";
inline constexpr char MIX[] = "post_mix";
inline constexpr char INVERSE[] = "post_inverse";
inline constexpr char GAMMA[] = "post_gamma";
inline constexpr char GAIN[] = "post_gain";
inline constexpr char SMOOTHING_RADIUS[] = "post_smoothing_radius";
inline constexpr char REMAP[] = "post_remap";
inline constexpr char SATURATE[] = "post_saturate";

} // namespace post_keys

/** @brief The passes post_process_heightmap() can run, in the order it runs them.
 *
 * Kept apart from post_process.hpp on purpose: every node includes that one,
 * while only the pass itself and the properties panel need to agree on what an
 * effect is and whether it is switched on.
 */
enum class PostEffect : int
{
  MIX,
  INVERT,
  GAMMA,
  GAIN,
  SMOOTHING,
  REMAP,
  SATURATE,
};

struct PostEffectInfo
{
  PostEffect               effect;
  std::string              name; ///< short name, as on a button: "Gamma"
  std::string              hint; ///< one line on what the pass does
  std::vector<std::string> keys; ///< attributes the pass reads
};

/// Every effect, in application order.
const std::vector<PostEffectInfo> &post_effect_catalog();

const PostEffectInfo &post_effect_info(PostEffect effect);

/// True for an attribute that belongs to the shared post-processing pass.
bool is_post_effect_attribute(const std::string &key);

/// True when `container` carries the attributes `effect` needs.
bool has_post_effect(const meta::AttributeContainer &container, PostEffect effect);

/// True when `container` carries any post-processing effect at all.
bool has_post_effects(const meta::AttributeContainer &container);

/** @brief Whether `effect` is part of the node's output chain.
 *
 * An effect is in the chain when it changes the output, or when the user added
 * it and has not moved it off its neutral value yet. The second half is what
 * `state.active` records on the slider-driven effects: without it a freshly
 * added Gamma, still at 1, would be indistinguishable from one never added.
 *
 * Anything that alters the output always counts, flag or no flag, so a project
 * or preset written before the flag existed shows exactly what it applies.
 */
bool is_post_effect_active(const meta::AttributeContainer &container, PostEffect effect);

/** @brief Add `effect` to the chain, or take it out.
 *
 * Taking an effect out puts its attributes back on their neutral values, so
 * the pass skips it. The two ranges keep the span they had and get it back
 * when they are added again.
 */
void set_post_effect_active(meta::AttributeContainer &container,
                            PostEffect                effect,
                            bool                      active);

} // namespace hesiod
