/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <filesystem>

#include <QCoreApplication>
#include <QStandardPaths>

#include "highmap/openmp.hpp"

#include "hesiod/app/app_context.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_worker.hpp"
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

ErrorManager &AppContext::get_error_manager() { return this->error_manager; }

const ErrorManager &AppContext::get_error_manager() const { return this->error_manager; }

int AppContext::load_project_model(const std::string &fname)
{
  Logger::log()->trace("AppContext::load_project_model: {}", fname);

  if (!std::filesystem::exists(std::filesystem::path(fname)))
  {
    Logger::log()->error("AppContext::load_project_model: file does not exist: {}",
                         fname);
    return 0;
  }

  this->new_project();

  int capped = 0;

  try
  {
    nlohmann::json json = json_from_file(fname);

    // lighter editing of large projects: graphs above 1K are loaded at 1K. Only
    // the loaded copy changes, not the file nor the bake resolution. Never in
    // the CLI modes, which export at the graph resolution
    if (this->app_settings.node_editor.open_projects_at_1k && !this->headless &&
        json.contains("graph_manager") && json["graph_manager"].contains("graph_nodes"))
      for (auto &[_, graph] : json["graph_manager"]["graph_nodes"].items())
      {
        if (!graph.contains("model_config"))
          continue;
        auto     &config = graph["model_config"];
        const int w = config.value("shape.x", 0);
        const int h = config.value("shape.y", 0);
        if (std::max(w, h) > 1024) // keeping the aspect ratio
        {
          config["shape.x"] = std::max(1, w * 1024 / std::max(w, h));
          config["shape.y"] = std::max(1, h * 1024 / std::max(w, h));
          capped++;
        }
      }

    this->project_model->json_from(json);
  }
  catch (const std::exception &e)
  {
    this->error_manager.push_error(
        ErrorCategory::IO,
        std::format("Failed to read project file '{}': {}", fname, e.what()));
  }
  catch (...)
  {
    this->error_manager.push_error(
        ErrorCategory::IO,
        std::format("Failed to read project file '{}': unknown error", fname));
  }

  return capped;
}

void AppContext::load_settings()
{
  Logger::log()->trace("AppContext::load_settings");

  std::string fname = get_config_file_path_auto("hesiod");

  // A settings file the user cannot open the application to fix is a dead end:
  // an unparseable number, a truncated write or a type that does not match what
  // a key expects used to escape all the way out of main() and kill startup
  // before any window appeared. Fall back to the compiled defaults and say so.
  try
  {
    nlohmann::json json = json_from_file(fname);
    this->settings_json_from(json);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("AppContext::load_settings: could not read the settings "
                         "file, starting from defaults instead ({}): {}",
                         fname,
                         e.what());

    this->reset_settings();
  }
}

void AppContext::new_project()
{
  Logger::log()->trace("AppContext::new_project");
  this->error_manager.clear();
  this->project_model = std::make_unique<ProjectModel>();

  // the graphs of an interactive session are updated in the background
  GraphWorker::enabled = [this]()
  { return !this->headless && this->app_settings.node_editor.async_update; };
  GraphWorker::post_to_gui = [](std::function<void()> fct)
  { QMetaObject::invokeMethod(qApp, std::move(fct), Qt::QueuedConnection); };
  GraphWorker::thread_setup = [this]()
  { hmap::init_openmp(this->app_settings.global.omp_num_threads); };
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

bool is_portable_mode(const QString &app_name)
{
  QDir    app_dir(QCoreApplication::applicationDirPath());
  QString portable_path = app_dir.filePath(app_name + ".json");
  QString portable_flag = app_dir.filePath("portable.flag");

  return QFileInfo::exists(portable_path) || QFileInfo::exists(portable_flag);
}

std::string get_config_file_path_auto(const QString &app_name)
{
  return get_config_file_path(app_name, is_portable_mode(app_name));
}

} // namespace hesiod
