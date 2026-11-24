/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/gui/widgets/controls/custom_qmenu.hpp"

namespace hesiod
{

void CustomQMenu::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::RightButton)
  {
    // close the menu when right-clicked, so it behaves the same on
    // both Linux and Windows
    this->close();
    event->accept();
  }
  else
  {
    QMenu::mousePressEvent(event);
  }
}

} // namespace hesiod
