/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDir>
#include <QFileInfo>

#include "highmap/opencl/gpu_opencl.hpp"

#include "hesiod/app/app_settings.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

AppSettings::Icons::Icons()
{
  Logger::log()->trace("AppSettings::Icons::Icons");

  // populate icons
  std::string path = "data/icons";
  QDir        dir(path.c_str());
  if (!dir.exists())
  {
    Logger::log()->error("AppSettings::Icons::Icons: directory does not exist: {}", path);
    return;
  }

  // Filter for .svg files
  QStringList filters;
  filters << "*.svg";
  QFileInfoList file_list = dir.entryInfoList(filters,
                                              QDir::Files | QDir::NoDotAndDotDot);

  for (const QFileInfo &file_info : file_list)
  {
    QString fileName = file_info.baseName(); // name without extension
    QIcon   icon(file_info.absoluteFilePath());
    this->icons_map[fileName.toStdString()] = icon;
  }
}

QIcon AppSettings::Icons::get(const std::string &name) const
{
  if (!this->icons_map.contains(name))
  {
    Logger::log()->error("AppSettings::Icons::get: unknown icon: {}", name);
    return QIcon();
  }
  else
  {
    return this->icons_map.at(name);
  }
}

void AppSettings::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("AppSettings::json_from");

  json_safe_get(json,
                "model.allow_broadcast_receive_within_same_graph",
                model.allow_broadcast_receive_within_same_graph);

  json_safe_get(json, "colors.bg_deep", colors.bg_deep);
  json_safe_get(json, "colors.bg_primary", colors.bg_primary);
  json_safe_get(json, "colors.bg_secondary", colors.bg_secondary);
  json_safe_get(json, "colors.text_primary", colors.text_primary);
  json_safe_get(json, "colors.text_disabled", colors.text_disabled);
  json_safe_get(json, "colors.accent", colors.accent);
  json_safe_get(json, "colors.accent_bw", colors.accent_bw);
  json_safe_get(json, "colors.border", colors.border);
  json_safe_get(json, "colors.hover", colors.hover);
  json_safe_get(json, "colors.pressed", colors.pressed);
  json_safe_get(json, "colors.separator", colors.separator);

  json_safe_get(json, "global.omp_num_threads", global.omp_num_threads);
  json_safe_get(json, "global.icon_path", global.icon_path);
  json_safe_get(json,
                "global.default_startup_project_file",
                global.default_startup_project_file);
  json_safe_get(json, "global.save_backup_file", global.save_backup_file);

  json_safe_get(json,
                "interface.enable_data_preview_in_node_body",
                interface.enable_data_preview_in_node_body);
  json_safe_get(json,
                "interface.enable_node_settings_in_node_body",
                interface.enable_node_settings_in_node_body);
  json_safe_get(json,
                "interface.enable_texture_downloader",
                interface.enable_texture_downloader);
  json_safe_get(json, "interface.enable_tool_tips", interface.enable_tool_tips);
  json_safe_get(json,
                "interface.enable_example_selector_at_startup",
                interface.enable_example_selector_at_startup);

  // OpenCL device
  {
    json_safe_get(json, "node_editor.gpu_device_name", node_editor.gpu_device_name);

    // if it's empty, let the one choosed by default by the OpenCL
    // wrapper
    if (!node_editor.gpu_device_name.empty())
    {
      auto cl_device_map = clwrapper::DeviceManager::get_instance()
                               .get_available_devices();

      for (auto &[device_id, device_name] : cl_device_map)
      {
        if (device_name == node_editor.gpu_device_name)
        {
          if (clwrapper::DeviceManager::get_instance().set_device(device_id))
            clwrapper::KernelManager::get_instance().build_program();
          else
            Logger::log()->error("OpenCL device selection failed");
        }
      }
    }
  }

  json_safe_get(json, "node_editor.default_resolution", node_editor.default_resolution);
  json_safe_get(json, "node_editor.default_tiling", node_editor.default_tiling);
  json_safe_get(json, "node_editor.default_overlap", node_editor.default_overlap);
  json_safe_get(json, "node_editor.preview_w", node_editor.preview_w);
  json_safe_get(json, "node_editor.preview_h", node_editor.preview_h);
  json_safe_get<std::string>(json, "node_editor.doc_path", node_editor.doc_path);
  json_safe_get(json,
                "node_editor.position_delta_when_duplicating_node",
                node_editor.position_delta_when_duplicating_node);
  json_safe_get(json, "node_editor.auto_layout_dx", node_editor.auto_layout_dx);
  json_safe_get(json, "node_editor.auto_layout_dy", node_editor.auto_layout_dy);
  json_safe_get(json,
                "node_editor.show_node_settings_pan",
                node_editor.show_node_settings_pan);
  json_safe_get(json,
                "node_editor.show_node_toolbar_in_settings_pan",
                node_editor.show_node_toolbar_in_settings_pan);
  json_safe_get(json, "node_editor.show_viewer", node_editor.show_viewer);
  json_safe_get(json,
                "node_editor.disable_during_update",
                node_editor.disable_during_update);
  json_safe_get(json, "node_editor.enable_node_groups", node_editor.enable_node_groups);

  json_safe_get(json, "viewer.width", viewer.width);
  json_safe_get(json, "viewer.height", viewer.height);
  json_safe_get(json, "viewer.add_heighmap_skirt", viewer.add_heighmap_skirt);

  // window
  {
    json_safe_get(json, "window.x", window.x);
    json_safe_get(json, "window.y", window.y);
    json_safe_get(json, "window.w", window.w);
    json_safe_get(json, "window.h", window.h);
    json_safe_get(json, "window.progress_bar_width", window.progress_bar_width);
    json_safe_get(json, "window.gm_x", window.gm_x);
    json_safe_get(json, "window.gm_y", window.gm_y);
    json_safe_get(json, "window.gm_w", window.gm_w);
    json_safe_get(json, "window.gm_h", window.gm_h);
  }
}

nlohmann::json AppSettings::json_to() const
{
  Logger::log()->trace("AppSettings::json_to");

  nlohmann::json json;

  json["model.allow_broadcast_receive_within_same_graph"] =
      model.allow_broadcast_receive_within_same_graph;

  json["colors.bg_deep"] = colors.bg_deep.name().toStdString();
  json["colors.bg_primary"] = colors.bg_primary.name().toStdString();
  json["colors.bg_secondary"] = colors.bg_secondary.name().toStdString();
  json["colors.text_primary"] = colors.text_primary.name().toStdString();
  json["colors.text_disabled"] = colors.text_disabled.name().toStdString();
  json["colors.accent"] = colors.accent.name().toStdString();
  json["colors.accent_bw"] = colors.accent_bw.name().toStdString();
  json["colors.border"] = colors.border.name().toStdString();
  json["colors.hover"] = colors.hover.name().toStdString();
  json["colors.pressed"] = colors.pressed.name().toStdString();
  json["colors.separator"] = colors.separator.name().toStdString();

  json["global.omp_num_threads"] = global.omp_num_threads;
  json["global.icon_path"] = global.icon_path;
  json["global.default_startup_project_file"] = global.default_startup_project_file;
  json["global.save_backup_file"] = global.save_backup_file;

  json["interface.enable_data_preview_in_node_body"] =
      interface.enable_data_preview_in_node_body;
  json["interface.enable_node_settings_in_node_body"] =
      interface.enable_node_settings_in_node_body;
  json["interface.enable_texture_downloader"] = interface.enable_texture_downloader;
  json["interface.enable_tool_tips"] = interface.enable_tool_tips;
  json["interface.enable_example_selector_at_startup"] =
      interface.enable_example_selector_at_startup;

  json["node_editor.gpu_device_name"] = node_editor.gpu_device_name;
  json["node_editor.default_resolution"] = node_editor.default_resolution;
  json["node_editor.default_tiling"] = node_editor.default_tiling;
  json["node_editor.default_overlap"] = node_editor.default_overlap;
  json["node_editor.preview_w"] = node_editor.preview_w;
  json["node_editor.preview_h"] = node_editor.preview_h;
  json["node_editor.doc_path"] = node_editor.doc_path;
  json["node_editor.position_delta_when_duplicating_node"] =
      node_editor.position_delta_when_duplicating_node;
  json["node_editor.auto_layout_dx"] = node_editor.auto_layout_dx;
  json["node_editor.auto_layout_dy"] = node_editor.auto_layout_dy;
  json["node_editor.show_node_settings_pan"] = node_editor.show_node_settings_pan;
  json["node_editor.show_node_toolbar_in_settings_pan"] =
      node_editor.show_node_toolbar_in_settings_pan;
  json["node_editor.show_viewer"] = node_editor.show_viewer;
  json["node_editor.disable_during_update"] = node_editor.disable_during_update;
  json["node_editor.enable_node_groups"] = node_editor.enable_node_groups;

  json["viewer.width"] = viewer.width;
  json["viewer.height"] = viewer.height;
  json["viewer.add_heighmap_skirt"] = viewer.add_heighmap_skirt;

  json["window.x"] = window.x;
  json["window.y"] = window.y;
  json["window.w"] = window.w;
  json["window.h"] = window.h;
  json["window.progress_bar_width"] = window.progress_bar_width;
  json["window.gm_x"] = window.gm_x;
  json["window.gm_y"] = window.gm_y;
  json["window.gm_w"] = window.gm_w;
  json["window.gm_h"] = window.gm_h;
  json["window.show_graph_manager_widget"] = window.show_graph_manager_widget;
  json["window.show_texture_downloader_widget"] = window.show_texture_downloader_widget;

  return json;
}

} // namespace hesiod
