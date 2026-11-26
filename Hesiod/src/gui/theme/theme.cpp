/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include "hesiod/gui/theme/oklch.hpp"
#include "hesiod/gui/theme/theme.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

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

ThemeDefinition ThemeDefinition::from_seeds(const std::string &name,
                                            const QColor      &ground,
                                            const QColor      &accent)
{
  using oklch::Lch;

  // --- normalize the seeds
  Lch g = oklch::to_lch(ground);
  g.L = std::clamp(g.L, 0.20, 0.29);
  g.C = std::min(g.C, 0.014);

  Lch a = oklch::to_lch(accent);
  if (a.C < 0.012)
    a.h = g.h; // a gray has no hue of its own: borrow the ground's
  a.L = std::clamp(a.L, a.C < 0.03 ? 0.80 : 0.66, 0.90);
  a.C = std::min(a.C, 0.135);

  auto neutral = [&g](double lightness, double chroma_scale = 1.0)
  { return oklch::from_lch({lightness, g.C * chroma_scale, g.h}); };

  ThemeDefinition t;
  t.name = name;
  t.is_dark = true;

  // a short, even ladder: base, panes a step above it, everything else
  // derived from those two in derive_app_colors()
  t.background = neutral(g.L);
  t.surface = neutral(g.L + 0.030);
  t.node_editor_bg = neutral(g.L);
  t.border = neutral(g.L + 0.112);

  t.text = neutral(0.945, 0.5);
  // labels stay readable on the lightest surface they can sit on (a popup)
  t.text_muted = oklch::ensure_contrast(neutral(0.76), neutral(g.L + 0.098), 4.5);

  t.accent = oklch::from_lch(a);
  return t;
}

std::vector<ThemeDefinition> ThemeDefinition::make_colorways()
{
  using oklch::from_lch;

  // Grounds are graphite: gray first, with only a breath of the hue. Accents
  // are mineral colors at moderate chroma, never neon.
  return {
      from_seeds("Slate", from_lch({0.235, 0.006, 255}), from_lch({0.75, 0.095, 252})),
      from_seeds("Clay", from_lch({0.240, 0.006, 55}), from_lch({0.75, 0.095, 48})),
      from_seeds("Moss", from_lch({0.235, 0.006, 150}), from_lch({0.77, 0.085, 148})),
      from_seeds("Sand", from_lch({0.240, 0.006, 85}), from_lch({0.83, 0.085, 88})),
      from_seeds("Heather", from_lch({0.235, 0.007, 300}), from_lch({0.77, 0.085, 302})),
      from_seeds("Coral", from_lch({0.240, 0.006, 25}), from_lch({0.75, 0.105, 26})),
      from_seeds("Graphite", from_lch({0.235, 0.003, 255}), from_lch({0.89, 0.004, 255})),
  };
}

QColor ThemeDefinition::derive_group_accent(const std::string &name)
{
  uint hash = 2166136261u;
  for (const char c : name)
    hash = (hash ^ static_cast<uint>(c)) * 16777619u;

  // any hue, but always the same perceived lightness and a moderate chroma:
  // a panel of differently colored groups stays one palette
  return oklch::from_lch({0.76, 0.095, static_cast<double>(hash % 360u)});
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
    // Steps in perceptual lightness from the pane surface, so that "a little
    // lighter" is the same visible amount whatever the theme's hue or depth.
    c.bg_secondary = oklch::shifted(this->surface, 0.048); // things you press
    c.text_disabled = oklch::mix(this->text_muted, this->surface, 0.50);
    c.accent_bw = this->text;
    c.hover = oklch::shifted(this->surface, 0.105);
    c.pressed = oklch::shifted(this->surface, 0.160);
    c.separator = oklch::shifted(this->surface, 0.052);
    c.building = oklch::from_lch({0.80, 0.110, 72});
    c.warning = oklch::from_lch({0.72, 0.135, 25});
  }
  else
  {
    c.bg_secondary = mix_colors(this->surface, QColor("#FFFFFF"), 0.70);
    c.text_disabled = mix_colors(this->text_muted, this->surface, 0.55);
    c.accent_bw = this->text;
    c.hover = mix_colors(this->surface, this->border, 0.40);
    c.pressed = mix_colors(this->surface, this->border, 0.75);
    c.separator = this->border;
    c.building = QColor("#D9822B");
    c.warning = QColor("#D9383A");
  }

  return c;
}

NodePalettePalette ThemeDefinition::derive_node_palette_palette() const
{
  NodePalettePalette s;

  if (this->is_dark)
  {
    s.surface = mix_colors(this->surface, this->background, 0.35);
    s.surface_hover = mix_colors(this->surface, QColor("#FFFFFF"), 0.05);
    s.surface_selected = mix_colors(this->surface, this->accent, 0.15);
    s.flyout_bg = mix_colors(this->background, this->surface, 0.25);
    s.flyout_border = this->border;
    s.text = this->text_muted;
    s.text_active = this->text;
  }
  else
  {
    s.surface = mix_colors(this->surface, QColor("#FFFFFF"), 0.20);
    s.surface_hover = mix_colors(this->surface, this->border, 0.25);
    s.surface_selected = mix_colors(this->surface, this->accent, 0.18);
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
