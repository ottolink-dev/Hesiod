/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#include <QColor>
#include <QPainter>
#include <QString>

#include "hesiod/gui/widgets/rail/rail_style.hpp"

namespace meta::qt
{
struct Theme;
}

// Shared by the viewport toolbar's pieces (rail, panels): the tools, colours
// and icons. The rail's edges and metrics are the generic ones of rail_style.
namespace hesiod::viewport
{

using namespace hesiod::rail;

// --- tools (rail entries); values are persisted nowhere, order is free
enum Tool : int
{
  ToolView2D, // the 2D / 3D switch: two cells sharing one sliding highlight
  ToolView3D,
  ToolResolution,
  ToolLighting,
  ToolCamera,
  ToolDisplay,
  ToolMaterial,
  ToolWater,
  ToolSky,
  ToolLighting2D,
  ToolColormap2D,
  ToolPreview, // which node outputs the view shows (hosts the viewer's own rows)
  ToolMore
};

constexpr double kPi = std::numbers::pi;

// icons (data/icons/vp_*.svg, settings_backup_restore), in `ink`
void paint_icon(QPainter &p, int tool, const QRectF &r, const QColor &ink);
void paint_reset(QPainter &p, const QPointF &c, qreal size, const QColor &ink);

QString                tool_title(int tool);
const meta::qt::Theme &viewport_theme(); // per-section accents for the panels
std::string            resolution_label(int res);

} // namespace hesiod::viewport
