/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>
#include <QColor>

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

private:
  QColor color;
};

} // namespace hesiod
