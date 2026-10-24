/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QPointer>
#include <QWidget>

namespace hesiod
{

// =====================================
// PanelFrame
// =====================================

// Rounded, bordered card that hosts one workspace pane (viewer, graph,
// settings...) over the darker application background, so each pane reads as
// its own surface. Children paint square corners, including the OpenGL viewer,
// so four small caps are stacked above the content to cut the corners round.
// The caps are transparent for mouse events: they only paint. The OpenGL
// viewer is a QOpenGLWidget, composed through Qt's own backing store, so the
// caps really are drawn over it.
//
// The frame follows its content's visibility: hiding the content (e.g. the
// View menu hiding the viewer) hides the whole card instead of leaving an empty
// frame behind.
class PanelFrame : public QWidget
{
public:
  explicit PanelFrame(QWidget *content, QWidget *parent = nullptr);

  QWidget *content() const { return this->p_content; }

  static constexpr int radius = 8;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void place_corners();

  QPointer<QWidget> p_content;
  QWidget          *corners[4] = {nullptr, nullptr, nullptr, nullptr};
};

} // namespace hesiod
