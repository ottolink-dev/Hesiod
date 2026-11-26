/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QMenu>

#include "hesiod/gui/motion/motion.hpp"

class QMenuBar;

namespace hesiod
{

// Menu used by the main menu bar and the context menus. A QMenu, except for
// what it draws:
// - the highlight under the active row is one rounded plate that glides from
//   a row to its neighbor, instead of going off here and on there;
// - shortcuts are painted smaller and dimmer than the action names, so the
//   eye lands on the action first. Qt still reserves the column width from
//   the shortcut, so the layout is unchanged;
// - a header row can name what the menu acts on.
// The fill and the highlight are painted here (the stylesheet leaves them
// transparent for QMenu#hsdMenu); sizes, text colors and the frame still come
// from the stylesheet.
class HsdMenu : public QMenu
{
public:
  explicit HsdMenu(const QString &title, QWidget *parent = nullptr);

  // submenu of the same kind (Qt's addMenu(QString) would create a plain QMenu)
  HsdMenu *add_sub_menu(const QString &title);

  // a row that only names what the menu acts on (a node, a file): it cannot be
  // chosen and is drawn as a quiet heading, dim and a little heavier
  QAction *add_header(const QString &title);

protected:
  void initStyleOption(QStyleOptionMenuItem *option,
                       const QAction        *action) const override;
  void paintEvent(QPaintEvent *event) override;

  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  void follow_active(); // points the highlight at Qt's active row

  // the highlight
  bool  lit = false; // it has been placed on a row since the menu opened
  QRect row;         // the row it is on, or gliding to
  Anim  plate_y{this};
  Anim  plate_height{this};
  Anim  plate_alpha{this};
};

// Adds an HsdMenu to a menu bar.
HsdMenu *add_menu(QMenuBar *menu_bar, const QString &title);

} // namespace hesiod
