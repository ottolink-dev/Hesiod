/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPixmap>

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

    // keep the source path so icons can be re-tinted once the palette is
    // loaded (see apply_text_color); load an untinted icon for now
    this->icon_paths[fileName.toStdString()] = file_info.absoluteFilePath().toStdString();
    this->icons_map[fileName.toStdString()] = QIcon(file_info.absoluteFilePath());
  }
}

void AppSettings::Icons::apply_text_color(const QColor &color)
{
  Logger::log()->trace("AppSettings::Icons::apply_text_color: {}",
                       color.name().toStdString());

  // The icon SVGs bake in the dark-theme foreground (#F4F4F5). Swap it for
  // the active text color so the icons follow the palette; other colors
  // (accent #5E81AC, black, ...) are left untouched.
  const QByteArray target = color.name().toUtf8();

  for (const auto &[name, path] : this->icon_paths)
  {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly))
      continue;

    QByteArray data = file.readAll();
    file.close();

    data.replace("#F4F4F5", target);
    data.replace("#f4f4f5", target);

    QBuffer      buffer(&data);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(QSize(64, 64)); // render crisp, let Qt downscale
    const QImage image = reader.read();
    if (image.isNull())
      continue; // svg image plugin missing or unreadable; keep default icon

    this->icons_map[name] = QIcon(QPixmap::fromImage(image));
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
  // configs written before the blank-startup change persisted the old
  // compiled default on every exit; those users expect the new blank
  // startup, not a pinned default file
  if (global.default_startup_project_file == "data/default.hsd")
    global.default_startup_project_file = "";
  json_safe_get(json, "global.save_backup_file", global.save_backup_file);
  json_safe_get(json, "global.recent_files", global.recent_files);
  json_safe_get(json, "global.max_recent_files", global.max_recent_files);

  json_safe_get(json,
                "interface.enable_data_preview_in_node_body",
                interface.enable_data_preview_in_node_body);
  json_safe_get(json,
                "interface.enable_node_settings_in_node_body",
                interface.enable_node_settings_in_node_body);
  json_safe_get(json,
                "interface.enable_texture_downloader",
                interface.enable_texture_downloader);
  json_safe_get(json,
                "interface.enable_heightmapper_widget",
                interface.enable_heightmapper_widget);
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
                "node_editor.show_node_library_pan",
                node_editor.show_node_library_pan);
  json_safe_get(json,
                "node_editor.show_node_settings_pan",
                node_editor.show_node_settings_pan);
  json_safe_get(json,
                "node_editor.node_settings_panel_width",
                node_editor.node_settings_panel_width);
  json_safe_get(json,
                "node_editor.node_settings_pan_width",
                node_editor.node_settings_panel_width);
  json_safe_get(json,
                "node_editor.show_node_toolbar_in_settings_pan",
                node_editor.show_node_toolbar_in_settings_pan);
  json_safe_get(json, "node_editor.show_viewer", node_editor.show_viewer);
  json_safe_get(json,
                "node_editor.disable_during_update",
                node_editor.disable_during_update);
  json_safe_get(json, "node_editor.enable_node_groups", node_editor.enable_node_groups);
  json_safe_get(json, "node_editor.live_update", node_editor.live_update);
  json_safe_get(json, "node_editor.port_radius", node_editor.port_radius);

  json_safe_get(json, "viewer.width", viewer.width);
  json_safe_get(json, "viewer.height", viewer.height);
  json_safe_get(json, "viewer.add_heighmap_skirt", viewer.add_heighmap_skirt);

  // window
  {
    json_safe_get(json, "window.geom_main.x", window.geom_main.x);
    json_safe_get(json, "window.geom_main.y", window.geom_main.y);
    json_safe_get(json, "window.geom_main.w", window.geom_main.w);
    json_safe_get(json, "window.geom_main.h", window.geom_main.h);
    json_safe_get(json, "window.progress_bar_width", window.progress_bar_width);
    json_safe_get(json, "window.geom_graph_manager.x", window.geom_graph_manager.x);
    json_safe_get(json, "window.geom_graph_manager.y", window.geom_graph_manager.y);
    json_safe_get(json, "window.geom_graph_manager.w", window.geom_graph_manager.w);
    json_safe_get(json, "window.geom_graph_manager.h", window.geom_graph_manager.h);
    json_safe_get(json, "window.geom_heightmapper.x", window.geom_heightmapper.x);
    json_safe_get(json, "window.geom_heightmapper.y", window.geom_heightmapper.y);
    json_safe_get(json, "window.geom_heightmapper.w", window.geom_heightmapper.w);
    json_safe_get(json, "window.geom_heightmapper.h", window.geom_heightmapper.h);
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
  json["global.recent_files"] = global.recent_files;
  json["global.max_recent_files"] = global.max_recent_files;

  json["interface.enable_data_preview_in_node_body"] =
      interface.enable_data_preview_in_node_body;
  json["interface.enable_node_settings_in_node_body"] =
      interface.enable_node_settings_in_node_body;
  json["interface.enable_texture_downloader"] = interface.enable_texture_downloader;
  json["interface.enable_heightmapper_widget"] = interface.enable_heightmapper_widget;
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
  json["node_editor.show_node_library_pan"] = node_editor.show_node_library_pan;
  json["node_editor.show_node_settings_pan"] = node_editor.show_node_settings_pan;
  json["node_editor.node_settings_panel_width"] = node_editor.node_settings_panel_width;
  json["node_editor.show_node_toolbar_in_settings_pan"] =
      node_editor.show_node_toolbar_in_settings_pan;
  json["node_editor.show_viewer"] = node_editor.show_viewer;
  json["node_editor.disable_during_update"] = node_editor.disable_during_update;
  json["node_editor.enable_node_groups"] = node_editor.enable_node_groups;
  json["node_editor.live_update"] = node_editor.live_update;
  json["node_editor.port_radius"] = node_editor.port_radius;

  json["viewer.width"] = viewer.width;
  json["viewer.height"] = viewer.height;
  json["viewer.add_heighmap_skirt"] = viewer.add_heighmap_skirt;

  json["window.geom_main.x"] = window.geom_main.x;
  json["window.geom_main.y"] = window.geom_main.y;
  json["window.geom_main.w"] = window.geom_main.w;
  json["window.geom_main.h"] = window.geom_main.h;
  json["window.progress_bar_width"] = window.progress_bar_width;
  json["window.geom_graph_manager.x"] = window.geom_graph_manager.x;
  json["window.geom_graph_manager.y"] = window.geom_graph_manager.y;
  json["window.geom_graph_manager.w"] = window.geom_graph_manager.w;
  json["window.geom_graph_manager.h"] = window.geom_graph_manager.h;
  json["window.geom_heightmapper.x"] = window.geom_heightmapper.x;
  json["window.geom_heightmapper.y"] = window.geom_heightmapper.y;
  json["window.geom_heightmapper.w"] = window.geom_heightmapper.w;
  json["window.geom_heightmapper.h"] = window.geom_heightmapper.h;
  json["window.show_graph_manager_widget"] = window.show_graph_manager_widget;
  json["window.show_texture_downloader_widget"] = window.show_texture_downloader_widget;
  json["window.show_heightmapper_widget"] = window.show_heightmapper_widget;

  return json;
}

} // namespace hesiod
