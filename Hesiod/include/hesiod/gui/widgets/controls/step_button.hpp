/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QAbstractButton>

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

private:
  bool plus;
};

// [-] value [+]: wraps `spin` (its own arrows hidden) in a frame of the given
// width, object name "stepper" for styling
QWidget *make_stepper(QAbstractSpinBox *spin, int width);

} // namespace hesiod
