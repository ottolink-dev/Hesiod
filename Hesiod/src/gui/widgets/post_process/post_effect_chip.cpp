/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEnterEvent>
#include <QPainter>
#include <QSignalBlocker>

#include "meta_qt/ui/glide.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/post_process/post_dock_metrics.hpp"
#include "hesiod/gui/widgets/post_process/post_effect_chip.hpp"
#include "hesiod/gui/widgets/post_process/post_effect_glyph.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

PostEffectChip::PostEffectChip(PostEffect             effect,
                               const meta::qt::Theme &theme,
                               QWidget               *parent)
    : QAbstractButton(parent), effect(effect), theme(theme)
{
  const PostEffectInfo &info = post_effect_info(effect);

  this->setCheckable(true);
  this->setFixedSize(post_dock::CHIP_SIZE, post_dock::CHIP_SIZE);
  this->setCursor(Qt::PointingHandCursor);
  this->setAttribute(Qt::WA_Hover);
  this->setFocusPolicy(Qt::TabFocus);
  this->setAccessibleName(QString::fromStdString(info.name));
  this->setToolTip(QString::fromStdString(info.name + ": " + info.hint));

  this->lit = new meta::qt::Glide(theme.metrics.switch_ms, this);
  this->hover = new meta::qt::Glide(theme.metrics.switch_ms, this);

  for (meta::qt::Glide *glide : {this->lit, this->hover})
    QObject::connect(glide, &meta::qt::Glide::tick, this, [this]() { this->update(); });
}

void PostEffectChip::enterEvent(QEnterEvent *event)
{
  // in at once, out over a moment: a pointer crossing the tray lights each
  // chip as it reaches it and leaves a short trail behind
  this->hover->jump(1.0);

  if (this->on_hover)
    this->on_hover(true);
  QAbstractButton::enterEvent(event);
}

void PostEffectChip::leaveEvent(QEvent *event)
{
  this->hover->to(0.0);

  if (this->on_hover)
    this->on_hover(false);
  QAbstractButton::leaveEvent(event);
}

void PostEffectChip::paintEvent(QPaintEvent *)
{
  // Followed from here rather than from toggled(): the dock sets the check
  // state with signals blocked when it syncs from the model, and the chip
  // has to glide for an undo just as it does for a click. The first paint
  // seats the state, there is nothing to animate from yet.
  if (!this->seated)
  {
    this->seated = true;
    this->lit_target = this->isChecked();
    const QSignalBlocker blocker(this->lit);
    this->lit->jump(this->lit_target ? 1.0 : 0.0);
  }
  else if (this->lit_target != this->isChecked())
  {
    this->lit_target = this->isChecked();
    this->lit->to(this->lit_target ? 1.0 : 0.0);
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const qreal  on = this->lit->current();
  const qreal  over = this->hover->current();
  const bool   down = this->isDown();
  const QRectF frame = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

  // off: nothing until the pointer arrives
  if (over > 0.0 || down)
  {
    painter.setPen(Qt::NoPen);
    painter.setBrush(with_alpha(down ? this->theme.section_header_press
                                     : this->theme.section_header_hover,
                                (down ? 1.0 : over) * (1.0 - on)));
    painter.drawRoundedRect(frame, 6, 6);
  }

  // on: the accent, as a tint and an outline
  if (on > 0.0)
  {
    painter.setPen(QPen(with_alpha(this->theme.accent, on * (0.7 + 0.25 * over)), 1));
    painter.setBrush(
        with_alpha(this->theme.accent, on * (down ? 0.38 : 0.22 + 0.08 * over)));
    painter.drawRoundedRect(frame, 6, 6);
  }

  if (this->hasFocus())
  {
    painter.setPen(QPen(this->theme.accent, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(frame, 6, 6);
  }

  paint_post_glyph(
      painter,
      QRectF(this->rect()).adjusted(5.5, 5.5, -5.5, -5.5),
      this->effect,
      canonical_post_glyph(this->effect),
      mix_colors(this->theme.ink_dim, this->theme.ink_primary, std::max(on, over)));
}

} // namespace hesiod
