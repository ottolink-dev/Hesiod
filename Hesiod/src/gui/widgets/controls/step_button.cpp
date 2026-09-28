/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QAbstractSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>

#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/controls/step_button.hpp"

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

  if (this->underMouse() && this->isEnabled())
  {
    p.setPen(Qt::NoPen);
    p.setBrush(this->isDown() ? t.border : t.field_hover);
    p.drawRoundedRect(QRectF(this->rect()).adjusted(2, 2, -2, -2), 5, 5);
  }

  QPen pen(this->isEnabled() ? t.ink_dim : t.ink_faint, 1.4);
  pen.setCapStyle(Qt::RoundCap);
  p.setPen(pen);
  const QPointF c(this->width() / 2.0, this->height() / 2.0);
  p.drawLine(c + QPointF(-4.0, 0.0), c + QPointF(4.0, 0.0));
  if (this->plus)
    p.drawLine(c + QPointF(0.0, -4.0), c + QPointF(0.0, 4.0));
}

void StepButton::enterEvent(QEnterEvent *event)
{
  this->update();
  QAbstractButton::enterEvent(event);
}

void StepButton::leaveEvent(QEvent *event)
{
  this->update();
  QAbstractButton::leaveEvent(event);
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
