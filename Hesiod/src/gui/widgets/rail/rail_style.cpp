/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <cmath>

#include <QIcon>
#include <QPixmap>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/rail/rail_style.hpp"

namespace hesiod::rail
{

QColor surface_color()
{
  const auto &c = HSD_CTX.app_settings.colors;
  return mix_colors(c.bg_deep, c.bg_primary, 0.80);
}

QColor border_color()
{
  return panel_border_color(); // the card border, shared
}

bool animations_on() { return HSD_CTX.app_settings.interface.enable_ui_animations; }

int anim_ms(int ms) { return animations_on() ? ms : 0; }

qreal rail_scale()
{
  return std::clamp(HSD_CTX.app_settings.viewer.toolbar_scale, 50, 200) / 100.0;
}

qreal scaled(int base) { return std::round(base * rail_scale()); }

qreal cell_px() { return scaled(kCell); }
qreal pad_px() { return scaled(kPad); }
qreal thick_px() { return scaled(kThick); }
qreal square_px() { return scaled(kSquare); }
qreal sep_px() { return scaled(kSep); }
qreal radius_px() { return scaled(kRadius); }

void paint_svg(QPainter      &p,
               const char    *name,
               const QPointF &c,
               qreal          size,
               const QColor  &ink,
               bool           tint)
{
  const qreal dpr = p.device() ? p.device()->devicePixelRatioF() : 1.0;
  const int   side = int(std::ceil(size));
  QPixmap     pixmap = HSD_ICON(name).pixmap(QSize(side, side), dpr);
  if (pixmap.isNull())
    return;

  p.save();
  if (tint)
  {
    QPainter recolor(&pixmap);
    recolor.setCompositionMode(QPainter::CompositionMode_SourceIn);
    recolor.fillRect(pixmap.rect(), ink);
  }
  else
    p.setOpacity(p.opacity() * ink.alphaF());
  p.drawPixmap(QRectF(c.x() - side / 2.0, c.y() - side / 2.0, side, side),
               pixmap,
               QRectF(pixmap.rect()));
  p.restore();
}

void paint_gear(QPainter &p, const QPointF &c, qreal size, const QColor &ink)
{
  paint_svg(p, "settings", c, size, ink);
}

} // namespace hesiod::rail
