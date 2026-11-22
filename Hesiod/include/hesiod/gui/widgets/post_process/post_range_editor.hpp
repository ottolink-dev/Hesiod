/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QString>
#include <QWidget>

#include <glm/glm.hpp>

#include "meta/core/attribute.hpp"
#include "meta/core/data_provider.hpp"
#include "meta_qt/ui/control.hpp"

namespace meta::qt
{
class RangeBar;
struct Theme;
} // namespace meta::qt

namespace hesiod
{

/** @brief Label and range bar on one line, for the two range stages.
 *
 * The stock range row stacks a label, the bar and a row of buttons, one of
 * which switches the range on and off. Here the stage being in the chain is
 * the switch, the label takes the same column a slider's does, and the three
 * presets live in the stage's context menu, which leaves a row about the
 * height of the bar.
 */
class PostRangeEditor final : public QWidget
{
public:
  PostRangeEditor(meta::Attribute<glm::vec2> &attr,
                  const meta::qt::RowContext &ctx,
                  const meta::qt::Theme      &theme,
                  QWidget                    *parent);

  std::function<void()> on_edit_started;
  std::function<void()> on_value_changed;
  std::function<void()> on_edit_ended;

  float     domain_min() const { return this->vmin; }
  float     domain_max() const { return this->vmax; }
  glm::vec2 value() const { return this->attr.value(); }

  /// Seat a range as one complete edit, for the presets.
  void commit(glm::vec2 range);

  /// Model to widget.
  void sync();

  QSize sizeHint() const override;

protected:
  void resizeEvent(QResizeEvent *event) override;

  void paintEvent(QPaintEvent *) override;

private:
  static constexpr int MIN_BAR_WIDTH = 110; ///< narrower, and the bar goes under
  static constexpr int LABEL_LINE = 24;     ///< the label's own line, when it does

  /// The column a slider row gives its label, so the two line up.
  int label_width() const;

  void remember(const glm::vec2 &range);

  void refresh_modified();

  meta::Attribute<glm::vec2> &attr;
  meta::qt::RowContext        ctx;
  const meta::qt::Theme      &theme;
  QString                     label;
  float                       vmin;
  float                       vmax;
  meta::qt::RangeBar         *bar = nullptr;
  meta::DataProvider          provider;
  bool                        editing = false;
  bool                        modified = false;
  bool                        stacked = false;
};

} // namespace hesiod
