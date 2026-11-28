/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QColor>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"

class QScrollArea;

namespace hesiod
{

class RailEdgeFade;

/** @brief The node library's scrollbar, kept out of the rows' way.
 *
 * A stock scrollbar takes its width out of the list, and folded the list is
 * one icon wide: the bar ended up against the icons, in the space they are
 * centered in. This one lives in the card's own edge padding, so the rows keep
 * their full width and the icons stay where they are whether or not the list
 * scrolls. It is quiet at rest, comes forward under the pointer, and the list
 * fades out at whichever end has more rows to come (RailEdgeFade). The wheel
 * glides; with interface animations off it steps.
 */
class RailScroller : public QWidget
{
  Q_OBJECT

public:
  /// @param area the list; its own vertical bar should be switched off
  /// @param sidebar the card the bar is drawn in, beside @p area
  RailScroller(QScrollArea *area, QWidget *sidebar);

  void set_colors(const QColor &ground, const QColor &ink);

  /// One wheel notch, in pixels.
  void set_step(double px);

  /// Follow the list's geometry and range (also called on resize and move).
  void sync();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  double track_length() const;
  QRectF thumb_rect() const;
  bool   wheel(QWheelEvent *event);
  void   glide_to(double value, int duration_ms);
  void   retarget();
  void   refresh();

  QPointer<QScrollArea> area;
  RailEdgeFade         *fade = nullptr;
  QColor                ground;
  QColor                ink;
  Anim                  strength; // how present the thumb is
  Anim                  glide;    // where the list is on its way to
  QTimer                linger;   // the thumb stays forward a moment after scrolling
  double                step = 0.0;
  double                grab_offset = 0.0;
  bool                  gliding = false;
  bool                  dragging = false;
  bool                  pointer_over = false;
  bool                  pointer_near = false;
};

} // namespace hesiod
