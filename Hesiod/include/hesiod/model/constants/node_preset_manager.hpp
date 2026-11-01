/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "meta/serialization/snapshot_manager.hpp"

namespace hesiod
{

struct PresetFileInfo
{
  std::string                                     author;
  std::string                                     comment;
  bool                                            enabled = true;
  std::string                                     file_path;
  int                                             format_version = 1;
  bool                                            is_bundled = false;
  std::map<std::string, std::vector<std::string>> node_type_to_presets;
  std::string                                     title;
};

class NodePresetManager
{
public:
  static NodePresetManager &get_instance();

  void clear();

  bool export_preset(const std::string &node_type,
                     const std::string &preset_name,
                     const std::string &file_path) const;

  std::vector<std::string> get_active_preset_names(const std::string &node_type) const;

  const std::vector<PresetFileInfo> &get_preset_files() const;

  meta::SnapshotManager &get_snapshot_manager(const std::string &node_type);

  std::string get_user_preset_file_path() const;

  bool import_preset_file(const std::string &file_path);

  void reload_presets();

  void rescan_files();

  void save_user_preset(const std::string &node_type, const std::string &preset_name);

  void set_file_enabled(const std::string &file_path, bool enabled);

private:
  NodePresetManager();
  ~NodePresetManager() = default;

  NodePresetManager(const NodePresetManager &) = delete;
  NodePresetManager &operator=(const NodePresetManager &) = delete;

  void load_preset_file(PresetFileInfo &file_info);

  std::vector<PresetFileInfo>                  preset_files;
  std::map<std::string, meta::SnapshotManager> snapshot_managers;
};

} // namespace hesiod
