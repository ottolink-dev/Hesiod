/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPainter>
#include <QResizeEvent>

#include "meta_qt/ui/glide.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/post_process/post_effect_chip.hpp"
#include "hesiod/gui/widgets/post_process/post_effect_tray.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

namespace
{

int chip_family(PostEffect effect)
{
  switch (effect)
  {
  case PostEffect::MIX:
    return 0;
  case PostEffect::INVERT:
  case PostEffect::GAMMA:
  case PostEffect::GAIN:
    return 1;
  case PostEffect::SMOOTHING:
    return 2;
  default:
    return 3;
  }
}

} // namespace

PostEffectTray::PostEffectTray(const meta::qt::Theme &theme, QWidget *parent)
    : QWidget(parent), theme(theme)
{
  this->setFixedHeight(post_dock::TRAY_HEIGHT);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  this->lit = new meta::qt::Glide(theme.metrics.section_ms, this);
  QObject::connect(this->lit, &meta::qt::Glide::tick, this, [this]() { this->update(); });
}

void PostEffectTray::clear()
{
  for (PostEffectChip *chip : this->chips)
  {
    chip->hide();
    chip->deleteLater();
  }

  this->chips.clear();
  this->hint.clear();
  this->update();
}

PostEffectChip *PostEffectTray::add_chip(PostEffect effect)
{
  auto *chip = new PostEffectChip(effect, this->theme, this);
  chip->on_hover = [this, chip](bool inside)
  {
    const QString name = QString::fromStdString(post_effect_info(chip->effect).name);

    if (inside)
      this->hint = chip->isChecked() ? QObject::tr("Remove %1").arg(name)
                                     : QObject::tr("Add %1").arg(name);
    else
      this->hint.clear();

    this->update();
  };

  chip->show();
  this->chips.push_back(chip);
  this->place_chips();
  return chip;
}

void PostEffectTray::set_live(bool new_live)
{
  if (new_live == this->live)
    return;

  this->live = new_live;

  // nothing to animate from on a tray that has not been on screen yet
  if (this->isVisible())
    this->lit->to(new_live ? 1.0 : 0.0);
  else
    this->lit->jump(new_live ? 1.0 : 0.0);
}

void PostEffectTray::refresh_hint()
{
  for (PostEffectChip *chip : this->chips)
    if (chip->underMouse() && chip->on_hover)
      chip->on_hover(true);
}

void PostEffectTray::resizeEvent(QResizeEvent *event)
{
  this->place_chips();
  QWidget::resizeEvent(event);
}

void PostEffectTray::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // --- terminal, and the wire reaching it once there is a stage above. The
  // ring fills in as the first stage arrives rather than switching.
  if (this->origin >= post_dock::GUTTER)
  {
    const qreal   t = this->lit->current();
    const QPointF centre(post_dock::SCOPE_SIZE / 2 + 0.5, this->height() / 2 + 0.5);

    if (t > 0.0)
    {
      painter.setPen(QPen(with_alpha(this->theme.ink_dim, 0.45 * t), 1));
      painter.drawLine(QPointF(centre.x(), 0), centre - QPointF(0, 6));

      painter.setPen(Qt::NoPen);
      painter.setBrush(with_alpha(this->theme.accent, t));
      painter.drawEllipse(centre, 2.0 + 1.5 * t, 2.0 + 1.5 * t);
    }

    if (t < 1.0)
    {
      painter.setPen(QPen(with_alpha(this->theme.ink_dim, 1.0 - t), 1.5));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(centre, 3.25, 3.25);
    }
  }

  // --- dividers between the chip families
  painter.setPen(QPen(with_alpha(this->theme.ink_dim, 0.3), 1));
  for (int x : this->dividers)
    painter.drawLine(QPointF(x + 0.5, this->height() / 2 - 6),
                     QPointF(x + 0.5, this->height() / 2 + 6));

  // --- caption, or what the chip under the pointer would do
  const bool    hinting = !this->hint.isEmpty();
  const QString text = hinting ? this->hint : QObject::tr("Post-process");
  const QRect   cell(this->chips_end + 10,
                   0,
                   this->width() - this->chips_end - 10,
                   this->height());

  painter.setFont(meta::qt::ui_font(12));
  if (painter.fontMetrics().horizontalAdvance(text) > cell.width())
    return; // the tooltips carry it on a panel this narrow

  painter.setPen(hinting ? this->theme.ink_secondary : this->theme.ink_dim);
  painter.drawText(cell, Qt::AlignRight | Qt::AlignVCenter, text);
}

void PostEffectTray::place_chips()
{
  this->dividers.clear();

  const auto total = [this](int gap, int group_gap)
  {
    int width = 0;
    for (size_t i = 0; i < this->chips.size(); ++i)
    {
      if (i > 0)
        width += chip_family(this->chips[i]->effect) ==
                         chip_family(this->chips[i - 1]->effect)
                     ? gap
                     : group_gap;
      width += post_dock::CHIP_SIZE;
    }
    return width;
  };

  // Give ground in steps on a narrow panel: first the air between families,
  // then the terminal's gutter.
  int gap = post_dock::CHIP_GAP;
  int group_gap = post_dock::GROUP_GAP;
  this->origin = post_dock::GUTTER;

  if (this->origin + total(gap, group_gap) > this->width())
    group_gap = post_dock::CHIP_GAP + 2;
  if (this->origin + total(gap, group_gap) > this->width())
    this->origin = 0;

  int       x = this->origin;
  const int y = (this->height() - post_dock::CHIP_SIZE) / 2;

  for (size_t i = 0; i < this->chips.size(); ++i)
  {
    if (i > 0)
    {
      const bool same = chip_family(this->chips[i]->effect) ==
                        chip_family(this->chips[i - 1]->effect);
      if (!same && group_gap == post_dock::GROUP_GAP)
        this->dividers.push_back(x + group_gap / 2);
      x += same ? gap : group_gap;
    }

    this->chips[i]->move(x, y);
    x += post_dock::CHIP_SIZE;
  }

  this->chips_end = x;
  this->update();
}

} // namespace hesiod
