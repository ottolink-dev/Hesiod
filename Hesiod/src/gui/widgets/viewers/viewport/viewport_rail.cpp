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
}

void ViewportRail::set_resolution_text(const QString &text)
{
  this->resolution_text = text;
  this->update();
}

void ViewportRail::set_view_mode(int type, bool animate)
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

void ViewportRail::paint_under_tools(QPainter &p, const QPointF &origin)
{
  // the 2D / 3D switch: a shallow track and a highlight sliding between its
  // two cells
  const int i2d = this->index_of(ToolView2D);
  const int i3d = this->index_of(ToolView3D);
  if (i2d < 0 || i3d < 0)
    return;

  const auto  &colors = HSD_CTX.app_settings.colors;
  const qreal  cell_radius = this->cell_radius();
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

bool ViewportRail::paint_cell(QPainter     &p,
                              const Item   &item,
                              const QRectF &cell,
                              bool          hover)
{
  if (item.tool != ToolView2D && item.tool != ToolView3D)
    return false;

  const auto &colors = HSD_CTX.app_settings.colors;
  const qreal on = item.tool == ToolView3D ? this->view_t : 1.0 - this->view_t;
  QColor      ink = mix_colors(
      mix_colors(surface_color(), colors.text_primary, hover ? 0.9 : 0.55),
      colors.accent.lighter(150),
      on);
  p.setPen(ink);
  p.setFont(meta::qt::ui_font(int(std::round(11 * rail_scale())), true));
  p.drawText(cell, Qt::AlignCenter, item.tool == ToolView2D ? "2D" : "3D");
  return true;
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
