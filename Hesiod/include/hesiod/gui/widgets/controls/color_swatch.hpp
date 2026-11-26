/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>
#include <QColor>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

// A colour field: a chip of the colour and its hex code, clicked to edit it.
class ColorSwatch final : public QAbstractButton
{
public:
  explicit ColorSwatch(int width, QWidget *parent = nullptr);

  void set_color(const QColor &value);

protected:
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

private:
  QColor color;
  Anim   hover_t{this};
  Anim   press_t{this};
  Anim   focus_t{this};
};

} // namespace hesiod
