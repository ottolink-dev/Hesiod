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

protected:
  void paint_tool(QPainter     &p,
                  const Item   &item,
                  const QRectF &cell,
                  const QColor &ink) override;

private:
  QString resolution_text = "1K";
};

} // namespace hesiod::viewport
