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

    // Re-render icons, swapping the baked foreground color (#F4F4F5) for
    // `color` so the icons follow the active palette. Call after the user
    // settings (colors) have been loaded; multi-color icons (accent, black)
    // are preserved.
    void apply_text_color(const QColor &color);

    std::map<std::string, QIcon>       icons_map;  // name -> (tinted) icon
    std::map<std::string, std::string> icon_paths; // name -> absolute .svg path
  } icons;

  struct Global
  {
    int         omp_num_threads = 8;
    std::string icon_path = "data/hesiod_icon.png";
    // empty = start with a blank project; set to a .hsd path to load that
    // file at startup instead
    std::string default_startup_project_file = "";
    std::string quick_start_html_file = "data/quick_start.html";
    std::string node_documentation_path = "data/node_documentation.json";
    std::string git_version_file = "data/git_version.txt";
    std::string ready_made_path = "data/bootstraps";
    bool        save_backup_file = true;
    std::string online_help_url = "https://hesioddoc.readthedocs.io/en/latest/";

    // recently opened/saved project files, most recent first
    std::vector<std::string> recent_files = {};
    int                      max_recent_files = 10;
  } global;

  struct Interface
  {
    bool enable_data_preview_in_node_body = true;
    bool enable_node_settings_in_node_body = false;
    bool enable_texture_downloader = true;
    bool enable_heightmapper_widget = true;
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
    bool        show_node_library_pan = true;
    bool        show_node_settings_pan = true;
    bool        show_node_toolbar_in_settings_pan = true;
    bool        show_viewer = true;
    int         max_bake_resolution = 8192 * 4;
    bool        disable_during_update = false;
    bool        enable_node_groups = true;
    bool        live_update = false;
  } node_editor;

  struct Viewer
  {
    int  width = 512;
    int  height = 512;
    bool add_heighmap_skirt = true;
  } viewer;

  struct Window // main window
  {
    struct WindowGeometry
    {
      int x = 0;
      int y = 0;
      int w = 1024;
      int h = 1024;
    };

    WindowGeometry geom_main;
    WindowGeometry geom_graph_manager;
    WindowGeometry geom_heightmapper;

    int progress_bar_width = 200;

    bool show_graph_manager_widget = false;
    bool show_texture_downloader_widget = false;
    bool show_heightmapper_widget = false;
  } window;
};

} // namespace hesiod