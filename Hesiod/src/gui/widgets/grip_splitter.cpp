/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/grip_splitter.hpp"
#include "hesiod/app/hesiod_application.hpp"

namespace hesiod
{

void GripSplitterHandle::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);
  painter.fillRect(rect(), HSD_CTX.app_settings.colors.bg_primary); // handle background

  // draw a small grip in the center
  const int handle_thickness = (orientation() == Qt::Horizontal) ? width() : height();
  const int grip_size = handle_thickness;
  const int grip_length = 20;

  QColor dot_color(HSD_CTX.app_settings.colors.text_secondary);
  painter.setBrush(dot_color);
  painter.setPen(Qt::NoPen);

  if (orientation() == Qt::Horizontal)
  {
    int cx = width() / 2;
    int cy = height() / 2;
    for (int i = -grip_length / 2; i <= grip_length / 2; i += 6)
      painter.drawEllipse(QPoint(cx, cy + i), grip_size / 2, grip_size / 2);
  }
  else
  {
    int cx = width() / 2;
    int cy = height() / 2;
    for (int i = -grip_length / 2; i <= grip_length / 2; i += 6)
      painter.drawEllipse(QPoint(cx + i, cy), grip_size / 2, grip_size / 2);
  }
}

} // namespace hesiod
