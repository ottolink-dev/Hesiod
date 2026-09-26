/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>
#include <any>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <set>

#include <QActionGroup>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/choice.hpp"
#include "meta/presets/glm.hpp"
#include "meta/presets/numeric.hpp"
#include "meta_qt/container_widget.hpp"
#include "meta_qt/designs/industrial/panel_chrome.hpp"
#include "meta_qt/ui/theme.hpp"

#include "qtr/keys.hpp"
#include "qtr/render_widget.hpp"
#include "qtr/water_colors.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/menu_chrome.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/viewers/viewport_controls.hpp"
#include "hesiod/gui/widgets/window_chrome.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

namespace
{

// --- tools (rail entries); values are persisted nowhere, order is free
enum Tool : int
{
  ToolView2D, // the 2D / 3D switch: two cells sharing one sliding highlight
  ToolView3D,
  ToolResolution,
  ToolLighting,
  ToolCamera,
  ToolDisplay,
  ToolMaterial,
  ToolWater,
  ToolSky,
  ToolLighting2D,
  ToolColormap2D,
  ToolPreview, // which node outputs the view shows (hosts the viewer's own rows)
  ToolMore
};

enum class Edge : int
{
  Left,
  Right,
  Top,
  Bottom
};

bool is_vertical(Edge e) { return e == Edge::Left || e == Edge::Right; }

// --- rail metrics (logical px)
constexpr int kCell = 34;       // one tool button
constexpr int kPad = 4;         // rail end padding
constexpr int kThick = 42;      // rail thickness
constexpr int kSquare = 38;     // folded rail while dragging
constexpr int kSep = 9;         // separator slot
constexpr int kMargin = 10;     // distance from the viewport edges
constexpr int kMagnet = 28;     // snap to a corner within this distance
constexpr int kCenterSnap = 32; // drop this close to an edge's middle to centre on it
constexpr int kRadius = 12;     // rail corners; cells use kRadius minus their inset

constexpr double kPi = std::numbers::pi;

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

// The rail's size follows the "Viewport toolbar size" setting (percent); the
// metrics above are its 100 % values. Rounded to whole pixels so the cells
// and hairlines stay crisp.
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

// ---------------------------------------------------------------------------
// icons, drawn as strokes on a 24 px grid so they share one weight
// ---------------------------------------------------------------------------

void paint_gear(QPainter &p, const QPointF &c, qreal size, const QColor &ink)
{
  const qreal s = size / 24.0;
  p.save();
  p.translate(c);
  p.setPen(Qt::NoPen);
  p.setBrush(ink);

  QPainterPath gear;
  gear.addEllipse(QPointF(0, 0), 7.2 * s, 7.2 * s);
  for (int i = 0; i < 8; ++i)
  {
    QPainterPath tooth;
    tooth.addRoundedRect(QRectF(-1.9 * s, -10.2 * s, 3.8 * s, 5.0 * s), 1.0 * s, 1.0 * s);
    QTransform t;
    t.rotate(i * 45.0);
    gear = gear.united(t.map(tooth));
  }
  QPainterPath hole;
  hole.addEllipse(QPointF(0, 0), 3.2 * s, 3.2 * s);
  p.drawPath(gear.subtracted(hole));
  p.restore();
}

void paint_icon(QPainter &p, int tool, const QRectF &r, const QColor &ink)
{
  const QPointF c = r.center();
  const qreal   s = std::min(r.width(), r.height()) / 24.0;

  p.save();
  QPen pen(ink, 1.5 * std::max(1.0, s * 0.9), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  switch (tool)
  {
  case ToolLighting:
  case ToolLighting2D:
  {
    p.drawEllipse(c, 3.6 * s, 3.6 * s);
    for (int i = 0; i < 8; ++i)
    {
      const qreal   a = i * kPi / 4.0;
      const QPointF d(std::cos(a), std::sin(a));
      p.drawLine(c + d * 6.4 * s, c + d * 8.6 * s);
    }
    break;
  }
  case ToolCamera:
  {
    const QRectF body(c.x() - 8 * s, c.y() - 4.5 * s, 16 * s, 11.5 * s);
    QPainterPath path;
    path.addRoundedRect(body, 2.4 * s, 2.4 * s);
    path.moveTo(c.x() - 3.2 * s, body.top());
    path.lineTo(c.x() - 2.0 * s, body.top() - 2.4 * s);
    path.lineTo(c.x() + 2.0 * s, body.top() - 2.4 * s);
    path.lineTo(c.x() + 3.2 * s, body.top());
    p.drawPath(path);
    p.drawEllipse(QPointF(c.x(), c.y() + 1.2 * s), 3.2 * s, 3.2 * s);
    break;
  }
  case ToolDisplay:
  {
    // three stacked layers
    for (int i = 0; i < 3; ++i)
    {
      const qreal  y = c.y() - 4.5 * s + i * 4.5 * s;
      QPainterPath layer;
      layer.moveTo(c.x(), y - 3.2 * s);
      layer.lineTo(c.x() + 8 * s, y);
      layer.lineTo(c.x(), y + 3.2 * s);
      layer.lineTo(c.x() - 8 * s, y);
      layer.closeSubpath();
      if (i == 0)
        p.drawPath(layer);
      else
      {
        // only the front edges of the lower layers show
        p.drawLine(QPointF(c.x() - 8 * s, y), QPointF(c.x(), y + 3.2 * s));
        p.drawLine(QPointF(c.x(), y + 3.2 * s), QPointF(c.x() + 8 * s, y));
      }
    }
    break;
  }
  case ToolMaterial:
  {
    const qreal rad = 7.8 * s;
    p.drawEllipse(c, rad, rad);
    // right half hatched: a lit sphere
    QPainterPath half;
    half.moveTo(c + QPointF(0, -rad));
    half.arcTo(QRectF(c.x() - rad, c.y() - rad, 2 * rad, 2 * rad), 90, -180);
    half.closeSubpath();
    p.save();
    p.setClipPath(half);
    for (qreal k = -2 * rad; k < 2 * rad; k += 3.2 * s)
      p.drawLine(QPointF(c.x() + k, c.y() + rad),
                 QPointF(c.x() + k + 2 * rad, c.y() - rad));
    p.restore();
    p.drawLine(c + QPointF(0, -rad), c + QPointF(0, rad));
    break;
  }
  case ToolWater:
  {
    for (int row = 0; row < 3; ++row)
    {
      QPainterPath wave;
      const qreal  y = c.y() - 5 * s + row * 5 * s;
      wave.moveTo(c.x() - 8.5 * s, y);
      for (int i = 1; i <= 34; ++i)
      {
        const qreal x = -8.5 + i * 0.5;
        wave.lineTo(c.x() + x * s, y + std::sin(x * 0.75) * 1.4 * s);
      }
      p.drawPath(wave);
    }
    break;
  }
  case ToolPreview:
  {
    // eye: two arcs meeting at the corners, and the iris
    const qreal  w = 9.0 * s;
    const qreal  h = 5.6 * s;
    QPainterPath eye;
    eye.moveTo(c.x() - w, c.y());
    eye.quadTo(QPointF(c.x(), c.y() - 2 * h), QPointF(c.x() + w, c.y()));
    eye.quadTo(QPointF(c.x(), c.y() + 2 * h), QPointF(c.x() - w, c.y()));
    p.drawPath(eye);
    p.drawEllipse(c, 2.8 * s, 2.8 * s);
    break;
  }
  case ToolSky:
  {
    // cloud: one outline, the union of three bumps over a flat base whose
    // bottom is tangent to the side bumps
    const auto disc = [&](qreal x, qreal y, qreal rad)
    {
      QPainterPath d;
      d.addEllipse(QPointF(c.x() + x * s, c.y() + y * s), rad * s, rad * s);
      return d;
    };
    QPainterPath cloud;
    cloud.addRect(QRectF(c.x() - 4.6 * s, c.y() + 1.4 * s, 9.2 * s, 3.4 * s));
    cloud = cloud.united(disc(-4.6, 1.4, 3.4));
    cloud = cloud.united(disc(4.6, 1.4, 3.4));
    cloud = cloud.united(disc(0.2, -1.4, 5.0));
    p.drawPath(cloud.simplified());
    break;
  }
  case ToolColormap2D:
  {
    const QRectF    box(c.x() - 8 * s, c.y() - 8 * s, 16 * s, 16 * s);
    QLinearGradient g(box.topLeft(), box.bottomRight());
    g.setColorAt(0.0, QColor("#3b0f70"));
    g.setColorAt(0.5, QColor("#de4968"));
    g.setColorAt(1.0, QColor("#fcfdbf"));
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.setOpacity(p.opacity() * 0.85);
    p.drawRoundedRect(box, 3 * s, 3 * s);
    p.restore();
    p.drawRoundedRect(box, 3 * s, 3 * s);
    break;
  }
  case ToolMore:
  {
    // sliders: three tracks, each broken around its knob
    const qreal knob = 2.1;
    for (const auto &[y, x] : {std::pair{-5.5, -3.0}, {0.0, 3.5}, {5.5, -0.5}})
    {
      const QPointF k = c + QPointF(x * s, y * s);
      p.drawLine(c + QPointF(-8 * s, y * s), k - QPointF(knob * s, 0));
      p.drawLine(k + QPointF(knob * s, 0), c + QPointF(8 * s, y * s));
      p.drawEllipse(k, knob * s, knob * s);
    }
    break;
  }
  default:
    break;
  }
  p.restore();
}

// "open reset" glyph for the panel header
void paint_reset(QPainter &p, const QPointF &c, qreal size, const QColor &ink)
{
  const qreal s = size / 24.0;
  p.save();
  p.setPen(QPen(ink, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);

  // a counter-clockwise arc with its head at the top, pointing on around the
  // circle: the arrowhead is built from the arc's own tangent there
  const qreal  R = 6.8 * s;
  const qreal  head_deg = 105.0; // Qt angles: counter-clockwise from 3 o'clock
  const qreal  sweep = 300.0;
  const QRectF ring(c.x() - R, c.y() - R, 2 * R, 2 * R);

  QPainterPath arc;
  arc.arcMoveTo(ring, head_deg - sweep);
  arc.arcTo(ring, head_deg - sweep, sweep);
  p.drawPath(arc);

  const qreal   a = head_deg * kPi / 180.0;
  const QPointF end = c + QPointF(std::cos(a), -std::sin(a)) * R;
  const QPointF dir(-std::sin(a), -std::cos(a)); // counter-clockwise, screen space
  const QPointF normal(dir.y(), -dir.x());
  const QPointF tip = end + dir * (1.4 * s);
  QPainterPath  head;
  head.moveTo(tip - dir * (3.6 * s) + normal * (3.2 * s));
  head.lineTo(tip);
  head.lineTo(tip - dir * (3.6 * s) - normal * (3.2 * s));
  p.drawPath(head);
  p.restore();
}

// ---------------------------------------------------------------------------
// settings specs: what each panel shows, and how a UI value maps onto the
// renderer's own (radians, inverted flags, zenith angles...)
// ---------------------------------------------------------------------------

struct Spec
{
  enum class Type
  {
    Bool,
    Float,
    Color,
    Choice,
    WaterPreset // not a renderer setting: sets both water colours
  };

  Type        type = Type::Float;
  std::string key; // renderer key (and attribute name)
  std::string label;
  std::string category;
  float       vmin = 0.f;
  float       vmax = 1.f;
  std::string format = "{:.2f}";
  bool        angle = false;            // radians in the renderer, degrees here
  bool        invert = false;           // bool shown the other way round
  bool        elevation_zenith = false; // renderer: zenith angle; here: elevation

  std::vector<std::pair<int, std::string>> items; // Choice
};

Spec fspec(std::string key,
           std::string label,
           std::string category,
           float       vmin,
           float       vmax,
           std::string format = "{:.2f}")
{
  Spec s;
  s.type = Spec::Type::Float;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  s.vmin = vmin;
  s.vmax = vmax;
  s.format = std::move(format);
  return s;
}

Spec aspec(std::string key,
           std::string label,
           std::string category,
           float       vmin,
           float       vmax)
{
  Spec s = fspec(std::move(key),
                 std::move(label),
                 std::move(category),
                 vmin,
                 vmax,
                 "{:.1f}");
  s.angle = true;
  return s;
}

Spec bspec(std::string key, std::string label, std::string category, bool invert = false)
{
  Spec s;
  s.type = Spec::Type::Bool;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  s.invert = invert;
  return s;
}

Spec cspec(std::string key, std::string label, std::string category)
{
  Spec s;
  s.type = Spec::Type::Color;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  return s;
}

Spec chspec(std::string                              key,
            std::string                              label,
            std::string                              category,
            std::vector<std::pair<int, std::string>> items)
{
  Spec s;
  s.type = Spec::Type::Choice;
  s.key = std::move(key);
  s.label = std::move(label);
  s.category = std::move(category);
  s.items = std::move(items);
  return s;
}

std::string visible_key(const std::string &mesh) { return "visible." + mesh; }

std::vector<Spec> specs_for(int tool)
{
  switch (tool)
  {
  case ToolLighting:
    return {aspec("light_phi", "Sun Azimuth", "Sun", -180.f, 180.f),
            aspec("light_theta", "Sun Elevation", "Sun", 0.f, 90.f),
            bspec("auto_rotate_light", "Auto Rotate", "Sun"),
            bspec("bypass_shadow_map", "Shadows", "Light", true),
            fspec("shadow_strength", "Shadow Strength", "Light", 0.f, 1.f),
            chspec("shadow_map_resolution",
                   "Shadow Resolution",
                   "Light",
                   {{512, "512"},
                    {1024, "1024"},
                    {2048, "2048"},
                    {4096, "4096"},
                    {8192, "8192"}}),
            bspec("add_ambiant_occlusion", "Ambient Occlusion", "Light"),
            fspec("ambiant_occlusion_strength", "AO Strength", "Light", 0.f, 1.f),
            fspec("ambiant_occlusion_radius", "AO Radius", "Light", 0.f, 0.5f, "{:.3f}")};

  case ToolCamera:
  {
    Spec fov = aspec("fov", "Field of View", "Camera", 10.f, 180.f);
    fov.format = "{:.0f}";
    return {fov,
            fspec("scale_h", "Height Scale", "Camera", 0.f, 2.f),
            bspec("show_orientation_gizmo", "Orientation Gizmo", "Camera"),
            bspec("keyboard_navigation_enabled", "Keyboard Controls", "Navigation"),
            chspec("keyboard_layout",
                   "Layout",
                   "Navigation",
                   {{0, "WASD (QWERTY)"}, {1, "ZQSD (AZERTY)"}}),
            fspec("camera_move_speed", "Move Speed", "Navigation", 0.1f, 10.f)};
  }

  case ToolDisplay:
    // terrain, water, points and path visibility are the Preview panel's eyes
    return {bspec(visible_key(qtr::keys::mesh::plane), "Ground Plane", "Layers"),
            bspec("wireframe_mode", "Wireframe", "Debug"),
            bspec("normal_visualization", "Normals", "Debug")};

  case ToolMaterial:
    // the albedo texture on/off is the Preview panel's texture eye
    return {fspec("gamma_correction", "Gamma", "Albedo", 0.01f, 4.f),
            bspec("apply_tonemap", "Tonemap", "Albedo"),
            fspec("normal_map_scaling", "Normal Strength", "Normal Map", 0.f, 2.f)};

  case ToolWater:
  {
    Spec preset;
    preset.type = Spec::Type::WaterPreset;
    preset.key = "water_preset";
    preset.label = "Preset";
    preset.category = "Water";
    int i = 0;
    for (const auto &[name, _] : qtr::water_colors)
    {
      std::string pretty = name;
      std::replace(pretty.begin(), pretty.end(), '_', ' ');
      if (!pretty.empty())
        pretty[0] = char(std::toupper(pretty[0]));
      preset.items.push_back({i++, pretty});
    }

    Spec angle = aspec("waves_alpha", "Wave Angle", "Waves", -180.f, 180.f);
    return {
        preset,
        cspec("color_shallow_water", "Shallow Color", "Water"),
        cspec("color_deep_water", "Deep Color", "Water"),
        fspec("water_color_depth", "Color Depth", "Water", 0.f, 0.2f, "{:.3f}"),
        fspec("water_spec_strength", "Specularity", "Water", 0.f, 1.f),
        bspec("add_water_foam", "Foam", "Foam"),
        fspec("foam_depth", "Foam Depth", "Foam", 0.f, 0.1f, "{:.3f}"),
        bspec("add_water_waves", "Waves", "Waves"),
        fspec("waves_kw", "Wavenumber", "Waves", 0.f, 2048.f, "{:.0f}"),
        fspec("waves_amplitude", "Amplitude", "Waves", 0.f, 0.1f, "{:.3f}"),
        fspec("waves_normal_amplitude", "Normal Amplitude", "Waves", 0.f, 0.1f, "{:.3f}"),
        angle,
        fspec("angle_spread_ratio", "Angle Spread", "Waves", 0.f, 0.1f, "{:.3f}"),
        bspec("animate_waves", "Animate", "Waves"),
        fspec("waves_speed", "Speed", "Waves", 0.f, 1.f)};
  }

  case ToolSky:
    return {chspec("background_mode",
                   "Background",
                   "Background",
                   {{0, "Sky"}, {1, "Void: black, with a grid"}}),
            bspec("show_skybox", "Skybox", "Sky"),
            chspec("skybox_mode", "Mode", "Sky", {{0, "Uniform color"}, {1, "Image"}}),
            cspec("skybox_color", "Sky Color", "Sky"),
            aspec("skybox_rotation", "Rotation", "Sky", -180.f, 180.f),
            bspec("add_fog", "Fog", "Fog"),
            fspec("fog_density", "Density", "Fog", 0.f, 100.f, "{:.1f}"),
            fspec("fog_height", "Height", "Fog", 0.f, 1.f),
            bspec("fog_match_skybox", "Match Skybox", "Fog"),
            cspec("fog_color", "Fog Color", "Fog"),
            bspec("add_atmospheric_scattering", "Scattering", "Scattering"),
            fspec("scattering_density", "Density", "Scattering", 0.f, 1.f),
            fspec("fog_strength", "Fog Strength", "Scattering", 0.f, 1.f),
            fspec("fog_scattering_ratio", "Scattering Ratio", "Scattering", 0.f, 1.f),
            cspec("rayleigh_color", "Rayleigh Color", "Scattering"),
            cspec("mie_color", "Mie Color", "Scattering")};

  case ToolLighting2D:
  {
    Spec elevation = aspec("2d.sun_zenith", "Sun Elevation", "Sun", 0.f, 90.f);
    elevation.elevation_zenith = true;
    return {aspec("2d.sun_azimuth", "Sun Azimuth", "Sun", -180.f, 180.f),
            elevation,
            bspec("2d.hillshading", "Hillshading", "Sun")};
  }

  case ToolColormap2D:
    return {chspec("2d.colormap",
                   "Colormap",
                   "Display",
                   {{0, "Gray"}, {1, "Viridis"}, {2, "Turbo"}, {3, "Magma"}})};

  default:
    return {};
  }
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

} // namespace

// =====================================
// PanelModel: a Meta container mirroring a set of renderer settings
// =====================================

class PanelModel
{
public:
  PanelModel(qtr::RenderWidget *renderer, std::vector<Spec> specs)
      : renderer(renderer), specs(std::move(specs))
  {
    for (const Spec &spec : this->specs)
      this->add(spec);
  }

  // Controls hold subscriptions to the attributes; they must be gone before
  // the container is.
  ~PanelModel() = default;

  meta::AttributeContainer &get_container() { return this->container; }

  meta::Attribute<float> *float_attr(const std::string &key)
  {
    auto it = this->floats.find(key);
    return it == this->floats.end() ? nullptr : it->second;
  }

  std::vector<std::string> categories() const
  {
    std::vector<std::string> out;
    for (const Spec &spec : this->specs)
      if (std::find(out.begin(), out.end(), spec.category) == out.end())
        out.push_back(spec.category);
    return out;
  }

  // default of `key` in the attribute's own (UI) type, for the modified state
  std::any default_ui(const std::string &key) const
  {
    const Spec *spec = this->find(key);
    if (!spec || !this->renderer)
      return {};

    if (spec->type == Spec::Type::WaterPreset)
      return this->preset_index_for(glm::vec3(0.25f, 0.85f, 0.80f));

    const std::any raw = this->renderer->default_setting(key);
    if (!raw.has_value())
      return {};

    switch (spec->type)
    {
    case Spec::Type::Bool:
    {
      const bool v = std::any_cast<bool>(raw);
      return spec->invert ? !v : v;
    }
    case Spec::Type::Float:
      return this->to_ui(*spec, std::any_cast<float>(raw));
    case Spec::Type::Color:
      return glm::vec4(std::any_cast<glm::vec3>(raw), 1.f);
    case Spec::Type::Choice:
      return std::any_cast<int>(raw);
    default:
      return {};
    }
  }

  // pull the renderer's current values (they change behind the panel: project
  // load, the orientation gizmo, auto rotation...)
  void sync_from_renderer()
  {
    if (!this->renderer)
      return;

    this->syncing = true;
    for (const Spec &spec : this->specs)
    {
      switch (spec.type)
      {
      case Spec::Type::Bool:
        if (bool *p = this->renderer->bool_setting(spec.key))
          this->set_if_changed(this->bools[spec.key], spec.invert ? !*p : *p);
        break;
      case Spec::Type::Float:
        if (float *p = this->renderer->float_setting(spec.key))
          this->set_if_changed(this->floats[spec.key], this->to_ui(spec, *p));
        break;
      case Spec::Type::Color:
        if (glm::vec3 *p = this->renderer->color_setting(spec.key))
          this->set_if_changed(this->colors[spec.key], glm::vec4(*p, 1.f));
        break;
      case Spec::Type::Choice:
        this->set_if_changed(this->ints[spec.key],
                             this->renderer->get_int_setting(spec.key));
        break;
      case Spec::Type::WaterPreset:
        if (glm::vec3 *p = this->renderer->color_setting("color_shallow_water"))
          this->set_if_changed(this->ints[spec.key], this->preset_index_for(*p));
        break;
      }
    }
    this->syncing = false;
  }

  void reset_to_defaults()
  {
    for (const Spec &spec : this->specs)
    {
      const std::any v = this->default_ui(spec.key);
      if (!v.has_value())
        continue;
      if (spec.type == Spec::Type::WaterPreset)
        continue; // the colours below reset the look already
      if (auto it = this->bools.find(spec.key); it != this->bools.end())
        it->second->set_value(std::any_cast<bool>(v));
      else if (auto it = this->floats.find(spec.key); it != this->floats.end())
        it->second->set_value(std::any_cast<float>(v));
      else if (auto it = this->colors.find(spec.key); it != this->colors.end())
        it->second->set_value(std::any_cast<glm::vec4>(v));
      else if (auto it = this->ints.find(spec.key); it != this->ints.end())
        it->second->set_value(std::any_cast<int>(v));
    }
  }

private:
  const Spec *find(const std::string &key) const
  {
    for (const Spec &spec : this->specs)
      if (spec.key == key)
        return &spec;
    return nullptr;
  }

  static float to_ui(const Spec &spec, float value)
  {
    if (!spec.angle)
      return value;
    const float deg = value * 180.f / float(kPi);
    return spec.elevation_zenith ? 90.f - deg : deg;
  }

  static float from_ui(const Spec &spec, float value)
  {
    if (!spec.angle)
      return value;
    const float deg = spec.elevation_zenith ? 90.f - value : value;
    return deg * float(kPi) / 180.f;
  }

  int preset_index_for(const glm::vec3 &shallow) const
  {
    int   i = 0, best = 0;
    float best_d = 1e9f;
    for (const auto &[name, pair] : qtr::water_colors)
    {
      const glm::vec3 d = pair.first - shallow;
      const float     dist = glm::dot(d, d);
      if (dist < best_d)
      {
        best_d = dist;
        best = i;
      }
      ++i;
    }
    return best;
  }

  template <typename T>
  static void set_if_changed(meta::Attribute<T> *attr, const T &value)
  {
    if (attr && !(attr->value() == value))
      attr->set_value(value);
  }

  // floats come back through a unit conversion (degrees <-> radians): only a
  // real change counts, not the rounding of the round trip
  static void set_if_changed(meta::Attribute<float> *attr, const float &value)
  {
    if (attr && std::abs(attr->value() - value) > 1e-4f * std::max(1.f, std::abs(value)))
      attr->set_value(value);
  }

  void add(const Spec &spec)
  {
    auto              &c = this->container;
    qtr::RenderWidget *r = this->renderer;

    const auto tag = [&spec](meta::AbstractAttribute &a)
    {
      a.metadata().try_add(std::string(meta::keys::ui::category),
                           std::string(spec.category));
    };

    switch (spec.type)
    {
    case Spec::Type::Bool:
    {
      bool      *p = r->bool_setting(spec.key);
      const bool value = p ? (spec.invert ? !*p : *p) : false;
      auto      &a = meta::presets::toggle_button(c, spec.key, spec.label, value);
      tag(a);
      this->bools[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const bool &v)
          {
            if (!this->renderer)
              return;
            if (bool *q = this->renderer->bool_setting(spec.key))
            {
              *q = spec.invert ? !v : v;
              this->renderer->settings_changed();
            }
          }));
      break;
    }
    case Spec::Type::Float:
    {
      float      *p = r->float_setting(spec.key);
      const float value = std::clamp(p ? to_ui(spec, *p) : spec.vmin,
                                     spec.vmin,
                                     spec.vmax);
      auto &a = meta::presets::slider_float(c,
                                            spec.key,
                                            spec.label,
                                            value,
                                            spec.vmin,
                                            spec.vmax,
                                            spec.format);
      tag(a);
      this->floats[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const float &v)
          {
            if (!this->renderer)
              return;
            if (float *q = this->renderer->float_setting(spec.key))
            {
              *q = from_ui(spec, v);
              this->renderer->settings_changed();
            }
          }));
      break;
    }
    case Spec::Type::Color:
    {
      glm::vec3 *p = r->color_setting(spec.key);
      auto      &a = meta::presets::color(c,
                                     spec.key,
                                     spec.label,
                                     glm::vec4(p ? *p : glm::vec3(1.f), 1.f));
      tag(a);
      this->colors[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const glm::vec4 &v)
          {
            if (!this->renderer)
              return;
            if (glm::vec3 *q = this->renderer->color_setting(spec.key))
            {
              *q = glm::vec3(v);
              this->renderer->settings_changed();
            }
          }));
      break;
    }
    case Spec::Type::Choice:
    {
      auto &a = meta::presets::enum_choice(c,
                                           spec.key,
                                           spec.label,
                                           spec.items,
                                           r->get_int_setting(spec.key));
      tag(a);
      this->ints[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this, spec](const int &v)
          {
            if (this->renderer)
              this->renderer->set_int_setting(spec.key, v);
          }));
      break;
    }
    case Spec::Type::WaterPreset:
    {
      glm::vec3 *p = r->color_setting("color_shallow_water");
      auto      &a = meta::presets::enum_choice(c,
                                           spec.key,
                                           spec.label,
                                           spec.items,
                                           p ? this->preset_index_for(*p) : 0);
      tag(a);
      this->ints[spec.key] = &a;
      this->connections.push_back(a.value_changed.subscribe(
          [this](const int &v)
          {
            // a sync from the renderer must not re-apply the nearest preset
            // over colours the user picked by hand
            if (this->syncing)
              return;
            int i = 0;
            for (const auto &[name, pair] : qtr::water_colors)
            {
              if (i++ != v)
                continue;
              if (auto it = this->colors.find("color_shallow_water");
                  it != this->colors.end())
                it->second->set_value(glm::vec4(pair.first, 1.f));
              if (auto it = this->colors.find("color_deep_water");
                  it != this->colors.end())
                it->second->set_value(glm::vec4(pair.second, 1.f));
              break;
            }
          }));
      break;
    }
    }
  }

  QPointer<qtr::RenderWidget>                         renderer;
  std::vector<Spec>                                   specs;
  meta::AttributeContainer                            container;
  std::map<std::string, meta::Attribute<bool> *>      bools;
  std::map<std::string, meta::Attribute<float> *>     floats;
  std::map<std::string, meta::Attribute<glm::vec4> *> colors;
  std::map<std::string, meta::Attribute<int> *>       ints;
  std::vector<meta::EventConnection>                  connections;
  bool                                                syncing = false;
};

// =====================================
// SunDome: drag the sun around the sky
// =====================================

// The sky hemisphere seen from straight above: the centre is the zenith, the
// rim the horizon, north up. Dragging places the sun; its distance from the
// centre is cos(elevation), its bearing the azimuth -- the same spherical
// mapping the renderer's light uses.
class SunDome final : public QWidget
{
public:
  SunDome(meta::Attribute<float> *azimuth,
          meta::Attribute<float> *elevation,
          QWidget                *parent)
      : QWidget(parent), azimuth(azimuth), elevation(elevation)
  {
    this->setObjectName("sunDome");
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setAutoFillBackground(false);
    this->setStyleSheet("QWidget#sunDome { background: transparent; }");
    this->setFixedHeight(176);
    this->setCursor(Qt::CrossCursor);
    this->setToolTip("Drag to move the sun. Centre: overhead, rim: horizon.");

    if (azimuth)
      this->connections.push_back(
          azimuth->value_changed.subscribe([this](const float &) { this->update(); }));
    if (elevation)
      this->connections.push_back(
          elevation->value_changed.subscribe([this](const float &) { this->update(); }));
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QPointF c = QRectF(this->rect()).center();
    const qreal   R = this->radius();

    // sky: lighter towards the sun, darker at the rim
    const QPointF   sun = this->sun_position();
    QRadialGradient sky(sun, R * 1.6, sun);
    sky.setColorAt(0.0, QColor("#8fa3b8"));
    sky.setColorAt(0.45, QColor("#5d6d80"));
    sky.setColorAt(1.0, QColor("#2c333d"));
    p.setPen(QPen(mix_colors(colors.bg_primary, colors.text_primary, 0.55), 1.5));
    p.setBrush(sky);
    p.drawEllipse(c, R, R);

    // elevation rings at 30 and 60 degrees, and the compass cross
    p.setBrush(Qt::NoBrush);
    QColor faint = colors.text_primary;
    faint.setAlphaF(0.10);
    p.setPen(QPen(faint, 1, Qt::DashLine));
    for (const qreal el : {30.0, 60.0})
    {
      const qreal r = std::cos(el * kPi / 180.0) * R;
      p.drawEllipse(c, r, r);
    }
    p.drawLine(c + QPointF(-R, 0), c + QPointF(R, 0));
    p.drawLine(c + QPointF(0, -R), c + QPointF(0, R));

    QColor letters = colors.text_primary;
    letters.setAlphaF(0.45);
    p.setPen(letters);
    p.setFont(meta::qt::ui_font(10, true));
    p.drawText(QRectF(c.x() - 8, c.y() - R + 3, 16, 14), Qt::AlignCenter, "N");
    p.drawText(QRectF(c.x() - 8, c.y() + R - 17, 16, 14), Qt::AlignCenter, "S");
    p.drawText(QRectF(c.x() + R - 17, c.y() - 7, 14, 14), Qt::AlignCenter, "E");
    p.drawText(QRectF(c.x() - R + 3, c.y() - 7, 14, 14), Qt::AlignCenter, "W");

    // the sun: glow, then disc
    QRadialGradient glow(sun, 26);
    glow.setColorAt(0.0, QColor(255, 244, 214, 200));
    glow.setColorAt(1.0, QColor(255, 244, 214, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(sun, 26, 26);
    p.setBrush(QColor("#fff6e0"));
    p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
    p.drawEllipse(sun, 11, 11);
  }

  void mousePressEvent(QMouseEvent *event) override { this->drag_to(event->position()); }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (event->buttons() & Qt::LeftButton)
      this->drag_to(event->position());
  }

private:
  qreal radius() const { return std::min(this->width(), this->height()) / 2.0 - 8.0; }

  QPointF sun_position() const
  {
    const QPointF c = QRectF(this->rect()).center();
    const qreal   R = this->radius();
    const qreal   az = (this->azimuth ? this->azimuth->value() : 0.f) * kPi / 180.0;
    const qreal   el = (this->elevation ? this->elevation->value() : 45.f) * kPi / 180.0;
    const qreal   r = std::cos(el) * R;
    // renderer: x = cos(el) sin(az), z = cos(el) cos(az); z points at the
    // viewer, which is down on this top view
    return c + QPointF(std::sin(az) * r, std::cos(az) * r);
  }

  void drag_to(const QPointF &pos)
  {
    const QPointF c = QRectF(this->rect()).center();
    const QPointF d = (pos - c) / this->radius();
    const qreal   r = std::min(1.0, std::hypot(d.x(), d.y()));
    const qreal   el = std::acos(r) * 180.0 / kPi;
    const qreal   az = std::atan2(d.x(), d.y()) * 180.0 / kPi;

    if (this->azimuth && r > 1e-3)
      this->azimuth->set_value(float(az));
    if (this->elevation)
      this->elevation->set_value(float(el));
    this->update();
  }

  meta::Attribute<float>            *azimuth = nullptr;
  meta::Attribute<float>            *elevation = nullptr;
  std::vector<meta::EventConnection> connections;
};

// =====================================
// SnapGuide: where the dragged rail will land
// =====================================

class SnapGuide final : public QWidget
{
public:
  explicit SnapGuide(QWidget *parent) : QWidget(parent)
  {
    this->setAttribute(Qt::WA_TransparentForMouseEvents);
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->hide();

    this->fade = new QVariantAnimation(this);
    this->fade->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->fade,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v)
                     {
                       this->opacity = v.toReal();
                       this->update();
                     });
    QObject::connect(this->fade,
                     &QVariantAnimation::finished,
                     this,
                     [this]()
                     {
                       if (this->opacity <= 0.01)
                         this->hide();
                     });
  }

  void set_target(const QRectF &rect)
  {
    this->target = rect;
    this->update();
  }

  void appear()
  {
    this->setGeometry(this->parentWidget()->rect());
    this->show();
    this->raise();
    this->run_fade(1.0);
  }

  void vanish() { this->run_fade(0.0); }

protected:
  void paintEvent(QPaintEvent *) override
  {
    if (this->target.isNull())
      return;

    const QColor accent = HSD_CTX.app_settings.colors.accent;
    QPainter     p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(this->opacity);

    QColor fill = accent;
    fill.setAlphaF(0.16);
    QColor edge = accent;
    edge.setAlphaF(0.75);
    p.setPen(QPen(edge, 1.5, Qt::DashLine));
    p.setBrush(fill);
    const qreal r = std::min(10.0,
                             std::min(this->target.width(), this->target.height()) / 2);
    p.drawRoundedRect(this->target.adjusted(0.75, 0.75, -0.75, -0.75), r, r);
  }

private:
  void run_fade(qreal to)
  {
    this->fade->stop();
    this->fade->setStartValue(this->opacity);
    this->fade->setEndValue(to);
    this->fade->setDuration(anim_ms(140));
    this->fade->start();
  }

  QRectF             target;
  qreal              opacity = 0.0;
  QVariantAnimation *fade = nullptr;
};

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

    // the press that just closed this tool's own menu (Qt replays it here):
    // it closes the menu, it does not reopen it
    if (this->owner->menu_guard.swallow_press(event->position().toPoint()))
    {
      this->pressed = false;
      this->pressed_index = -1;
    }
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

    this->owner->menu_guard.release(); // that click is over

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

// =====================================
// ViewportPanel
// =====================================

// A small icon button for the panel header (reset, close).
class HeaderButton final : public QAbstractButton
{
public:
  enum class Kind
  {
    Reset,
    Close
  };

  HeaderButton(Kind kind, QWidget *parent) : QAbstractButton(parent), kind(kind)
  {
    this->setFixedSize(28, 26);
    this->setCursor(Qt::PointingHandCursor);
    this->setAttribute(Qt::WA_Hover);
    this->setToolTip(kind == Kind::Reset ? "Reset this panel to defaults" : "Close");
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    const auto &colors = HSD_CTX.app_settings.colors;
    QPainter    p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (this->underMouse())
    {
      p.setPen(Qt::NoPen);
      p.setBrush(mix_colors(surface_color(), colors.text_primary, 0.08));
      p.drawRoundedRect(QRectF(this->rect()).adjusted(1, 1, -1, -1), 6, 6);
    }

    const QColor  ink = mix_colors(surface_color(),
                                  colors.text_primary,
                                  this->underMouse() ? 0.95 : 0.65);
    const QPointF c = QRectF(this->rect()).center();
    if (this->kind == Kind::Reset)
      paint_reset(p, c, 20, ink);
    else
    {
      QPen pen(ink, 1.4, Qt::SolidLine, Qt::RoundCap);
      p.setPen(pen);
      p.drawLine(c + QPointF(-4.5, -4.5), c + QPointF(4.5, 4.5));
      p.drawLine(c + QPointF(4.5, -4.5), c + QPointF(-4.5, 4.5));
    }
  }

private:
  Kind kind;
};

class ViewportPanel final : public QWidget
{
public:
  ViewportPanel(const QString &title, std::unique_ptr<PanelModel> model, QWidget *parent)
      : QWidget(parent), model(std::move(model))
  {
    this->setObjectName("hsdViewportPanel");
    this->setAttribute(Qt::WA_NoSystemBackground);
    this->setFixedWidth(kWidth);
    this->hide();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 6, 10);
    root->setSpacing(6);

    // header: reset | title | close
    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 4, 0);
    header->setSpacing(4);
    auto *reset = new HeaderButton(HeaderButton::Kind::Reset, this);
    this->reset_button = reset;
    header->addWidget(reset);
    auto *label = new QLabel(title, this);
    label->setObjectName("viewportPanelTitle");
    header->addWidget(label, 1);
    auto *close = new HeaderButton(HeaderButton::Kind::Close, this);
    header->addWidget(close);
    root->addLayout(header);

    this->scroll = new QScrollArea(this);
    this->scroll->setObjectName("viewportPanelScroll");
    this->scroll->setWidgetResizable(true);
    this->scroll->setFrameShape(QFrame::NoFrame);
    this->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    this->scroll->viewport()->setAutoFillBackground(false);

    this->content = new QWidget();
    this->content->setObjectName("viewportPanelContent");
    this->content->setAutoFillBackground(false);
    this->content_layout = new QVBoxLayout(this->content);
    this->content_layout->setContentsMargins(0, 0, 4, 0);
    this->content_layout->setSpacing(8);
    this->scroll->setWidget(this->content);
    root->addWidget(this->scroll, 1);
    this->content->installEventFilter(this);

    const auto &colors = HSD_CTX.app_settings.colors;
    QString     css = QString(R"(
      QLabel#viewportPanelTitle { color: %1; font-size: 13px; font-weight: 600;
        background: transparent; padding-left: 4px; }
      QWidget#sunDome, QScrollArea#viewportPanelScroll, QWidget#viewportPanelContent {
        background: transparent; border: none; }
      QPushButton#viewportAction {
        background: %2; color: %1; border: 1px solid %3; border-radius: 7px;
        padding: 6px 12px; font-size: 12px; }
      QPushButton#viewportAction:hover { border-color: %4; }
      QWidget#viewportPreviewRows { background: transparent; }
      QWidget#viewportPreviewRows QLabel { color: %1; font-size: 12px; background: transparent; }
      QWidget#viewportPreviewRows QComboBox {
        background: %2; border: 1px solid %3; border-radius: 6px;
        padding: 3px 8px; min-height: 20px; font-size: 12px; }
      QWidget#viewportPreviewRows QComboBox:hover { border-color: %4; }
      QWidget#viewportPreviewRows QComboBox::drop-down { border: none; width: 18px; }
    )")
                      .arg(colors.text_primary.name(),
                           mix_colors(surface_color(), colors.text_primary, 0.06).name(),
                           border_color().name(),
                           colors.accent.name());
    this->setStyleSheet(css);
    this->scroll->setStyleSheet(
        meta::qt::industrial::scrollbar_stylesheet(viewport_theme()));

    this->anim = new QVariantAnimation(this);
    this->anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(this->anim,
                     &QVariantAnimation::valueChanged,
                     this,
                     [this](const QVariant &v) { this->apply_progress(v.toReal()); });
    QObject::connect(this->anim,
                     &QVariantAnimation::finished,
                     this,
                     [this]()
                     {
                       if (this->closing)
                       {
                         this->hide();
                         this->closing = false;
                       }
                       // effects make every repaint an offscreen pass: drop it
                       this->setGraphicsEffect(nullptr);
                       this->effect = nullptr;
                     });

    // While open, follow the renderer: values also change behind the panel
    // (auto-rotating sun, the orientation gizmo, a project load...).
    this->follow = new QTimer(this);
    this->follow->setInterval(40);
    QObject::connect(this->follow,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                       if (!this->isVisible())
                         this->follow->stop();
                       else if (this->model && !this->closing)
                         this->model->sync_from_renderer();
                     });

    QObject::connect(reset,
                     &QAbstractButton::clicked,
                     this,
                     [this]()
                     {
                       if (this->model)
                         this->model->reset_to_defaults();
                     });
    QObject::connect(close,
                     &QAbstractButton::clicked,
                     this,
                     [this]()
                     {
                       if (this->on_close)
                         this->on_close();
                     });
  }

  ~ViewportPanel() override
  {
    // rows subscribe to the model's attributes: remove them first
    delete this->scroll;
    this->scroll = nullptr;
  }

  PanelModel *get_model() const { return this->model.get(); }
  void        set_reset_visible(bool visible) { this->reset_button->setVisible(visible); }
  QVBoxLayout *body() const { return this->content_layout; }

  std::function<void()> on_close;

  // open next to the rail, from its side
  void popup(const QRect &rail, const QRect &item, Edge edge)
  {
    if (this->model)
      this->model->sync_from_renderer();

    const QRect vr = this->parentWidget()->rect().adjusted(kMargin,
                                                           kMargin,
                                                           -kMargin,
                                                           -kMargin);
    const int   gap = 8;

    // The panel keeps to the side of the rail, never over it: over it, a
    // second click on the tool would land on the panel instead of closing it.
    QRect area = vr;
    switch (edge)
    {
    case Edge::Right:
      area.setRight(rail.left() - gap - 1);
      break;
    case Edge::Left:
      area.setLeft(rail.right() + gap + 1);
      break;
    case Edge::Top:
      area.setTop(rail.bottom() + gap + 1);
      break;
    case Edge::Bottom:
      area.setBottom(rail.top() - gap - 1);
      break;
    }

    this->fit_area = area;
    this->fit_edge = edge;
    this->fit_item = item;

    this->slide_from = edge == Edge::Right  ? QPoint(12, 0)
                       : edge == Edge::Left ? QPoint(-12, 0)
                       : edge == Edge::Top  ? QPoint(0, -12)
                                            : QPoint(0, 12);
    this->closing = false;
    this->place(this->measured_height());
    this->move(this->home + this->slide_from);
    this->show();
    this->raise();
    this->run(1.0, 190);
    this->follow->start();

    // Rows only report their real height once shown, and sections animate
    // open and shut: follow the content (eventFilter) rather than measuring
    // once, so the panel grows and shrinks with it.
    this->fit_height();
  }

  void fit_height()
  {
    this->content->adjustSize();
    const int h = this->measured_height();
    if (h == this->height())
      return;
    this->place(h);
    this->move(this->home + this->slide_from * (1.0 - this->progress));
  }

  int measured_height() const
  {
    const int content_h = this->content->sizeHint().height() + 8 + 34 + 18;
    const int room = std::max(1, this->fit_area.height());
    return std::min(room, std::max(160, content_h));
  }

  // size to `h` and settle `home` inside the area beside the rail
  void place(int h)
  {
    const QRect &area = this->fit_area;
    const QRect &item = this->fit_item;
    this->setFixedHeight(h);

    QPoint pos;
    switch (this->fit_edge)
    {
    case Edge::Right:
      pos = QPoint(area.right() + 1 - kWidth, item.top() - 6);
      break;
    case Edge::Left:
      pos = QPoint(area.left(), item.top() - 6);
      break;
    case Edge::Top:
      pos = QPoint(item.left() - 6, area.top());
      break;
    case Edge::Bottom: // grows upwards from just above the rail
      pos = QPoint(item.left() - 6, area.bottom() + 1 - h);
      break;
    }
    pos.setX(std::clamp(pos.x(),
                        area.left(),
                        std::max(area.left(), area.right() + 1 - kWidth)));
    pos.setY(
        std::clamp(pos.y(), area.top(), std::max(area.top(), area.bottom() + 1 - h)));
    this->home = pos;
  }

  void dismiss(bool animate)
  {
    if (!this->isVisible())
      return;
    if (!animate || anim_ms(1) == 0)
    {
      this->anim->stop();
      this->setGraphicsEffect(nullptr);
      this->effect = nullptr;
      this->hide();
      return;
    }
    this->closing = true;
    this->run(0.0, 140);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5), 11, 11);
    p.fillPath(shape, surface_color());
    p.setPen(QPen(border_color(), 1));
    p.drawPath(shape);
  }

  // Scrolling past the end of the content arrives here (the scroll area
  // ignores it at its limits): stop it, or it would zoom the view below.
  void wheelEvent(QWheelEvent *event) override { event->accept(); }

  // Same for clicks anywhere on the panel that its rows leave unhandled: the
  // renderer would take the press as the start of a camera drag, and never
  // see a release handled by the row.
  void mousePressEvent(QMouseEvent *event) override { event->accept(); }
  void mouseReleaseEvent(QMouseEvent *event) override { event->accept(); }
  void mouseMoveEvent(QMouseEvent *event) override { event->accept(); }
  void mouseDoubleClickEvent(QMouseEvent *event) override { event->accept(); }

  void keyPressEvent(QKeyEvent *event) override
  {
    if (event->key() == Qt::Key_Escape && this->on_close)
    {
      this->on_close();
      return;
    }
    QWidget::keyPressEvent(event);
  }

private:
  static constexpr int kWidth = 400;

  void run(qreal to, int ms)
  {
    if (!this->effect)
    {
      this->effect = new QGraphicsOpacityEffect(this);
      this->effect->setOpacity(this->progress);
      this->setGraphicsEffect(this->effect);
    }
    this->anim->stop();
    this->anim->setStartValue(this->progress);
    this->anim->setEndValue(to);
    this->anim->setDuration(anim_ms(ms));
    this->anim->start();
    if (anim_ms(ms) == 0)
      this->apply_progress(to);
  }

  void apply_progress(qreal t)
  {
    this->progress = t;
    if (this->effect)
      this->effect->setOpacity(t);
    this->move(this->home + this->slide_from * (1.0 - t));
  }

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    if (watched == this->content && this->isVisible() &&
        (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize) &&
        !this->fit_pending)
    {
      // coalesce: a section animating fires one of these per frame
      this->fit_pending = true;
      QTimer::singleShot(0,
                         this,
                         [this]()
                         {
                           this->fit_pending = false;
                           this->fit_height();
                         });
    }
    return QWidget::eventFilter(watched, event);
  }

  std::unique_ptr<PanelModel> model;
  QAbstractButton            *reset_button = nullptr;
  QScrollArea                *scroll = nullptr;
  QWidget                    *content = nullptr;
  QVBoxLayout                *content_layout = nullptr;

  // placement of the last popup, for re-fitting as the content changes
  QRect fit_area; // viewport part beside the rail
  Edge  fit_edge = Edge::Right;
  QRect fit_item;
  bool  fit_pending = false;

  QTimer                 *follow = nullptr; // syncs from the renderer while open
  QVariantAnimation      *anim = nullptr;
  QGraphicsOpacityEffect *effect = nullptr;
  qreal                   progress = 0.0;
  bool                    closing = false;
  QPoint                  home;
  QPoint                  slide_from;
};

// =====================================
// ViewportControls
// =====================================

namespace
{
// every live toolbar, for settings that apply to all of them
std::set<ViewportControls *> &live_controls()
{
  static std::set<ViewportControls *> controls;
  return controls;
}
} // namespace

ViewportControls::ViewportControls(qtr::RenderWidget        *renderer,
                                   QPointer<GraphNodeWidget> graph,
                                   QObject                  *parent)
    : QObject(parent), renderer(renderer), graph(graph)
{
  if (!renderer)
    return;

  // this UI replaces the renderer's own ImGui window
  renderer->set_settings_window_visible(false);

  this->guide = new SnapGuide(renderer);
  this->rail = new ViewportRail(this, this->guide, renderer);
  live_controls().insert(this);
  this->rebuild_rail();
  this->rail->show();

  renderer->installEventFilter(this);

  if (graph)
  {
    auto update_resolution = [this]()
    {
      if (!this->graph || !this->rail)
        return;
      GraphNode *gno = this->graph->get_p_graph_node();
      if (gno && gno->get_config_ref())
        this->rail->set_resolution_text(
            QString::fromStdString(resolution_label(gno->get_config_ref()->shape.x)));
    };
    update_resolution();
    QObject::connect(graph, &GraphNodeWidget::config_changed, this, update_resolution);
  }
}

ViewportControls::~ViewportControls() { live_controls().erase(this); }

void ViewportControls::relayout_all()
{
  for (ViewportControls *controls : live_controls())
    if (controls->rail)
    {
      controls->close_panel(false);
      controls->rail->place();
    }
}

void ViewportControls::set_preview_content(QWidget *content)
{
  if (!this->renderer || !content)
    return;

  QPointer<ViewportPanel> &panel = this->panels[ToolPreview];
  if (!panel)
  {
    panel = new ViewportPanel(
        tool_title(ToolPreview),
        std::make_unique<PanelModel>(this->renderer.data(), std::vector<Spec>{}),
        this->renderer);
    panel->on_close = [this]() { this->close_panel(true); };
    panel->set_reset_visible(false);
  }

  content->setObjectName("viewportPreviewRows");
  panel->body()->addWidget(content);
  panel->body()->addStretch(1);
}

bool ViewportControls::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->renderer && event->type() == QEvent::Resize && this->rail)
  {
    this->close_panel(false);
    this->rail->place();
  }

  // a click in the view itself (the panel and the rail keep theirs) puts the
  // open panel away, like any popover
  if (watched == this->renderer && event->type() == QEvent::MouseButtonPress &&
      this->open_tool >= 0)
    this->close_panel(true);

  return QObject::eventFilter(watched, event);
}

void ViewportControls::rebuild_rail()
{
  if (!this->rail)
    return;

  using Item = ViewportRail::Item;
  std::vector<Item> items;
  items.push_back({ToolView2D, "2D view: the heightmap seen from above", false, false});
  items.push_back({ToolView3D, "3D view: the lit terrain", false, false});
  items.push_back({ToolResolution, "Preview resolution", true, true});
  items.push_back(
      {ToolPreview, "Preview: which node outputs the view shows", false, true});

  if (this->render_type == 0)
  {
    items.push_back({ToolLighting2D, "Lighting", true});
    items.push_back({ToolColormap2D, "Colormap", false});
  }
  else
  {
    items.push_back({ToolLighting, "Lighting", true});
    items.push_back({ToolCamera, "Camera", false});
    items.push_back({ToolDisplay, "Display", true});
    items.push_back({ToolMaterial, "Material", false});
    items.push_back({ToolWater, "Water", true});
    items.push_back({ToolSky, "Sky & atmosphere", false});
  }
  items.push_back({ToolMore, "More", true});

  this->rail->set_items(std::move(items));
}

void ViewportControls::set_render_type(int type)
{
  if (type == this->render_type)
    return;
  this->close_panel(false);
  this->render_type = type;
  if (this->rail)
    this->rail->set_view_mode(type, this->rail->isVisible());
  this->rebuild_rail();
}

void ViewportControls::on_tool_activated(int tool, const QRect &item_rect)
{
  if (tool == ToolView2D || tool == ToolView3D)
  {
    // app-wide, so every graph's viewer follows (and new ones start that way)
    const int type = tool == ToolView2D ? 0 : 1;
    if (type != this->render_type)
      HSD_APP->set_viewer_render_type(type);
    return;
  }

  if (tool == ToolResolution)
  {
    this->show_resolution_menu(item_rect);
    return;
  }
  if (tool == ToolMore)
  {
    this->show_more_menu(item_rect);
    return;
  }

  if (this->open_tool == tool)
    this->close_panel(true);
  else
    this->open_panel(tool);
}

void ViewportControls::open_panel(int tool)
{
  if (!this->renderer || !this->rail)
    return;

  this->close_panel(false);

  QPointer<ViewportPanel> &panel = this->panels[tool];

  // the preview panel holds the viewer's rows (set_preview_content)
  if (tool == ToolPreview)
  {
    if (!panel)
      return;
    if (this->on_preview_opened)
      this->on_preview_opened();
  }

  if (!panel)
  {
    auto  model = std::make_unique<PanelModel>(this->renderer.data(), specs_for(tool));
    auto *p_model = model.get();
    panel = new ViewportPanel(tool_title(tool), std::move(model), this->renderer);
    panel->on_close = [this]() { this->close_panel(true); };

    // sun dome above the lighting sliders
    if (tool == ToolLighting || tool == ToolLighting2D)
    {
      const bool two_d = tool == ToolLighting2D;
      auto      *dome = new SunDome(
          p_model->float_attr(two_d ? "2d.sun_azimuth" : "light_phi"),
          p_model->float_attr(two_d ? "2d.sun_zenith" : "light_theta"),
          panel);
      panel->body()->addWidget(dome);
    }

    // the settings themselves, in the properties design
    meta::qt::RowContext ctx;
    ctx.theme = &viewport_theme();
    ctx.default_value = [p_model](const std::string &key)
    { return p_model->default_ui(key); };

    meta::qt::ContainerRenderOptions options;
    options.design = properties_panel_design().design;
    options.row_context = ctx;
    options.category_policy = meta::qt::CategoryPolicy::CP_MERGED;
    options.root_category_name = std::string{};

    QWidget *rows = meta::qt::render(p_model->get_container(), options, panel);
    panel->body()->addWidget(rows);

    // one-shot actions
    if (tool == ToolCamera || tool == ToolColormap2D)
    {
      auto *button = new QPushButton(tool == ToolCamera ? "Reset camera" : "Reset view",
                                     panel);
      button->setObjectName("viewportAction");
      button->setCursor(Qt::PointingHandCursor);
      QPointer<qtr::RenderWidget> r = this->renderer;
      QObject::connect(button,
                       &QPushButton::clicked,
                       panel,
                       [r, tool]()
                       {
                         if (!r)
                           return;
                         if (tool == ToolCamera)
                           r->reset_camera();
                         else
                           r->reset_view_2d();
                       });
      panel->body()->addWidget(button);
    }

    panel->body()->addStretch(1);
  }

  this->open_tool = tool;
  this->rail->set_active(tool);
  panel->popup(this->rail->docked_rect().toAlignedRect(),
               this->rail->item_rect(tool),
               this->rail->get_edge());
  this->rail->raise(); // the rail stays clickable above any panel
}

void ViewportControls::close_panel(bool animate)
{
  if (this->open_tool >= 0)
    if (auto it = this->panels.find(this->open_tool);
        it != this->panels.end() && it->second)
      it->second->dismiss(animate);

  this->open_tool = -1;
  if (this->rail)
    this->rail->set_active(-1);
}

void ViewportControls::show_resolution_menu(const QRect &item_rect)
{
  if (!this->graph)
    return;

  GraphNode *gno = this->graph->get_p_graph_node();
  const int  current = gno && gno->get_config_ref() ? gno->get_config_ref()->shape.x : 0;

  HsdMenu  menu("Preview Resolution", this->renderer);
  QAction *title = menu.addAction("Preview Resolution");
  title->setEnabled(false);

  auto *group = new QActionGroup(&menu);
  for (const int res : {512, 1024, 2048, 4096})
  {
    QAction *action = menu.addAction(QString("%1 × %1").arg(res));
    action->setCheckable(true);
    action->setChecked(res == current);
    action->setData(res);
    group->addAction(action);
  }

  this->rail->set_active(ToolResolution);
  const QPoint anchor = this->rail->get_edge() == Edge::Left
                            ? item_rect.topRight() + QPoint(8, 0)
                            : item_rect.topLeft() -
                                  QPoint(menu.sizeHint().width() + 8, 0);
  QAction     *chosen = menu.exec(this->renderer->mapToGlobal(anchor));
  // the item's area on the rail: a press there that closed the menu is replayed
  this->menu_guard.arm(this->rail, item_rect.translated(-this->rail->pos()));
  this->rail->set_active(this->open_tool);

  if (chosen && chosen->data().isValid() && this->graph)
    this->graph->apply_new_config(chosen->data().toInt());
}

void ViewportControls::show_more_menu(const QRect &item_rect)
{
  HsdMenu menu("Viewport", this->renderer);

  QAction *reset_all = menu.addAction("Reset all viewport settings");
  QAction *reset_camera = nullptr;
  QAction *gizmo = nullptr;
  QAction *void_bg = nullptr;

  if (this->render_type == 1)
  {
    reset_camera = menu.addAction("Reset camera");
    menu.addSeparator();
    gizmo = menu.addAction("Orientation gizmo");
    gizmo->setCheckable(true);
    if (bool *p = this->renderer->bool_setting("show_orientation_gizmo"))
      gizmo->setChecked(*p);
    void_bg = menu.addAction("Void background");
    void_bg->setCheckable(true);
    void_bg->setChecked(this->renderer->get_int_setting("background_mode") == 1);
  }
  else
    menu.addAction("Reset view")->setData(QString("reset_view"));

  this->rail->set_active(ToolMore);
  const QPoint anchor = this->rail->get_edge() == Edge::Left
                            ? item_rect.topRight() + QPoint(8, 0)
                            : item_rect.topLeft() -
                                  QPoint(menu.sizeHint().width() + 8, 0);
  QAction     *chosen = menu.exec(this->renderer->mapToGlobal(anchor));
  this->menu_guard.arm(this->rail, item_rect.translated(-this->rail->pos()));
  this->rail->set_active(this->open_tool);

  if (!chosen || !this->renderer)
    return;

  if (chosen == reset_all)
    this->reset_all();
  else if (chosen == reset_camera)
    this->renderer->reset_camera();
  else if (chosen == gizmo)
  {
    if (bool *p = this->renderer->bool_setting("show_orientation_gizmo"))
    {
      *p = gizmo->isChecked();
      this->renderer->settings_changed();
    }
  }
  else if (chosen == void_bg)
    this->renderer->set_int_setting("background_mode", void_bg->isChecked() ? 1 : 0);
  else if (chosen->data().toString() == "reset_view")
    this->renderer->reset_view_2d();
}

void ViewportControls::reset_all()
{
  if (!this->renderer)
    return;

  // every tool's settings, whether or not its panel was ever opened
  for (const int tool : {ToolLighting,
                         ToolCamera,
                         ToolDisplay,
                         ToolMaterial,
                         ToolWater,
                         ToolSky,
                         ToolLighting2D,
                         ToolColormap2D})
  {
    if (auto it = this->panels.find(tool); it != this->panels.end() && it->second)
      it->second->get_model()->reset_to_defaults();
    else
      PanelModel(this->renderer.data(), specs_for(tool)).reset_to_defaults();
  }
}

QMargins ViewportControls::reserved_margins() const
{
  if (!this->rail)
    return QMargins();

  const int band = thick_px() + kMargin + 6;
  switch (this->rail->get_edge())
  {
  case Edge::Left:
    return QMargins(band, 0, 0, 0);
  case Edge::Right:
    return QMargins(0, 0, band, 0);
  case Edge::Top:
    return QMargins(0, band, 0, 0);
  case Edge::Bottom:
    return QMargins(0, 0, 0, band);
  }
  return QMargins();
}

void ViewportControls::notify_layout_changed()
{
  // The orientation gizmo sits in the top-right corner: when the docked rail
  // covers that corner, move the gizmo out of its way (left of a rail on the
  // right edge, below one on the top edge).
  if (this->renderer && this->rail && !this->rail->is_moving())
  {
    const QRectF vr = QRectF(this->renderer->rect());
    const QRectF gizmo_zone(vr.right() - 132, vr.top(), 132, 132);
    const QRectF docked = this->rail->docked_rect();
    const qreal  band = thick_px() + kMargin - 4;

    float top = 0.f, right = 0.f;
    if (docked.intersects(gizmo_zone))
    {
      if (this->rail->get_edge() == Edge::Right)
        right = float(band);
      else if (this->rail->get_edge() == Edge::Top)
        top = float(band);
    }
    this->renderer->set_overlay_insets(top, right);
  }

  if (this->on_layout_changed)
    this->on_layout_changed();
}

nlohmann::json ViewportControls::json_to() const
{
  return this->rail ? this->rail->json_to() : nlohmann::json::object();
}

void ViewportControls::json_from(const nlohmann::json &json)
{
  if (this->rail)
    this->rail->json_from(json);
}

} // namespace hesiod
