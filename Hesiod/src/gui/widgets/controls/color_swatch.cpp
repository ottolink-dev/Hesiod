/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPainter>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/controls/color_swatch.hpp"
#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"

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

  const QRectF frame = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  QColor       edge = this->underMouse() ? t.ink_faint : t.border;
  if (this->hasFocus())
    edge = t.accent;
  p.setPen(QPen(edge, 1));
  p.setBrush(this->underMouse() ? t.field_hover : t.field);
  p.drawRoundedRect(frame, 6, 6);

  const QRectF chip(5.5, 5.5, 34, this->height() - 11.0);
  p.setPen(QPen(mix_colors(this->color, QColor("#000000"), 0.35), 1));
  p.setBrush(this->color);
  p.drawRoundedRect(chip, 4, 4);

  p.setPen(t.ink_dim);
  p.setFont(meta::qt::mono_font(12));
  p.drawText(
      QRectF(chip.right() + 10, 0, this->width() - chip.right() - 14, this->height()),
      Qt::AlignVCenter | Qt::AlignLeft,
      this->color.name().toUpper());
}

void ColorSwatch::enterEvent(QEnterEvent *event)
{
  this->update();
  QAbstractButton::enterEvent(event);
}

void ColorSwatch::leaveEvent(QEvent *event)
{
  this->update();
  QAbstractButton::leaveEvent(event);
}

} // namespace hesiod
