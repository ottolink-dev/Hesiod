/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QMouseEvent>

#include "hesiod/gui/widgets/shell/menu_chrome.hpp"

namespace hesiod
{

class CustomQMenu : public HsdMenu
{
public:
  using HsdMenu::HsdMenu;

protected:
  void mousePressEvent(QMouseEvent *event) override;
};

} // namespace hesiod