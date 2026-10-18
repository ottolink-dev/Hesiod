/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <utility>
#include <vector>

#include <QWidget>

#include "nlohmann/json.hpp"

#include "hesiod/gui/widgets/rail/rail_style.hpp"

class QVariantAnimation;

namespace hesiod::rail
{

class RailOwner;
class SnapGuide;

// =====================================
// ToolRail
// =====================================

// A floating toolbar docked to an edge of its parent, which the user can drag
// to any other edge: it folds into a square meanwhile, a SnapGuide shows where
// it will land. Its tools are reported to its owner (RailOwner).
class ToolRail : public QWidget
{
public:
  struct Item
  {
    int     tool;
    QString tip;
    bool    separator_before = false;
    bool    flyout = true; // draws the corner mark: opens a panel or menu
  };

  ToolRail(RailOwner *owner, SnapGuide *guide, QWidget *parent);

  void set_items(std::vector<Item> new_items);
  void set_active(int tool);

  // draws a tool's icon in `ink`
  std::function<void(QPainter &, int tool, const QRectF &, const QColor &ink)>
      icon_painter;

  // until the user moves it: along this edge, this far from the top
  Edge  default_edge = Edge::Right;
  qreal default_offset = 0.0;

  Edge get_edge() const { return this->edge; }

  // where the rail sits once docked (it may be folded or flying right now)
  QRectF docked_rect() const;
  bool   is_moving() const { return this->dragging || this->flying; }

  // the item's rectangle, in the parent's coordinates
  QRect item_rect(int tool) const;

  // The placement the user chose (pref_*), saved with the project, is kept
  // apart from where the rail is shown right now (edge / along / anchor):
  // that one is derived from it on every layout, so a parent that is still
  // tiny at restore time, or briefly too short, never overwrites the choice.
  // Along an edge the choice is a fraction of the free range (0: one corner,
  // 1: the other), which survives resizes where a pixel offset would not.
  nlohmann::json json_to() const;
  void           json_from(const nlohmann::json &json);

  // dock where the user chose, derived for the current parent and tool set
  void place();

protected:
  // for rails with tools of their own kind: under the tools, a whole cell
  // instead of a tool (true when it did), and a tool's face (by default its
  // icon_painter icon)
  virtual void paint_under_tools(QPainter &, const QPointF & /* origin */) {}
  virtual bool paint_cell(QPainter &, const Item &, const QRectF & /* cell */, bool)
  {
    return false;
  }
  virtual void paint_tool(QPainter     &p,
                          const Item   &item,
                          const QRectF &cell,
                          const QColor &ink);

  // cell `i` relative to the docked rail's top-left, and which one has a tool
  QRectF cell_rect(size_t i) const;
  int    index_of(int tool) const;
  qreal  cell_radius() const; // cells are inset from the rail: their corners follow it

  bool event(QEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  // a docked placement: the edge, the rail's top-left, and whether it is
  // centred on that edge (then it stays centred when the parent resizes)
  struct Dock
  {
    Edge    edge = Edge::Right;
    QPointF anchor;
    bool    centered = false;
  };

  QRectF                  viewport_rect() const;
  qreal                   length() const;
  QSizeF                  full_size(Edge e) const;
  int                     index_at(const QPointF &local) const;
  QPointF                 docked_center(const QPointF &anchor, Edge e) const;
  QRectF                  square_rect() const;
  void                    apply_geometry();
  std::pair<qreal, qreal> along_range(Edge e) const;
  qreal                   middle_along(Edge e) const;
  Dock                    snap(Edge e, qreal along_center) const;
  QPointF                 anchor_at(Edge e, qreal pos) const;
  bool                    fits(Edge e) const;
  Dock                    target_for(const QPointF &c) const;
  void                    land(const Dock &dock);
  QPointF                 clamp_center(const QPointF &p) const;
  void                    update_guide();
  void                    animate_expand(qreal to, int ms);
  void                    begin_drag(const QPointF &in_parent);
  void                    end_drag();

  RailOwner        *owner = nullptr;
  SnapGuide        *guide = nullptr;
  std::vector<Item> items;
  int               active_tool = -1;
  int               hovered = -1;

  // docked placement, as shown now (derived in place())
  Edge    edge = Edge::Right;
  qreal   along = 0.0; // position of the rail's start along its edge
  bool    centered = false;
  QPointF anchor; // docked rail's top-left, parent coordinates

  // ... and as the user chose it (saved with the project)
  bool  placed = false; // false: the default spot
  Edge  pref_edge = Edge::Right;
  qreal pref_t = 0.0; // along the edge's free range: 0 one corner, 1 the other
  bool  pref_centered = false;

  // drag state
  bool    pressed = false;
  int     pressed_index = -1;
  QPointF press_pos;
  bool    dragging = false;
  bool    flying = false;
  QPointF drag_center;
  Dock    pending;

  qreal              expand_t = 1.0; // 0: folded square, 1: docked rail
  QVariantAnimation *expand_anim = nullptr;
  QVariantAnimation *flight = nullptr;
};

} // namespace hesiod::rail
