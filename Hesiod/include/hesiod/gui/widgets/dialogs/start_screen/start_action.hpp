/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QAbstractButton>

#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/widgets/dialogs/start_screen/arrival.hpp"

namespace hesiod
{

// One of the two ways to begin, in the start screen's rail: a glyph, what it
// does, and the keys that do the same. The first rests on a quiet gray plate,
// the second has no plate until the pointer is on it.
class StartAction final : public QAbstractButton
{
  Q_OBJECT

public:
  enum class Glyph
  {
    New,
    Open
  };

  StartAction(const QString &label,
              const QString &keys,
              Glyph          glyph,
              bool           primary,
              QWidget       *parent = nullptr);

  Arrival arrival;

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  QString keys_text;
  Glyph   glyph;
  bool    primary;
  Anim    hover_t{this};
  Anim    press_t{this};
};

} // namespace hesiod
