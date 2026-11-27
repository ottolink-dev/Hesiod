/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/controls/controls_style.hpp"
#include "hesiod/gui/widgets/controls/toggle_switch.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent), travel(this, 0.0), hover_t(this, 0.0), press_t(this, 0.0)
{
  this->setCheckable(true);
  this->setCursor(Qt::PointingHandCursor);
  this->setFixedSize(38, 22);

  QObject::connect(this,
                   &QAbstractButton::toggled,
                   this,
                   [this](bool checked)
                   {
                     const double target = checked ? 1.0 : 0.0;
                     if (!motion::enabled() || !this->isVisible())
                       this->travel.set(target);
                     else
                       this->travel.to(target, motion::toggle, motion::spring());
                   });
}

void ToggleSwitch::sync() { this->travel.set(this->isChecked() ? 1.0 : 0.0); }

void ToggleSwitch::paintEvent(QPaintEvent *)
{
  const ControlTones t = control_tones();
  QPainter           p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF track = QRectF(this->rect()).adjusted(1, 1, -1, -1);
  const qreal  r = track.height() / 2.0;

  // the spring carries the knob a little past its seat; colors stop at the seat
  const qreal position = this->travel.value();
  const qreal k = std::clamp(position, 0.0, 1.0);
  const qreal h = std::clamp(this->hover_t.value(), 0.0, 1.0);
  const qreal press = std::clamp(this->press_t.value(), 0.0, 1.0);

  const QColor off = mix_colors(t.field, t.field_hover, h);
  const QColor off_edge = mix_colors(t.border,
                                     t.ink_faint,
                                     h * controls::toggle_edge_hover);
  const QColor on_fill = mix_colors(t.accent,
                                    on_accent_color(),
                                    h * controls::toggle_on_hover);
  const QColor edge = mix_colors(off_edge, on_fill, k);
  p.setPen(QPen(edge, 1));
  p.setBrush(mix_colors(off, on_fill, k));
  p.drawRoundedRect(track, r, r);

  // the knob lengthens a little under a press and in flight, round again at rest
  const qreal d = track.height() - 6.0;
  const qreal flight_stretch = controls::toggle_flight_stretch *
                               (1.0 - std::abs(2.0 * k - 1.0));
  const qreal stretch = std::max(flight_stretch, controls::toggle_press_stretch * press);
  const qreal x = track.left() + 3.0 + (track.width() - 6.0 - d) * position -
                  stretch / 2.0;
  const QRectF knob(std::clamp(x, track.left() + 1.5, track.right() - 1.5 - d - stretch),
                    track.top() + 3.0,
                    d + stretch,
                    d);
  p.setPen(Qt::NoPen);
  const QColor off_knob = mix_colors(t.ink_dim, t.ink, h * controls::toggle_knob_hover);
  p.setBrush(mix_colors(off_knob, on_accent_color(), k));
  p.drawRoundedRect(knob, d / 2.0, d / 2.0);
}

void ToggleSwitch::enterEvent(QEnterEvent *event)
{
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent *event)
{
  this->hover_t.to(0.0, motion::hover_out, motion::leave());
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::leaveEvent(event);
}

void ToggleSwitch::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent *event)
{
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::mouseReleaseEvent(event);
}

} // namespace hesiod
