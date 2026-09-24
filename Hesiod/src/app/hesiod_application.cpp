/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <omp.h>

#include "meta_qt/designs/stock/stock.hpp"

#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/openmp.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/gui/project_ui.hpp"
#include "hesiod/gui/widgets/about_dialog.hpp"
#include "hesiod/gui/widgets/batch_export_progress_dialog.hpp"
#include "hesiod/gui/widgets/documentation_popup.hpp"
#include "hesiod/gui/widgets/error_dialog.hpp"
#include "hesiod/gui/widgets/example_selector_dialog.hpp"
#include "hesiod/gui/widgets/fractional_repaint_filter.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/bake_config_dialog.hpp"
#include "hesiod/gui/widgets/graph_manager_widget.hpp"
#include "hesiod/gui/widgets/graph_tabs_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/project_settings_dialog.hpp"
#include "hesiod/gui/widgets/scrollable_dialog.hpp"
#include "hesiod/gui/widgets/splash_screen.hpp"
#include "hesiod/gui/widgets/tool_tip_blocker.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/constants/color_gradient.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/utils.hpp"

namespace fs = std::filesystem;

namespace hesiod
{

HesiodApplication::HesiodApplication(int &argc, char **argv, StartupMode mode)
    : QApplication(argc, argv)
{
  Logger::log()->trace("HesiodApplication::HesiodApplication");

  // --- Initialization

  // context
  this->context.initialize();

  if (mode == StartupMode::ContextOnly)
  {
    meta::qt::stock::register_design();
    this->headless = true;
    this->context.headless = true;
    return;
  }

  // force icons visibility in the menu bar
  this->setAttribute(Qt::AA_DontShowIconsInMenus, false);
  QStyle *style = QApplication::style();
  style->setProperty("iconInMenu", true);
  QApplication::setStyle(style);

  // start OpenCL
  hmap::gpu::init_opencl();

  // init OpenMP
  hmap::init_openmp(this->context.app_settings.global.omp_num_threads);

  // OpenCV info
  const std::string cv_log_fname = "opencv_build_information.log";
  Logger::log()->trace(
      "HesiodApplication::HesiodApplication: OpenCV build information dumped in: {}",
      cv_log_fname);
  string_to_file(hmap::get_opencv_build_information(), cv_log_fname);

  // for colormaps loading
  hesiod::ColorGradientManager::get_instance();

  // Blender streamer
  this->blender_streamer.start();

  // meta design registration
  meta::qt::stock::register_design();

  // --- Batch CLI mode if requested

  args::ArgumentParser parser("Hesiod.");
  std::string          startup_file;
  int                  ret = cli::parse_args(parser, argc, argv, startup_file);

  if (ret >= 0)
  {
    // stop here, skip the rest
    this->headless = true;
    this->headless_exit_code = ret;
    return;
  }

  // --- Continue with GUI

  this->installEventFilter(new FractionalRepaintFilter(this));

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  // launch splash
  SplashScreen *splash = new SplashScreen();

  // apply style
  this->setWindowIcon(QIcon(this->context.app_settings.global.icon_path.c_str()));
  apply_global_style(this->get_qapp());
  apply_animation_settings(this->context.app_settings.interface.enable_ui_animations);

  // main window
  this->main_window = new MainWindow();

  // crash-recovery snapshots (GUI mode only: headless runs never edit)
  this->autosave = std::make_unique<AutosaveManager>(
      AutosaveManager::default_directory());
  this->autosave->set_project_json_provider([this]()
                                            { return this->project_file_json(); });
  this->autosave->set_enabled(this->context.app_settings.global.enable_autosave);
  this->autosave->set_interval(
      std::chrono::seconds(this->context.app_settings.global.autosave_interval_s));

  // (after MainWindow creation)
  splash->show_message("Loading project...");

  // a pending recovery snapshot wins over the command-line file and the
  // example selector: the user has unsaved work to decide about first
  if (!this->offer_recovery())
  {
    std::string fname = "";
    bool        keep_name = false; // project name not kept (examples, default project)

    if (!startup_file.empty())
    {
      if (fs::exists(startup_file))
      {
        fname = fs::absolute(fs::path(startup_file)).lexically_normal().string();
        keep_name = true; // opened as a regular project, saving writes back to it
      }
      else
      {
        Logger::log()->warn("HesiodApplication::HesiodApplication: project file "
                            "requested on the command line does not exist, falling back "
                            "to normal startup: {}",
                            startup_file);
      }
    }

    if (fname.empty() &&
        this->context.app_settings.interface.enable_example_selector_at_startup)
    {
      std::string           path = this->context.app_settings.global.ready_made_path;
      ExampleSelectorDialog ex_dialog(QString::fromStdString(path));
      ex_dialog.exec();

      // Closing the window is a decision not to open anything, so the app stops
      // rather than dropping the user into a project they never asked for. New
      // Project falls through with an empty filename, which is what starts one.
      if (ex_dialog.outcome() == ExampleSelectorDialog::Outcome::Closed)
      {
        splash->close();
        delete splash;
        ::exit(0);
      }

      if (ex_dialog.outcome() == ExampleSelectorDialog::Outcome::OpenFile)
      {
        fname = ex_dialog.selected_file().toStdString();
        keep_name = ex_dialog.selected_is_project();
      }
    }

    this->load_project_model_and_ui(fname, keep_name);

    if (keep_name)
      this->add_recent_file(fname);
  }

  // others
  splash->show_message("Opening UI...");

  this->setup_menu_bar();
  this->installEventFilter(new ToolTipBlocker);

  this->notify("Ready");

  splash->close();
}

HesiodApplication::~HesiodApplication() = default;

void HesiodApplication::rebuild_recent_files_menu()
{
  Logger::log()->trace("HesiodApplication::rebuild_recent_files_menu");

  this->recent_files_menu->clear();

  const std::vector<std::string> recent_files = this->context.app_settings.global
                                                    .recent_files;

  if (recent_files.empty())
  {
    QAction *empty_action = this->recent_files_menu->addAction("(no recent files)");
    empty_action->setEnabled(false);
    return;
  }

  for (const std::string &fname : recent_files)
  {
    QAction *action = this->recent_files_menu->addAction(QString::fromStdString(fname));
    this->connect(action,
                  &QAction::triggered,
                  this,
                  [this, fname]() { this->on_open_recent(fname); });
  }

  this->recent_files_menu->addSeparator();

  QAction *clear_action = this->recent_files_menu->addAction("Clear Recent Files");
  this->connect(clear_action,
                &QAction::triggered,
                this,
                [this]() { this->context.app_settings.global.recent_files.clear(); });
}

void HesiodApplication::add_recent_file(const std::string &fname)
{
  Logger::log()->trace("HesiodApplication::add_recent_file: {}", fname);

  auto &global = this->context.app_settings.global;

  const std::string abs_path = fs::absolute(fs::path(fname)).lexically_normal().string();

  auto &list = global.recent_files;
  list.erase(std::remove(list.begin(), list.end(), abs_path), list.end());
  list.insert(list.begin(), abs_path);

  if (static_cast<int>(list.size()) > global.max_recent_files)
    list.resize(std::max(global.max_recent_files, 0));
}

void HesiodApplication::cleanup()
{
  Logger::log()->trace("HesiodApplication::cleanup");

  // the event loop is pumped below while the model and UI are inconsistent
  AutosaveSuspender suspend_autosave(this->autosave.get());

  // the project on its way out is either saved or explicitly discarded
  if (this->autosave)
    this->autosave->discard();

  if (this->project_ui)
    this->project_ui->cleanup();

  if (this->context.project_model)
    this->context.project_model->cleanup();
}

bool HesiodApplication::confirm_discard_unsaved_changes(const QString &action_title)
{
  if (!this->context.project_model || !this->context.project_model->get_is_dirty())
    return true;

  QMessageBox::StandardButton reply = QMessageBox::warning(
      this->main_window,
      action_title,
      "The project has unsaved changes.",
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);

  if (reply == QMessageBox::Save)
  {
    this->on_save();
    // still dirty means the user backed out of the save-as dialog
    return !this->context.project_model->get_is_dirty();
  }

  return reply == QMessageBox::Discard;
}

BlenderStreamer &HesiodApplication::get_blender_streamer()
{
  return this->blender_streamer;
}

AppContext &HesiodApplication::get_context() { return this->context; }

const AppContext &HesiodApplication::get_context() const { return this->context; }

ProjectUI *HesiodApplication::get_project_ui_ref() { return this->project_ui.get(); }

AutosaveManager *HesiodApplication::get_autosave_manager_ref()
{
  return this->autosave.get();
}

QApplication &HesiodApplication::get_qapp() { return *static_cast<QApplication *>(this); }

bool HesiodApplication::is_headless() const { return this->headless; }

int HesiodApplication::get_exit_code() const { return this->headless_exit_code; }

void HesiodApplication::load_project_model_and_ui(const std::string &fname,
                                                  bool               keep_name)
{
  Logger::log()->trace("HesiodApplication::load_project_model_and_ui: fname [{}]", fname);

  // the event loop is pumped below while the model and UI are inconsistent
  AutosaveSuspender suspend_autosave(this->autosave.get());

  this->notify(std::format("Loading project... {}", fname));

  const std::string actual_fname = fname.empty() ? this->context.app_settings.global
                                                       .default_startup_project_file
                                                 : fname;

  this->cleanup();

  // --- model

  // a missing file must fall back to the blank project too: load_project_model
  // would return without creating a model and the UI would dereference null
  const bool blank_startup = actual_fname.empty() || !fs::exists(actual_fname);

  if (blank_startup)
  {
    if (!actual_fname.empty())
      Logger::log()->warn("HesiodApplication::load_project_model_and_ui: project "
                          "file does not exist, starting with a blank project: {}",
                          actual_fname);

    // no startup file: blank project with a single empty graph, ready to use
    this->context.new_project();

    auto config = std::make_shared<GraphConfig>();
    auto graph = std::make_shared<GraphNode>("graph", config);
    this->context.project_model->get_graph_manager_ref()->add_graph_node(graph, "graph");
  }
  else
  {
    this->context.load_project_model(actual_fname);

    if (!this->headless && this->context.get_error_manager().has_errors())
    {
      const auto &errors = this->context.get_error_manager().get_errors();
      const QString
          message = QString("The project '%1' was loaded with %2 warning(s)/error(s). "
                            "Some nodes or links could not be restored:")
                        .arg(QString::fromStdString(actual_fname))
                        .arg(errors.size());

      ErrorDialog dialog("Project Loading Warnings",
                         message,
                         errors,
                         true,
                         this->main_window);

      if (dialog.exec() == QDialog::Rejected)
      {
        Logger::log()->info("User cancelled loading project after warnings: {}",
                            actual_fname);
        this->load_project_model_and_ui("", false);
        return;
      }
    }
  }

  // --- UI

  // remove first old central widget (if any) so it doesn't linger
  // (main_window is null in headless CLI modes: --snapshot / --inventory)
  if (this->main_window)
  {
    if (QWidget *old = this->main_window->takeCentralWidget())
    {
      old->setParent(nullptr);
      old->deleteLater();
    }
  }

  this->project_ui = std::make_unique<ProjectUI>();

  this->project_ui->initialize(this->context.project_model.get());

  if (!blank_startup)
    this->project_ui->load_ui_state(actual_fname);

  if (this->main_window)
    this->main_window->setCentralWidget(this->project_ui->get_widget());

  // reset other visibility state
  this->project_ui->get_graph_manager_widget_ref()->setVisible(
      this->context.app_settings.window.show_graph_manager_widget);

  if (this->context.app_settings.interface.enable_texture_downloader)
  {
    this->project_ui->get_texture_downloader_ref()->setVisible(
        this->context.app_settings.window.show_texture_downloader_widget);
  }

  if (this->context.app_settings.interface.enable_heightmapper_widget)
  {
    this->project_ui->get_heightmapper_widget_ref()->setVisible(
        this->context.app_settings.window.show_heightmapper_widget);
  }

  // --- connections

  // ProjectUI -> Project
  this->connect(this->project_ui->get_graph_manager_widget_ref(),
                &GraphManagerWidget::has_changed,
                [this]() { this->context.project_model->on_has_changed(); });

  this->connect(this->project_ui->get_graph_tabs_widget_ref(),
                &GraphTabsWidget::has_changed,
                [this]() { this->context.project_model->on_has_changed(); });

  this->connect(this->project_ui->get_graph_tabs_widget_ref(),
                &GraphTabsWidget::node_library_toggle_requested,
                this,
                &HesiodApplication::on_toggle_node_library_pan);

  // Project -> HesiodApplication
  this->context.project_model->project_name_changed = [this]()
  { this->on_project_name_changed(); };

  this->context.project_model->is_dirty_changed = [this]()
  { this->on_project_name_changed(); };

  this->context.project_model->has_changed = [this]()
  {
    if (this->autosave)
      this->autosave->mark_changed();
  };

  // re-key now: cleanup() reset the model path without firing
  // project_name_changed, so a blank or example project must not keep
  // writing under the previous project's key. The keep_name set_path()
  // below re-keys again to the named file.
  if (this->autosave)
    this->autosave->set_project_path(this->context.project_model->get_path());

  // Project model and UI -> MainWindow
  if (this->main_window)
    this->main_window->setup_connections_with_project();

  // rename whether fname is empty or not
  if (keep_name)
    this->context.project_model->set_path(fname);

  this->notify("Project loaded successfully.");
}

void HesiodApplication::notify(const std::string &msg, int timeout)
{
  Logger::log()->trace("HesiodApplication::notify: {}", msg);
  if (this->main_window)
    this->main_window->notify(msg, timeout);
}

void HesiodApplication::on_application_settings_action()
{
  Logger::log()->trace("HesiodApplication::on_application_settings_action");

  // The settings pane is long and gets longer with the interface scale, so it
  // goes in a scrolling dialog capped to the screen; a plain QDialog takes the
  // pane's full height and pushes OK off the bottom at 200%.
  AppSettingsWindow *settings_window = new AppSettingsWindow();

  ScrollableDialog dialog(settings_window,
                          "Application Settings",
                          QDialogButtonBox::Ok,
                          this->main_window);

  dialog.setModal(true);
  dialog.exec();
}

void HesiodApplication::on_export_batch()
{
  Logger::log()->trace("HesiodApplication::on_export_batch");

  // --- retrieve export parameters from user

  BakeConfig bake_settings = this->context.project_model->get_bake_config();

  BakeConfigDialog dialog(this->context.app_settings.node_editor.max_bake_resolution,
                          bake_settings);

  if (dialog.exec() != QDialog::Accepted)
    return;

  bake_settings = dialog.get_bake_settings();

  // the variant loop pumps the event loop while it touches the model
  AutosaveSuspender suspend_autosave(this->autosave.get());

  Logger::log()->trace("HesiodApplication::on_export_batch: size = {}, nvariants = {}",
                       bake_settings.resolution,
                       bake_settings.nvariants);

  // --- setup export repertory

  this->notify("Baking and exporting...");

  // show batch export progress dialog
  BatchExportProgressDialog progress(this->main_window);
  progress.show();
  QCoreApplication::processEvents();

  for (int k = 0; k < bake_settings.nvariants + 1; ++k)
  {
    const std::string msg = std::format("Baking variants {}/{}...",
                                        k,
                                        bake_settings.nvariants + 1);
    this->notify(msg);

    const std::string variant_name = (k == 0) ? "Base" : ("Variant " + std::to_string(k));
    progress.set_variant(k + 1, bake_settings.nvariants + 1, variant_name);
    QCoreApplication::processEvents(); // render progress dialog

    const fs::path project_path = this->context.project_model->get_path();

    // build export path based on project name, if available
    fs::path export_path = project_path.filename();
    if (export_path.empty())
      export_path = "export";
    else
      export_path += "_export";

    export_path = project_path.empty() ? export_path
                                       : project_path.parent_path() / export_path;

    if (k > 0)
      export_path /= "variants_" + std::to_string(k);

    Logger::log()->info("HesiodApplication::on_export_batch: export path: {}",
                        export_path.string());

    // create it
    if (!fs::exists(export_path))
    {
      Logger::log()->trace(
          "HesiodApplication::on_export_batch: creating export repertory {}",
          export_path.string());
      fs::create_directories(export_path);
    }

    // --- save an hesiod file and tweak it

    // save graph node to a temporary file
    fs::path fname = export_path / "hesiod_bake.hsd";
    this->save_project_model_and_ui(fname.string());

    // force auto_export for export nodes and overwrite export paths
    override_export_nodes_settings(fname.string(),
                                   export_path,
                                   static_cast<uint>(k),
                                   bake_settings);

    // --- run

    // retrieve config of the first graph
    auto graph_nodes = this->context.project_model->get_graph_manager_ref()
                           ->get_graph_nodes();
    auto         it = graph_nodes.begin();
    GraphConfig *p_config = it != graph_nodes.end() ? it->second->get_config_ref()
                                                    : nullptr;

    if (p_config)
    {
      GraphConfig bake_config = *p_config;

      if (bake_settings.min_memory)
      {
        bake_config.storage_mode = hmap::StorageMode::VA_DISK_LRU_MIN;
        bake_config.cm_cpu.mode = hmap::ForEachMode::VA_SEQUENTIAL;
        bake_config.cm_gpu.mode = hmap::ForEachMode::VA_SEQUENTIAL;
      }
      else if (bake_settings.force_distributed)
      {
        bake_config.cm_cpu.mode = hmap::ForEachMode::VA_DISTRIBUTED;
        bake_config.cm_gpu.mode = hmap::ForEachMode::VA_DISTRIBUTED;
      }

      // define bake shape, keep the aspect ratio
      float      scale = static_cast<float>(bake_settings.resolution) / p_config->shape.x;
      glm::ivec2 bake_shape = {static_cast<int>(scale * p_config->shape.x),
                               static_cast<int>(scale * p_config->shape.y)};

      // compute tiling based on max tile shape
      glm::ivec2 bake_tiling = p_config->tiling;
      if (bake_settings.max_tile_resolution > 0)
      {
        bake_tiling = {std::max(1,
                                (bake_shape.x + bake_settings.max_tile_resolution - 1) /
                                    bake_settings.max_tile_resolution),
                       std::max(1,
                                (bake_shape.y + bake_settings.max_tile_resolution - 1) /
                                    bake_settings.max_tile_resolution)};
      }

      bake_config.set_shape(bake_shape);
      bake_config.set_tiling(bake_tiling);

      Logger::log()->trace("HesiodApplication::on_export_batch: bake_shape: ({}, {}), "
                           "bake_tiling: ({}, {})",
                           bake_shape.x,
                           bake_shape.y,
                           bake_tiling.x,
                           bake_tiling.y);

      // populate the scheduled list in topological update order for progress display
      std::vector<NodeExportStatus> scheduled_nodes;
      auto graph_manager_ref = this->context.project_model->get_graph_manager_ref();
      if (graph_manager_ref)
      {
        for (const auto &graph_id : graph_manager_ref->get_graph_order())
        {
          GraphNode *p_graph = graph_manager_ref->get_graph_ref_by_id(graph_id);
          if (!p_graph)
            continue;

          std::vector<std::string> dirty_ids;
          for (const auto &[nid, p_node] : p_graph->get_nodes())
            dirty_ids.push_back(nid);

          std::vector<std::string> sorted_ids = p_graph->topological_sort(dirty_ids);
          for (const auto &nid : sorted_ids)
          {
            BaseNode        *p_base = p_graph->get_node_ref_by_id<BaseNode>(nid);
            NodeExportStatus st;
            st.node_id = nid;
            st.node_label = p_base ? p_base->get_label() : nid;
            st.node_type = p_base ? p_base->get_node_type() : "";
            st.state = NodeComputeState::Pending;
            scheduled_nodes.push_back(st);
          }
        }
      }
      progress.set_node_list(scheduled_nodes);

      // launch worker process for isolated bake execution
      QProcess    worker_process;
      QStringList worker_args;
      worker_args << "-b" << QString::fromStdString(fname.string());
      worker_args << QString::fromStdString(
          std::format("--shape={},{}", bake_shape.x, bake_shape.y));
      worker_args << QString::fromStdString(
          std::format("--tiling={},{}", bake_tiling.x, bake_tiling.y));
      worker_args << QString::fromStdString(
          std::format("--overlap={}", bake_config.overlap));

      if (bake_settings.min_memory)
        worker_args << "--min-memory";
      else if (bake_settings.force_distributed)
        worker_args << "--force-distributed";

      worker_args << "--ipc";

      std::string current_node_computing;
      QByteArray  stdout_buffer;

      auto parse_ipc_line = [&progress, &current_node_computing](const QString &line)
      {
        if (line.startsWith("HSD_IPC:NODE_STARTED:"))
        {
          std::string node_id = line.mid(21).trimmed().toStdString();
          current_node_computing = node_id;
          progress.on_node_started(node_id);
        }
        else if (line.startsWith("HSD_IPC:NODE_FINISHED:"))
        {
          QString     rest = line.mid(22).trimmed();
          int         sep = rest.lastIndexOf(':');
          std::string node_id;
          bool        ok = true;
          if (sep != -1)
          {
            node_id = rest.left(sep).toStdString();
            ok = (rest.mid(sep + 1) != "0");
          }
          else
          {
            node_id = rest.toStdString();
          }
          if (current_node_computing == node_id)
            current_node_computing.clear();
          progress.on_node_finished(node_id, ok);
        }
      };

      QObject::connect(&worker_process,
                       &QProcess::readyReadStandardOutput,
                       [&]()
                       {
                         stdout_buffer.append(worker_process.readAllStandardOutput());
                         int newline_pos = -1;
                         while ((newline_pos = stdout_buffer.indexOf('\n')) != -1)
                         {
                           QByteArray line_bytes = stdout_buffer.left(newline_pos);
                           stdout_buffer.remove(0, newline_pos + 1);
                           QString line = QString::fromUtf8(line_bytes).trimmed();
                           if (!line.isEmpty())
                             parse_ipc_line(line);
                         }
                       });

      QObject::connect(&worker_process,
                       &QProcess::readyReadStandardError,
                       [&]()
                       {
                         QByteArray err_bytes = worker_process.readAllStandardError();
                         Logger::log()->warn("Hesiod worker stderr: {}",
                                             err_bytes.toStdString());
                       });

      auto cancel_connection = QObject::connect(
          &progress,
          &BatchExportProgressDialog::request_cancel,
          [&]()
          {
            if (worker_process.state() != QProcess::NotRunning)
            {
              Logger::log()->info("Killing bake worker process on "
                                  "user request");
              worker_process.kill();
            }
          });

      worker_process.start(QCoreApplication::applicationFilePath(), worker_args);

      while (worker_process.state() != QProcess::NotRunning)
      {
        worker_process.waitForFinished(100);
        QCoreApplication::processEvents();
      }

      QObject::disconnect(cancel_connection);

      // flush any remaining stdout buffer
      if (!stdout_buffer.isEmpty())
      {
        QString line = QString::fromUtf8(stdout_buffer).trimmed();
        if (!line.isEmpty())
          parse_ipc_line(line);
      }

      // check if user canceled
      if (progress.is_canceled())
      {
        Logger::log()->info("Bake process canceled by user.");
        if (!current_node_computing.empty())
          progress.on_node_finished(current_node_computing, false);

        progress.on_export_canceled();
        progress.exec();
        this->notify("Baking canceled.");
        return;
      }

      // check exit status
      if (worker_process.exitStatus() == QProcess::CrashExit ||
          worker_process.exitCode() != 0)
      {
        Logger::log()->error("Worker process crashed or exited with error code {}.",
                             worker_process.exitCode());
        if (!current_node_computing.empty())
          progress.on_node_finished(current_node_computing, false);

        progress.on_export_failed("Worker process failed during bake execution.");
        progress.exec();
        return;
      }
    }
  }

  // save config
  this->context.project_model->set_bake_config(bake_settings);

  progress.set_overall_progress(bake_settings.nvariants + 1, bake_settings.nvariants + 1);
  progress.on_export_finished();
  progress.exec();

  this->notify("Baking and exporting terminated.");
}

void HesiodApplication::on_load()
{
  Logger::log()->trace("HesiodApplication::on_load");

  if (!this->confirm_discard_unsaved_changes("Load"))
    return;

  fs::path path = this->context.project_model->get_path();

  QString load_fname = QFileDialog::getOpenFileName(this->main_window,
                                                    "Load...",
                                                    path.string().c_str(),
                                                    "Hesiod files (*.hsd)");

  if (!load_fname.isNull() && !load_fname.isEmpty())
  {
    this->load_project_model_and_ui(load_fname.toStdString());
    this->add_recent_file(load_fname.toStdString());
  }
}

void HesiodApplication::on_open_recent(const std::string &fname)
{
  Logger::log()->trace("HesiodApplication::on_open_recent: {}", fname);

  if (!fs::exists(fname))
  {
    QMessageBox::warning(
        this->main_window,
        "Open Recent",
        QString("This file no longer exists and has been removed from the list:\n%1")
            .arg(fname.c_str()));

    auto &list = this->context.app_settings.global.recent_files;
    list.erase(std::remove(list.begin(), list.end(), fname), list.end());
    return;
  }

  if (!this->confirm_discard_unsaved_changes("Open Recent"))
    return;

  this->load_project_model_and_ui(fname);
  this->add_recent_file(fname);
}

bool HesiodApplication::offer_recovery()
{
  Logger::log()->trace("HesiodApplication::offer_recovery");

  if (!this->autosave)
    return false;

  for (const AutosaveManager::Entry &entry : this->autosave->scan())
  {
    const QString snapshot = QString::fromStdString(entry.snapshot.string());

    if (!entry.readable)
    {
      QMessageBox box(this->main_window);
      box.setIcon(QMessageBox::Warning);
      box.setWindowTitle("Recover unsaved work");
      box.setText("A recovery file could not be read.");
      box.setInformativeText(snapshot);
      QPushButton *delete_button = box.addButton("Delete", QMessageBox::DestructiveRole);
      QPushButton *keep_button = box.addButton("Keep", QMessageBox::RejectRole);
      box.setDefaultButton(keep_button);
      box.setEscapeButton(keep_button);
      box.exec();

      if (box.clickedButton() == delete_button)
      {
        std::error_code ec;
        fs::remove(entry.snapshot, ec);

        if (ec)
          Logger::log()->warn("HesiodApplication::offer_recovery: could not remove {}: "
                              "{}",
                              entry.snapshot.string(),
                              ec.message());

        // a stray temp file from an interrupted write goes with it
        fs::remove(fs::path(entry.snapshot.string() + ".tmp"), ec);
      }
      continue;
    }

    const std::string name = entry.project_path.empty()
                                 ? "Untitled project"
                                 : entry.project_path.stem().string();
    const std::string file = entry.project_path.empty() ? "(never saved)"
                                                        : entry.project_path.string();

    QMessageBox box(this->main_window);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle("Recover unsaved work");
    box.setText("Unsaved work from a previous session was found.");
    box.setInformativeText(QString::fromStdString(
        std::format("Project: {}\nFile: {}\nSnapshot taken: {}\n\nRestore it "
                    "now?\n\nLater keeps the snapshot for the next launch.",
                    name,
                    file,
                    entry.saved_at)));
    QPushButton *restore_button = box.addButton("Restore", QMessageBox::AcceptRole);
    QPushButton *discard_button = box.addButton("Discard", QMessageBox::DestructiveRole);
    QPushButton *later_button = box.addButton("Later", QMessageBox::RejectRole);
    box.setDefaultButton(restore_button);
    box.setEscapeButton(later_button);
    box.exec();

    if (box.clickedButton() == discard_button)
    {
      std::error_code ec;
      fs::remove(entry.snapshot, ec);

      if (ec)
        Logger::log()->warn("HesiodApplication::offer_recovery: could not remove {}: {}",
                            entry.snapshot.string(),
                            ec.message());
      else
        Logger::log()->info("HesiodApplication::offer_recovery: discarded {}",
                            entry.snapshot.string());

      // a stray temp file from an interrupted write goes with it
      fs::remove(fs::path(entry.snapshot.string() + ".tmp"), ec);

      continue;
    }

    if (box.clickedButton() != restore_button)
    {
      // Later: park the snapshot under a deferred name, out of reach of this
      // session's live snapshot, which would otherwise overwrite it on the next
      // tick or delete it on the next save. scan() still lists it and adopt()
      // re-keys it if it is restored later.
      const fs::path parked = AutosaveManager::deferred_path(entry.snapshot);

      if (parked != entry.snapshot)
      {
        std::error_code ec;
        fs::rename(entry.snapshot, parked, ec);

        if (ec)
          Logger::log()->warn("HesiodApplication::offer_recovery: could not park {} as "
                              "{}: {}",
                              entry.snapshot.string(),
                              parked.string(),
                              ec.message());
      }

      continue;
    }

    if (this->restore_snapshot(entry))
      return true;
  }

  return false;
}

bool HesiodApplication::restore_snapshot(const AutosaveManager::Entry &entry)
{
  Logger::log()->info("HesiodApplication::restore_snapshot: {}", entry.snapshot.string());

  try
  {
    this->load_project_model_and_ui(entry.snapshot.string(), /* keep_name */ false);
  }
  catch (const std::exception &e)
  {
    Logger::log()->error("HesiodApplication::restore_snapshot: could not load {}: {}",
                         entry.snapshot.string(),
                         e.what());
    QMessageBox::warning(
        this->main_window,
        "Recover unsaved work",
        QString("The recovery file could not be loaded and has been kept:\n%1\n\n%2")
            .arg(QString::fromStdString(entry.snapshot.string()), e.what()));
    this->load_project_model_and_ui("", false);
    return false;
  }

  if (!entry.project_path.empty())
  {
    this->context.project_model->set_path(entry.project_path);
    this->add_recent_file(entry.project_path.string());
  }

  // recovered work is unsaved by definition; the snapshot becomes this
  // instance's live one and is refreshed on the next tick
  this->context.project_model->set_is_dirty(true);
  this->autosave->adopt(entry.snapshot);
  this->autosave->mark_changed();

  this->notify(std::format("Recovered unsaved work from {}", entry.snapshot.string()));
  return true;
}

void HesiodApplication::on_load_ready_made()
{
  Logger::log()->trace("HesiodApplication::on_load_ready_made");

  if (!this->confirm_discard_unsaved_changes("Open Ready-made Example"))
    return;

  std::string           path = this->context.app_settings.global.ready_made_path;
  ExampleSelectorDialog ex_dialog(QString::fromStdString(path));
  ex_dialog.exec();

  switch (ex_dialog.outcome())
  {
  case ExampleSelectorDialog::Outcome::OpenFile:
  {
    const std::string fname = ex_dialog.selected_file().toStdString();
    const bool        keep_name = ex_dialog.selected_is_project();
    this->load_project_model_and_ui(fname, keep_name);
    if (keep_name)
      this->add_recent_file(fname);
    break;
  }

  case ExampleSelectorDialog::Outcome::NewProject:
    // Reached from the menu bar with a project already open, where this used
    // to close the window and leave that project untouched. An empty filename
    // is what load_project_model_and_ui() treats as a fresh start.
    this->load_project_model_and_ui("", false);
    break;

  case ExampleSelectorDialog::Outcome::Closed:
    // Dismissed from the menu bar, so keep whatever is already open. Only the
    // startup path treats closing as a reason to quit.
    break;
  }
}

void HesiodApplication::on_new()
{
  Logger::log()->trace("HesiodApplication::on_new");

  if (!this->confirm_discard_unsaved_changes("New"))
    return;

  this->cleanup();
  this->load_project_model_and_ui();
}

void HesiodApplication::on_online_help()
{
  QDesktopServices::openUrl(
      QUrl(this->context.app_settings.global.online_help_url.c_str()));
}

void HesiodApplication::on_project_name_changed()
{
  if (this->autosave)
    this->autosave->set_project_path(this->context.project_model->get_path());

  std::string title = this->context.project_model->get_name() + " [" +
                      this->context.project_model->get_path().string() + "]";

  if (this->context.project_model->get_is_dirty())
    title += "*";

  if (this->main_window)
    this->main_window->setWindowTitle(title.c_str());
}

void HesiodApplication::on_project_settings()
{
  auto *dialog = new ProjectSettingsDialog(this->context.project_model.get());
  dialog->exec();
}

void HesiodApplication::on_quit()
{
  Logger::log()->trace("HesiodApplication::on_quit");

  // QApplication::quit() first closes every top-level window (Qt >= 6.1), so
  // the unsaved-changes prompt and the geometry/settings save both run once in
  // MainWindow::closeEvent, which aborts the quit if the user declines.
  QApplication::quit();
}

void HesiodApplication::on_save()
{
  Logger::log()->trace("HesiodApplication::on_save");

  fs::path path = this->context.project_model->get_path();

  if (path.empty())
    this->on_save_as();
  else
  {
    if (this->save_project_model_and_ui(path.string()))
    {
      this->context.project_model->set_path(path);
      this->add_recent_file(path.string());
    }
  }
}

void HesiodApplication::on_save_as()
{
  Logger::log()->trace("HesiodApplication::on_save_as");

  fs::path path = this->context.project_model->get_path();

  QString new_fname = QFileDialog::getSaveFileName(this->main_window,
                                                   "Save as...",
                                                   path.string().c_str(),
                                                   "Hesiod files (*.hsd)");

  if (!new_fname.isNull() && !new_fname.isEmpty())
  {
    fs::path clean_path = fs::path(new_fname.toStdString());
    clean_path = ensure_extension(clean_path, ".hsd");

    Logger::log()->trace("HesiodApplication::on_save_as: clean_path: {}",
                         clean_path.string());

    if (this->save_project_model_and_ui(clean_path.string()))
    {
      this->context.project_model->set_path(clean_path.string());
      this->add_recent_file(clean_path.string());
    }
  }
}

void HesiodApplication::on_save_copy()
{
  Logger::log()->trace("HesiodApplication::on_save_copy");

  fs::path path = this->context.project_model->get_path();

  Logger::log()->trace("{}", path.string());

  if (path.empty())
    this->on_save_as();
  else
  {
    fs::path fname = insert_before_extension(path, "_" + timestamp());
    this->save_project_model_and_ui(fname.string());
  }
}

void HesiodApplication::on_toggle_node_library_pan()
{
  bool new_state = !this->context.app_settings.node_editor.show_node_library_pan;
  this->context.app_settings.node_editor.show_node_library_pan = new_state;

  if (this->show_node_library_pan_action)
    this->show_node_library_pan_action->setChecked(new_state);

  if (this->project_ui)
    this->project_ui->get_graph_tabs_widget_ref()->set_show_node_library_pan(new_state);
}

nlohmann::json HesiodApplication::project_file_json() const
{
  nlohmann::json json = this->context.project_model->json_to();

  // UI state travels with the project; there is none in headless modes
  if (this->project_ui)
    json.update(this->project_ui->ui_state_json_to());

  json["Hesiod version"] = "v" + std::to_string(HESIOD_VERSION_MAJOR) + "." +
                           std::to_string(HESIOD_VERSION_MINOR) + "." +
                           std::to_string(HESIOD_VERSION_PATCH);
  json["saved_at"] = timestamp();
  return json;
}

void HesiodApplication::save_backup(const std::string &fname)
{
  Logger::log()->trace("HesiodApplication::save_backup: {}", fname);

  // if the file exists, create a backup
  const std::string fname_ext = ensure_extension(fname, ".hsd").string();

  if (fs::exists(fname_ext))
  {
    fs::path original_path(fname_ext);
    fs::path backup_path = insert_before_extension(original_path, ".bak");

    try
    {
      fs::copy_file(original_path, backup_path, fs::copy_options::overwrite_existing);
      Logger::log()->trace("HesiodApplication::save_backup: backup created: {}",
                           backup_path.string());
    }
    catch (const std::exception &e)
    {
      Logger::log()->error(
          "HesiodApplication::save_backup: failed to create backup for {}: {}",
          fname_ext,
          e.what());
    }
  }
}

bool HesiodApplication::save_project_model_and_ui(const std::string &fname)
{
  Logger::log()->trace("HesiodApplication::save_project_model_and_ui: {}", fname);

  this->notify(std::format("Saving changes..."));

  if (this->context.app_settings.global.save_backup_file)
    this->save_backup(fname);

  // create a copy before saving
  fs::path file_path(fname);

  if (fs::exists(file_path))
  {
    try
    {
      // create a temp folder if it doesn't exist
      fs::path temp_dir = fs::temp_directory_path() / "hesiod_temp";
      fs::create_directories(temp_dir);

      // copy file to temp folder with same filename
      fs::path backup_path = temp_dir / file_path.filename();
      fs::copy_file(file_path, backup_path, fs::copy_options::overwrite_existing);

      Logger::log()->trace("HesiodApplication::save_project_model_and_ui: backup of "
                           "existing file saved to temporary folder: {}",
                           backup_path.string());

      // now delete the existing file
      fs::remove(file_path);
    }
    catch (const std::exception &e)
    {
      Logger::log()->error("HesiodApplication::save_project_model_and_ui: Error handling "
                           "existing file '{}': {}",
                           fname,
                           e.what());
    }
  }

  // proceed with saving
  if (!json_to_file(this->project_file_json(),
                    fname,
                    /* merge_with_existing_content */ true))
  {
    Logger::log()->error("HesiodApplication::save_project_model_and_ui: could not write "
                         "{}",
                         fname);

    // the project stays dirty and keeps its recovery snapshot
    if (this->main_window)
      QMessageBox::warning(this->main_window,
                           "Save",
                           QString("Could not write the project file:\n%1\n\nThe "
                                   "project is still unsaved.")
                               .arg(QString::fromStdString(fname)));

    return false;
  }

  this->context.project_model->set_is_dirty(false);

  // the saved file now holds everything the recovery snapshot did
  if (this->autosave)
    this->autosave->discard();

  this->notify(std::format("Project saved successfully, {}.", fname));
  return true;
}

void HesiodApplication::setup_menu_bar()
{
  Logger::log()->trace("HesiodApplication::setup_menu_bar");

  // --- leftmost Help

  QMenu *help = this->main_window->menuBar()->addMenu("");
  help->setIcon(QIcon("data/hesiod_icon.png"));

  auto *quick_help = new QAction("Quick Help", this);
  help->addAction(quick_help);

  auto *online_help = new QAction("Online Help", this);
  online_help->setIcon(HSD_ICON("link"));
  help->addAction(online_help);

  help->addSeparator();

  auto *about = new QAction("&About", this);
  help->addAction(about);

  // --- file

  QMenu *file_menu = this->main_window->menuBar()->addMenu("&File");

  auto *new_action = new QAction("New", this);
  new_action->setShortcut(tr("Ctrl+N"));
  new_action->setIcon(HSD_ICON("add"));
  file_menu->addAction(new_action);

  file_menu->addSeparator();

  auto *load_action = new QAction("Open", this);
  load_action->setShortcut(tr("Ctrl+O"));
  file_menu->addAction(load_action);

  this->recent_files_menu = file_menu->addMenu("Open Recent");

  auto *rmade_action = new QAction("Open Ready-made Example", this);
  rmade_action->setIcon(HSD_ICON("landscape"));
  file_menu->addAction(rmade_action);

  auto *save = new QAction("Save", this);
  save->setShortcut(tr("Ctrl+S"));
  save->setIcon(HSD_ICON("save"));
  file_menu->addAction(save);

  auto *save_as = new QAction("Save As", this);
  save_as->setShortcut(tr("Ctrl+Shift+S"));
  save_as->setIcon(HSD_ICON("save_as"));
  file_menu->addAction(save_as);

  auto *save_copy = new QAction("Save a copy", this);
  save_copy->setShortcut(tr("Ctrl+Alt+S"));
  file_menu->addAction(save_copy);

  file_menu->addSeparator();

  auto *export_batch = new QAction("Bake and Export (High Resolution)", this);
  export_batch->setShortcut(tr("Alt+E"));
  export_batch->setIcon(HSD_ICON("bakery_dining"));
  file_menu->addAction(export_batch);

  file_menu->addSeparator();

  auto *settings_action = new QAction("Application Settings", this);
  settings_action->setIcon(HSD_ICON("settings"));
  file_menu->addAction(settings_action);

  file_menu->addSeparator();

  auto *quit = new QAction("Quit", this);
  quit->setShortcut(tr("Ctrl+Q"));
  quit->setIcon(HSD_ICON("exit_to_app"));
  file_menu->addAction(quit);

  // --- project

  QMenu *project_menu = this->main_window->menuBar()->addMenu("&Project");

  auto *project_settings_action = new QAction("Project Settings", this);
  project_settings_action->setIcon(HSD_ICON("tune"));
  project_menu->addAction(project_settings_action);

  QMenu *graph_menu = this->main_window->menuBar()->addMenu("&Graph");

  auto *new_graph = new QAction("New graph", this);
  new_graph->setIcon(HSD_ICON("account_tree"));
  graph_menu->addAction(new_graph);

  graph_menu->addSeparator();

  auto *reseed = new QAction("Advance Random Seeds", this);
  reseed->setShortcut(tr("Alt+R"));
  graph_menu->addAction(reseed);

  auto *reseed_back = new QAction("Reverse Random Seeds", this);
  reseed_back->setShortcut(tr("Alt+Shift+R"));
  graph_menu->addAction(reseed_back);

  // --- view

  QMenu *view_menu = this->main_window->menuBar()->addMenu("&View");

  auto *show_layout_manager = new QAction("Graph Layout Manager", this);
  show_layout_manager->setCheckable(true);
  show_layout_manager->setChecked(
      this->context.app_settings.window.show_graph_manager_widget);
  view_menu->addAction(show_layout_manager);

  // texture dld
  auto *show_texture_downloader = new QAction("Texture Downloader", this);
  show_texture_downloader->setIcon(HSD_ICON("cloud_download"));
  if (this->context.app_settings.interface.enable_texture_downloader)
  {
    view_menu->addSeparator();
    view_menu->addAction(show_texture_downloader);
  }

  auto *show_heightmapper_widget = new QAction("Tangram Heightmapper", this);
  show_heightmapper_widget->setIcon(HSD_ICON("public"));
  if (this->context.app_settings.interface.enable_heightmapper_widget)
  {
    view_menu->addAction(show_heightmapper_widget);
  }

  view_menu->addSeparator();

  auto *show_viewer_action = new QAction("Show Viewer in Main Window", this);
  show_viewer_action->setCheckable(true);
  {
    bool state = this->context.app_settings.node_editor.show_viewer;
    show_viewer_action->setChecked(state);
    view_menu->addAction(show_viewer_action);
  }

  auto *show_node_settings_pan_action = new QAction("Show Node Settings Panel", this);
  show_node_settings_pan_action->setCheckable(true);
  {
    bool state = this->context.app_settings.node_editor.show_node_settings_pan;
    show_node_settings_pan_action->setChecked(state);
    view_menu->addAction(show_node_settings_pan_action);
  }

  this->show_node_library_pan_action = new QAction("Show Node Library Panel", this);
  this->show_node_library_pan_action->setCheckable(true);
  {
    bool state = this->context.app_settings.node_editor.show_node_library_pan;
    this->show_node_library_pan_action->setChecked(state);
    view_menu->addAction(this->show_node_library_pan_action);
  }

  // --- connections

  this->connect(new_action, &QAction::triggered, this, &HesiodApplication::on_new);
  this->connect(load_action, &QAction::triggered, this, &HesiodApplication::on_load);
  this->connect(this->recent_files_menu,
                &QMenu::aboutToShow,
                this,
                &HesiodApplication::rebuild_recent_files_menu);
  this->connect(rmade_action,
                &QAction::triggered,
                this,
                &HesiodApplication::on_load_ready_made);

  this->connect(save, &QAction::triggered, this, &HesiodApplication::on_save);
  this->connect(save_as, &QAction::triggered, this, &HesiodApplication::on_save_as);
  this->connect(save_copy, &QAction::triggered, this, &HesiodApplication::on_save_copy);
  this->connect(settings_action,
                &QAction::triggered,
                this,
                &HesiodApplication::on_application_settings_action);
  this->connect(export_batch,
                &QAction::triggered,
                this,
                &HesiodApplication::on_export_batch);

  this->connect(project_settings_action,
                &QAction::triggered,
                this,
                &HesiodApplication::on_project_settings);

  this->connect(quick_help,
                &QAction::triggered,
                this,
                &HesiodApplication::show_quick_help);
  this->connect(online_help,
                &QAction::triggered,
                this,
                &HesiodApplication::on_online_help);
  this->connect(about, &QAction::triggered, this, &HesiodApplication::show_about);

  // quit
  this->connect(quit, &QAction::triggered, this, &HesiodApplication::on_quit);

  // satellite widgets

  this->connect(show_viewer_action,
                &QAction::triggered,
                this,
                [this, show_viewer_action]()
                {
                  bool new_state = !this->context.app_settings.node_editor.show_viewer;
                  this->context.app_settings.node_editor.show_viewer = new_state;
                  show_viewer_action->setChecked(new_state);
                  this->project_ui->get_graph_tabs_widget_ref()->set_show_viewer(
                      new_state);
                });

  this->connect(
      show_node_settings_pan_action,
      &QAction::triggered,
      this,
      [this, show_node_settings_pan_action]()
      {
        bool new_state = !this->context.app_settings.node_editor.show_node_settings_pan;
        this->context.app_settings.node_editor.show_node_settings_pan = new_state;
        show_node_settings_pan_action->setChecked(new_state);
        this->project_ui->get_graph_tabs_widget_ref()->set_show_node_settings_widget(
            new_state);
      });

  this->connect(this->show_node_library_pan_action,
                &QAction::triggered,
                this,
                &HesiodApplication::on_toggle_node_library_pan);

  this->connect(
      show_layout_manager,
      &QAction::triggered,
      this,
      [this, show_layout_manager]()
      {
        bool state = this->project_ui->get_graph_manager_widget_ref()->isVisible();
        this->context.app_settings.window.show_graph_manager_widget = !state;
        this->project_ui->get_graph_manager_widget_ref()->setVisible(!state);
        show_layout_manager->setChecked(!state);
      });

  if (this->context.app_settings.interface.enable_texture_downloader)
  {
    this->connect(
        show_texture_downloader,
        &QAction::triggered,
        this,
        [this, show_texture_downloader]()
        {
          bool state = this->project_ui->get_texture_downloader_ref()->isVisible();
          this->context.app_settings.window.show_texture_downloader_widget = !state;
          this->project_ui->get_texture_downloader_ref()->setVisible(!state);
          show_texture_downloader->setChecked(!state);
        });
  }

  if (this->context.app_settings.interface.enable_heightmapper_widget)
  {
    this->connect(
        show_heightmapper_widget,
        &QAction::triggered,
        this,
        [this, show_heightmapper_widget]()
        {
          bool state = this->project_ui->get_heightmapper_widget_ref()->isVisible();
          this->context.app_settings.window.show_heightmapper_widget = !state;
          this->project_ui->get_heightmapper_widget_ref()->setVisible(!state);
          this->project_ui->get_heightmapper_widget_ref()->reload();
          show_heightmapper_widget->setChecked(!state);
        });
  }

  // graphs
  this->connect(
      new_graph,
      &QAction::triggered,
      this,
      [this]()
      { this->project_ui->get_graph_manager_widget_ref()->on_new_graph_request(); });

  this->connect(reseed,
                &QAction::triggered,
                this,
                [this]()
                { this->project_ui->get_graph_manager_widget_ref()->on_reseed(); });

  this->connect(reseed_back,
                &QAction::triggered,
                this,
                [this]()
                { this->project_ui->get_graph_manager_widget_ref()->on_reseed(true); });
}

void HesiodApplication::show()
{
  Logger::log()->trace("HesiodApplication::show");
  this->main_window->show();

  // the zoom-to-content deferred while deserializing the project fires before
  // the main window is shown, so it fits against a not-yet-laid-out viewport
  // and the graph can end up off-screen; refit once geometries are final
  QTimer::singleShot(0,
                     this,
                     [this]()
                     {
                       if (this->project_ui &&
                           this->project_ui->get_graph_tabs_widget_ref())
                         this->project_ui->get_graph_tabs_widget_ref()->zoom_to_content();
                     });
}

void HesiodApplication::show_about()
{
  auto *dlg = new AboutDialog();
  dlg->exec();
}

void HesiodApplication::show_quick_help()
{
  Logger::log()->trace("HesiodApplication::show_quick_help");

  std::string html_source = "";

  std::ifstream file(this->context.app_settings.global.quick_start_html_file);
  if (file.is_open())
  {
    std::ostringstream buffer;
    buffer << file.rdbuf(); // read the entire file into the buffer
    html_source = buffer.str();
    file.close();
  }

  DocumentationPopup *popup = new DocumentationPopup("Hesiod Quick Help", html_source);

  popup->setAttribute(Qt::WA_DeleteOnClose);
  popup->show();
}

} // namespace hesiod
