/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QScrollBar>
#include <QWheelEvent>

#include "hesiod/gui/widgets/dialogs/start_screen/glide_scroll_area.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_screen_style.hpp"

namespace hesiod
{

GlideScrollArea::GlideScrollArea(QWidget *parent) : QScrollArea(parent)
{
  this->position.on_tick = [this]()
  {
    if (QScrollBar *bar = this->verticalScrollBar())
      bar->setValue(qRound(this->position.value()));
  };

  // dragging the bar takes over from a glide still under way
  QObject::connect(this->verticalScrollBar(),
                   &QScrollBar::sliderPressed,
                   this,
                   [this]() { this->position.set(this->verticalScrollBar()->value()); });
}

void GlideScrollArea::wheelEvent(QWheelEvent *event)
{
  // a touchpad already scrolls by the pixel
  QScrollBar *bar = this->verticalScrollBar();
  if (!bar || !event->pixelDelta().isNull() || event->angleDelta().y() == 0 ||
      !motion::enabled())
  {
    QScrollArea::wheelEvent(event);
    return;
  }

  const double from = this->position.running() ? this->position.target() : bar->value();
  const double target = std::clamp<double>(from - event->angleDelta().y() *
                                                      start_screen::wheel_step,
                                           bar->minimum(),
                                           bar->maximum());
  this->position.set(bar->value());
  this->position.to(target, motion::start_scroll_glide, motion::enter());
  event->accept();
}

} // namespace hesiod
