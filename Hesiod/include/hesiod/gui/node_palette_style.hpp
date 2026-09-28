/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QColor>

namespace hesiod
{

/** @brief Colours, spacing and motion for the node palette sidebar.
 *
 * A plain value type on purpose: the widget never reads application settings,
 * it is handed a style. That keeps it testable without an application context
 * and makes a live restyle a single assignment rather than a rebuild. It also
 * lets AppSettings hold one of these without depending on any widget.
 *
 * Every metric is a *logical* pixel count at 100% interface scale. Nothing here
 * multiplies them: Qt's high-DPI scaling (see ui_scale.hpp) does that, so
 * changing the interface scale twice cannot compound into these numbers.
 */
struct NodePaletteStyle
{
  // --- surfaces
  QColor surface{"#242424"};          ///< resting category button
  QColor surface_hover{"#303030"};    ///< hovered category button
  QColor surface_selected{"#3b3b3b"}; ///< category whose flyout is open
  QColor flyout_bg{"#1e1e1e"};
  QColor flyout_border{"#3a3a3a"};
  QColor text{"#b9b9b9"};
  QColor text_active{"#f2f2f2"};

  // --- metrics, logical px
  int button_height = 40;
  int button_spacing = 6;
  int rail_padding = 8;
  int icon_size = 22;
  int corner_radius = 8;
  int flyout_row_height = 28;
  int flyout_padding = 6;

  // --- motion
  bool animations = true;
  int  animation_ms = 120;

  /// 0 keeps the category glyph neutral, 1 paints it in the category colour.
  double accent_strength = 1.0;
};

} // namespace hesiod
