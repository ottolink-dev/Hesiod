/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QMouseEvent>
#include <QPainter>

#include "hesiod/gui/widgets/node_widgets/node_palette/fold_button.hpp"
#include "hesiod/gui/widgets/node_widgets/node_palette/node_palette_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace np = node_palette;

FoldButton::FoldButton(QWidget *parent)
    : QAbstractButton(parent), hover_t(this, 0.0), press_t(this, 0.0)
{
  this->setCursor(Qt::PointingHandCursor);
  this->setFocusPolicy(Qt::TabFocus);
  this->setAttribute(Qt::WA_Hover, true);
}

void FoldButton::animate(Anim               &anim,
                         double              target,
                         int                 duration_ms,
                         const QEasingCurve &curve)
{
  if (!this->style.animations || this->style.animation_ms <= 0)
    anim.set(target);
  else
    anim.to(target, duration_ms, curve);
}

void FoldButton::enterEvent(QEnterEvent *event)
{
  this->animate(this->hover_t, 1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void FoldButton::leaveEvent(QEvent *event)
{
  this->animate(this->hover_t, 0.0, motion::hover_out, motion::leave());
  this->animate(this->press_t, 0.0, motion::press_out, motion::enter());
  QAbstractButton::leaveEvent(event);
}

void FoldButton::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->animate(this->press_t, 1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void FoldButton::mouseReleaseEvent(QMouseEvent *event)
{
  this->animate(this->press_t, 0.0, motion::press_out, motion::enter());
  QAbstractButton::mouseReleaseEvent(event);
}

void FoldButton::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  const double hover = std::clamp(this->hover_t.value(), 0.0, 1.0);
  const double press = std::clamp(this->press_t.value(), 0.0, 1.0);
  const double lit = std::max(hover, press);

  // a gray plate under the pointer, a step brighter while held
  if (lit > np::hidden_below)
  {
    const QColor fill = mix_colors(this->style.surface_hover,
                                   this->style.surface_selected,
                                   press);
    painter.setPen(Qt::NoPen);
    painter.setBrush(with_alpha(fill, fill.alphaF() * lit));
    const double r = std::min<double>(this->style.corner_radius, box.height() / 2.0);
    painter.drawRoundedRect(box, r, r);
  }

  QPen pen(mix_colors(this->style.text, this->style.text_active, lit));
  pen.setWidthF(np::fold_pen);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);

  // the magnifier (folded) and the pane (unfolded) crossfade in place: at
  // 16 px a scaled mark is a blurred one
  const QPointF c = box.center();
  const double  magnifier_a = 1.0 - np::smoothstep(np::magnifier_fade_from,
                                                  np::magnifier_fade_to,
                                                  this->reveal);
  const double  panel_a = np::smoothstep(np::panel_fade_from,
                                        np::panel_fade_to,
                                        this->reveal);

  if (magnifier_a > np::hidden_below)
  {
    painter.setOpacity(magnifier_a);
    const double o = np::magnifier_offset;
    painter.drawEllipse(QPointF(c.x() - o, c.y() - o),
                        np::magnifier_radius,
                        np::magnifier_radius);
    painter.drawLine(
        QPointF(c.x() + np::magnifier_handle_from, c.y() + np::magnifier_handle_from),
        QPointF(c.x() + np::magnifier_handle_to, c.y() + np::magnifier_handle_to));
  }

  if (panel_a > np::hidden_below)
  {
    painter.setOpacity(panel_a);
    const QRectF panel(c.x() - np::panel_width * 0.5,
                       c.y() - np::panel_height * 0.5,
                       np::panel_width,
                       np::panel_height);
    painter.drawRoundedRect(panel, np::panel_radius, np::panel_radius);
    painter.drawLine(QPointF(panel.left() + np::panel_divider, panel.top()),
                     QPointF(panel.left() + np::panel_divider, panel.bottom()));
  }
}

void FoldButton::set_look(const NodePalettePalette &new_style, bool folded)
{
  this->style = new_style;
  this->setToolTip(folded ? "Show the category names and the search" : "Fold to icons");
  this->update();
}

void FoldButton::set_reveal(double amount)
{
  this->reveal = std::clamp(amount, 0.0, 1.0);
  this->update();
}

} // namespace hesiod
