/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */

#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/node_widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"

namespace hesiod::viewport
{

void paint_icon(QPainter &p, int tool, const QRectF &r, const QColor &ink)
{
  const qreal size = std::min(r.width(), r.height());
  const char *name = nullptr;
  switch (tool)
  {
  case ToolLighting:
  case ToolLighting2D:
    name = "vp_sun";
    break;
  case ToolCamera:
    name = "vp_camera";
    break;
  case ToolDisplay:
    name = "vp_layers";
    break;
  case ToolMaterial:
    name = "vp_material";
    break;
  case ToolWater:
    name = "vp_water";
    break;
  case ToolPreview:
    name = "vp_eye";
    break;
  case ToolSky:
    name = "vp_cloud";
    break;
  case ToolColormap2D:
    // the gradient is the point of this one: not recoloured
    paint_svg(p, "vp_colormap", r.center(), size, ink, false);
    return;
  case ToolMore:
    name = "vp_tune";
    break;
  default:
    return;
  }
  paint_svg(p, name, r.center(), size, ink);
}

void paint_reset(QPainter &p, const QPointF &c, qreal size, const QColor &ink)
{
  paint_svg(p, "settings_backup_restore", c, size, ink);
}

QString tool_title(int tool)
{
  switch (tool)
  {
  case ToolLighting:
  case ToolLighting2D:
    return "Lighting";
  case ToolCamera:
    return "Camera";
  case ToolDisplay:
    return "Display";
  case ToolMaterial:
    return "Material";
  case ToolWater:
    return "Water";
  case ToolPreview:
    return "Preview";
  case ToolSky:
    return "Sky & Atmosphere";
  case ToolColormap2D:
    return "Colormap";
  default:
    return QString();
  }
}

// per-section accents: a panel reads as a set of coloured groups, like the
// node settings do
const meta::qt::Theme &viewport_theme()
{
  static const meta::qt::Theme theme = []()
  {
    meta::qt::Theme t = *properties_panel_design().theme;
    t.group_accents = {{"Sun", QColor("#cfa143")},
                       {"Light", QColor("#c06478")},
                       {"Camera", QColor("#7d9cc0")},
                       {"Navigation", QColor("#3aa899")},
                       {"Layers", QColor("#7aa86a")},
                       {"Debug", QColor("#9a9a9a")},
                       {"Albedo", QColor("#d98a5e")},
                       {"Normal Map", QColor("#a08bb8")},
                       {"Water", QColor("#4a9ecc")},
                       {"Foam", QColor("#8fb8cf")},
                       {"Waves", QColor("#3aa899")},
                       {"Sky", QColor("#7d9cc0")},
                       {"Fog", QColor("#9a9a9a")},
                       {"Scattering", QColor("#a08bb8")},
                       {"Display", QColor("#3aa899")},
                       {"Background", QColor("#9a9a9a")}};
    return t;
  }();
  return theme;
}

std::string resolution_label(int res)
{
  if (res >= 1024 && res % 1024 == 0)
    return std::to_string(res / 1024) + "K";
  return std::to_string(res);
}

} // namespace hesiod::viewport
