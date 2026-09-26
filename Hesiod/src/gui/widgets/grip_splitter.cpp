/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/grip_splitter.hpp"
#include "hesiod/app/hesiod_application.hpp"

namespace hesiod
{

bool GripSplitterHandle::event(QEvent *event)
{
  switch (event->type())
  {
  case QEvent::HoverEnter:
  case QEvent::HoverLeave:
    // hover only changes the look (paintEvent reads underMouse()); it must
    // not count as a press, or the grip stays in its dragging state
    this->update();
    break;
  case QEvent::MouseButtonPress:
    this->pressed = true;
    this->update();
    break;
  case QEvent::MouseButtonRelease:
    this->pressed = false;
    this->update();
    break;
  default:
    break;
  }
  return QSplitterHandle::event(event);
}

void GripSplitterHandle::paintEvent(QPaintEvent *)
{
  const auto &colors = HSD_CTX.app_settings.colors;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), colors.bg_deep); // the gap between cards

  const bool active = this->underMouse() || this->pressed;

  // short rounded pill across the middle of the gap
  const qreal thickness = 2.0;
  const qreal length = active ? 36.0 : 24.0;
  QRectF      pill;
  if (orientation() == Qt::Horizontal)
    pill = QRectF((width() - thickness) / 2.0,
                  (height() - length) / 2.0,
                  thickness,
                  length);
  else
    pill = QRectF((width() - length) / 2.0,
                  (height() - thickness) / 2.0,
                  length,
                  thickness);

  QColor color = active ? colors.accent : colors.border;
  if (!active)
    color.setAlphaF(0.55);

  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawRoundedRect(pill, thickness / 2.0, thickness / 2.0);
}

} // namespace hesiod
