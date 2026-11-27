/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QtGlobal>

// Shared by the start screen (ExampleSelectorDialog) and its parts (Arrival,
// StartAction, RecentRow, ExampleTile, GlideScrollArea): their lengths and
// ratios, in logical pixels. Their durations are in motion.hpp with the others.
namespace hesiod::start_screen
{

// --- the card
constexpr int   shadow = 22;       // transparent margin holding the painted shadow
constexpr int   shadow_drop = 5;   // the shadow sits this much lower than the card
constexpr int   shadow_alpha = 26; // its darkest step, out of 255
constexpr qreal card_radius = 14.0;
constexpr qreal card_width_ratio = 0.62;  // of the screen's available width
constexpr qreal card_height_ratio = 0.74; // of its available height
constexpr int   card_min_width = 880;
constexpr int   card_max_width = 1120;
constexpr int   card_min_height = 560;
constexpr int   card_max_height = 724;
constexpr int   card_screen_margin = 48; // the card never comes closer to the screen edge
constexpr int   close_inset = 10;        // the close button, from the card's corner
constexpr int   close_width = 36;
constexpr int   close_height = 30;

// --- the rail: the name, the two ways to begin, the recent projects
constexpr int   rail_width = 280;
constexpr int   rail_inset = 14; // left and right
constexpr int   rail_bottom = 16;
constexpr int   rail_head = 104;       // room for the name above the actions
constexpr int   rail_section_gap = 22; // between the actions and the recent projects
constexpr int   caption_inset = 13;    // captions line up with the rows' text
constexpr qreal brand_left = 26.0;     // the name, from the card's corner
constexpr qreal brand_top = 28.0;
constexpr qreal brand_height = 32.0;
constexpr qreal brand_tagline_gap = 40.0; // the line under the name, from its top
constexpr qreal brand_rise = 8.0;         // how far below its place the name starts
constexpr int   recent_max = 6;           // recent projects listed

// --- the rows of the rail (StartAction, RecentRow)
constexpr int   action_height = 40;
constexpr int   recent_height = 46;
constexpr qreal row_radius = 9.0;
constexpr qreal row_inset = 13.0; // text from the row's left and right
constexpr qreal row_rise = 7.0;   // how far below its place a row starts
// the plate under a row: this far from the rail tone toward the text color, at
// rest for the first action, under the pointer for the others; a press takes
// a little of it back
constexpr qreal plate_lift = 0.075;
constexpr qreal plate_lift_hover = 0.04; // added under the pointer, first action only
constexpr qreal plate_press = 0.02;
constexpr qreal focus_heat = 0.6; // keyboard focus shows as this much of the hover

// --- the pictures (ExampleTile, GlideScrollArea)
constexpr qreal tile_radius = 10.0;
constexpr int   tile_gap = 12;
constexpr int   tile_columns = 3;
constexpr int   tile_right_room = 22; // clear of the scroll bar
constexpr int   tile_bottom_air = 26; // under the last row
constexpr qreal tile_rise = 12.0;     // how far below its place a picture starts
constexpr qreal tile_zoom = 0.05;     // how much closer a picture draws under the pointer
constexpr int   tile_preview_width = 1500; // a preview is never read wider than this
constexpr qreal tile_inset = 13.0;
constexpr qreal tile_inset_hero = 18.0;
constexpr qreal tile_shade = 96.0; // the shade under the name, at most this high
constexpr qreal tile_shade_hero = 150.0;
constexpr qreal tile_arrow = 12.0; // radius of the round arrow under the pointer
constexpr qreal tile_arrow_hero = 15.0;
constexpr qreal wheel_step = 0.8; // pixels scrolled per eighth of a degree of the wheel

// --- the examples side
constexpr int examples_left = 28;
constexpr int examples_top = 26;
constexpr int examples_right = 6;
constexpr int examples_lede_gap = 18; // from the line under the heading to the pictures

} // namespace hesiod::start_screen
