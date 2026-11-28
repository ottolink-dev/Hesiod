/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QColor>
#include <QPixmap>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

/** @brief The node library folding or unfolding, as one picture.
 *
 * Folding the real widgets frame by frame means a new layout of the rail, a
 * repaint of every row, a repaint of the graph underneath and a move of the
 * toolbar beside it, sixty times a second, with an edge that can only stand
 * on whole pixels and glyphs that slide while it moves. So the fold is not
 * done live.
 *
 * The sidebar and the toolbar are put in their final state at once, under
 * this widget, which holds a picture of what was there before, one of what is
 * there now and one of the graph behind them, and draws the way from one to
 * the other. A frame of it is three pictures and a rounded rectangle, the edge
 * is placed in fractions of a pixel, and nothing inside it moves but the edge:
 * the glyphs stand still, the names fade as the edge passes over them. It
 * deletes itself when it has landed.
 */
class FoldFlight : public QWidget
{
  Q_OBJECT

public:
  struct Shot
  {
    QPixmap content;  // the sidebar's rows and header, no card behind them
    QPointF origin;   // where that picture sits, in this widget
    QRectF  card;     // the card, in this widget
    QPointF rail_pos; // the companion's top-left, in this widget
    double  reveal = 0.0;
  };

  struct Look
  {
    QColor card_bg;
    QColor seam;
    double radius = 0.0;
  };

  FoldFlight(QWidget              *host,
             const QRect          &bounds,
             const QPixmap        &backdrop,
             const Shot           &from,
             const Shot           &to,
             const QPixmap        &rail,
             const Look           &look,
             std::function<void()> on_landed);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QPixmap               backdrop;
  Shot                  from;
  Shot                  to;
  QPixmap               rail;
  Look                  look;
  std::function<void()> on_landed;
  Anim                  progress;
};

} // namespace hesiod
