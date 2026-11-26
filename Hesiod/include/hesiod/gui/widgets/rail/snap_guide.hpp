/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QWidget>

#include "hesiod/gui/motion/motion.hpp"

namespace hesiod::rail
{

// =====================================
// SnapGuide: where the dragged rail will land
// =====================================

class SnapGuide final : public QWidget
{
public:
  explicit SnapGuide(QWidget *parent);

  void set_target(const QRectF &rect);
  void appear();
  void vanish();

protected:
  void paintEvent(QPaintEvent *) override;

private:
  QRectF current_rect() const;

  QRectF from_rect;
  QRectF to_rect;
  bool   has_target = false;
  Anim   fade{this, 0.0};
  Anim   glide{this, 1.0};
};

} // namespace hesiod::rail
