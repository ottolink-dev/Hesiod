/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QtGlobal>

// Shared by the shell's transient pieces (HsdMenu, PopupEntrance, StatusLine):
// their lengths and ratios, in logical pixels. Their durations are in
// motion.hpp with the others.
namespace hesiod::shell
{

// --- menus (HsdMenu)
constexpr qreal menu_plate_radius = 7.0;  // QMenu::item's border-radius in darkstyle.css
constexpr int   menu_shortcut_inset = 18; // a shortcut's right end, from the row's
constexpr qreal menu_shortcut_scale = 0.86; // a shortcut's font size, from the row's
// a shortcut's ink: this far from the pane color toward the text color, and
// further on the highlighted row
constexpr qreal menu_shortcut_ink = 0.55;
constexpr qreal menu_shortcut_ink_active = 0.80;

// --- popup entrance (PopupEntrance)
constexpr int popup_rise = 12;  // a dialog comes up from this far below its place
constexpr int popup_unfold = 6; // a menu or a list comes this far out of what opened it
// the fade runs ahead of the travel: the window is solid at 1 / 1.35 of the way
constexpr qreal popup_fade_lead = 1.35;
// a menu whose bottom is above the pointer plus this much opened upward
constexpr int popup_pointer_margin = 14;

// --- status bar message (StatusLine)
constexpr int   status_inset = 8;  // the left inset a status bar message has always had
constexpr qreal status_rise = 4.0; // how far below its place a message starts
constexpr int   status_width_hint = 240;   // preferred width in the status bar
constexpr int   status_min_width = 40;     // narrowest it gets
constexpr int   status_height_padding = 8; // added to the font height

} // namespace hesiod::shell
