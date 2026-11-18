/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once

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

} // namespace hesiod
