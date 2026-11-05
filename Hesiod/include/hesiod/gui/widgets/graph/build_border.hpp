/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once
#include <functional>

#include <QPainterPath>
#include <QWidget>

class QVariantAnimation;

namespace hesiod
{

// A dashed line running along an outline of its parent while the graph is
// updated, if enabled in the settings (node_editor.show_build_border). Only
// the line is repainted, which FractionalRepaintFilter keeps partial.
class BuildBorder : public QWidget
{
public:
  static constexpr int width = 2;

  BuildBorder(QWidget *parent, std::function<QPainterPath()> outline); // parent coords

  void set_active(bool active);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void update_line();

  std::function<QPainterPath()> outline;
  QVariantAnimation            *motion = nullptr;
  qreal                         phase = 0.0; // along a dash and a gap, 0 to 1
};

} // namespace hesiod
