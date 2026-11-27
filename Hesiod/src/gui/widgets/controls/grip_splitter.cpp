/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/controls/grip_splitter.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/controls/controls_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

#include <algorithm>

namespace hesiod
{

void GripSplitterHandle::sync_motion()
{
  const bool active = this->hovered || this->pressed;
  this->live.to(active ? 1.0 : 0.0,
                active ? motion::hover_in : motion::hover_out,
                active ? motion::enter() : motion::leave());
  this->press_t.to(this->pressed ? 1.0 : 0.0,
                   this->pressed ? motion::press_in : motion::press_out,
                   motion::enter());
}

bool GripSplitterHandle::event(QEvent *event)
{
  switch (event->type())
  {
  case QEvent::HoverEnter:
    this->hovered = true;
    this->sync_motion();
    break;
  case QEvent::HoverLeave:
    this->hovered = false;
    this->sync_motion();
    break;
  case QEvent::MouseButtonPress:
    this->pressed = true;
    this->sync_motion();
    break;
  case QEvent::MouseButtonRelease:
    this->pressed = false;
    this->sync_motion();
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

  const qreal t = std::clamp(this->live.value(), 0.0, 1.0);
  const qreal p = std::clamp(this->press_t.value(), 0.0, 1.0);

  // short rounded pill across the middle of the gap
  const qreal thickness = controls::grip_thickness + controls::grip_thickness_pressed * p;
  const qreal length = controls::grip_length + controls::grip_length_hover * t +
                       controls::grip_length_pressed * p;
  QRectF pill;
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

  // gray at rest and in use: the grip brightens, it does not take a color
  QColor rest = colors.border;
  rest.setAlphaF(float(controls::grip_rest_alpha));
  const QColor color = mix_colors(rest, colors.text_secondary, t);

  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawRoundedRect(pill, thickness / 2.0, thickness / 2.0);
}

} // namespace hesiod
