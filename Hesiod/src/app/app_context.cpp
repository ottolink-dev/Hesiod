/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <filesystem>

#include <QStandardPaths>

#include "hesiod/app/app_context.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

void AppContext::initialize()
{
  Logger::log()->trace("AppContext::initialize");

  this->load_settings();
  this->load_node_documentation();
}

void AppContext::load_node_documentation()
{
  Logger::log()->trace("AppContext::load_node_documentation");

  const std::string doc_path = this->app_settings.global.node_documentation_path;

  // loading data
  try
  {
    std::ifstream file(doc_path);

    if (!file.is_open())
    {
      Logger::log()->error("Could not open documentation file: {}", doc_path);
      throw std::runtime_error("Documentation file not found");
    }

    file >> this->node_documentation;
    Logger::log()->trace("JSON successfully loaded from {}", doc_path);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("Error loading documentation: {}", e.what());
    this->node_documentation = nlohmann::json::object();
  }
}

void AppContext::load_project_model(const std::string &fname)
{
  Logger::log()->trace("AppContext::load_project_model: {}", fname);

  if (!std::filesystem::exists(std::filesystem::path(fname)))
  {
    Logger::log()->error("AppContext::load_project_model: file does not exist: {}",
                         fname);
    return;
  }

  this->new_project();

  nlohmann::json json = json_from_file(fname);
  this->project_model->json_from(json);
}

void AppContext::load_settings()
{
  Logger::log()->trace("AppContext::load_settings");

  std::string    fname = get_config_file_path_auto("hesiod");
  nlohmann::json json = json_from_file(fname);

  this->settings_json_from(json);
}

void AppContext::new_project()
{
  Logger::log()->trace("AppContext::new_project");
  this->project_model = std::make_unique<ProjectModel>();
}

void AppContext::reset_settings()
{
  // reset to default values
  this->app_settings = AppSettings();
  this->style_settings = StyleSettings();
}

void AppContext::restore_state()
{
  if (this->saved_state.is_null() || this->saved_state.empty())
    return;

  try
  {
    this->settings_json_from(this->saved_state);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("Failed to restore state: {}", e.what());
    return;
  }
}

void AppContext::save_project_model(const std::string &fname) const
{
  Logger::log()->trace("AppContext::save_project_model: {}", fname);

  nlohmann::json json = this->project_model->json_to();
  json_to_file(json, fname, /* merge_with_existing_content */ true);
}

void AppContext::save_state() const { this->saved_state = this->settings_json_to(); }

void AppContext::save_settings() const
{
  Logger::log()->trace("AppContext::save_settings");

  // put everything into a json and dump it
  nlohmann::json json = this->settings_json_to();

  std::string fname = get_config_file_path_auto("hesiod");
  bool        merge_with_existing_content = true;
  json_to_file(json, fname, merge_with_existing_content);
}

void AppContext::settings_json_from(nlohmann::json const &json)
{
  Logger::log()->trace("AppContext::settings_json_from");

  // --- settings

  if (json.contains("app_settings"))
    this->app_settings.json_from(json["app_settings"]);
  else
    Logger::log()->error("AppContext::settings_json_from: could not parse app_settings");

  if (json.contains("style_settings"))
    this->style_settings.json_from(json["style_settings"]);
  else
    Logger::log()->error(
        "AppContext::settings_json_from: could not parse style_settings");
}

nlohmann::json AppContext::settings_json_to() const
{
  Logger::log()->trace("AppContext::settings_json_to");

  nlohmann::json json;
  json["app_settings"] = this->app_settings.json_to();
  json["style_settings"] = this->style_settings.json_to();
  return json;
}

// --- HELPERS ---

std::string get_config_file_path(const QString &app_name, bool portable_mode)
{
  QString path;

  if (portable_mode)
  {
    // Portable mode → store config next to the executable
    QDir app_dir(QCoreApplication::applicationDirPath());
    path = app_dir.filePath(app_name + ".json");
  }
  else
  {
    // Standard per-user config
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
    {
      // Fallback: use AppData if ConfigLocation unavailable
      dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }

    QDir().mkpath(dir); // ensure the directory exists
    path = dir + QDir::separator() + app_name + ".json";
  }

  return path.toStdString();
}

std::string get_config_file_path_auto(const QString &app_name)
{
  QDir    app_dir(QCoreApplication::applicationDirPath());
  QString portable_path = app_dir.filePath(app_name + ".json");
  QString portable_flag = app_dir.filePath("portable.flag");

  bool use_portable = QFileInfo::exists(portable_path) ||
                      QFileInfo::exists(portable_flag);

  return get_config_file_path(app_name, use_portable);
}

} // namespace hesiod
