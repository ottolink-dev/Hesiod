/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QAbstractButton>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/theme/node_palette_palette.hpp"

namespace hesiod
{

/** @brief The one button in the node library's header.
 *
 * Unfolded it folds the rail to its icons; folded it is a magnifier that
 * unfolds it again and puts the caret in the search field, which is what one
 * wants a folded rail's header for. The two marks crossfade in place along the
 * fold.
 */
class FoldButton : public QAbstractButton
{
  Q_OBJECT

public:
  explicit FoldButton(QWidget *parent = nullptr);

  void set_look(const NodePalettePalette &style, bool folded);

  /// 0 folded .. 1 unfolded, as the rail is now.
  void set_reveal(double amount);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void animate(Anim &anim, double target, int duration_ms, const QEasingCurve &curve);

  NodePalettePalette style;
  double             reveal = 1.0;
  Anim               hover_t;
  Anim               press_t;
};

} // namespace hesiod
