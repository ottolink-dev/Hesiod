/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <algorithm>

#include <QtGlobal>
#include <qnamespace.h>

// Shared by the node library in the graph editor (NodePaletteSidebar) and its
// parts (CategoryRailButton, FoldButton, FoldFlight, RailScroller,
// RailEdgeFade, SearchFadeVeil, PaletteFlyoutCard): their lengths and ratios, in logical
// pixels. The user's own metrics (row height, spacing, icon size, radius) stay in
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

// --- a category's card (PaletteFlyoutCard); its rows are the palette's
// flyout_row_height and flyout_padding
constexpr int   flyout_header_height = 33; // the category's name above its rows
constexpr int   flyout_max_list_height = 330;
constexpr int   flyout_gap = 6;         // from the rail's card, or the parent card
constexpr int   flyout_min_width = 200; // the card's width follows its longest row
constexpr int   flyout_max_width = 296;
constexpr int   flyout_title_room = 46;  // beside the title: its dot and the margins
constexpr int   flyout_leaf_room = 34;   // beside a node's name: margins
constexpr int   flyout_branch_room = 58; // beside a sub-list's name: count and chevron
constexpr int   flyout_icon = 16;        // the glyph ahead of a row's name
constexpr int   flyout_icon_left = 9;    // from the row's edge
constexpr int   flyout_icon_gap = 6;     // between the glyph and the name, in the width
constexpr qreal flyout_text_gap = 9.0;   // the same, where the name is drawn
constexpr qreal flyout_text_min = 20.0;
constexpr int   flyout_separator = 9;     // the room a separator takes
constexpr qreal flyout_rule_inset = 14.0; // a separator, from the card's sides
constexpr qreal flyout_title_rule_inset = 10.0;
constexpr qreal flyout_title_font = 0.90; // of the interface font
constexpr qreal flyout_count_font = 0.84; // a sub-list's node count
constexpr qreal flyout_dot = 3.2;         // the category color, ahead of the title
constexpr qreal flyout_dot_left = 16.0;
constexpr qreal flyout_title_left = 27.0;
constexpr qreal flyout_title_right = 14.0;
constexpr qreal flyout_chevron_right = 10.0; // from the row's end
constexpr qreal flyout_chevron_arm = 3.5;
constexpr qreal flyout_chevron_pen = 1.35;
constexpr qreal flyout_chevron_room = 12.0;
constexpr qreal flyout_count_gap = 4.0;
constexpr qreal flyout_count_room = 10.0;
constexpr qreal flyout_plate_radius = 7.0;  // concentric with the card's corners
constexpr qreal flyout_plate_inset = 1.0;   // the highlight, above and below its row
constexpr qreal flyout_hit_margin = 4.0;    // around the card, still "on" it
constexpr qreal flyout_travel = 8.0;        // the card comes in from the rail's side
constexpr qreal flyout_opacity_lead = 1.45; // opaque before its travel ends
constexpr qreal flyout_rest_ink = 0.62;     // a row's glyph at rest, of accent_strength
constexpr qreal flyout_count_ink = 0.45;    // the node count, of the text color
constexpr qreal flyout_rule_ink = 0.65;     // a separator, of the seam
constexpr qreal flyout_title_rule_ink = 0.75;
constexpr qreal flyout_swap_from = 0.25; // the rows, at the start of a category swap
constexpr qreal flyout_shown = 0.25;     // past this the card counts as open (a glide)
constexpr qreal flyout_dot_ink = 0.92;
constexpr qreal flyout_no_icon_dot = 2.0; // a row without a glyph keeps its column
// the soft shadow under the card: a few rounded layers, black at low alpha
constexpr int   flyout_shadow = 14;     // margin around the card for it
constexpr int   flyout_shadow_rise = 2; // the card sits this much above center
constexpr int   flyout_shadow_layers = 4;
constexpr qreal flyout_shadow_spread = 2.2; // per layer
constexpr qreal flyout_shadow_drop = 1.25;  // per layer
constexpr qreal flyout_shadow_alpha = 20.0; // the innermost layer, of 255
constexpr qreal flyout_shadow_fade = 3.5;   // less per layer
constexpr qreal flyout_shadow_round = 0.45; // corner growth, of the spread
// the card's own scrollbar
constexpr qreal flyout_thumb_width = 3.0;
constexpr qreal flyout_thumb_edge = 5.5; // from the card's edge
constexpr qreal flyout_thumb_inset = 5.0;
constexpr qreal flyout_thumb_min = 24.0;
constexpr qreal flyout_thumb_ink = 0.28;
constexpr qreal flyout_thumb_hovered = 0.65;
constexpr qreal flyout_wheel_rows = 2.5; // one wheel notch, in rows

// --- the search results
constexpr int search_popup_min_width = 340; // a name and its category, uncut
constexpr int search_max_rows = 14;
constexpr int search_result_icon = 16;
// what a result's row knows besides its text
constexpr int search_type_role = Qt::UserRole + 1;     // node type
constexpr int search_category_role = Qt::UserRole + 2; // top-level category

// 0 below `from`, 1 above `to`, an S between
inline double smoothstep(double from, double to, double x)
{
  if (to <= from)
    return x >= to ? 1.0 : 0.0;
  const double u = std::clamp((x - from) / (to - from), 0.0, 1.0);
  return u * u * (3.0 - 2.0 * u);
}

} // namespace hesiod::node_palette
