/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once

#include <optional>
#include <string>

#include <QColor>
#include <nlohmann/json.hpp>

#include "hesiod/app/app_settings.hpp"
#include "hesiod/gui/node_palette_style.hpp"

namespace hesiod
{

/**
 * @brief Core 7-color token set defining an entire Hesiod visual theme.
 *
 * All secondary, hover, pressed, card, and widget tones are deterministically
 * derived in C++ from these anchor tokens.
 */
struct ThemeDefinition
{
  std::string name{"Dark"};
  bool        is_dark{true};

  // --- Core 7 token colors
  QColor background{"#191919"};
  QColor surface{"#2B2B2B"};
  QColor text{"#F4F4F5"};
  QColor text_muted{"#949495"};
  QColor accent{"#5E81AC"};
  QColor border{"#5B5B5B"};
  QColor node_editor_bg{"#2B2B2B"};

  // --- Built-in compiled presets
  static ThemeDefinition make_default_dark();
  static ThemeDefinition make_default_light();

  // --- Derived helpers
  static QColor derive_group_accent(const std::string &name);

  // --- Derived subsystem configurations
  AppSettings::Colors derive_app_colors() const;
  NodePaletteStyle    derive_node_palette_style() const;

  // --- Serialization
  nlohmann::json                        json_to() const;
  static std::optional<ThemeDefinition> json_from(const nlohmann::json &j);
};

} // namespace hesiod
