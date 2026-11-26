/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QFocusEvent>
#include <QMouseEvent>
#include <QPainter>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/controls/color_swatch.hpp"
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/controls/controls_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

ColorSwatch::ColorSwatch(int width, QWidget *parent) : QAbstractButton(parent)
{
  this->setCursor(Qt::PointingHandCursor);
  this->setFixedSize(width, 30);
}

void ColorSwatch::set_color(const QColor &value)
{
  this->color = value;
  this->setToolTip(value.name(value.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb));
  this->update();
}

void ColorSwatch::paintEvent(QPaintEvent *)
{
  const ControlTones t = control_tones();
  QPainter           p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const qreal h = std::clamp(this->hover_t.value(), 0.0, 1.0);
  const qreal press = std::clamp(this->press_t.value(), 0.0, 1.0);
  const qreal f = std::clamp(this->focus_t.value(), 0.0, 1.0);

  const QRectF frame = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QColor       edge = mix_colors(t.border, t.ink_faint, h);
  edge = mix_colors(edge, t.accent, f);
  QColor fill = mix_colors(t.field, t.field_hover, h);
  fill = mix_colors(fill, t.field, press * controls::swatch_press_settle);
  p.setPen(QPen(edge, 1));
  p.setBrush(fill);
  p.drawRoundedRect(frame, 6, 6);

  const qreal  inset = controls::swatch_press_inset * press;
  const QRectF chip(5.5 + inset,
                    5.5 + inset,
                    34.0 - 2.0 * inset,
                    this->height() - 11.0 - 2.0 * inset);
  p.setPen(QPen(mix_colors(this->color, QColor("#000000"), 0.35), 1));
  p.setBrush(this->color);
  p.drawRoundedRect(chip, 4, 4);

  p.setPen(mix_colors(t.ink_dim, t.ink, h * controls::swatch_ink_hover));
  p.setFont(meta::qt::mono_font(12));
  p.drawText(
      QRectF(chip.right() + 10, 0, this->width() - chip.right() - 14, this->height()),
      Qt::AlignVCenter | Qt::AlignLeft,
      this->color.name().toUpper());
}

void ColorSwatch::enterEvent(QEnterEvent *event)
{
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void ColorSwatch::leaveEvent(QEvent *event)
{
  this->hover_t.to(0.0, motion::hover_out, motion::leave());
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::leaveEvent(event);
}

void ColorSwatch::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void ColorSwatch::mouseReleaseEvent(QMouseEvent *event)
{
  this->press_t.to(0.0, motion::press_out, motion::enter());
  QAbstractButton::mouseReleaseEvent(event);
}

void ColorSwatch::focusInEvent(QFocusEvent *event)
{
  this->focus_t.to(1.0, motion::focus_in, motion::enter());
  QAbstractButton::focusInEvent(event);
}

void ColorSwatch::focusOutEvent(QFocusEvent *event)
{
  this->focus_t.to(0.0, motion::focus_out, motion::leave());
  QAbstractButton::focusOutEvent(event);
}

} // namespace hesiod
