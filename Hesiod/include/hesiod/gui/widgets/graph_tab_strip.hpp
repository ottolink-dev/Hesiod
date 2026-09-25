/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <vector>

#include <QStringList>
#include <QTransform>
#include <QWidget>

class QTimer;

namespace hesiod
{

// =====================================
// GraphTabStrip
// =====================================

// The graphs of a project as tabs along an edge of the node editor, in the
// manner of browser tabs: the current one is part of the editor (same
// background, its outline flowing into the editor's border through concave
// feet), the others sit back quietly and lift on hover. Switching animates the
// tabs rising and settling. A trailing "+" asks for a new graph.
//
// Laid out in "strip space" (x along the edge, y across it, y growing towards
// the editor); a vertical strip is the same drawing with x and y swapped, its
// labels turned to read top to bottom.
class GraphTabStrip : public QWidget
{
public:
  explicit GraphTabStrip(Qt::Orientation orientation = Qt::Vertical,
                         QWidget        *parent = nullptr);

  void set_tabs(const QStringList &names, int current);

  Qt::Orientation orientation() const { return this->orient; }

  std::function<void(int)> on_selected; // a tab was clicked
  std::function<void()>    on_new;      // the "+" was clicked
  std::function<void(int, const QPoint &)>
      on_context_menu; // right click: tab, global pos

  static constexpr int kThickness = 36; // across the strip, overlap included
  static constexpr int kOverlap = 2;    // drawn over the card's border

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  bool event(QEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  struct Tab
  {
    QString name;
    QRectF  rect;         // full (active) geometry, strip space
    qreal   active = 0.0; // 0: sitting back, 1: part of the editor
    qreal   hover = 0.0;
  };

  // strip space <-> widget space (a swap of x and y when vertical: its own
  // inverse)
  QTransform to_widget() const;
  qreal      length() const;

  void   layout_tabs();
  void   step();
  int    tab_at(const QPointF &strip_pos) const;
  QRectF plus_rect() const;

  Qt::Orientation  orient;
  std::vector<Tab> tabs;
  int              current = -1;
  int              hovered = -1; // tab index, or -2 for the "+"
  qreal            plus_hover = 0.0;
  QTimer          *ticker = nullptr;
};

// The node editor's card with the tab strip laid over one of its edges (the
// left one for a vertical strip, the top one otherwise).
class TabbedCard : public QWidget
{
public:
  TabbedCard(QWidget *card, GraphTabStrip *strip, QWidget *parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  QSize strip_extent() const;

  QWidget       *card;
  GraphTabStrip *strip;
};

} // namespace hesiod
