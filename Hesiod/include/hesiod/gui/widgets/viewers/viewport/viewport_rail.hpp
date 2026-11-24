/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include "hesiod/gui/widgets/rail/tool_rail.hpp"

class QVariantAnimation;

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

  // 0: 2D, 1: 3D; the switch's highlight slides across
  void set_view_mode(int type, bool animate);

protected:
  void paint_under_tools(QPainter &p, const QPointF &origin) override;
  bool paint_cell(QPainter &p, const Item &item, const QRectF &cell, bool hover) override;
  void paint_tool(QPainter     &p,
                  const Item   &item,
                  const QRectF &cell,
                  const QColor &ink) override;

private:
  QString            resolution_text = "1K";
  qreal              view_t = 1.0; // 2D/3D switch highlight: 0 on 2D, 1 on 3D
  QVariantAnimation *view_anim = nullptr;
};

} // namespace hesiod::viewport
