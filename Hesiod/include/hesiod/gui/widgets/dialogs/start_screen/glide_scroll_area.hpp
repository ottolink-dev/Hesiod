/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QScrollArea>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod
{

// The start screen's pictures. The wheel sets a place to reach and the view
// glides there, so a notch of the wheel is a movement that can be followed
// rather than a jump. A touchpad, and the wheel with animations off, scroll
// as usual.
class GlideScrollArea final : public QScrollArea
{
  Q_OBJECT

public:
  explicit GlideScrollArea(QWidget *parent = nullptr);

protected:
  void wheelEvent(QWheelEvent *event) override;

private:
  Anim position{0.0};
};

} // namespace hesiod
