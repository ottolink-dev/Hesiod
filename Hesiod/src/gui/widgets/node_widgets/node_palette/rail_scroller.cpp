/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>

#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/rail_edge_fade.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/rail_scroller.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace np = node_palette;

RailScroller::RailScroller(QScrollArea *area, QWidget *sidebar)
    : QWidget(sidebar), area(area), strength(this, 0.0), glide(0.0)
{
  this->setAttribute(Qt::WA_NoSystemBackground, true);
  this->setMouseTracking(true);
  this->fade = new RailEdgeFade(sidebar);

  if (!this->area)
    return;

  QScrollBar *bar = this->area->verticalScrollBar();

  this->glide.on_tick = [this]()
  {
    if (!this->area)
      return;
    this->gliding = true;
    this->area->verticalScrollBar()->setValue(int(std::lround(this->glide.value())));
    this->gliding = false;
  };

  this->connect(bar, &QScrollBar::rangeChanged, this, [this](int, int) { this->sync(); });
  this->connect(bar,
                &QScrollBar::valueChanged,
                this,
                [this](int value)
                {
                  if (!this->gliding && !this->glide.running())
                    this->glide.set(value);
                  this->refresh();
                });

  this->linger.setSingleShot(true);
  this->linger.setInterval(motion::library_scroll_linger);
  this->connect(&this->linger, &QTimer::timeout, this, [this]() { this->retarget(); });

  this->area->viewport()->installEventFilter(this);
  this->area->installEventFilter(this);
  if (sidebar)
    sidebar->installEventFilter(this);

  this->sync();
}

void RailScroller::enterEvent(QEnterEvent *)
{
  this->pointer_over = true;
  this->retarget();
}

bool RailScroller::eventFilter(QObject *watched, QEvent *event)
{
  if (!this->area)
    return QWidget::eventFilter(watched, event);

  switch (event->type())
  {
  case QEvent::Wheel:
    if (watched == this->area->viewport() &&
        this->wheel(static_cast<QWheelEvent *>(event)))
      return true;
    break;
  case QEvent::Resize:
  case QEvent::Move:
    if (watched == this->area || watched == this->parentWidget())
      this->sync();
    break;
  case QEvent::Enter:
    if (watched == this->parentWidget())
    {
      this->pointer_near = true;
      this->retarget();
    }
    break;
  case QEvent::Leave:
    if (watched == this->parentWidget())
    {
      this->pointer_near = false;
      this->retarget();
    }
    break;
  default:
    break;
  }
  return QWidget::eventFilter(watched, event);
}

void RailScroller::glide_to(double value, int duration_ms)
{
  if (!this->area)
    return;

  QScrollBar  *bar = this->area->verticalScrollBar();
  const double target = std::clamp<double>(value, bar->minimum(), bar->maximum());
  if (motion::enabled())
    this->glide.to(target, duration_ms, motion::enter());
  else
  {
    this->glide.set(target);
    bar->setValue(int(std::lround(target)));
  }
}

void RailScroller::leaveEvent(QEvent *)
{
  this->pointer_over = false;
  this->retarget();
}

void RailScroller::mouseMoveEvent(QMouseEvent *event)
{
  if (!this->dragging || !this->area)
    return;

  QScrollBar  *bar = this->area->verticalScrollBar();
  const QRectF thumb = this->thumb_rect();
  const double travel = this->track_length() - thumb.height();
  if (travel <= 0.0)
    return;

  const double at = std::clamp(
      (event->position().y() - this->grab_offset - np::thumb_inset) / travel,
      0.0,
      1.0);
  const double value = bar->minimum() + at * (bar->maximum() - bar->minimum());
  this->glide.set(value);
  bar->setValue(int(std::lround(value)));
  event->accept();
}

void RailScroller::mousePressEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton || !this->area)
    return;

  QScrollBar  *bar = this->area->verticalScrollBar();
  const QRectF thumb = this->thumb_rect();
  const double y = event->position().y();

  if (y >= thumb.top() && y <= thumb.bottom())
  {
    this->dragging = true;
    this->grab_offset = y - thumb.top();
  }
  else
  {
    // beside the thumb: a page towards the pointer
    const double page = bar->pageStep() * (y < thumb.top() ? -1.0 : 1.0);
    this->glide_to(this->glide.target() + page, motion::library_page_glide);
  }
  this->retarget();
  event->accept();
}

void RailScroller::mouseReleaseEvent(QMouseEvent *event)
{
  this->dragging = false;
  this->retarget();
  event->accept();
}

void RailScroller::paintEvent(QPaintEvent *)
{
  const double a = std::clamp(this->strength.value(), 0.0, 1.0);
  if (a <= np::hidden_below || !this->area)
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(with_alpha(this->ink, a));
  painter.drawRoundedRect(this->thumb_rect(),
                          np::thumb_width * 0.5,
                          np::thumb_width * 0.5);
}

void RailScroller::refresh()
{
  if (!this->area || !this->fade)
    return;

  const QScrollBar *bar = this->area->verticalScrollBar();
  this->fade->set_state(
      this->ground,
      std::clamp((bar->value() - bar->minimum()) / np::fade_ramp, 0.0, 1.0),
      std::clamp((bar->maximum() - bar->value()) / np::fade_ramp, 0.0, 1.0));
  this->update();
}

void RailScroller::retarget()
{
  const double target = this->dragging       ? np::thumb_dragged
                        : this->pointer_over ? np::thumb_hovered
                        : (this->pointer_near || this->linger.isActive())
                            ? np::thumb_near
                            : np::thumb_rest;
  if (std::abs(target - this->strength.target()) < 0.001)
    return;

  const bool up = target > this->strength.value();
  this->strength.to(target,
                    up ? motion::hover_in : motion::hover_out,
                    up ? motion::enter() : motion::leave());
}

void RailScroller::set_colors(const QColor &new_ground, const QColor &new_ink)
{
  this->ground = new_ground;
  this->ink = new_ink;
  this->refresh();
}

void RailScroller::set_step(double px) { this->step = std::max(np::wheel_min_step, px); }

void RailScroller::sync()
{
  QWidget *host = this->parentWidget();
  if (!this->area || !host || !this->fade)
    return;

  QScrollBar *bar = this->area->verticalScrollBar();
  const bool  needed = bar->maximum() > bar->minimum();
  const QRect list = this->area->geometry();

  this->setGeometry(list.right() + 1,
                    list.top(),
                    std::max(0, host->width() - list.right() - 1),
                    list.height());
  this->fade->setGeometry(list);
  this->setVisible(needed);
  this->fade->setVisible(needed);
  if (needed)
  {
    this->fade->raise();
    this->raise();
  }
  this->refresh();
  this->retarget();
}

QRectF RailScroller::thumb_rect() const
{
  if (!this->area)
    return QRectF();

  const QScrollBar *bar = this->area->verticalScrollBar();
  const double      range = bar->maximum() - bar->minimum();
  const double      track = this->track_length();
  const double shown = range > 0.0 ? bar->pageStep() / (range + bar->pageStep()) : 1.0;
  const double length = std::clamp(track * shown,
                                   std::min(np::thumb_min_length, track),
                                   track);
  const double at = range > 0.0 ? (bar->value() - bar->minimum()) / range : 0.0;

  // close to the card's edge, the far side of the padding from the icons
  return QRectF(this->width() - np::thumb_edge - np::thumb_width,
                np::thumb_inset + (track - length) * at,
                np::thumb_width,
                length);
}

double RailScroller::track_length() const
{
  return std::max(0.0, this->height() - 2.0 * np::thumb_inset);
}

bool RailScroller::wheel(QWheelEvent *event)
{
  QScrollBar *bar = this->area->verticalScrollBar();
  if (bar->maximum() <= bar->minimum())
    return false;

  const double delta = !event->pixelDelta().isNull()
                           ? -event->pixelDelta().y()
                           : -(event->angleDelta().y() / 120.0) * this->step;
  this->glide_to(this->glide.target() + delta, motion::library_scroll_glide);
  this->linger.start();
  this->retarget();
  event->accept();
  return true;
}

} // namespace hesiod
