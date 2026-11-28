/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include "hesiod/gui/motion/motion.hpp"

class QPainter;
class QWidget;

namespace hesiod
{

// Each part of the start screen arrives on its own: it fades in over the last
// few pixels of a short rise. The parts start a beat apart, which is what makes
// the screen compose itself instead of appearing. With interface animations
// off every part is in place at once.
class Arrival
{
public:
  explicit Arrival(QWidget *owner);

  void arrive(int delay_ms);

  // call first in paintEvent; false when there is nothing to draw yet
  bool apply(QPainter &p, qreal rise) const;

private:
  QWidget *widget = nullptr;
  Anim     amount;
};

} // namespace hesiod
