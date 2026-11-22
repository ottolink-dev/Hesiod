/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <vector>

#include <QString>
#include <QWidget>

#include "hesiod/gui/widgets/post_process/post_dock_metrics.hpp"
#include "hesiod/model/nodes/post_effects.hpp"

namespace meta::qt
{
class Glide;
struct Theme;
} // namespace meta::qt

namespace hesiod
{

class PostEffectChip;

/** @brief Every effect the node offers, as a row of toggles.
 *
 * The one part of the dock that is always there, and its last row: the stages
 * stack upwards from it. Attached to the foot of the properties panel, which
 * is where a node shown on its own has it, that makes the tray the part that
 * never moves. A toggle that has just been clicked is still under the pointer,
 * ready to be clicked again, and the dock grows away from it.
 *
 * The chain ends here. The terminal on the left is where the wire through the
 * stages arrives, the node's output, and it is hollow while the chain is empty.
 */
class PostEffectTray final : public QWidget
{
public:
  PostEffectTray(const meta::qt::Theme &theme, QWidget *parent);

  void clear();

  PostEffectChip *add_chip(PostEffect effect);

  /// Whether anything is in the chain, which is what lights the terminal.
  void set_live(bool new_live);

  /// The hint reads "Add" or "Remove", so it goes stale when a chip flips.
  void refresh_hint();

protected:
  void resizeEvent(QResizeEvent *event) override;

  void paintEvent(QPaintEvent *) override;

private:
  void place_chips();

  const meta::qt::Theme        &theme;
  std::vector<PostEffectChip *> chips;
  std::vector<int>              dividers;
  QString                       hint;
  int                           origin = post_dock::GUTTER;
  int                           chips_end = post_dock::GUTTER;
  bool                          live = false;
  meta::qt::Glide              *lit = nullptr; ///< 0 chain empty, 1 something in it
};

} // namespace hesiod
