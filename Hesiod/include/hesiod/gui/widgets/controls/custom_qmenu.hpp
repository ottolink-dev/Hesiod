/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QMenu>
#include <QMouseEvent>

namespace hesiod
{

class CustomQMenu : public QMenu
{
public:
  using QMenu::QMenu;

protected:
  void mousePressEvent(QMouseEvent *event) override;
};

} // namespace hesiod