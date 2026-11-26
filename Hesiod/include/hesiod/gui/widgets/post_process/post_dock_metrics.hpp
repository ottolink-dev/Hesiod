/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once

/// Sizes the pieces of the post-process dock share, in logical pixels.
namespace hesiod::post_dock
{

inline constexpr int SCOPE_SIZE = 22;  ///< the display at the head of a stage
inline constexpr int GUTTER = 32;      ///< the scope and the gap to the stage's rows
inline constexpr int STAGE_PAD = 2;    ///< air above and below a stage
inline constexpr int CHIP_SIZE = 24;   ///< a tray toggle
inline constexpr int CHIP_GAP = 2;     ///< between toggles of one family
inline constexpr int GROUP_GAP = 11;   ///< between families, a divider sits in it
inline constexpr int TRAY_HEIGHT = 34; ///< the tray, on one line

/// Dynamic property a stage row sets to say where its first line is centred.
inline constexpr char HEAD_CENTRE_PROPERTY[] = "stage_head_centre";

} // namespace hesiod::post_dock
