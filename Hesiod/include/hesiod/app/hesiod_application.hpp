/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include <QApplication>
#include <QCoreApplication>
#include <QProgressBar>
#include <QStandardPaths>

#include "nlohmann/json.hpp"

#include "hesiod/app/app_context.hpp"
#include "hesiod/bridges/blender/blender_streamer.hpp"
#include "hesiod/gui/widgets/app_settings_window.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/bake_config_dialog.hpp"
#include "hesiod/gui/widgets/main_window.hpp"

#define HSD_CTX                                                                          \
  static_cast<hesiod::HesiodApplication *>(QCoreApplication::instance())->get_context()

#define HSD_APP static_cast<hesiod::HesiodApplication *>(QCoreApplication::instance())

namespace hesiod
{

// forward
class ProjectUI;

// =====================================
// HesiodApplication
// =====================================
class HesiodApplication : public QApplication
{
  Q_OBJECT
public:
  HesiodApplication(int &argc, char **argv);
  ~HesiodApplication();

  bool is_headless() const;
  void load_project_model_and_ui(const std::string &fname = "", bool keep_name = true);
  void save_project_model_and_ui(const std::string &fname);
  void save_backup(const std::string &fname);
  void show();

  void notify(const std::string &msg = "", int timeout = 5000);

  // --- Context
  QApplication     &get_qapp();
  AppContext       &get_context();
  const AppContext &get_context() const;
  ProjectUI        *get_project_ui_ref();
  BlenderStreamer  &get_blender_streamer();

private slots:
  // --- User actions
  void on_application_settings_action();
  void on_export_batch();
  void on_load();
  void on_load_ready_made();
  void on_new();
  void on_online_help();
  void on_project_settings();
  void on_quit();
  void on_save();
  void on_save_as();
  void on_save_copy();
  void show_about();
  void show_quick_help();

  // --- Internal
  void on_project_name_changed();

private:
  void cleanup();
  void setup_menu_bar();

  // --- Members (respect order for deletion)
  AppContext                 context;
  MainWindow                *main_window = nullptr;  // null in headless CLI modes
  std::unique_ptr<ProjectUI> project_ui;          // because top-level UI
  AppSettingsWindow         *app_settings_window; // owned by MainWindow

  BlenderStreamer blender_streamer;

  bool headless = false;
};

} // namespace hesiod