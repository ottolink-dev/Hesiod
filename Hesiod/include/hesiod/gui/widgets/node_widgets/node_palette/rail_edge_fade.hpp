/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <QColor>
#include <QWidget>

namespace hesiod
{

/// Lets the node library's rows run out softly where the list goes on beyond
/// the card. Laid over the list by RailScroller, transparent to the mouse.
class RailEdgeFade : public QWidget
{
  Q_OBJECT

public:
  explicit RailEdgeFade(QWidget *parent = nullptr);

  /// @param top, bottom how strong each soft end is, 0 .. 1
  void set_state(const QColor &ground, double top, double bottom);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QColor ground;
  double top = 0.0;
  double bottom = 0.0;
};

} // namespace hesiod
