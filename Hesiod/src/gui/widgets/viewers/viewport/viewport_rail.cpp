/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <cmath>

#include <QPainter>
#include <QVariantAnimation>

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_rail.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

ViewportRail::ViewportRail(rail::RailOwner *owner,
                           rail::SnapGuide *guide,
                           QWidget         *parent)
    : rail::ToolRail(owner, guide, parent)
{
  this->icon_painter = paint_icon;

  // below the 3D view's gizmo
  this->default_edge = Edge::Right;
  this->default_offset = 140.0;
}

void ViewportRail::set_resolution_text(const QString &text)
{
  this->resolution_text = text;
  this->update();
}

void ViewportRail::paint_tool(QPainter     &p,
                              const Item   &item,
                              const QRectF &cell,
                              const QColor &ink)
{
  if (item.tool != ToolResolution)
    return rail::ToolRail::paint_tool(p, item, cell, ink);

  p.setPen(QColor("#8cc97a"));
  p.setFont(meta::qt::ui_font(int(std::round(11 * rail_scale())), true));
  p.drawText(cell, Qt::AlignCenter, this->resolution_text);
}

} // namespace hesiod::viewport
