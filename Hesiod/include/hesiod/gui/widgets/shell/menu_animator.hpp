/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QObject>

class QApplication;

namespace hesiod
{

// Application-wide filter that gives every transient window an entrance: popup
// menus, combo box lists, tooltips and the frameless dialogs. A short fade
// with a few pixels of travel (PopupEntrance), stepped by the animation clock
// (motion.hpp). It replaces Qt's own menu, combo and tooltip effects, which
// animate a grabbed screenshot on a plain timer and stutter.
//
// Menus, lists and tooltips also get their rounded corners and their edge
// from the window manager here (Windows 11; a no-op elsewhere).
//
// Follows interface.enable_ui_animations.
class MenuAnimator : public QObject
{
public:
  using QObject::QObject;

  static void install(QApplication &app);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
};

} // namespace hesiod
