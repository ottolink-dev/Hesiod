/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QtGlobal>

// Shared by the node editor's own pieces (GraphTabStrip, BuildProgress): how
// far their hover and press states go, in logical pixels and ratios. Their
// durations are in motion.hpp with the others.
namespace hesiod::graph_style
{

// --- tab strip (GraphTabStrip)
constexpr qreal tab_press_sink = 0.6;   // a held resting tab sinks this far
constexpr qreal tab_press_tone = 0.08;  // and its fill brightens by this much
constexpr qreal plus_press_inset = 0.7; // the held "+" shrinks this much on each side
constexpr qreal plus_press_tone = 0.1;  // and its plate brightens by this much

// --- build progress (BuildProgress)
constexpr int   build_rise = 8; // the pill comes up from this far below its place
constexpr qreal build_hover_tint = 0.12; // a held build's pill, toward the warning color
constexpr qreal build_edge_hover = 0.22; // its edge, this much more opaque on hover
constexpr qreal build_ink_hover = 0.14;  // its text, toward the text color on hover

} // namespace hesiod::graph_style
