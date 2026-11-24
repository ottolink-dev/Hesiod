/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "hesiod/gui/theme/theme.hpp"

namespace hesiod
{

class ThemeRegistry
{
public:
  static ThemeRegistry &instance();

  void scan_themes();

  std::vector<std::string> available_theme_names() const;

  bool has_theme(const std::string &name) const;

  ThemeDefinition get_theme(const std::string &name) const;

  void register_theme(const ThemeDefinition &theme);

  bool save_user_theme(const ThemeDefinition &theme);

private:
  ThemeRegistry();

  std::map<std::string, ThemeDefinition> themes;
};

} // namespace hesiod
