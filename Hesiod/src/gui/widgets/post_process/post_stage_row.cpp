/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QContextMenuEvent>
#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QVBoxLayout>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/gui/widgets/node_widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/post_process/post_dock_metrics.hpp"
#include "hesiod/gui/widgets/post_process/post_stage_row.hpp"
#include "hesiod/gui/widgets/post_process/post_stage_scope.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"

namespace hesiod
{

PostStageRow::PostStageRow(PostEffect             effect,
                           const meta::qt::Theme &theme,
                           QWidget               *parent)
    : QWidget(parent), theme(theme)
{
  this->content = new QVBoxLayout(this);
  this->content->setContentsMargins(post_dock::GUTTER,
                                    post_dock::STAGE_PAD,
                                    0,
                                    post_dock::STAGE_PAD);
  this->content->setSpacing(2);

  this->scope = new PostStageScope(effect, theme, this);
  this->place_scope();
}

void PostStageRow::set_first(bool new_first)
{
  if (new_first == this->first)
    return;

  this->first = new_first;
  this->update();
}

bool PostStageRow::event(QEvent *event)
{
  const bool handled = QWidget::event(event);

  // rows arrive after the stage is built, and a range changes shape with
  // the panel's width
  if (event->type() == QEvent::LayoutRequest)
    this->place_scope();

  return handled;
}

void PostStageRow::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setPen(QPen(with_alpha(this->theme.ink_dim, 0.45), 1));

  const qreal x = post_dock::SCOPE_SIZE / 2 + 0.5;
  const QRect head = this->scope->geometry();

  // down to the next stage, or to the terminal on the tray
  if (!this->first)
    painter.drawLine(QPointF(x, 0), QPointF(x, head.top() - 3));
  painter.drawLine(QPointF(x, head.bottom() + 4), QPointF(x, this->height()));
}

void PostStageRow::contextMenuEvent(QContextMenuEvent *event)
{
  if (!this->fill_menu)
    return;

  QMenu menu(this);
  style_panel_menu(menu, this->theme);
  this->fill_menu(menu);

  if (!menu.isEmpty())
    menu.exec(event->globalPos());
}

void PostStageRow::place_scope()
{
  int centre = this->theme.metrics.row_height / 2;

  if (QLayoutItem *item = this->content->itemAt(0))
    if (QWidget *head = item->widget())
    {
      const QVariant declared = head->property(post_dock::HEAD_CENTRE_PROPERTY);
      centre = declared.isValid()
                   ? declared.toInt()
                   : std::min(head->sizeHint().height(), this->theme.metrics.row_height) /
                         2;
    }

  const QPoint spot(0,
                    post_dock::STAGE_PAD +
                        std::max(0, centre - post_dock::SCOPE_SIZE / 2));
  if (spot != this->scope->pos())
  {
    this->scope->move(spot);
    this->update();
  }
}

} // namespace hesiod
