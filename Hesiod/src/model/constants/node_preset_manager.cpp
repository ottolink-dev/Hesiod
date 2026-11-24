/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <filesystem>
#include <fstream>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include "hesiod/logger.hpp"
#include "hesiod/model/constants/node_preset_manager.hpp"

namespace fs = std::filesystem;

namespace hesiod
{

namespace
{

std::string get_user_presets_dir()
{
  QString base_dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  if (base_dir.isEmpty())
    base_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  QDir dir(base_dir);
  dir.mkpath("presets");
  return dir.filePath("presets").toStdString();
}

} // namespace

NodePresetManager &NodePresetManager::get_instance()
{
  static NodePresetManager instance;
  return instance;
}

NodePresetManager::NodePresetManager()
{
  Logger::log()->info("NodePresetManager::NodePresetManager: initializing...");
  this->rescan_files();
}

void NodePresetManager::clear()
{
  this->preset_files.clear();
  this->snapshot_managers.clear();
}

void NodePresetManager::delete_user_preset(const std::string &node_type,
                                           const std::string &preset_name)
{
  Logger::log()->trace("NodePresetManager::delete_user_preset: {}::{}",
                       node_type,
                       preset_name);

  // Search through all writable (non-bundled) preset files that contain this preset
  for (const auto &file_info : this->preset_files)
  {
    if (file_info.is_bundled)
      continue;

    auto it = file_info.node_type_to_presets.find(node_type);
    if (it == file_info.node_type_to_presets.end())
      continue;

    const auto &preset_list = it->second;
    if (std::find(preset_list.begin(), preset_list.end(), preset_name) ==
        preset_list.end())
      continue;

    // Found in this file; remove it from JSON
    try
    {
      nlohmann::json root;
      std::ifstream  in(file_info.file_path);
      if (in.is_open())
        in >> root;
      in.close();

      if (root.contains("presets") && root["presets"].is_object() &&
          root["presets"].contains(node_type) && root["presets"][node_type].is_object())
      {
        root["presets"][node_type].erase(preset_name);
        if (root["presets"][node_type].empty())
          root["presets"].erase(node_type);

        std::ofstream out(file_info.file_path);
        if (out.is_open())
          out << root.dump(2);
      }
    }
    catch (const std::exception &e)
    {
      Logger::log()->error(
          "NodePresetManager::delete_user_preset: error removing from {}: {}",
          file_info.file_path,
          e.what());
    }
  }

  this->rescan_files();
}

bool NodePresetManager::export_preset(const std::string &node_type,
                                      const std::string &preset_name,
                                      const std::string &file_path) const
{
  Logger::log()->trace("NodePresetManager::export_preset: {}::{}",
                       node_type,
                       preset_name);

  auto it = this->snapshot_managers.find(node_type);
  if (it == this->snapshot_managers.end() || !it->second.has(preset_name))
  {
    Logger::log()->error("NodePresetManager::export_preset: preset '{}/{}' not found",
                         node_type,
                         preset_name);
    return false;
  }

  try
  {
    nlohmann::json root;
    root["format_version"] = 1;
    root["title"] = preset_name;
    root["presets"][node_type][preset_name] = it->second.load(preset_name);

    std::ofstream out(file_path);
    if (!out.is_open())
    {
      Logger::log()->error("NodePresetManager::export_preset: cannot open {}", file_path);
      return false;
    }

    out << root.dump(2);
    return true;
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("NodePresetManager::export_preset: exception {}", e.what());
    return false;
  }
}

std::vector<std::string> NodePresetManager::get_active_preset_names(
    const std::string &node_type) const
{
  auto it = this->snapshot_managers.find(node_type);
  if (it == this->snapshot_managers.end())
    return {};

  return it->second.names();
}

const std::vector<PresetFileInfo> &NodePresetManager::get_preset_files() const
{
  return this->preset_files;
}

meta::SnapshotManager &NodePresetManager::get_snapshot_manager(
    const std::string &node_type)
{
  return this->snapshot_managers[node_type];
}

std::string NodePresetManager::get_user_preset_file_path() const
{
  return (fs::path(get_user_presets_dir()) / "node_presets_user.json").string();
}

bool NodePresetManager::import_preset_file(const std::string &file_path)
{
  Logger::log()->trace("NodePresetManager::import_preset_file: {}", file_path);

  if (!fs::exists(file_path))
  {
    Logger::log()->error("NodePresetManager::import_preset_file: file does not exist: {}",
                         file_path);
    return false;
  }

  // copy or copy-if-not-in-user-dir
  const std::string user_dir = get_user_presets_dir();
  fs::path          src_path(file_path);
  fs::path          dst_path = fs::path(user_dir) / src_path.filename();

  std::error_code ec;
  if (src_path != dst_path)
  {
    fs::copy_file(src_path, dst_path, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
      Logger::log()->error("NodePresetManager::import_preset_file: copy error: {}",
                           ec.message());
      return false;
    }
  }

  this->rescan_files();
  return true;
}

void NodePresetManager::load_preset_file(PresetFileInfo &file_info)
{
  file_info.node_type_to_presets.clear();

  std::ifstream file(file_info.file_path);
  if (!file.is_open())
  {
    Logger::log()->error("NodePresetManager::load_preset_file: could not open {}",
                         file_info.file_path);
    return;
  }

  try
  {
    nlohmann::json root;
    file >> root;

    file_info.format_version = root.value("format_version", 1);
    file_info.title = root.value("title", fs::path(file_info.file_path).stem().string());
    file_info.author = root.value("author", "");
    file_info.comment = root.value("comment", "");

    if (root.contains("presets") && root["presets"].is_object())
    {
      for (const auto &[node_type, presets_obj] : root["presets"].items())
      {
        if (!presets_obj.is_object())
          continue;

        for (const auto &[preset_name, snapshot_json] : presets_obj.items())
        {
          file_info.node_type_to_presets[node_type].push_back(preset_name);

          if (file_info.enabled)
          {
            auto &sm = this->snapshot_managers[node_type];
            sm.save(preset_name, snapshot_json);
          }
        }
      }
    }
    // Also support direct snapshot dictionary or legacy preset formats gracefully
    else if (root.contains("name") && root.contains("data"))
    {
      const std::string name = root.value("name", "Preset");
      const std::string node_type = root.value("node_type", "");
      if (!node_type.empty())
      {
        file_info.node_type_to_presets[node_type].push_back(name);
        if (file_info.enabled)
          this->snapshot_managers[node_type].save(name, root["data"]);
      }
    }
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("NodePresetManager::load_preset_file: failed to parse {}: {}",
                         file_info.file_path,
                         e.what());
  }
}

void NodePresetManager::reload_presets()
{
  Logger::log()->trace("NodePresetManager::reload_presets");

  for (auto &[_, sm] : this->snapshot_managers)
    sm.clear();

  for (auto &file_info : this->preset_files)
  {
    if (file_info.enabled)
      this->load_preset_file(file_info);
  }
}

void NodePresetManager::rescan_files()
{
  Logger::log()->trace("NodePresetManager::rescan_files");

  // Keep track of previously enabled states
  std::map<std::string, bool> prev_enabled;
  for (const auto &info : this->preset_files)
    prev_enabled[info.file_path] = info.enabled;

  this->preset_files.clear();

  std::vector<std::pair<fs::path, bool>> scan_dirs;

  // 1. App binary / executable dir
  if (qApp)
  {
    fs::path exe_dir(QCoreApplication::applicationDirPath().toStdString());
    scan_dirs.emplace_back(exe_dir, false);
  }

  // 2. Bundled presets directory (e.g. data/presets or Hesiod/data/presets)
  scan_dirs.emplace_back(fs::path("data/presets"), true);
  scan_dirs.emplace_back(fs::path("Hesiod/data/presets"), true);

  // 3. User config presets directory
  scan_dirs.emplace_back(fs::path(get_user_presets_dir()), false);

  std::vector<std::string> seen_paths;

  for (const auto &[dir, is_bundled] : scan_dirs)
  {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
      continue;

    for (const auto &entry : fs::directory_iterator(dir, ec))
    {
      if (!entry.is_regular_file(ec))
        continue;

      const fs::path    filename = entry.path().filename();
      const std::string name_str = filename.string();

      // Look for files starting with "node_presets_" and ending with ".json"
      if (name_str.rfind("node_presets_", 0) == 0 && entry.path().extension() == ".json")
      {
        const std::string canonical = fs::canonical(entry.path(), ec).string();
        const std::string norm_path = canonical.empty() ? entry.path().string()
                                                        : canonical;

        if (std::find(seen_paths.begin(), seen_paths.end(), norm_path) !=
            seen_paths.end())
          continue;

        seen_paths.push_back(norm_path);

        PresetFileInfo info;
        info.file_path = norm_path;
        info.is_bundled = is_bundled;
        info.enabled = prev_enabled.contains(norm_path) ? prev_enabled[norm_path] : true;

        this->load_preset_file(info);
        this->preset_files.push_back(std::move(info));
      }
    }
  }

  this->reload_presets();
}

void NodePresetManager::save_user_preset(const std::string &node_type,
                                         const std::string &preset_name)
{
  Logger::log()->trace("NodePresetManager::save_user_preset: {}::{}",
                       node_type,
                       preset_name);

  auto it = this->snapshot_managers.find(node_type);
  if (it == this->snapshot_managers.end() || !it->second.has(preset_name))
    return;

  const std::string user_file = this->get_user_preset_file_path();
  nlohmann::json    root;

  if (fs::exists(user_file))
  {
    try
    {
      std::ifstream in(user_file);
      if (in.is_open())
        in >> root;
    }
    catch (const std::exception &e)
    {
      Logger::log()->warn(
          "NodePresetManager::save_user_preset: reading existing file: {}",
          e.what());
    }
  }

  root["format_version"] = 1;
  root["title"] = "User Presets";
  root["comment"] = "User created presets saved across projects.";
  root["presets"][node_type][preset_name] = it->second.load(preset_name);

  try
  {
    std::ofstream out(user_file);
    if (out.is_open())
      out << root.dump(2);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("NodePresetManager::save_user_preset: write failed: {}",
                         e.what());
  }

  this->rescan_files();
}

void NodePresetManager::set_file_enabled(const std::string &file_path, bool enabled)
{
  for (auto &info : this->preset_files)
  {
    if (info.file_path == file_path)
    {
      if (info.enabled != enabled)
      {
        info.enabled = enabled;
        this->reload_presets();
      }
      return;
    }
  }
}

} // namespace hesiod
