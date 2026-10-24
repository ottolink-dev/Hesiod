/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QEvent>
#include <QObject>

#include "hesiod/app/hesiod_application.hpp"

namespace hesiod
{

class ToolTipBlocker : public QObject
{
public:
  bool eventFilter(QObject *obj, QEvent *event) override
  {
    if (event->type() == QEvent::ToolTip &&
        !HSD_CTX.app_settings.interface.enable_tool_tips)
      return true; // block

    return QObject::eventFilter(obj, event);
  }
};

} // namespace hesiod