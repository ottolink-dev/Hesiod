/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <filesystem>
#include <functional>
#include <string>

#include "nlohmann/json.hpp"

#include "hesiod/model/graph/bake_config.hpp"
#include "hesiod/model/graph/graph_manager.hpp"

namespace hesiod
{

// =====================================
// ProjectModel
// =====================================
class ProjectModel
{
public:
  explicit ProjectModel();

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  void cleanup();
  void initialize();

  BakeConfig            get_bake_config() const;
  std::string           get_comment() const;
  GraphManager         *get_graph_manager_ref();
  bool                  get_is_dirty() const;
  std::string           get_name() const;
  std::filesystem::path get_path() const;
  void                  set_bake_config(const BakeConfig &new_bake_config);
  void                  set_comment(const std::string &new_comment);
  void                  set_is_dirty(bool new_state);
  void                  set_path(const std::filesystem::path &new_path);
  void                  set_path(const std::string &new_path);
  void                  set_name(const std::string &new_name);

  // --- Callbacks
  std::function<void()> project_name_changed;
  std::function<void()> is_dirty_changed;
  // fired on every change, unlike is_dirty_changed which only fires on the
  // clean -> dirty transition; feeds the autosave "changed since snapshot" flag
  std::function<void()> has_changed;

  void on_has_changed();

private:
  // --- Members
  std::string                   name;
  std::filesystem::path         path;
  std::string                   comment;
  BakeConfig                    bake_config;
  std::shared_ptr<GraphManager> graph_manager;
  bool                          is_dirty = false;
};

} // namespace hesiod