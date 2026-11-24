/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QEnterEvent>
#include <QPainter>
#include <QTableWidgetItem>
#include <QWidget>

#include "hesiod/app/hesiod_application.hpp"

namespace hesiod
{

class TableHoverWidget : public QWidget
{
public:
  explicit TableHoverWidget(QTableWidgetItem *item, QWidget *parent = nullptr)
      : QWidget(parent), item(item)
  {
    this->setMouseTracking(true);
  }

protected:
  void enterEvent(QEnterEvent *event) override
  {
    this->hovered = true;
    this->update();
    QWidget::enterEvent(event); // call base
  }

  void leaveEvent(QEvent *event) override
  {
    this->hovered = false;
    this->update();
    QWidget::leaveEvent(event); // call base
  }

  void paintEvent(QPaintEvent *event) override
  {

    QPainter p(this);

    if (this->hovered)
    {
      int width = 2;
      p.setPen(QPen(HSD_CTX.app_settings.colors.accent_bw, width));
      p.drawRect(this->rect().adjusted(width, width, -width, -width));
    }

    if (item && item->isSelected())
    {
      int width = 1;
      p.setPen(QPen(HSD_CTX.app_settings.colors.accent_bw, width));
      p.drawRect(rect().adjusted(width, width, -width, -width));
    }

    QWidget::paintEvent(event);
  }

private:
  bool              hovered = false;
  QTableWidgetItem *item = nullptr;
};

} // namespace hesiod