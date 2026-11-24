/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QPaintEvent>
#include <QPainter>
#include <QSplitter>
#include <QSplitterHandle>
#include <QWidget>

namespace hesiod
{

// =====================================
// GripSplitterHandle
// =====================================

// Drawn as a gap in the application background between two panel cards, with
// a short grip pill that brightens while hovered or dragged.
class GripSplitterHandle : public QSplitterHandle
{
public:
  GripSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
      : QSplitterHandle(orientation, parent)
  {
    this->setAttribute(Qt::WA_Hover);
  }

protected:
  bool event(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  bool pressed = false;
};

// =====================================
// GripSplitter
// =====================================

class GripSplitter : public QSplitter
{
public:
  // spacing between panel cards, shared with the non-splitter gaps
  static constexpr int gap = 6;

  explicit GripSplitter(Qt::Orientation orientation, QWidget *parent = nullptr)
      : QSplitter(orientation, parent)
  {
    this->setHandleWidth(gap);
  }

protected:
  QSplitterHandle *createHandle() override
  {
    return new GripSplitterHandle(orientation(), this);
  }
};

} // namespace hesiod
