/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>
#include <memory>
#include <string>

#include <QWidget>

#include "meta_qt/ui/control.hpp"

#include "hesiod/model/nodes/post_effects.hpp"

namespace hesiod
{

class BaseNode;

/** @brief A node's output chain, at the foot of its properties.
 *
 * Post-processing used to be a section of its own on every node that has an
 * output: eight rows, most of them sitting on a value that does nothing, and
 * no way to tell from the header whether any of it was in use.
 *
 * Here it is a chain instead. A tray lists every pass the node offers as a
 * small toggle, and a pass only takes up a row once it is switched on. The
 * rows are the stages of the chain, in the order the pass applies them, each
 * with a small display of what it does to the signal. A node that uses none
 * shows the tray and nothing else.
 *
 * The dock edits the node's current container, and follows it when the node
 * switches to another. It owns no model state: whether a stage is shown is
 * read back from the attributes on every sync (see is_post_effect_active()),
 * so undo, presets and a reset all land here without being told about it.
 *
 * Edits are reported the way a Meta row reports them, as edit_started /
 * value_changed / edit_ended, and the host decides what to do with each.
 */
class PostProcessDock : public QWidget
{
  Q_OBJECT

public:
  /// Returns the node the dock edits, or nullptr once it is gone.
  using NodeResolver = std::function<BaseNode *()>;

  /**
   * @param design      Meta design the stage rows are built with.
   * @param row_context Passed to those rows; its theme also draws the dock.
   * @param as_card     Draw on a section card, for designs that have them.
   */
  PostProcessDock(NodeResolver                resolver,
                  const std::string          &design,
                  const meta::qt::RowContext &row_context,
                  bool                        as_card,
                  QWidget                    *parent = nullptr);
  ~PostProcessDock() override;

  /// True when any of the node's containers carries a post-processing effect.
  static bool is_needed(BaseNode &node);

  /// Bring the stages, their values and the tray in line with the model.
  void sync_from_model();

signals:
  void edit_started();
  void value_changed();
  void edit_ended();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

} // namespace hesiod
