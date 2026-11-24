/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QColor>
#include <QPainter>

// Shared by the pieces of a floating toolbar (ToolRail, SnapGuide), as used by
// the 3D view and the node editor: the rail's edges and metrics, colours and
// icons.
namespace hesiod::rail
{

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
constexpr int kMargin = 10;     // distance from the parent's edges
constexpr int kMagnet = 28;     // snap to a corner within this distance
constexpr int kCenterSnap = 32; // drop this close to an edge's middle to centre on it
constexpr int kRadius = 12;     // rail corners; cells use kRadius minus their inset

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

// A data/icons SVG, `size` px square centred on `c`, recoloured to `ink`
// (alpha included). Icons with colours of their own are drawn as they are
// (tint false), only faded by the ink's alpha.
void paint_svg(QPainter      &p,
               const char    *name,
               const QPointF &c,
               qreal          size,
               const QColor  &ink,
               bool           tint = true);
void paint_gear(QPainter &p, const QPointF &c, qreal size, const QColor &ink);

} // namespace hesiod::rail
