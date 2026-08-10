/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QPaintEvent>
#include <QSplitter>
#include <QSplitterHandle>
#include <QWidget>
#include <QPainter>

namespace hesiod
{

// =====================================
// GripSplitterHandle
// =====================================

class GripSplitterHandle : public QSplitterHandle
{
public:
  GripSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
      : QSplitterHandle(orientation, parent)
  {
  }

protected:
  void paintEvent(QPaintEvent *event) override;
};

// =====================================
// GripSplitter
// =====================================

class GripSplitter : public QSplitter
{
public:
  using QSplitter::QSplitter;

protected:
  QSplitterHandle *createHandle() override
  {
    return new GripSplitterHandle(orientation(), this);
  }
};

} // namespace hesiod