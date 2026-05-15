/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStatusBar>
#include <QUrl>

#include <omp.h>

#include "highmap/opencl/gpu_opencl.hpp"
#include "highmap/openmp.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/cli/batch_mode.hpp"
#include "hesiod/gui/project_ui.hpp"
#include "hesiod/gui/widgets/about_dialog.hpp"
#include "hesiod/gui/widgets/bake_config_dialog.hpp"
#include "hesiod/gui/widgets/documentation_popup.hpp"
#include "hesiod/gui/widgets/example_selector_dialog.hpp"
#include "hesiod/gui/widgets/graph_manager_widget.hpp"
#include "hesiod/gui/widgets/graph_tabs_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/project_settings_dialog.hpp"
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

HesiodApplication::HesiodApplication(int &argc, char **argv) : QApplication(argc, argv)
{
  Logger::log()->trace("HesiodApplication::HesiodApplication");

  // --- Initialization

  // context
  this->context.initialize();

  // force icons visibility in the menu bar
  this->setAttribute(Qt::AA_DontShowIconsInMenus, false);
  QStyle *style = QApplication::style();
  style->setProperty("iconInMenu", true);
  QApplication::setStyle(style);

  // start OpenCL
  hmap::gpu::init_opencl();

  // init OpenMP
  hmap::init_openmp(this->context.app_settings.global.omp_num_threads);

  // for colormaps loading
  hesiod::ColorGradientManager::get_instance();

  // --- Batch CLI mode if requested

  args::ArgumentParser parser("Hesiod.");
  int                  ret = cli::parse_args(parser, argc, argv);

  if (ret >= 0)
  {
    // stop here, skip the rest
    this->headless = true;
    return;
  }

  // --- Continue with GUI

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  // launch splash
  SplashScreen *splash = new SplashScreen();

  // apply style
  this->setWindowIcon(QIcon(this->context.app_settings.global.icon_path.c_str()));
  apply_global_style(this->get_qapp());

  // main window
  this->main_window = new MainWindow();

  // (after MainWindow creation)
  splash->show_message("Loading project...");

  std::string fname = "";

  if (this->context.app_settings.interface.enable_example_selector_at_startup)
  {
    std::string path = this->context.app_settings.global.ready_made_path;
    auto       *ex_dialog = new ExampleSelectorDialog(QString::fromStdString(path));
    bool        ret = ex_dialog->exec();

    if (ret)
      fname = ex_dialog->selected_file().toStdString();
  }

  bool keep_name = false; // project name not kept
  this->load_project_model_and_ui(fname, keep_name);

  // others
  splash->show_message("Opening UI...");

  this->setup_menu_bar();
  this->installEventFilter(new ToolTipBlocker);

  this->notify("Ready");

  splash->close();
}

HesiodApplication::~HesiodApplication() = default;

void HesiodApplication::cleanup()
{
  Logger::log()->trace("HesiodApplication::cleanup");

  if (this->project_ui)
    this->project_ui->cleanup();

  if (this->context.project_model)
    this->context.project_model->cleanup();
}

AppContext &HesiodApplication::get_context() { return this->context; }

const AppContext &HesiodApplication::get_context() const { return this->context; }

ProjectUI *HesiodApplication::get_project_ui_ref() { return this->project_ui.get(); }

QApplication &HesiodApplication::get_qapp() { return *static_cast<QApplication *>(this); }

bool HesiodApplication::is_headless() const { return this->headless; }

void HesiodApplication::load_project_model_and_ui(const std::string &fname,
                                                  bool               keep_name)
{
  Logger::log()->trace("HesiodApplication::load_project_model_and_ui: fname [{}]", fname);

  this->notify(std::format("Loading project... {}", fname));

  const std::string actual_fname = fname.empty() ? this->context.app_settings.global
                                                       .default_startup_project_file
                                                 : fname;

  this->cleanup();

  // --- model

  this->context.load_project_model(actual_fname);

  // --- UI

  // remove first old central widget (if any) so it doesn't linger
  if (QWidget *old = this->main_window->takeCentralWidget())
  {
    old->setParent(nullptr);
    old->deleteLater();
  }

  this->project_ui = std::make_unique<ProjectUI>();

  this->project_ui->initialize(this->context.project_model.get());
  this->project_ui->load_ui_state(actual_fname);

  this->main_window->setCentralWidget(this->project_ui->get_widget());

  // reset other visibility state
  this->project_ui->get_graph_manager_widget_ref()->setVisible(
      this->context.app_settings.window.show_graph_manager_widget);

  if (this->context.app_settings.interface.enable_texture_downloader)
  {
    this->project_ui->get_texture_downloader_ref()->setVisible(
        this->context.app_settings.window.show_texture_downloader_widget);
  }

  // --- connections

  // ProjectUI -> Project
  this->connect(this->project_ui->get_graph_manager_widget_ref(),
                &GraphManagerWidget::has_changed,
                [this]() { this->context.project_model->on_has_changed(); });

  this->connect(this->project_ui->get_graph_tabs_widget_ref(),
                &GraphTabsWidget::has_changed,
                [this]() { this->context.project_model->on_has_changed(); });

  // Project -> HesiodApplication
  this->context.project_model->project_name_changed = [this]()
  { this->on_project_name_changed(); };

  this->context.project_model->is_dirty_changed = [this]()
  { this->on_project_name_changed(); };

  // Project model and UI -> MainWindow
  this->main_window->setup_connections_with_project();

  // rename whether fname is empty or not
  if (keep_name)
    this->context.project_model->set_path(fname);

  this->notify("Project loaded successfully.");
}

void HesiodApplication::notify(const std::string &msg, int timeout)
{
  Logger::log()->trace("HesiodApplication::notify: {}", msg);
  this->main_window->notify(msg, timeout);
}

void HesiodApplication::on_application_settings_action()
{
  Logger::log()->trace("HesiodApplication::on_application_settings_action");

  // initialize app settings widget
  AppSettingsWindow *settings_window = new AppSettingsWindow(this->main_window);

  // open in a dialog
  QDialog dialog(this->main_window);
  dialog.setWindowTitle("Application Settings");

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(settings_window);

  QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);
  button_box->button(QDialogButtonBox::Ok)->setDefault(true);
  this->connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(button_box);

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

  Logger::log()->trace("MainWindow::on_export_batch: size = {}, nvariants = {}",
                       bake_settings.resolution,
                       bake_settings.nvariants);

  // --- setup export repertory

  this->notify("Baking and exporting...");

  // block UI
  QProgressDialog progress(tr("Baking and exporting..."),
                           QString(),
                           0,
                           0,
                           this->main_window);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setCancelButton(nullptr);
  progress.setMinimumDuration(0); // show immediately
  progress.show();
  QCoreApplication::processEvents();

  for (int k = 0; k < bake_settings.nvariants + 1; ++k)
  {
    const std::string msg = std::format("Baking variants {}/{}...",
                                        k,
                                        bake_settings.nvariants + 1);
    this->notify(msg);

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

    Logger::log()->info("MainWindow::on_export_batch: export path: {}",
                        export_path.string());

    // create it
    if (!fs::exists(export_path))
    {
      Logger::log()->trace("MainWindow::on_export_batch: creating export repertory {}",
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

      if (bake_settings.force_distributed)
      {
        bake_config.cm_cpu.mode = hmap::ForEachMode::VA_DISTRIBUTED;
        bake_config.cm_gpu.mode = hmap::ForEachMode::VA_DISTRIBUTED;
      }

      // run batch node
      hesiod::cli::run_batch_mode(
          fname.string(),
          glm::ivec2(bake_settings.resolution, bake_settings.resolution),
          bake_config.tiling,
          bake_config.overlap,
          &bake_config);
    }
  }

  // save config
  this->context.project_model->set_bake_config(bake_settings);

  // unblock UI
  progress.close();

  this->notify("Baking and exporting terminated.");
}

void HesiodApplication::on_load()
{
  Logger::log()->trace("HesiodApplication::on_load");

  fs::path path = this->context.project_model->get_path();

  QString load_fname = QFileDialog::getOpenFileName(this->main_window,
                                                    "Load...",
                                                    path.string().c_str(),
                                                    "Hesiod files (*.hsd)");

  if (!load_fname.isNull() && !load_fname.isEmpty())
    this->load_project_model_and_ui(load_fname.toStdString());
}

void HesiodApplication::on_load_ready_made()
{
  Logger::log()->trace("HesiodApplication::on_load_ready_made");

  std::string path = this->context.app_settings.global.ready_made_path;
  auto       *ex_dialog = new ExampleSelectorDialog(QString::fromStdString(path));
  bool        ret = ex_dialog->exec();

  if (ret)
  {
    std::string fname = ex_dialog->selected_file().toStdString();
    bool        keep_name = false;
    this->load_project_model_and_ui(fname, keep_name);
  }
}

void HesiodApplication::on_new()
{
  Logger::log()->trace("HesiodApplication::on_new");

  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(nullptr,
                                "New",
                                "Clear everything, are you sure?",
                                QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes)
  {
    this->cleanup();
    this->load_project_model_and_ui();
  }
}

void HesiodApplication::on_online_help()
{
  QDesktopServices::openUrl(
      QUrl(this->context.app_settings.global.online_help_url.c_str()));
}

void HesiodApplication::on_project_name_changed()
{
  std::string title = this->context.project_model->get_name() + " [" +
                      this->context.project_model->get_path().string() + "]";

  if (this->context.project_model->get_is_dirty())
    title += "*";

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

  QMessageBox::StandardButton reply;
  reply = QMessageBox::question(nullptr,
                                "Quit",
                                "Quitting the application, are you sure?",
                                QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes)
  {
    QApplication::quit();
    this->main_window->save_geometry();
    this->context.save_settings();
  }
}

void HesiodApplication::on_save()
{
  Logger::log()->trace("HesiodApplication::on_save");

  fs::path path = this->context.project_model->get_path();

  if (path.empty())
    this->on_save_as();
  else
  {
    this->save_project_model_and_ui(path.string());
    this->context.project_model->set_path(path);
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

    this->save_project_model_and_ui(clean_path.string());
    this->context.project_model->set_path(clean_path.string());
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

void HesiodApplication::save_project_model_and_ui(const std::string &fname)
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
  this->context.save_project_model(fname);
  this->context.project_model->set_is_dirty(false);
  this->project_ui->save_ui_state(fname);

  // add some global info
  nlohmann::json json;
  json["Hesiod version"] = "v" + std::to_string(HESIOD_VERSION_MAJOR) + "." +
                           std::to_string(HESIOD_VERSION_MINOR) + "." +
                           std::to_string(HESIOD_VERSION_PATCH);

  json["saved_at"] = timestamp();
  json_to_file(json, fname, /* merge_with_existing_content */ true);

  this->notify(std::format("Project saved successfully, {}.", fname));
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

  view_menu->addSeparator();

  auto *show_viewer_action = new QAction("Show Viewer in Main Window", this);
  show_viewer_action->setCheckable(true);
  {
    bool state = this->context.app_settings.node_editor.show_viewer;
    show_viewer_action->setChecked(state);
    view_menu->addAction(show_viewer_action);
  }

  auto *show_node_settings_pan_action = new QAction("Show Node Settings Pan", this);
  show_node_settings_pan_action->setCheckable(true);
  {
    bool state = this->context.app_settings.node_editor.show_node_settings_pan;
    show_node_settings_pan_action->setChecked(state);
    view_menu->addAction(show_node_settings_pan_action);
  }

  // --- connections

  this->connect(new_action, &QAction::triggered, this, &HesiodApplication::on_new);
  this->connect(load_action, &QAction::triggered, this, &HesiodApplication::on_load);
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
