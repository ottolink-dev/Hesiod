/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QColor>
#include <QIcon>

#include "nlohmann/json.hpp"

#define HSD_ICON(name)                                                                   \
  static_cast<hesiod::HesiodApplication *>(QCoreApplication::instance())                 \
      ->get_context()                                                                    \
      .app_settings.icons.get(name)

namespace hesiod
{

struct AppSettings
{
  AppSettings() = default;

  // --- Serialization

  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  // --- Data

  struct Model
  {
    bool allow_broadcast_receive_within_same_graph = true;
  } model;

  struct Colors
  {
    QColor bg_deep{"#191919"};
    QColor bg_primary{"#2B2B2B"};
    QColor bg_secondary{"#4B4B4B"};
    QColor text_primary{"#F4F4F5"};
    QColor text_secondary{"#949495"};
    QColor text_disabled{"#3C3C3C"};
    QColor accent{"#5E81AC"};
    QColor accent_bw{"#FFFFFF"};
    QColor border{"#5B5B5B"};
    QColor hover{"#8B8B8B"};
    QColor pressed{"#ABABAB"};
    QColor separator{"#ABABAB"};
  } colors;

  struct Icons
  {
    explicit Icons();
    QIcon get(const std::string &name) const;

    std::map<std::string, QIcon> icons_map; // populated in ctor
  } icons;

  struct Global
  {
    int         omp_num_threads = 8;
    std::string icon_path = "data/hesiod_icon.png";
    std::string default_startup_project_file = "data/default.hsd";
    std::string quick_start_html_file = "data/quick_start.html";
    std::string node_documentation_path = "data/node_documentation.json";
    std::string git_version_file = "data/git_version.txt";
    std::string ready_made_path = "data/bootstraps";
    bool        save_backup_file = true;
    std::string online_help_url = "https://hesioddoc.readthedocs.io/en/latest/";
  } global;

  struct Interface
  {
    bool enable_data_preview_in_node_body = true;
    bool enable_node_settings_in_node_body = false;
    bool enable_texture_downloader = true;
    bool enable_tool_tips = true;
    bool enable_example_selector_at_startup = true;
  } interface;

  struct NodeEditor
  {
    std::string gpu_device_name = ""; // let CLWrapper decides
    int         default_resolution = 1024;
    int         default_tiling = 4;
    float       default_overlap = 0.5f;
    int         preview_w = 128;
    int         preview_h = 128;
    std::string doc_path = "data/node_documentation.json";
    float       position_delta_when_duplicating_node = 200.f;
    float       auto_layout_dx = 256.f;
    float       auto_layout_dy = 384.f;
    bool        show_node_settings_pan = true;
    bool        show_node_toolbar_in_settings_pan = true;
    bool        show_viewer = true;
    int         max_bake_resolution = 8192 * 4;
    bool        disable_during_update = false;
    bool        enable_node_groups = true;
  } node_editor;

  struct Viewer
  {
    int  width = 512;
    int  height = 512;
    bool add_heighmap_skirt = true;
  } viewer;

  struct Window // main window
  {
    int x = 0;
    int y = 0;
    int w = 1024;
    int h = 1024;
    int progress_bar_width = 200;

    int gm_x = 0; // graph manager geometry
    int gm_y = 0;
    int gm_w = 1024;
    int gm_h = 1024;

    bool show_graph_manager_widget = false;
    bool show_texture_downloader_widget = false;
  } window;
};

} // namespace hesiod