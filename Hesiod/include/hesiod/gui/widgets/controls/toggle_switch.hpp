/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>

class QVariantAnimation;

namespace hesiod
{

// Pill switch replacing the bare checkbox. The knob slides on toggle (unless
// UI animations are off or the switch is not shown yet).
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

private:
  QVariantAnimation *animation = nullptr;
  qreal              position = 0.0;
};

} // namespace hesiod
