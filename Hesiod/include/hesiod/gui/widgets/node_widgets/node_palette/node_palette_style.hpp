/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <algorithm>

#include <QtGlobal>

// Shared by the node library in the graph editor (NodePaletteSidebar) and its
// parts (CategoryRailButton, FoldButton, FoldFlight, RailScroller,
// RailEdgeFade, SearchFadeVeil): their lengths and ratios, in logical pixels.
// The user's own metrics (row height, spacing, icon size, radius) stay in
// NodePalettePalette; the durations are in motion.hpp with the others.
namespace hesiod::node_palette
{

// --- the card
constexpr int   outer = 10; // card to the editor's edges, as the tool rail keeps
constexpr int   header_min_height = 26; // the search field and the fold button
constexpr int   header_gap = 4;         // between the search field and the fold button
constexpr int   search_min_width = 60;
constexpr int   rows_min_width = 120; // the unfolded rows, however short the names
constexpr int   card_bottom_slack = 2;
constexpr qreal card_radius_extra = 4.0;  // the card's corners, beyond the rows' radius
constexpr qreal show_opacity_lead = 1.25; // the card is opaque before its slide ends
constexpr int   flight_margin = 2;        // the fold picture, around what it covers
constexpr int   search_padding = 6;       // left and right, inside the field

// --- the fold: where in the reveal (0 folded .. 1 unfolded) each part fades
constexpr qreal folded_fade_from = 0.30; // the folded picture going as the rail opens
constexpr qreal folded_fade_to = 0.90;
constexpr qreal unfolded_fade_from = 0.10; // the unfolded picture coming up
constexpr qreal unfolded_fade_to = 0.70;
constexpr qreal search_fade_from = 0.18; // the search field dissolving into the card
constexpr qreal search_fade_to = 0.80;
constexpr qreal hidden_below = 0.004; // a fade this faint is not drawn at all

// --- a category row (CategoryRailButton)
constexpr qreal glyph_inset = 0.1; // of the icon size, on each side
constexpr qreal
    glyph_rest_strength = 0.62;         // the category color at rest, of accent_strength
constexpr qreal label_fade_from = 0.18; // the name, along the reveal
constexpr qreal label_fade_to = 0.82;
constexpr qreal label_draw_width = 240.0; // drawn this wide and clipped, never re-elided
constexpr qreal chevron_room = 14.0;      // kept free for the chevron at the row's end
constexpr int   chevron_hint = 18;        // the same room, in the row's size hint
constexpr qreal chevron_arm = 4.0;
constexpr qreal chevron_travel = 3.5; // it comes in from the left as the row warms
constexpr qreal chevron_pen = 1.4;
constexpr qreal open_bar_height = 14.0; // the category color, on the open row's edge
constexpr qreal open_bar_width = 3.0;
constexpr qreal open_bar_left = 1.5;

// --- the fold button (FoldButton)
constexpr qreal fold_pen = 1.5;
constexpr qreal magnifier_radius = 4.5;
constexpr qreal magnifier_offset = 1.5; // the lens, up and left of the middle
constexpr qreal magnifier_handle_from = 1.9;
constexpr qreal magnifier_handle_to = 5.5;
constexpr qreal panel_width = 14.0; // the "sidebar" mark: a pane with a divider
constexpr qreal panel_height = 11.0;
constexpr qreal panel_radius = 2.5;
constexpr qreal panel_divider = 5.0;
constexpr qreal magnifier_fade_from = 0.12; // along the reveal
constexpr qreal magnifier_fade_to = 0.72;
constexpr qreal panel_fade_from = 0.28;
constexpr qreal panel_fade_to = 0.88;

// --- the search field's leading magnifier, drawn at 3x
constexpr int   search_icon_side = 16;
constexpr qreal search_icon_ratio = 3.0;
constexpr qreal search_icon_pen = 1.4;
constexpr qreal search_icon_center = 7.0;
constexpr qreal search_icon_radius = 4.0;
constexpr qreal search_icon_handle_from = 10.1;
constexpr qreal search_icon_handle_to = 13.2;

// --- the scrollbar (RailScroller) and the soft ends of the list (RailEdgeFade)
constexpr qreal thumb_width = 3.0;
constexpr qreal thumb_inset = 3.0; // the track stops short of the list's ends
constexpr qreal thumb_edge = 2.5;  // from the card's edge to the thumb
constexpr qreal thumb_min_length = 28.0;
constexpr qreal thumb_dragged = 0.62; // how present the thumb is ...
constexpr qreal thumb_hovered = 0.50;
constexpr qreal thumb_near = 0.32; // the pointer on the card, or just scrolled
constexpr qreal thumb_rest = 0.15;
constexpr qreal wheel_min_step = 8.0;
constexpr qreal wheel_rows = 2.0;  // one wheel notch, in rows
constexpr qreal fade_reach = 22.0; // the soft end of the list
constexpr qreal fade_ramp = 26.0;  // how far into the list the soft end builds up
constexpr qreal fade_solid = 0.96;
constexpr qreal fade_mid = 0.42;
constexpr qreal fade_mid_at = 0.45;

// 0 below `from`, 1 above `to`, an S between
inline double smoothstep(double from, double to, double x)
{
  if (to <= from)
    return x >= to ? 1.0 : 0.0;
  const double u = std::clamp((x - from) / (to - from), 0.0, 1.0);
  return u * u * (3.0 - 2.0 * u);
}

} // namespace hesiod::node_palette
