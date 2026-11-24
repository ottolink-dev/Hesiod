/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QMenu>
#include <QObject>

class QApplication;
class QMenuBar;

namespace hesiod
{

// =====================================
// HsdMenu
// =====================================

// Menu used by the main menu bar. Identical to QMenu except for the shortcut
// column: Qt draws shortcuts in the item font and colour, as loud as the
// action names. Here they are painted smaller and dimmer, so the eye lands on
// the action first. Qt still reserves the column width from the shortcut, so
// the layout is unchanged.
class HsdMenu : public QMenu
{
public:
  explicit HsdMenu(const QString &title, QWidget *parent = nullptr);

  // submenu of the same kind (Qt's addMenu(QString) would create a plain QMenu)
  HsdMenu *add_sub_menu(const QString &title);

protected:
  void initStyleOption(QStyleOptionMenuItem *option,
                       const QAction        *action) const override;
  void paintEvent(QPaintEvent *event) override;
};

// Adds an HsdMenu to a menu bar.
HsdMenu *add_menu(QMenuBar *menu_bar, const QString &title);

// =====================================
// MenuAnimator
// =====================================

// Application-wide filter giving every popup menu a short fade + drop-in when
// it opens (replacing Qt's own menu effects, which animate a screenshot of the
// menu and stutter). Follows interface.enable_ui_animations.
class MenuAnimator : public QObject
{
public:
  using QObject::QObject;

  static void install(QApplication &app);

  static constexpr int duration_ms = 140;
  static constexpr int drop_px = 6;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
};

} // namespace hesiod
