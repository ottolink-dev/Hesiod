/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "hesiod/gui/theme/theme_registry.hpp"
#include "hesiod/logger.hpp"

namespace fs = std::filesystem;

namespace hesiod
{

namespace
{

fs::path get_user_themes_dir()
{
  const char *home = std::getenv("HOME");
  if (!home)
    return fs::path("./themes");
  return fs::path(home) / ".config" / "hesiod" / "themes";
}

std::vector<fs::path> get_theme_search_paths()
{
  std::vector<fs::path> paths;

  paths.push_back(get_user_themes_dir());
  paths.push_back(fs::path("data/themes"));
  paths.push_back(fs::path("Hesiod/data/themes"));

  return paths;
}

} // namespace

ThemeRegistry::ThemeRegistry()
{
  // Built-in themes are always compiled-in and registered first
  this->register_theme(ThemeDefinition::make_default_dark());
  this->register_theme(ThemeDefinition::make_default_light());

  this->scan_themes();
}

ThemeRegistry &ThemeRegistry::instance()
{
  static ThemeRegistry s_instance;
  return s_instance;
}

void ThemeRegistry::scan_themes()
{
  for (const auto &dir_path : get_theme_search_paths())
  {
    std::error_code ec;
    if (!fs::exists(dir_path, ec) || !fs::is_directory(dir_path, ec))
      continue;

    for (const auto &entry : fs::directory_iterator(dir_path, ec))
    {
      if (ec || !entry.is_regular_file())
        continue;

      if (entry.path().extension() == ".json")
      {
        std::ifstream file(entry.path());
        if (!file.is_open())
          continue;

        try
        {
          nlohmann::json j;
          file >> j;
          auto theme_opt = ThemeDefinition::json_from(j);
          if (theme_opt)
          {
            if (theme_opt->name.empty())
              theme_opt->name = entry.path().stem().string();

            this->register_theme(*theme_opt);
            Logger::log()->info("ThemeRegistry: loaded theme '{}' from {}",
                                theme_opt->name,
                                entry.path().string());
          }
        }
        catch (const std::exception &ex)
        {
          Logger::log()->warn("ThemeRegistry: failed parsing theme from {}: {}",
                              entry.path().string(),
                              ex.what());
        }
      }
    }
  }
}

std::vector<std::string> ThemeRegistry::available_theme_names() const
{
  std::vector<std::string> names;
  names.reserve(this->themes.size());

  // Guarantee Dark and Light are first if present
  if (this->has_theme("Dark"))
    names.push_back("Dark");
  if (this->has_theme("Light"))
    names.push_back("Light");

  for (const auto &[name, _] : this->themes)
  {
    if (name != "Dark" && name != "Light")
      names.push_back(name);
  }

  return names;
}

bool ThemeRegistry::has_theme(const std::string &name) const
{
  return this->themes.find(name) != this->themes.end();
}

ThemeDefinition ThemeRegistry::get_theme(const std::string &name) const
{
  auto it = this->themes.find(name);
  if (it != this->themes.end())
    return it->second;

  return ThemeDefinition::make_default_dark();
}

void ThemeRegistry::register_theme(const ThemeDefinition &theme)
{
  this->themes[theme.name] = theme;
}

bool ThemeRegistry::save_user_theme(const ThemeDefinition &theme)
{
  try
  {
    fs::path        dir = get_user_themes_dir();
    std::error_code ec;
    fs::create_directories(dir, ec);

    fs::path      file_path = dir / (theme.name + ".json");
    std::ofstream out(file_path);
    if (!out.is_open())
      return false;

    out << theme.json_to().dump(2);
    this->register_theme(theme);
    return true;
  }
  catch (...)
  {
    return false;
  }
}

} // namespace hesiod
