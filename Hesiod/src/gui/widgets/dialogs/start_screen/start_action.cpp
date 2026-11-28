/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEnterEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include "hesiod/gui/widgets/controls/control_tones.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_action.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/start_screen_style.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "meta_qt/ui/theme.hpp"

namespace hesiod
{

namespace ss = start_screen;

StartAction::StartAction(const QString &label,
                         const QString &keys,
                         Glyph          glyph,
                         bool           primary,
                         QWidget       *parent)
    : QAbstractButton(parent), arrival(this), keys_text(keys), glyph(glyph),
      primary(primary)
{
  this->setText(label);
  this->setAccessibleName(label);
  this->setCursor(Qt::PointingHandCursor);
  this->setFocusPolicy(Qt::StrongFocus);
  this->setFixedHeight(ss::action_height);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  this->setAttribute(Qt::WA_Hover);
}

void StartAction::enterEvent(QEnterEvent *event)
{
  this->hover_t.to(1.0, motion::hover_in, motion::enter());
  QAbstractButton::enterEvent(event);
}

void StartAction::leaveEvent(QEvent *event)
{
  this->hover_t.to(0.0, motion::hover_out, motion::change());
  QAbstractButton::leaveEvent(event);
}

void StartAction::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
    this->press_t.to(1.0, motion::press_in, motion::enter());
  QAbstractButton::mousePressEvent(event);
}

void StartAction::mouseReleaseEvent(QMouseEvent *event)
{
  this->press_t.to(0.0, motion::press_out, motion::change());
  QAbstractButton::mouseReleaseEvent(event);
}

void StartAction::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
  {
    this->click();
    return;
  }
  QAbstractButton::keyPressEvent(event);
}

void StartAction::paintEvent(QPaintEvent *)
{
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  if (!this->arrival.apply(p, ss::row_rise))
    return;

  const ControlTones t = control_tones();
  const qreal        hover = std::clamp(
      std::max(this->hover_t.value(), this->hasFocus() ? ss::focus_heat : 0.0),
      0.0,
      1.0);
  const qreal  held = std::clamp(this->press_t.value(), 0.0, 1.0);
  const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

  // gray plates only: the first action rests on one and lifts a little under
  // the pointer, the second has one only under the pointer
  QColor fill;
  if (this->primary)
    fill = mix_colors(t.content,
                      t.ink,
                      ss::plate_lift + ss::plate_lift_hover * hover -
                          ss::plate_press * held);
  else
    fill = with_alpha(
        mix_colors(t.content, t.ink, ss::plate_lift - ss::plate_press * held),
        hover);
  p.setPen(Qt::NoPen);
  p.setBrush(fill);
  p.drawRoundedRect(box, ss::row_radius, ss::row_radius);

  // glyph
  const QColor  mark = this->primary ? t.ink : mix_colors(t.ink_faint, t.ink, hover);
  const QPointF c(box.left() + 21.0, box.center().y());
  p.setPen(QPen(mark, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  if (this->glyph == Glyph::New)
  {
    p.drawLine(c + QPointF(-5.5, 0.0), c + QPointF(5.5, 0.0));
    p.drawLine(c + QPointF(0.0, -5.5), c + QPointF(0.0, 5.5));
  }
  else
  {
    // a folder: a tab on the left of its top edge
    QPainterPath folder;
    folder.moveTo(c + QPointF(-6.5, -4.5));
    folder.lineTo(c + QPointF(-2.0, -4.5));
    folder.lineTo(c + QPointF(-0.2, -2.6));
    folder.lineTo(c + QPointF(6.5, -2.6));
    folder.lineTo(c + QPointF(6.5, 5.0));
    folder.lineTo(c + QPointF(-6.5, 5.0));
    folder.closeSubpath();
    p.drawPath(folder);
  }

  // label
  p.setFont(meta::qt::ui_font(13, this->primary));
  p.setPen(mix_colors(t.ink_dim, t.ink, this->primary ? 1.0 : hover));
  p.drawText(box.adjusted(42.0, 0.0, -70.0, 0.0),
             Qt::AlignLeft | Qt::AlignVCenter,
             this->text());

  // the keys, quietly, at the far end
  p.setFont(meta::qt::ui_font(11));
  p.setPen(t.ink_faint);
  p.drawText(box.adjusted(0.0, 0.0, -ss::row_inset, 0.0),
             Qt::AlignRight | Qt::AlignVCenter,
             this->keys_text);
}

} // namespace hesiod
