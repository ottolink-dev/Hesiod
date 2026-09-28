/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPainter>
#include <QVariantAnimation>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/controls/toggle_switch.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"

namespace hesiod
{

ToggleSwitch::ToggleSwitch(QWidget *parent) : QAbstractButton(parent)
{
  this->setCheckable(true);
  this->setCursor(Qt::PointingHandCursor);
  this->setFixedSize(38, 22);

  this->animation = new QVariantAnimation(this);
  this->animation->setDuration(140);
  this->animation->setEasingCurve(QEasingCurve::OutCubic);
  QObject::connect(this->animation,
                   &QVariantAnimation::valueChanged,
                   this,
                   [this](const QVariant &value)
                   {
                     this->position = value.toReal();
                     this->update();
                   });

  QObject::connect(this,
                   &QAbstractButton::toggled,
                   this,
                   [this](bool checked)
                   {
                     const qreal target = checked ? 1.0 : 0.0;
                     if (!HSD_CTX.app_settings.interface.enable_ui_animations ||
                         !this->isVisible())
                     {
                       this->position = target;
                       this->update();
                       return;
                     }
                     this->animation->stop();
                     this->animation->setStartValue(this->position);
                     this->animation->setEndValue(target);
                     this->animation->start();
                   });
}

void ToggleSwitch::sync()
{
  this->animation->stop();
  this->position = this->isChecked() ? 1.0 : 0.0;
  this->update();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
  const ControlTones t = control_tones();
  QPainter           p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF track = QRectF(this->rect()).adjusted(1, 1, -1, -1);
  const qreal  r = track.height() / 2.0;

  const QColor off = this->underMouse() ? t.field_hover : t.field;
  QColor       edge = mix_colors(t.border, t.accent, this->position);
  if (this->hasFocus())
    edge = t.accent.lighter(130);
  p.setPen(QPen(edge, 1));
  p.setBrush(mix_colors(off, t.accent, this->position));
  p.drawRoundedRect(track, r, r);

  const qreal  d = track.height() - 6.0;
  const qreal  x = track.left() + 3.0 + (track.width() - 6.0 - d) * this->position;
  const QRectF knob(x, track.top() + 3.0, d, d);
  p.setPen(Qt::NoPen);
  p.setBrush(mix_colors(t.ink_dim, QColor("#ffffff"), this->position));
  p.drawEllipse(knob);
}

void ToggleSwitch::enterEvent(QEnterEvent *event)
{
  this->update();
  QAbstractButton::enterEvent(event);
}

void ToggleSwitch::leaveEvent(QEvent *event)
{
  this->update();
  QAbstractButton::leaveEvent(event);
}

} // namespace hesiod
