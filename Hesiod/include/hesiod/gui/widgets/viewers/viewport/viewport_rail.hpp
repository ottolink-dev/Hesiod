/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include "hesiod/gui/motion/motion.hpp"
#include "hesiod/gui/widgets/rail/tool_rail.hpp"

namespace hesiod::viewport
{

// =====================================
// ViewportRail
// =====================================

// The viewport's toolbar: a ToolRail with the viewport's tools (see Tool), the
// 2D / 3D switch and the graph's preview resolution among them.
class ViewportRail final : public rail::ToolRail
{
public:
  ViewportRail(rail::RailOwner *owner, rail::SnapGuide *guide, QWidget *parent);

  void set_resolution_text(const QString &text);

  // 0: 2D, 1: 3D; the switch's highlight springs across
  void set_view_mode(int type, bool animate);

protected:
  void paint_under_tools(QPainter &p, const QPointF &origin) override;
  bool paint_cell(QPainter &p, const Item &item, const QRectF &cell, qreal heat) override;
  void paint_tool(QPainter     &p,
                  const Item   &item,
                  const QRectF &cell,
                  const QColor &ink) override;
  bool uses_hover_pill(const Item &item) const override;

private:
  QString resolution_text = "1K";
  Anim    view_t{this, 1.0}; // 2D/3D switch highlight: 0 on 2D, 1 on 3D
};

} // namespace hesiod::viewport
