/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>
#include <QColor>
#include <QVariantAnimation>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/theme/node_palette_palette.hpp"

namespace hesiod
{

/** @brief One category in the vertical rail: glyph, label, hover/open state.
 *
 * Custom-painted rather than a styled QPushButton because the resting/hover
 * cross-fade has to be interruptible and has to settle *immediately* when
 * animations are switched off, which a stylesheet :hover cannot express.
 */
class CategoryRailButton : public QAbstractButton
{
  Q_OBJECT

public:
  CategoryRailButton(const QString            &category,
                     const QColor             &accent,
                     const NodePalettePalette &style,
                     QWidget                  *parent = nullptr);

  void set_style(const NodePalettePalette &style);

  /// True while this category's flyout is open; drives the selected surface.
  void set_open(bool state);
  bool is_open() const;

  /// Current surface color, i.e. where the cross-fade actually is.
  QColor current_surface() const;

  /// 0: glyph only (the rail is folded), 1: glyph and label; in between while
  /// the rail folds.
  void   set_reveal(double amount);
  double get_reveal() const;

  QSize sizeHint() const override;

signals:
  /// Pointer entered the button. The rail uses this to switch open flyouts.
  void hovered();

protected:
  bool event(QEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QColor target_surface() const;
  void   retarget_surface();

  QString            category;
  QColor             accent;
  NodePalettePalette style;
  bool               open = false;
  bool               hovering = false;
  double             reveal = 1.0;
  QColor             surface;
  QVariantAnimation  surface_animation;
  Anim               heat_t; // 0 cold .. 1 under the pointer or open
  Anim               open_t; // the open category's bar
};

} // namespace hesiod
