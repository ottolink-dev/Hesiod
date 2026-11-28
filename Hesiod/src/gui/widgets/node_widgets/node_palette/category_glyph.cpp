/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <cmath>
#include <map>

#include <QPainter>
#include <QPainterPath>

#include "hesiod/gui/widgets/node_widgets/node_palette/category_glyph.hpp"

namespace hesiod
{

namespace
{
constexpr double pi = 3.14159265358979323846;
} // namespace

void draw_category_glyph(QPainter     &painter,
                         const QRectF &box,
                         const QColor &color,
                         int           shape)
{
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);

  QPen pen(color);
  pen.setWidthF(std::max(1.2, box.width() * 0.09));
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  const QPointF c = box.center();
  const double  r = box.width() * 0.5;

  switch (shape)
  {
  case 0: // hexagon
  {
    QPolygonF poly;
    for (int i = 0; i < 6; ++i)
    {
      const double a = pi / 6.0 + i * pi / 3.0;
      poly << QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
    }
    painter.drawPolygon(poly);
    break;
  }
  case 1: // ring with a core
    painter.drawEllipse(c, r, r);
    painter.setBrush(color);
    painter.drawEllipse(c, r * 0.22, r * 0.22);
    break;
  case 2: // waves
  {
    QPainterPath path;
    path.moveTo(box.left(), c.y() - r * 0.35);
    path.cubicTo(box.left() + box.width() * 0.33,
                 c.y() - r * 1.1,
                 box.left() + box.width() * 0.66,
                 c.y() + r * 0.4,
                 box.right(),
                 c.y() - r * 0.35);
    painter.drawPath(path);
    painter.translate(0, r * 0.7);
    painter.drawPath(path);
    break;
  }
  case 3: // ridge / triangle
  {
    QPolygonF poly;
    poly << QPointF(box.left(), box.bottom()) << QPointF(c.x(), box.top())
         << QPointF(box.right(), box.bottom());
    painter.drawPolyline(poly);
    break;
  }
  case 4: // grid
    for (int i = 0; i <= 2; ++i)
    {
      const double t = box.left() + box.width() * i * 0.5;
      painter.drawLine(QPointF(t, box.top()), QPointF(t, box.bottom()));
      const double u = box.top() + box.height() * i * 0.5;
      painter.drawLine(QPointF(box.left(), u), QPointF(box.right(), u));
    }
    break;
  case 5: // droplet
  {
    QPainterPath path;
    path.moveTo(c.x(), box.top());
    path.quadTo(box.right(), c.y(), c.x(), box.bottom());
    path.quadTo(box.left(), c.y(), c.x(), box.top());
    painter.drawPath(path);
    break;
  }
  case 6: // scatter
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    for (const QPointF &p : {QPointF(-0.5, -0.4),
                             QPointF(0.45, -0.15),
                             QPointF(-0.15, 0.5),
                             QPointF(0.2, 0.2),
                             QPointF(-0.45, 0.1)})
      painter.drawEllipse(QPointF(c.x() + p.x() * r * 1.6, c.y() + p.y() * r * 1.6),
                          r * 0.16,
                          r * 0.16);
    break;
  case 8: // rock
  {
    QPolygonF rock;
    rock << QPointF(box.left() + box.width() * 0.15, box.bottom() - box.height() * 0.10)
         << QPointF(box.left() + box.width() * 0.05, box.top() + box.height() * 0.55)
         << QPointF(box.left() + box.width() * 0.35, box.top() + box.height() * 0.15)
         << QPointF(box.left() + box.width() * 0.75, box.top() + box.height() * 0.22)
         << QPointF(box.right() - box.width() * 0.05, box.top() + box.height() * 0.62)
         << QPointF(box.right() - box.width() * 0.18, box.bottom() - box.height() * 0.10);
    painter.drawPolygon(rock);

    // facet lines
    painter.drawLine(
        QPointF(box.left() + box.width() * 0.35, box.top() + box.height() * 0.15),
        QPointF(box.left() + box.width() * 0.48, box.top() + box.height() * 0.55));
    painter.drawLine(
        QPointF(box.left() + box.width() * 0.48, box.top() + box.height() * 0.55),
        QPointF(box.right() - box.width() * 0.05, box.top() + box.height() * 0.62));
    painter.drawLine(
        QPointF(box.left() + box.width() * 0.48, box.top() + box.height() * 0.55),
        QPointF(box.left() + box.width() * 0.32, box.bottom() - box.height() * 0.10));
    break;
  }
  case 9: // graph io (bidirectional arrows)
  {
    const double y_top = box.top() + box.height() * 0.32;
    const double y_bot = box.top() + box.height() * 0.68;
    const double x_l = box.left() + box.width() * 0.12;
    const double x_r = box.right() - box.width() * 0.12;
    const double ah = box.height() * 0.16;

    // top arrow pointing right (->)
    painter.drawLine(QPointF(x_l, y_top), QPointF(x_r, y_top));
    painter.drawLine(QPointF(x_r - ah, y_top - ah), QPointF(x_r, y_top));
    painter.drawLine(QPointF(x_r - ah, y_top + ah), QPointF(x_r, y_top));

    // bottom arrow pointing left (<-)
    painter.drawLine(QPointF(x_l, y_bot), QPointF(x_r, y_bot));
    painter.drawLine(QPointF(x_l + ah, y_bot - ah), QPointF(x_l, y_bot));
    painter.drawLine(QPointF(x_l + ah, y_bot + ah), QPointF(x_l, y_bot));
    break;
  }
  case 10: // funnel
  {
    QPolygonF funnel;
    funnel << QPointF(box.left(), box.top() + r * 0.25)
           << QPointF(box.right(), box.top() + r * 0.25)
           << QPointF(c.x() + r * 0.25, c.y() + r * 0.05)
           << QPointF(c.x() + r * 0.25, box.bottom() - r * 0.1)
           << QPointF(c.x() - r * 0.25, box.bottom() - r * 0.4)
           << QPointF(c.x() - r * 0.25, c.y() + r * 0.05);
    painter.drawPolygon(funnel);
    break;
  }
  case 11: // two arrows, there and back
  {
    const double y0 = c.y() - r * 0.42;
    const double y1 = c.y() + r * 0.42;
    const double a = r * 0.3;
    painter.drawLine(QPointF(box.left(), y0), QPointF(box.right(), y0));
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.right() - a, y0 - a) << QPointF(box.right(), y0)
                         << QPointF(box.right() - a, y0 + a));
    painter.drawLine(QPointF(box.left(), y1), QPointF(box.right(), y1));
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.left() + a, y1 - a) << QPointF(box.left(), y1)
                         << QPointF(box.left() + a, y1 + a));
    break;
  }
  case 12: // the corners of a frame
  {
    const double k = r * 0.6;
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.left(), box.top() + k) << box.topLeft()
                         << QPointF(box.left() + k, box.top()));
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.right() - k, box.top()) << box.topRight()
                         << QPointF(box.right(), box.top() + k));
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.right(), box.bottom() - k) << box.bottomRight()
                         << QPointF(box.right() - k, box.bottom()));
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.left() + k, box.bottom()) << box.bottomLeft()
                         << QPointF(box.left(), box.bottom() - k));
    break;
  }
  case 13: // three dots
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    for (int i = -1; i <= 1; ++i)
      painter.drawEllipse(QPointF(c.x() + i * r * 0.72, c.y()), r * 0.17, r * 0.17);
    break;
  case 14: // two overlapping circles
    painter.drawEllipse(QPointF(c.x() - r * 0.34, c.y()), r * 0.62, r * 0.62);
    painter.drawEllipse(QPointF(c.x() + r * 0.34, c.y()), r * 0.62, r * 0.62);
    break;
  case 15: // a range of peaks
    painter.drawPolyline(QPolygonF()
                         << QPointF(box.left(), box.bottom() - r * 0.1)
                         << QPointF(box.left() + box.width() * 0.34, box.top() + r * 0.5)
                         << QPointF(box.left() + box.width() * 0.52, c.y() + r * 0.3)
                         << QPointF(box.left() + box.width() * 0.72, box.top() + r * 0.1)
                         << QPointF(box.right(), box.bottom() - r * 0.1));
    break;
  case 16: // up and out of a tray
  {
    const double tray = c.y() + r * 0.3;
    painter.drawPolyline(QPolygonF() << QPointF(box.left(), tray) << box.bottomLeft()
                                     << box.bottomRight() << QPointF(box.right(), tray));
    painter.drawLine(QPointF(c.x(), c.y() + r * 0.35), QPointF(c.x(), box.top()));
    painter.drawPolyline(QPolygonF() << QPointF(c.x() - r * 0.42, box.top() + r * 0.42)
                                     << QPointF(c.x(), box.top())
                                     << QPointF(c.x() + r * 0.42, box.top() + r * 0.42));
    break;
  }
  case 17: // cube
  {
    QPolygonF hex;
    for (int i = 0; i < 6; ++i)
    {
      const double a = pi / 6.0 + i * pi / 3.0;
      hex << QPointF(c.x() + r * std::cos(a), c.y() + r * std::sin(a));
    }
    painter.drawPolygon(hex);
    // the three edges that meet at the near corner
    painter.drawLine(c, hex.at(1));
    painter.drawLine(c, hex.at(3));
    painter.drawLine(c, hex.at(5));
    break;
  }
  case 18: // one path forking into two
  {
    const QPointF from(box.left() + r * 0.2, c.y());
    const QPointF up(box.right() - r * 0.2, box.top() + r * 0.3);
    const QPointF down(box.right() - r * 0.2, box.bottom() - r * 0.3);
    for (const QPointF &to : {up, down})
    {
      QPainterPath path;
      path.moveTo(from);
      path.cubicTo(QPointF(c.x(), from.y()), QPointF(c.x(), to.y()), to);
      painter.drawPath(path);
    }
    painter.setBrush(color);
    for (const QPointF &dot : {from, up, down})
      painter.drawEllipse(dot, r * 0.16, r * 0.16);
    break;
  }
  case 19: // arch under a deck
  {
    const double deck = c.y() - r * 0.4;
    painter.drawLine(QPointF(box.left(), deck), QPointF(box.right(), deck));
    QPainterPath arch;
    arch.moveTo(box.left() + r * 0.2, box.bottom());
    arch.cubicTo(QPointF(box.left() + r * 0.2, deck + r * 0.15),
                 QPointF(box.right() - r * 0.2, deck + r * 0.15),
                 QPointF(box.right() - r * 0.2, box.bottom()));
    painter.drawPath(arch);
    break;
  }
  case 20: // crosshair
    painter.drawEllipse(c, r * 0.62, r * 0.62);
    painter.drawLine(QPointF(c.x(), box.top()), QPointF(c.x(), c.y() - r * 0.3));
    painter.drawLine(QPointF(c.x(), box.bottom()), QPointF(c.x(), c.y() + r * 0.3));
    painter.drawLine(QPointF(box.left(), c.y()), QPointF(c.x() - r * 0.3, c.y()));
    painter.drawLine(QPointF(box.right(), c.y()), QPointF(c.x() + r * 0.3, c.y()));
    break;
  case 21: // a circle and a square
    painter.drawEllipse(QPointF(box.left() + r * 0.62, box.top() + r * 0.62),
                        r * 0.6,
                        r * 0.6);
    painter.drawRect(QRectF(c.x() - r * 0.1, c.y() - r * 0.1, r * 1.08, r * 1.08));
    break;
  case 22: // leaf
  {
    QPainterPath path;
    path.moveTo(c.x(), box.top());
    path.quadTo(box.right(), c.y(), c.x(), box.bottom());
    path.quadTo(box.left(), c.y(), c.x(), box.top());
    painter.drawPath(path);
    painter.drawLine(QPointF(c.x(), c.y() - r * 0.2),
                     QPointF(c.x(), box.bottom() + r * 0.3));
    break;
  }
  case 23: // plus
    painter.drawLine(QPointF(c.x(), box.top() + r * 0.15),
                     QPointF(c.x(), box.bottom() - r * 0.15));
    painter.drawLine(QPointF(box.left() + r * 0.15, c.y()),
                     QPointF(box.right() - r * 0.15, c.y()));
    break;
  default: // node / connector
    painter.drawLine(QPointF(box.left(), c.y()), QPointF(box.right(), c.y()));
    painter.drawEllipse(QPointF(box.left() + r * 0.3, c.y()), r * 0.28, r * 0.28);
    painter.drawEllipse(QPointF(box.right() - r * 0.3, c.y()), r * 0.28, r * 0.28);
    break;
  }

  painter.restore();
}

int category_glyph_shape(const QString &category)
{
  // The categories Hesiod ships each have a glyph of their own: hashed onto a
  // handful of shapes, several came out as the same connector or the same
  // ring, and folded to icons alone the rail could not be read at all.
  static const std::map<QString, int> known = {{"Boundaries", 12},
                                               {"Bridges", 19},
                                               {"Converter", 11},
                                               {"Debug", 20},
                                               {"Erosion", 6},
                                               {"Export", 16},
                                               {"Filter", 10},
                                               {"Flora", 22},
                                               {"Geometry", 17},
                                               {"Graph IO", 9},
                                               {"Hydrology", 2},
                                               {"Math", 23},
                                               {"Operator", 14},
                                               {"Primitive", 21},
                                               {"Rock", 8},
                                               {"Routing", 18},
                                               {"Terrain Features", 15},
                                               {"Texture", 4},
                                               {"WIP", 13}};

  if (const auto it = known.find(category); it != known.end())
    return it->second;

  unsigned int hash = 2166136261u;
  for (const QChar c : category)
    hash = (hash ^ static_cast<unsigned int>(c.unicode())) * 16777619u;
  return static_cast<int>(hash % 8u);
}

} // namespace hesiod
