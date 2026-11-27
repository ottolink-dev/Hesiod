/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QEnterEvent>
#include <QFocusEvent>
#include <QPainter>

#include "meta_qt/ui/glide.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/post_process/post_dock_metrics.hpp"
#include "hesiod/gui/widgets/post_process/post_stage_scope.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

PostStageScope::PostStageScope(PostEffect             effect,
                               const meta::qt::Theme &theme,
                               QWidget               *parent)
    : QAbstractButton(parent), effect(effect), theme(theme),
      glyph(canonical_post_glyph(effect))
{
  const QString name = QString::fromStdString(post_effect_info(effect).name);

  this->setFixedSize(post_dock::SCOPE_SIZE, post_dock::SCOPE_SIZE);
  this->setCursor(Qt::PointingHandCursor);
  this->setAttribute(Qt::WA_Hover);
  this->setFocusPolicy(Qt::TabFocus);
  this->setAccessibleName(QObject::tr("Remove %1").arg(name));
  this->setToolTip(QObject::tr("Remove %1").arg(name));

  this->armed = new meta::qt::Glide(theme.metrics.switch_ms, this);
  QObject::connect(this->armed,
                   &meta::qt::Glide::tick,
                   this,
                   [this]() { this->update(); });
}

void PostStageScope::set_glyph(const PostEffectGlyph &new_glyph)
{
  if (new_glyph.a == this->glyph.a && new_glyph.b == this->glyph.b)
    return;

  this->glyph = new_glyph;
  this->update();
}

void PostStageScope::enterEvent(QEnterEvent *event)
{
  this->armed->to(1.0);
  QAbstractButton::enterEvent(event);
}

void PostStageScope::leaveEvent(QEvent *event)
{
  this->armed->to(this->hasFocus() ? 1.0 : 0.0);
  QAbstractButton::leaveEvent(event);
}

void PostStageScope::focusInEvent(QFocusEvent *event)
{
  this->armed->to(1.0);
  QAbstractButton::focusInEvent(event);
}

void PostStageScope::focusOutEvent(QFocusEvent *event)
{
  this->armed->to(this->underMouse() ? 1.0 : 0.0);
  QAbstractButton::focusOutEvent(event);
}

void PostStageScope::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const qreal  t = this->armed->current();
  const QRectF well = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

  painter.setPen(
      QPen(mix_colors(this->theme.field_border, this->theme.field_border_hover, t), 1));
  painter.setBrush(this->isDown() ? this->theme.field_editing : this->theme.field);
  painter.drawRoundedRect(well, 6, 6);

  // the trace and the cross trade places rather than swap
  if (t < 1.0)
  {
    painter.setOpacity(1.0 - t);
    paint_post_glyph(painter,
                     QRectF(this->rect()).adjusted(5, 5, -5, -5),
                     this->effect,
                     this->glyph,
                     this->theme.ink_icon);
  }

  if (t > 0.0)
  {
    const QPointF c = well.center();
    const qreal   d = 2.0 + 1.5 * t;

    painter.setOpacity(t);
    painter.setPen(QPen(this->theme.ink_primary, 1.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(c + QPointF(-d, -d), c + QPointF(d, d));
    painter.drawLine(c + QPointF(-d, d), c + QPointF(d, -d));
  }
}

} // namespace hesiod
