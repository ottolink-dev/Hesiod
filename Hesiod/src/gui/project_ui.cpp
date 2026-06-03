/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QCoreApplication>
#include <QTimer>

#include "hesiod/app/app_settings.hpp"
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/project_ui.hpp"
#include "hesiod/gui/widgets/graph_manager_widget.hpp"
#include "hesiod/gui/widgets/graph_tabs_widget.hpp"
#include "hesiod/gui/widgets/heightmapper_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/project_model.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

ProjectUI::ProjectUI(QWidget *parent) : QWidget(parent)
{
  Logger::log()->trace("Project::Project");
  this->setAttribute(Qt::WA_DeleteOnClose);
}

void ProjectUI::cleanup()
{
  Logger::log()->trace("ProjectUI::cleanup");

  this->graph_manager_widget->deleteLater();
  this->graph_tabs_widget->deleteLater();

  if (this->texture_downloader)
    this->texture_downloader->deleteLater();

  if (this->heightmapper_widget)
    this->heightmapper_widget->deleteLater();

  QCoreApplication::processEvents();
}

GraphManagerWidget *ProjectUI::get_graph_manager_widget_ref()
{
  return this->graph_manager_widget;
}

GraphTabsWidget *ProjectUI::get_graph_tabs_widget_ref()
{
  return this->graph_tabs_widget;
}

HeightmapperWidget *ProjectUI::get_heightmapper_widget_ref()
{
  return this->heightmapper_widget.get();
}

qtd::TextureDownloader *ProjectUI::get_texture_downloader_ref()
{
  return this->texture_downloader.get();
}

QWidget *ProjectUI::get_widget()
{
  return static_cast<QWidget *>(this->graph_tabs_widget);
}

void ProjectUI::load_ui_state(const std::string &fname)
{
  Logger::log()->trace("ProjectUI::load: {}", fname);

  if (!std::filesystem::exists(std::filesystem::path(fname)))
  {
    Logger::log()->error("ProjectUI::load: file does not exist: {}", fname);
    return;
  }

  nlohmann::json json = json_from_file(fname);

  if (!json.contains("graph_manager_widget") || !json.contains("graph_tabs_widget"))
  {
    Logger::log()->error("Project::json_from: could not parse json");
    return;
  }

  this->graph_manager_widget->json_from(json["graph_manager_widget"]);
  this->graph_tabs_widget->json_from(json["graph_tabs_widget"]);
}

void ProjectUI::initialize(ProjectModel *project)
{
  Logger::log()->trace("ProjectUI::initialize");

  // safeguards...
  if (!project)
    Logger::log()->error("ProjectUI::initialize: project ptr is NULL");

  GraphManager *p_graph_manager = project->get_graph_manager_ref();

  if (!p_graph_manager)
    Logger::log()->error("ProjectUI::initialize: project graph_manager ptr is NULL");

  // initialize UI components (only tabs assign to the current parent
  // widget, so that the graph manager and the texture downloader will
  // show as floating windows)
  this->graph_manager_widget = new GraphManagerWidget(p_graph_manager->get_shared(),
                                                      /* parent */ nullptr); // top window
  this->graph_tabs_widget = new GraphTabsWidget(p_graph_manager->get_shared(), this);

  {
    bool state = HSD_CTX.app_settings.node_editor.show_node_settings_pan;
    this->graph_tabs_widget->set_show_node_settings_widget(state);
  }

  {
    bool state = HSD_CTX.app_settings.node_editor.show_viewer;
    QTimer::singleShot(0,
                       [this, state]()
                       { this->get_graph_tabs_widget_ref()->set_show_viewer(state); });
  }

  if (HSD_CTX.app_settings.interface.enable_texture_downloader)
  {
    // no parent, top-level window (managed by a QPointer)
    this->texture_downloader = new qtd::TextureDownloader();
  }

  if (HSD_CTX.app_settings.interface.enable_heightmapper_widget)
  {
    // no parent, top-level window (managed by a QPointer)
    this->heightmapper_widget = new HeightmapperWidget();
  }

  // connections
  this->setup_connections();
}

void ProjectUI::save_ui_state(const std::string &fname) const
{
  Logger::log()->trace("ProjectUI::save: {}", fname);

  nlohmann::json json;
  json["graph_manager_widget"] = this->graph_manager_widget->json_to();
  json["graph_tabs_widget"] = this->graph_tabs_widget->json_to();
  json_to_file(json, fname, /* merge_with_existing_content */ true);
}

void ProjectUI::setup_connections()
{
  Logger::log()->trace("Project::setup_connections");

  // GraphManagerWidget -> GraphTabsWidget
  this->connect(this->graph_manager_widget,
                &GraphManagerWidget::graph_removed,
                this->graph_tabs_widget,
                &GraphTabsWidget::update_tab_widget);

  this->connect(this->graph_manager_widget,
                &GraphManagerWidget::list_reordered,
                this->graph_tabs_widget,
                &GraphTabsWidget::update_tab_widget);

  this->connect(this->graph_manager_widget,
                &GraphManagerWidget::new_graph_added,
                this->graph_tabs_widget,
                &GraphTabsWidget::update_tab_widget);

  this->connect(this->graph_manager_widget,
                &GraphManagerWidget::selected_graph_changed,
                this->graph_tabs_widget,
                &GraphTabsWidget::set_selected_tab);

  // GraphTabsWidget -> GraphManagerWidget
  this->connect(this->graph_tabs_widget,
                &GraphTabsWidget::has_been_cleared,
                this->graph_manager_widget,
                &GraphManagerWidget::update_combobox);

  this->connect(this->graph_tabs_widget,
                &GraphTabsWidget::new_node_created,
                this,
                [this](const std::string &graph_id, const std::string & /* id */)
                { this->graph_manager_widget->update_combobox(graph_id); });

  this->connect(this->graph_tabs_widget,
                &GraphTabsWidget::node_deleted,
                this,
                [this](const std::string &graph_id, const std::string & /* id */)
                { this->graph_manager_widget->update_combobox(graph_id); });

  this->connect(this->graph_tabs_widget,
                &GraphTabsWidget::update_started,
                this,
                [this]() { Q_EMIT this->graph_manager_widget->update_started(); });

  this->connect(this->graph_tabs_widget,
                &GraphTabsWidget::update_finished,
                this,
                [this]() { Q_EMIT this->graph_manager_widget->update_finished(); });

  // qtd::TextureDownloader -> GraphTabsWidget
  this->connect(this->texture_downloader,
                &qtd::TextureDownloader::textures_retrieved,
                this->graph_tabs_widget,
                &GraphTabsWidget::on_textures_request);

  // HeightmapperWidget -> GraphTabsWidget
  this->connect(this->heightmapper_widget,
                &HeightmapperWidget::heightmap_download_ready,
                this->graph_tabs_widget,
                &GraphTabsWidget::on_heightmap_download_ready);
}

} // namespace hesiod
