/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "hesiod/gui/theme/theme.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

namespace
{

QColor mix(const QColor &a, const QColor &b, double t)
{
  t = std::clamp(t, 0.0, 1.0);
  return QColor(static_cast<int>(a.red() * (1.0 - t) + b.red() * t),
                static_cast<int>(a.green() * (1.0 - t) + b.green() * t),
                static_cast<int>(a.blue() * (1.0 - t) + b.blue() * t));
}

} // namespace

ThemeDefinition ThemeDefinition::make_default_dark()
{
  ThemeDefinition t;
  t.name = "Dark";
  t.is_dark = true;
  t.background = QColor("#191919");
  t.surface = QColor("#2B2B2B");
  t.text = QColor("#F4F4F5");
  t.text_muted = QColor("#949495");
  t.accent = QColor("#5E81AC");
  t.border = QColor("#5B5B5B");
  t.node_editor_bg = QColor("#2B2B2B");
  return t;
}

ThemeDefinition ThemeDefinition::make_default_light()
{
  ThemeDefinition t;
  t.name = "Light";
  t.is_dark = false;
  t.background = QColor("#E1E2E4");
  t.surface = QColor("#EDEDED");
  t.text = QColor("#1C1E21");
  t.text_muted = QColor("#5E636A");
  t.accent = QColor("#3B679B");
  t.border = QColor("#C3C7CE");
  t.node_editor_bg = QColor("#E1E2E4"); // Follow theme background
  return t;
}

QColor ThemeDefinition::derive_group_accent(const std::string &name)
{
  uint hash = 2166136261u;
  for (const char c : name)
    hash = (hash ^ static_cast<uint>(c)) * 16777619u;

  return QColor::fromHsl(static_cast<int>(hash % 360u), 115, 145);
}

AppSettings::Colors ThemeDefinition::derive_app_colors() const
{
  AppSettings::Colors c;

  c.bg_deep = this->background;
  c.bg_primary = this->surface;
  c.text_primary = this->text;
  c.text_secondary = this->text_muted;
  c.accent = this->accent;
  c.border = this->border;

  if (this->is_dark)
  {
    c.bg_secondary = mix(this->surface, QColor("#FFFFFF"), 0.12);
    c.text_disabled = mix(this->text_muted, this->background, 0.65);
    c.accent_bw = QColor("#FFFFFF");
    c.hover = mix(this->surface, QColor("#FFFFFF"), 0.35);
    c.pressed = mix(this->surface, QColor("#FFFFFF"), 0.50);
    c.separator = mix(this->border, this->text_muted, 0.30);
    c.building = QColor("#FFB86C");
    c.warning = QColor("#FF5555");
  }
  else
  {
    c.bg_secondary = mix(this->surface, QColor("#FFFFFF"), 0.70);
    c.text_disabled = mix(this->text_muted, this->surface, 0.55);
    c.accent_bw = this->text;
    c.hover = mix(this->surface, this->border, 0.40);
    c.pressed = mix(this->surface, this->border, 0.75);
    c.separator = this->border;
    c.building = QColor("#D9822B");
    c.warning = QColor("#D9383A");
  }

  return c;
}

NodePaletteStyle ThemeDefinition::derive_node_palette_style() const
{
  NodePaletteStyle s;

  if (this->is_dark)
  {
    s.surface = mix(this->surface, this->background, 0.35);
    s.surface_hover = mix(this->surface, QColor("#FFFFFF"), 0.05);
    s.surface_selected = mix(this->surface, this->accent, 0.15);
    s.flyout_bg = mix(this->background, this->surface, 0.25);
    s.flyout_border = this->border;
    s.text = this->text_muted;
    s.text_active = this->text;
  }
  else
  {
    s.surface = mix(this->surface, QColor("#FFFFFF"), 0.20);
    s.surface_hover = mix(this->surface, this->border, 0.25);
    s.surface_selected = mix(this->surface, this->accent, 0.18);
    s.flyout_bg = QColor("#FFFFFF");
    s.flyout_border = this->border;
    s.text = this->text_muted;
    s.text_active = this->text;
  }

  return s;
}

nlohmann::json ThemeDefinition::json_to() const
{
  nlohmann::json j;
  j["name"] = this->name;
  j["is_dark"] = this->is_dark;

  nlohmann::json colors;
  colors["background"] = this->background.name().toStdString();
  colors["surface"] = this->surface.name().toStdString();
  colors["text"] = this->text.name().toStdString();
  colors["text_muted"] = this->text_muted.name().toStdString();
  colors["accent"] = this->accent.name().toStdString();
  colors["border"] = this->border.name().toStdString();
  colors["node_editor_bg"] = this->node_editor_bg.name().toStdString();

  j["colors"] = colors;
  return j;
}

std::optional<ThemeDefinition> ThemeDefinition::json_from(const nlohmann::json &j)
{
  if (!j.is_object() || !j.contains("colors") || !j["colors"].is_object())
    return std::nullopt;

  ThemeDefinition t;

  if (j.contains("name") && j["name"].is_string())
    t.name = j["name"].get<std::string>();

  if (j.contains("is_dark") && j["is_dark"].is_boolean())
    t.is_dark = j["is_dark"].get<bool>();

  const auto &c = j["colors"];

  auto get_color = [&](const std::string &key, QColor &target)
  {
    if (c.contains(key) && c[key].is_string())
    {
      QColor col(QString::fromStdString(c[key].get<std::string>()));
      if (col.isValid())
        target = col;
    }
  };

  get_color("background", t.background);
  get_color("surface", t.surface);
  get_color("text", t.text);
  get_color("text_muted", t.text_muted);
  get_color("accent", t.accent);
  get_color("border", t.border);
  get_color("node_editor_bg", t.node_editor_bg);

  return t;
}

} // namespace hesiod
