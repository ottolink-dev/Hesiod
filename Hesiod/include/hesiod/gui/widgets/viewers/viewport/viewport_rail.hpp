/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QWidget>

#include "nlohmann/json.hpp"

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/viewers/viewport/snap_guide.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"
#include "hesiod/gui/widgets/viewers/viewport_controls.hpp"

namespace hesiod::viewport
{

// =====================================
// ViewportRail
// =====================================

class ViewportRail final : public QWidget
{
public:
  struct Item
  {
    int     tool;
    QString tip;
    bool    separator_before = false;
    bool    flyout = true; // draws the corner mark: opens a panel or menu
  };

  ViewportRail(ViewportControls *owner, SnapGuide *guide, QWidget *parent)
      : QWidget(parent), owner(owner), guide(guide)
  {
    this->setObjectName("hsdViewportRail");
    this->setMouseTracking(true);
    this->setAttribute(Qt::WA_Hover);
    this->setCursor(Qt::OpenHandCursor);

    // Over the GL view a child is painted as a root: without these Qt fills
    // its whole rectangle first, and the rounded corners sit on a square.
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAutoFillBackground(false);
    this->setStyleSheet("QWidget#hsdViewportRail { background: transparent; }");

    this->view_anim = new QVariantAnimation(this);
    this->view_anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->view_anim,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v)
                     {
                       this->view_t = v.toReal();
                       this->update();
                     });

    this->expand_anim = new QVariantAnimation(this);
    this->expand_anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->expand_anim,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v)
                     {
                       this->expand_t = v.toReal();
                       this->apply_geometry();
                     });

    this->flight = new QVariantAnimation(this);
    this->flight->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->flight,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v)
                     {
                       this->drag_center = v.toPointF();
                       this->apply_geometry();
                     });
    QObject::connect(this->flight,
                     &QVariantAnimation::finished,
                     this,
                     [this]()
                     {
                       // landed: take the docked placement and unfold there
                       this->flying = false;
                       this->land(this->pending);
                       this->animate_expand(1.0, 220);
                       this->owner->notify_layout_changed();
                     });
  }

  void set_items(std::vector<Item> new_items)
  {
    this->items = std::move(new_items);
    this->hovered = -1;
    this->place();
  }

  void set_active(int tool)
  {
    this->active_tool = tool;
    this->update();
  }

  void set_resolution_text(const QString &text)
  {
    this->resolution_text = text;
    this->update();
  }

  // 0: 2D, 1: 3D; the switch's highlight slides across
  void set_view_mode(int type, bool animate)
  {
    const qreal to = type == 0 ? 0.0 : 1.0;
    this->view_anim->stop();
    if (!animate || anim_ms(1) == 0)
    {
      this->view_t = to;
      this->update();
      return;
    }
    this->view_anim->setStartValue(this->view_t);
    this->view_anim->setEndValue(to);
    this->view_anim->setDuration(anim_ms(220));
    this->view_anim->start();
  }

  Edge get_edge() const { return this->edge; }

  // where the rail sits once docked (it may be folded or flying right now)
  QRectF docked_rect() const { return QRectF(this->anchor, this->full_size(this->edge)); }
  bool   is_moving() const { return this->dragging || this->flying; }

  // the item's rectangle, in the parent's (the viewport's) coordinates
  QRect item_rect(int tool) const
  {
    for (size_t i = 0; i < this->items.size(); ++i)
      if (this->items[i].tool == tool)
        return this->cell_rect(i).translated(this->anchor).toAlignedRect();
    return QRect();
  }

  // The placement the user chose (pref_*), saved with the project, is kept
  // apart from where the rail is shown right now (edge / along / anchor):
  // that one is derived from it on every layout, so a viewport that is still
  // tiny at restore time, or briefly too short, never overwrites the choice.
  // Along an edge the choice is a fraction of the free range (0: one corner,
  // 1: the other), which survives resizes where a pixel offset would not.
  nlohmann::json json_to() const
  {
    return {{"edge", int(this->pref_edge)},
            {"along_t", this->pref_t},
            {"centered", this->pref_centered},
            {"placed", this->placed}};
  }

  void json_from(const nlohmann::json &json)
  {
    // tolerant, like the rest of the .hsd loader: a missing or mistyped value
    // leaves the current one
    const auto number = [&json](const char *key, double &out)
    {
      if (json.contains(key) && json[key].is_number())
        out = json[key].get<double>();
    };

    if (!json.is_object())
      return;

    double edge_value = double(int(this->pref_edge));
    number("edge", edge_value);
    this->pref_edge = Edge(std::clamp(int(edge_value), 0, 3));

    if (json.contains("centered") && json["centered"].is_boolean())
      this->pref_centered = json["centered"].get<bool>();

    double t = this->pref_t;
    number("along_t", t);
    if (!json.contains("along_t"))
      number("along", t); // older files: a fraction of the whole edge
    this->pref_t = std::clamp(t, 0.0, 1.0);

    // files from before "placed" existed always held a chosen position
    this->placed = true;
    if (json.contains("placed") && json["placed"].is_boolean())
      this->placed = json["placed"].get<bool>();

    this->place();
  }

  // dock where the user chose, derived for the current viewport and tool set
  void place()
  {
    if (this->dragging || this->flying)
      return;

    const QRectF vr = this->viewport_rect();
    if (vr.width() < 10 || vr.height() < 10)
      return;

    Edge e = this->placed ? this->pref_edge : Edge::Right;
    bool centered = this->placed && this->pref_centered;

    // an edge too short for the rail (small viewport, or the longer 3D tool
    // set): show it on a crossing edge it fits along, centred there, for now
    if (!this->fits(e))
    {
      const Edge alt = is_vertical(e) ? Edge::Bottom : Edge::Right;
      if (this->fits(alt))
      {
        e = alt;
        centered = true;
      }
    }

    const auto [lo, hi] = this->along_range(e);
    qreal pos;
    if (centered)
      pos = this->middle_along(e);
    else if (!this->placed)
      pos = std::clamp(vr.top() + 140.0, lo, hi); // default: below the gizmo
    else
      pos = lo + this->pref_t * (hi - lo);

    this->edge = e;
    this->centered = centered;
    this->along = pos;
    this->anchor = this->anchor_at(this->edge, this->along);
    this->expand_t = 1.0;
    this->apply_geometry();
    this->raise();
    this->owner->notify_layout_changed();
  }

protected:
  bool event(QEvent *event) override
  {
    if (event->type() == QEvent::ToolTip)
    {
      const auto *help = static_cast<QHelpEvent *>(event);
      const int   index = this->index_at(help->pos());
      if (index >= 0 && this->expand_t > 0.99)
      {
        const QRect local = this->cell_rect(index)
                                .translated(this->anchor - QPointF(this->pos()))
                                .toAlignedRect();
        QToolTip::showText(help->globalPos(),
                           this->items[index].tip + "\n\nDrag to move the toolbar",
                           this,
                           local);
      }
      else
        QToolTip::showText(help->globalPos(), "Drag to move the toolbar", this);
      return true;
    }
    return QWidget::event(event);
  }

  void leaveEvent(QEvent *event) override
  {
    this->hovered = -1;
    this->update();
    QWidget::leaveEvent(event);
  }

  // the wheel over the rail must not zoom the view underneath
  void wheelEvent(QWheelEvent *event) override { event->accept(); }

  void mousePressEvent(QMouseEvent *event) override
  {
    if (event->button() != Qt::LeftButton || this->flying)
      return;
    this->pressed = true;
    this->press_pos = event->position();
    this->pressed_index = this->index_at(event->position());
    event->accept();
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    const QPointF in_parent = this->mapToParent(event->position());

    if (this->pressed && !this->dragging &&
        (event->position() - this->press_pos).manhattanLength() > 6)
      this->begin_drag(in_parent);

    if (this->dragging)
    {
      this->drag_center = this->clamp_center(in_parent);
      this->update_guide();
      this->apply_geometry();
      return;
    }

    const int index = this->index_at(event->position());
    if (index != this->hovered)
    {
      this->hovered = index;
      this->setCursor(index >= 0 ? Qt::PointingHandCursor : Qt::OpenHandCursor);
      this->update();
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    if (event->button() != Qt::LeftButton)
      return;

    const bool was_pressed = this->pressed;
    this->pressed = false;

    if (this->dragging)
    {
      this->end_drag();
      return;
    }

    if (was_pressed && this->pressed_index >= 0 &&
        this->pressed_index == this->index_at(event->position()))
    {
      const int tool = this->items[this->pressed_index].tool;
      this->owner->on_tool_activated(tool, this->item_rect(tool));
    }
  }

  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF box = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal  radius = std::min(qreal(radius_px()),
                                  std::min(box.width(), box.height()) / 2.0);
    QPainterPath shape;
    shape.addRoundedRect(box, radius, radius);

    p.fillPath(shape, surface_color());
    p.setPen(QPen(this->dragging ? colors.accent : border_color(), 1));
    p.drawPath(shape);

    p.setClipPath(shape);

    // cells are inset from the rail: their corners follow its curve
    const qreal cell_radius = radius_px() - (thick_px() - cell_px()) / 2.0 - 1.0;

    // tools, laid out on the docked rail and revealed as the rail unfolds
    const qreal items_alpha = std::clamp((this->expand_t - 0.35) / 0.65, 0.0, 1.0);
    if (items_alpha > 0.0)
    {
      p.save();
      p.setOpacity(items_alpha);
      const QPointF origin = this->anchor - QPointF(this->pos());

      // the 2D / 3D switch: a shallow track and a highlight sliding between
      // its two cells
      const int i2d = this->index_of(ToolView2D);
      const int i3d = this->index_of(ToolView3D);
      if (i2d >= 0 && i3d >= 0)
      {
        const QRectF a = this->cell_rect(i2d).translated(origin).adjusted(1, 1, -1, -1);
        const QRectF b = this->cell_rect(i3d).translated(origin).adjusted(1, 1, -1, -1);
        p.setPen(Qt::NoPen);
        p.setBrush(mix_colors(surface_color(), colors.bg_deep, 0.55));
        p.drawRoundedRect(a.united(b), cell_radius, cell_radius);

        const qreal  t = this->view_t;
        const QRectF knob(a.left() + (b.left() - a.left()) * t,
                          a.top() + (b.top() - a.top()) * t,
                          a.width(),
                          a.height());
        p.setBrush(mix_colors(surface_color(), colors.accent, 0.30));
        p.drawRoundedRect(knob.adjusted(1.5, 1.5, -1.5, -1.5),
                          cell_radius - 1,
                          cell_radius - 1);
      }

      for (size_t i = 0; i < this->items.size(); ++i)
      {
        const Item  &item = this->items[i];
        const QRectF cell = this->cell_rect(i).translated(origin);

        if (item.tool == ToolView2D || item.tool == ToolView3D)
        {
          const qreal on = item.tool == ToolView3D ? this->view_t : 1.0 - this->view_t;
          const bool  hover = int(i) == this->hovered;
          QColor      ink = mix_colors(
              mix_colors(surface_color(), colors.text_primary, hover ? 0.9 : 0.55),
              colors.accent.lighter(150),
              on);
          p.setPen(ink);
          p.setFont(meta::qt::ui_font(int(std::round(11 * rail_scale())), true));
          p.drawText(cell, Qt::AlignCenter, item.tool == ToolView2D ? "2D" : "3D");
          continue;
        }

        if (item.separator_before)
        {
          QColor line = colors.text_primary;
          line.setAlphaF(0.10);
          p.setPen(QPen(line, 1));
          if (is_vertical(this->edge))
          {
            const qreal y = cell.top() - sep_px() / 2.0;
            p.drawLine(QPointF(cell.left() + 4, y), QPointF(cell.right() - 4, y));
          }
          else
          {
            const qreal x = cell.left() - sep_px() / 2.0;
            p.drawLine(QPointF(x, cell.top() + 4), QPointF(x, cell.bottom() - 4));
          }
        }

        const bool active = item.tool == this->active_tool;
        const bool hover = int(i) == this->hovered;

        if (active || hover)
        {
          p.setPen(Qt::NoPen);
          p.setBrush(active ? mix_colors(surface_color(), colors.accent, 0.22)
                            : mix_colors(surface_color(), colors.text_primary, 0.07));
          p.drawRoundedRect(cell.adjusted(1, 1, -1, -1), cell_radius, cell_radius);
        }

        QColor ink = mix_colors(surface_color(),
                                colors.text_primary,
                                hover ? 0.95 : 0.72);
        if (active)
          ink = colors.accent.lighter(135);

        if (item.tool == ToolResolution)
        {
          p.setPen(QColor("#8cc97a"));
          p.setFont(meta::qt::ui_font(int(std::round(11 * rail_scale())), true));
          p.drawText(cell, Qt::AlignCenter, this->resolution_text);
        }
        else
          paint_icon(p,
                     item.tool,
                     cell.adjusted(5 * rail_scale(),
                                   5 * rail_scale(),
                                   -5 * rail_scale(),
                                   -5 * rail_scale()),
                     ink);

        if (item.flyout)
        {
          // corner mark: this tool opens something
          QColor mark = ink;
          mark.setAlphaF(0.55);
          p.setPen(QPen(mark, 1.1, Qt::SolidLine, Qt::RoundCap));
          const QPointF br = cell.bottomRight() - QPointF(4.5, 4.5) * rail_scale();
          p.drawLine(br, br - QPointF(3.5 * rail_scale(), 0));
          p.drawLine(br, br - QPointF(0, 3.5 * rail_scale()));
        }
      }
      p.restore();
    }

    // folded: a single settings square
    const qreal gear_alpha = std::clamp(1.0 - this->expand_t / 0.5, 0.0, 1.0);
    if (gear_alpha > 0.0)
    {
      p.setOpacity(gear_alpha);
      const QPointF c = this->square_rect().center() - QPointF(this->pos());
      paint_gear(p,
                 c,
                 20 * rail_scale(),
                 this->dragging ? colors.accent.lighter(135)
                                : mix_colors(surface_color(), colors.text_primary, 0.85));
    }
  }

private:
  QRectF viewport_rect() const
  {
    return this->parentWidget() ? QRectF(this->parentWidget()->rect()) : QRectF();
  }

  qreal length() const
  {
    qreal len = 2 * pad_px();
    for (const Item &item : this->items)
      len += cell_px() + (item.separator_before ? sep_px() : 0);
    return len;
  }

  QSizeF full_size(Edge e) const
  {
    return is_vertical(e) ? QSizeF(thick_px(), this->length())
                          : QSizeF(this->length(), thick_px());
  }

  // cell `i` relative to the docked rail's top-left
  QRectF cell_rect(size_t i) const
  {
    qreal along_pos = pad_px();
    for (size_t k = 0; k <= i && k < this->items.size(); ++k)
    {
      if (this->items[k].separator_before)
        along_pos += sep_px();
      if (k < i)
        along_pos += cell_px();
    }
    const qreal across = (thick_px() - cell_px()) / 2.0;
    return is_vertical(this->edge) ? QRectF(across, along_pos, cell_px(), cell_px())
                                   : QRectF(along_pos, across, cell_px(), cell_px());
  }

  int index_of(int tool) const
  {
    for (size_t i = 0; i < this->items.size(); ++i)
      if (this->items[i].tool == tool)
        return int(i);
    return -1;
  }

  int index_at(const QPointF &local) const
  {
    if (this->expand_t < 0.99)
      return -1;
    const QPointF in_rail = local + QPointF(this->pos()) - this->anchor;
    for (size_t i = 0; i < this->items.size(); ++i)
      if (this->cell_rect(i).contains(in_rail))
        return int(i);
    return -1;
  }

  // centre of a rail docked at `anchor` on `e`
  QPointF docked_center(const QPointF &anchor, Edge e) const
  {
    return QRectF(anchor, this->full_size(e)).center();
  }

  // the folded square, wherever it currently is: it stands for the rail's
  // centre, so the rail folds into it and unfolds out of it symmetrically
  QRectF square_rect() const
  {
    const QPointF c = (this->dragging || this->flying)
                          ? this->drag_center
                          : this->docked_center(this->anchor, this->edge);
    return QRectF(c - QPointF(square_px() / 2.0, square_px() / 2.0),
                  QSizeF(square_px(), square_px()));
  }

  void apply_geometry()
  {
    const QRectF full(this->anchor, this->full_size(this->edge));
    const QRectF sq = this->square_rect();
    const qreal  t = this->expand_t;

    const QRectF r(sq.left() + (full.left() - sq.left()) * t,
                   sq.top() + (full.top() - sq.top()) * t,
                   sq.width() + (full.width() - sq.width()) * t,
                   sq.height() + (full.height() - sq.height()) * t);
    this->setGeometry(r.toAlignedRect());
    this->update();
  }

  // a docked placement: the edge, the rail's top-left, and whether it is
  // centred on that edge (then it stays centred when the viewport resizes)
  struct Dock
  {
    Edge    edge = Edge::Right;
    QPointF anchor;
    bool    centered = false;
  };

  // range of the rail's start along edge `e`
  std::pair<qreal, qreal> along_range(Edge e) const
  {
    const QRectF vr = this->viewport_rect();
    const QSizeF size = this->full_size(e);
    const qreal  lo = (is_vertical(e) ? vr.top() : vr.left()) + kMargin;
    const qreal  hi = std::max(
        lo,
        (is_vertical(e) ? vr.bottom() - size.height() : vr.right() - size.width()) -
            kMargin);
    return {lo, hi};
  }

  qreal middle_along(Edge e) const
  {
    const auto [lo, hi] = this->along_range(e);
    return std::round((lo + hi) / 2.0);
  }

  // Dock on `e` with the rail centred on `along_center`. One rule each, so a
  // spot on the edge always gives one result: centred when the drop is near
  // the middle of the edge, flush into a corner when the rail would end near
  // one, otherwise exactly where it was dropped.
  Dock snap(Edge e, qreal along_center) const
  {
    const QRectF vr = this->viewport_rect();
    const QSizeF size = this->full_size(e);
    const qreal  len = is_vertical(e) ? size.height() : size.width();
    const auto [lo, hi] = this->along_range(e);
    const qreal edge_mid = is_vertical(e) ? vr.center().y() : vr.center().x();

    Dock  dock{e, QPointF(), false};
    qreal pos = std::clamp(along_center - len / 2.0, lo, hi);

    if (std::abs(along_center - edge_mid) < kCenterSnap)
    {
      pos = this->middle_along(e);
      dock.centered = true;
    }
    else if (pos - lo < kMagnet)
      pos = lo;
    else if (hi - pos < kMagnet)
      pos = hi;

    dock.anchor = this->anchor_at(e, pos);
    return dock;
  }

  // top-left of a rail docked on `e`, starting at `pos` along it
  QPointF anchor_at(Edge e, qreal pos) const
  {
    const QRectF vr = this->viewport_rect();
    const QSizeF size = this->full_size(e);
    if (is_vertical(e))
      return QPointF(e == Edge::Left ? vr.left() + kMargin
                                     : vr.right() - kMargin - size.width(),
                     pos);
    return QPointF(pos,
                   e == Edge::Top ? vr.top() + kMargin
                                  : vr.bottom() - kMargin - size.height());
  }

  // whether the rail fits along edge `e`
  bool fits(Edge e) const
  {
    const QRectF vr = this->viewport_rect();
    const qreal  room = is_vertical(e) ? vr.height() : vr.width();
    return this->length() + 2 * kMargin <= room;
  }

  // where a rail dropped with its folded square at `c` docks: the nearest
  // edge it fits along, centred on the drop point
  Dock target_for(const QPointF &c) const
  {
    const QRectF                                vr = this->viewport_rect();
    const std::array<std::pair<Edge, qreal>, 4> edges = {
        {{Edge::Left, c.x() - vr.left()},
         {Edge::Right, vr.right() - c.x()},
         {Edge::Top, c.y() - vr.top()},
         {Edge::Bottom, vr.bottom() - c.y()}}};
    Edge  e = Edge::Bottom;
    qreal best = std::numeric_limits<qreal>::max();
    bool  any_fits = false;
    for (const auto &[candidate, distance] : edges)
    {
      const bool ok = this->fits(candidate);
      // a fitting edge beats any other; among equals, the nearest
      if ((ok && !any_fits) || (ok == any_fits && distance < best))
      {
        e = candidate;
        best = distance;
        any_fits = any_fits || ok;
      }
    }

    return this->snap(e, is_vertical(e) ? c.y() : c.x());
  }

  void land(const Dock &dock)
  {
    this->edge = dock.edge;
    this->anchor = dock.anchor;
    this->centered = dock.centered;
    this->along = is_vertical(dock.edge) ? dock.anchor.y() : dock.anchor.x();

    // the user's choice, as a fraction of the free range along the edge
    const auto [lo, hi] = this->along_range(dock.edge);
    this->pref_edge = dock.edge;
    this->pref_centered = dock.centered;
    this->pref_t = hi > lo ? std::clamp((this->along - lo) / (hi - lo), 0.0, 1.0) : 0.0;
    this->placed = true;
  }

  QPointF clamp_center(const QPointF &p) const
  {
    const QRectF vr = this->viewport_rect().adjusted(square_px() / 2.0,
                                                     square_px() / 2.0,
                                                     -square_px() / 2.0,
                                                     -square_px() / 2.0);
    return QPointF(std::clamp(p.x(), vr.left(), std::max(vr.left(), vr.right())),
                   std::clamp(p.y(), vr.top(), std::max(vr.top(), vr.bottom())));
  }

  void update_guide()
  {
    const Dock dock = this->target_for(this->drag_center);
    this->guide->set_target(QRectF(dock.anchor, this->full_size(dock.edge)));
  }

  void animate_expand(qreal to, int ms)
  {
    this->expand_anim->stop();
    this->expand_anim->setStartValue(this->expand_t);
    this->expand_anim->setEndValue(to);
    this->expand_anim->setDuration(anim_ms(ms));
    this->expand_anim->start();
    if (anim_ms(ms) == 0)
    {
      this->expand_t = to;
      this->apply_geometry();
    }
  }

  void begin_drag(const QPointF &in_parent)
  {
    this->owner->close_panel(false);
    this->dragging = true;
    this->hovered = -1;
    this->setCursor(Qt::ClosedHandCursor);

    // the square starts where the press was, then follows the cursor
    this->drag_center = this->clamp_center(in_parent);
    this->guide->appear();
    this->update_guide();
    this->raise();
    this->animate_expand(0.0, 160);
  }

  void end_drag()
  {
    this->dragging = false;
    this->setCursor(Qt::OpenHandCursor);
    this->guide->vanish();

    this->pending = this->target_for(this->drag_center);
    const QPointF landing = this->docked_center(this->pending.anchor, this->pending.edge);

    // glide the folded square to where the first tool will be, then unfold
    this->flying = true;
    this->expand_anim->stop();
    this->expand_t = 0.0;
    this->flight->stop();
    this->flight->setStartValue(this->drag_center);
    this->flight->setEndValue(landing);
    this->flight->setDuration(anim_ms(200));
    this->flight->start();
    if (anim_ms(200) == 0)
    {
      this->drag_center = landing;
      this->flight->stop();
      this->flying = false;
      this->land(this->pending);
      this->expand_t = 1.0;
      this->apply_geometry();
      this->owner->notify_layout_changed();
    }
  }

  ViewportControls *owner = nullptr;
  SnapGuide        *guide = nullptr;
  std::vector<Item> items;
  QString           resolution_text = "1K";
  int               active_tool = -1;
  int               hovered = -1;

  // docked placement, as shown now (derived in place())
  Edge    edge = Edge::Right;
  qreal   along = 0.0; // position of the rail's start along its edge
  bool    centered = false;
  QPointF anchor; // docked rail's top-left, parent coordinates

  // ... and as the user chose it (saved with the project)
  bool  placed = false; // false: the default spot, below the gizmo
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

  qreal              view_t = 1.0; // 2D/3D switch highlight: 0 on 2D, 1 on 3D
  QVariantAnimation *view_anim = nullptr;
};

} // namespace hesiod::viewport
