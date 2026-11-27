/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QPainter>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/post_process/post_stage_label.hpp"

namespace hesiod
{

PostStageLabel::PostStageLabel(const QString         &text,
                               const meta::qt::Theme &theme,
                               QWidget               *parent)
    : QWidget(parent), text(text), theme(theme)
{
  this->setFixedHeight(theme.metrics.row_height);
  this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void PostStageLabel::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setFont(meta::qt::row_label_font());
  // on at all means off its default, which is what the brighter ink is for
  painter.setPen(this->theme.state_ink(true, false));
  painter.drawText(
      this->rect(),
      Qt::AlignLeft | Qt::AlignVCenter,
      painter.fontMetrics().elidedText(this->text, Qt::ElideRight, this->width()));
}

} // namespace hesiod
