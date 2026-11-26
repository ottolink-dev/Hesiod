/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QAbstractSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>

#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/controls/controls_style.hpp"
#include "hesiod/gui/widgets/controls/step_button.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

StepButton::StepButton(bool plus, QWidget *parent) : QAbstractButton(parent), plus(plus)
{
  this->setFixedSize(26, 28);
  this->setAutoRepeat(true);
  this->setAutoRepeatDelay(350);
  this->setAutoRepeatInterval(60);
  this->setFocusPolicy(Qt::NoFocus);
  this->setCursor(Qt::PointingHandCursor);
  this->setToolTip(plus ? "Increase" : "Decrease");
}

void StepButton::paintEvent(QPaintEvent *)
{
  const ControlTones t = control_tones();
  QPainter           p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // the plate fades in under the pointer; held, it deepens smoothly
  const qreal heat = this->isEnabled() ? std::clamp(this->hover_t.value(), 0.0, 1.0)
                                       : 0.0;
  const qreal press = this->isEnabled() ? std::clamp(this->press_t.value(), 0.0, 1.0)
                                        : 0.0;
  const qreal lit = std::max(heat, press);
  if (lit > 0.002)
  {
    QColor plate = mix_colors(t.field_hover, t.border, press);
    plate.setAlphaF(plate.alphaF() * lit);
    p.setPen(Qt::NoPen);
    p.setBrush(plate);
    p.drawRoundedRect(QRectF(this->rect()).adjusted(2, 2, -2, -2), 5, 5);
  }

  const QColor ink = this->isEnabled()
                         ? mix_colors(t.ink_dim, t.ink, lit * controls::step_ink_hover)
                         : t.ink_faint;
  QPen         pen(ink, 1.4);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  const QPointF c(this->width() / 2.0, this->height() / 2.0);
  const qreal   arm = controls::step_arm * (1.0 - controls::step_arm_pressed * press);
  p.drawLine(c + QPointF(-arm, 0.0), c + QPointF(arm, 0.0));
  if (this->plus)
    p.drawLine(c + QPointF(0.0, -arm), c + QPointF(0.0, arm));
}

void StepButton::enterEvent(QEnterEvent *event)
{
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void StepButton::leaveEvent(QEvent *event)
{
  this->hover_t.to(0.0, motion::hover_out, motion::leave());
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::leaveEvent(event);
}

void StepButton::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void StepButton::mouseReleaseEvent(QMouseEvent *event)
{
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::mouseReleaseEvent(event);
}

QWidget *make_stepper(QAbstractSpinBox *spin, int width)
{
  auto *box = new QFrame();
  box->setObjectName("stepper");
  box->setFixedWidth(width);

  auto *layout = new QHBoxLayout(box);
  layout->setContentsMargins(1, 1, 1, 1);
  layout->setSpacing(0);

  spin->setParent(box);
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setAlignment(Qt::AlignCenter);
  spin->setFrame(false);
  spin->setKeyboardTracking(false);

  auto *minus = new StepButton(false, box);
  auto *plus = new StepButton(true, box);
  QObject::connect(minus, &QAbstractButton::clicked, spin, &QAbstractSpinBox::stepDown);
  QObject::connect(plus, &QAbstractButton::clicked, spin, &QAbstractSpinBox::stepUp);

  layout->addWidget(minus);
  layout->addWidget(spin, 1);
  layout->addWidget(plus);
  return box;
}

} // namespace hesiod
