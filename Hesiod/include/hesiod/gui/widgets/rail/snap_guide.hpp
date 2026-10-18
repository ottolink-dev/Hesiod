/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QWidget>

class QVariantAnimation;

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
  void run_fade(qreal to);

  QRectF             target;
  qreal              opacity = 0.0;
  QVariantAnimation *fade = nullptr;
};

} // namespace hesiod::rail
