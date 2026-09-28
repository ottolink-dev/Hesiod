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

namespace meta::qt
{
struct Theme;
}

// Shared by the viewport toolbar's pieces (rail, panels, snap guide): the
// tools, the rail's edges and metrics, colours and icons.
namespace hesiod::viewport
{

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

enum class Edge : int
{
  Left,
  Right,
  Top,
  Bottom
};

inline bool is_vertical(Edge e) { return e == Edge::Left || e == Edge::Right; }

// --- rail metrics (logical px)
constexpr int kCell = 34;       // one tool button
constexpr int kPad = 4;         // rail end padding
constexpr int kThick = 42;      // rail thickness
constexpr int kSquare = 38;     // folded rail while dragging
constexpr int kSep = 9;         // separator slot
constexpr int kMargin = 10;     // distance from the viewport edges
constexpr int kMagnet = 28;     // snap to a corner within this distance
constexpr int kCenterSnap = 32; // drop this close to an edge's middle to centre on it
constexpr int kRadius = 12;     // rail corners; cells use kRadius minus their inset

constexpr double kPi = std::numbers::pi;

QColor surface_color();
QColor border_color();
bool   animations_on();
int    anim_ms(int ms); // 0 when UI animations are off

// The rail's size follows the "Viewport toolbar size" setting (percent); the
// metrics above are its 100 % values. Rounded to whole pixels so the cells
// and hairlines stay crisp.
qreal rail_scale();
qreal scaled(int base);
qreal cell_px();
qreal pad_px();
qreal thick_px();
qreal square_px();
qreal sep_px();
qreal radius_px();

// icons (data/icons/vp_*.svg, settings, settings_backup_restore), in `ink`
void paint_icon(QPainter &p, int tool, const QRectF &r, const QColor &ink);
void paint_gear(QPainter &p, const QPointF &c, qreal size, const QColor &ink);
void paint_reset(QPainter &p, const QPointF &c, qreal size, const QColor &ink);

QString                tool_title(int tool);
const meta::qt::Theme &viewport_theme(); // per-section accents for the panels
std::string            resolution_label(int res);

} // namespace hesiod::viewport
