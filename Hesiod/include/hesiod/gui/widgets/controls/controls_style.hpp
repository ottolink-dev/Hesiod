/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QtGlobal>

// Shared by the small controls (ColorSwatch, GripSplitterHandle, StepButton,
// ToggleSwitch): how far their hover and press states go, in logical pixels and
// ratios. Their durations are in motion.hpp with the others.
namespace hesiod::controls
{

// --- color swatch (ColorSwatch)
constexpr qreal swatch_press_inset = 0.6;   // the color chip sinks this far when pressed
constexpr qreal swatch_press_settle = 0.55; // a press takes the hover fill this far back
constexpr qreal swatch_ink_hover = 0.45; // the hex code, this far toward the text color

// --- splitter grip (GripSplitterHandle)
constexpr qreal grip_thickness = 2.0;
constexpr qreal grip_thickness_pressed = 1.0; // added while dragged
constexpr qreal grip_length = 24.0;
constexpr qreal grip_length_hover = 12.0;  // added under the pointer
constexpr qreal grip_length_pressed = 6.0; // added while dragged
constexpr qreal grip_rest_alpha = 0.55;

// --- step button (StepButton)
constexpr qreal step_arm = 4.0;          // half the length of the - and + strokes
constexpr qreal step_arm_pressed = 0.12; // the strokes shorten by this part when held
constexpr qreal step_ink_hover = 0.75;   // the strokes, this far toward the text color

// --- toggle switch (ToggleSwitch)
constexpr qreal toggle_flight_stretch = 4.0; // knob length added half-way across
constexpr qreal toggle_press_stretch = 2.4;  // knob length added under a press
constexpr qreal toggle_edge_hover = 0.45;    // off edge, toward the faint ink on hover
constexpr qreal toggle_on_hover = 0.08;      // on track, toward the knob color on hover
constexpr qreal toggle_knob_hover = 0.35;    // off knob, toward the text color on hover

} // namespace hesiod::controls
