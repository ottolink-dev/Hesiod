/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QWidget>

#include "hesiod/model/nodes/post_effects.hpp"

class QMenu;
class QVBoxLayout;

namespace meta::qt
{
struct Theme;
}

namespace hesiod
{

class PostStageScope;

/// One effect in the chain: its scope in the gutter, its rows beside it.
class PostStageRow final : public QWidget
{
public:
  PostStageRow(PostEffect effect, const meta::qt::Theme &theme, QWidget *parent);

  /// The first stage has nothing above it to wire to.
  void set_first(bool new_first);

  QVBoxLayout                 *content = nullptr;
  PostStageScope              *scope = nullptr;
  std::function<void(QMenu &)> fill_menu;

protected:
  bool event(QEvent *event) override;

  void paintEvent(QPaintEvent *) override;

  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  /** @brief Put the scope level with the stage's first line.
   *
   * How tall that line is belongs to the design the rows are built with: a
   * slider is a full row in one and barely taller than its text in another. A
   * row whose first line is not simply its upper half says where it is.
   */
  void place_scope();

  const meta::qt::Theme &theme;
  bool                   first = false;
};

} // namespace hesiod
