/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QPainter>
#include <QTimer>
#include <QWidget>

#include "hesiod/gui/widgets/dialogs/start_screen/arrival.hpp"

namespace hesiod
{

Arrival::Arrival(QWidget *owner) : widget(owner), amount(owner, 0.0) {}

void Arrival::arrive(int delay_ms)
{
  if (!motion::enabled() || !this->widget)
  {
    this->amount.set(1.0);
    if (this->widget)
      this->widget->update();
    return;
  }

  // the timer is tied to the widget, so it does not fire once the widget is gone
  QTimer::singleShot(delay_ms,
                     this->widget,
                     [this]()
                     { this->amount.to(1.0, motion::start_part_in, motion::enter()); });
}

bool Arrival::apply(QPainter &p, qreal rise) const
{
  const qreal a = std::clamp(this->amount.value(), 0.0, 1.0);
  if (a <= 0.001)
    return false;
  p.setOpacity(a);
  p.translate(0.0, rise * (1.0 - a));
  return true;
}

} // namespace hesiod
