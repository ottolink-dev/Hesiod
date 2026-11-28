/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QColor>
#include <QWidget>

namespace hesiod
{

/// Dissolves the node library's search field into the card as the rail folds.
class SearchFadeVeil : public QWidget
{
  Q_OBJECT

public:
  explicit SearchFadeVeil(QWidget *parent = nullptr);

  /// @param opacity 0 the field shows through .. 1 only the card's color
  void set_state(const QColor &ground, double opacity);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor ground;
  double veil_alpha = 0.0;
};

} // namespace hesiod
