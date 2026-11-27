/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

// Pill switch replacing the bare checkbox. The knob is thrown across on toggle
// -- a little past its seat, then home -- unless UI animations are off or the
// switch is not shown yet.
class ToggleSwitch final : public QAbstractButton
{
public:
  explicit ToggleSwitch(QWidget *parent = nullptr);

  // jump to the checked state without animating (after a blocked setChecked)
  void sync();

protected:
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  Anim travel;  // 0 off .. 1 on; may pass 1 briefly
  Anim hover_t; // 0 resting .. 1 hovered
  Anim press_t; // 0 released .. 1 pressed
};

} // namespace hesiod
