/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>

#include "hesiod/gui/motion/motion.hpp"

class QAbstractSpinBox;

namespace hesiod
{

// The - / + ends of a stepper, painted so the glyphs sit exactly centred.
// Auto-repeats while held.
class StepButton final : public QAbstractButton
{
public:
  StepButton(bool plus, QWidget *parent);

protected:
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  bool plus;
  Anim hover_t{this}; // 0 at rest, 1 under the pointer
  Anim press_t{this}; // 0 released, 1 held
};

// [-] value [+]: wraps `spin` (its own arrows hidden) in a frame of the given
// width, object name "stepper" for styling
QWidget *make_stepper(QAbstractSpinBox *spin, int width);

} // namespace hesiod
