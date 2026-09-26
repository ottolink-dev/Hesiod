/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QHBoxLayout>
#include <QPushButton>
#include <QTabBar>

#include "gnodegui/style.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/project_ui.hpp"
#include "hesiod/gui/widgets/graph_manager_widget.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/graph_tab_strip.hpp"
#include "hesiod/gui/widgets/graph_tabs_widget.hpp"
#include "hesiod/gui/widgets/graph_workspace_widget.hpp"
#include "hesiod/gui/widgets/menu_chrome.hpp"
#include "hesiod/gui/widgets/message_dialog.hpp"
#include "hesiod/gui/widgets/node_settings_widget.hpp"
#include "hesiod/gui/widgets/viewers/viewer_3d.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/receive_node.hpp"

namespace hesiod
{

GraphTabsWidget::GraphTabsWidget(std::weak_ptr<GraphManager> p_graph_manager,
                                 QWidget                    *parent)
    : QWidget(parent), p_graph_manager(p_graph_manager)
{
  Logger::log()->trace("GraphTabsWidget::GraphTabsWidget");

  AppContext &ctx = HSD_CTX;

  // styles (GNodeGUI)
  GN_STYLE->viewer.add_new_icon = false;
  GN_STYLE->viewer.add_load_save_icons = false;
  GN_STYLE->viewer.add_group = ctx.app_settings.node_editor.enable_node_groups;
  GN_STYLE->node.color_port_data = ctx.style_settings.data_color_map;
  GN_STYLE->node.color_category = ctx.style_settings.category_color_map;
  GN_STYLE->node.port_radius = ctx.app_settings.node_editor.port_radius;

  // outer frame of the workspace: the dark base the panel cards sit on
  this->setObjectName("hsdBase");
  this->setAttribute(Qt::WA_StyledBackground);

  this->main_layout = new QHBoxLayout(this);
  this->main_layout->setContentsMargins(0, 2, 6, 0);
  this->main_layout->setSpacing(0);
  this->setLayout(this->main_layout);

  this->tab_widget = new QTabWidget(this);
  this->tab_widget->setObjectName("hsdGraphTabs");
  this->tab_widget->setDocumentMode(true);
  this->tab_widget->setTabPosition(QTabWidget::West);
  // the tabs are drawn on the node editor's own edge (GraphTabStrip, one per
  // workspace, kept in step by sync_tab_strips); the widget only switches pages
  this->tab_widget->tabBar()->hide();
  this->main_layout->addWidget(this->tab_widget);
  this->connect(this->tab_widget,
                &QTabWidget::currentChanged,
                this,
                [this](int) { this->sync_tab_strips(); });
  this->update_tab_widget();

  this->setup_connections(); // "permanent" ones
}

void GraphTabsWidget::clear()
{
  Logger::log()->trace("GraphTabsWidget::clear");

  // clear tabs
  while (this->tab_widget->count() > 0)
    this->tab_widget->removeTab(0);

  for (auto &[id, gww] : this->graph_workspace_widget_map)
  {
    if (gww && gww->get_graph_node_widget())
    {
      gww->get_graph_node_widget()->clear_graphic_scene();
      gww->get_graph_node_widget()->close();
    }
  }

  this->graph_workspace_widget_map.clear();
}

std::string GraphTabsWidget::get_selected_graph_id() const
{
  QString selected_tab_text = this->tab_widget->tabText(this->tab_widget->currentIndex());
  return selected_tab_text.toStdString();
}

void GraphTabsWidget::on_heightmap_download_ready(
    const HeightmapperWidget::DownloadInfo &info)
{
  Logger::log()->trace("GraphTabsWidget::on_heightmap_download_ready");

  QImage img = QImage::fromData(info.data, "PNG");
  img = img.convertToFormat(QImage::Format_Grayscale16);

  std::string graph_id = this->get_selected_graph_id();

  QPointer<GraphWorkspaceWidget> gww = this->graph_workspace_widget_map.at(graph_id);
  if (gww && gww->get_graph_node_widget())
    gww->get_graph_node_widget()->add_import_heightmap_node(img);
  else
    Logger::log()->error("GraphTabsWidget::on_heightmap_download_ready: dangling ptr");
}

void GraphTabsWidget::on_textures_request(const std::vector<std::string> &texture_paths)
{
  Logger::log()->trace("GraphTabsWidget::on_textures_request");

  if (texture_paths.empty())
    return;

  std::string graph_id = this->get_selected_graph_id();

  QPointer<GraphWorkspaceWidget> gww = this->graph_workspace_widget_map.at(graph_id);
  if (gww && gww->get_graph_node_widget())
    gww->get_graph_node_widget()->add_import_texture_nodes(texture_paths);
  else
    Logger::log()->error("GraphTabsWidget::on_textures_request: dangling ptr");
}

void GraphTabsWidget::json_from(nlohmann::json const &json)
{
  this->update_tab_widget();

  if (json.contains("graph_node_widgets"))
    for (auto &[id, gww] : this->graph_workspace_widget_map)
      if (gww)
        gww->json_from(json["graph_node_widgets"]);
}

nlohmann::json GraphTabsWidget::json_to() const
{
  nlohmann::json json;

  for (auto &[id, gww] : this->graph_workspace_widget_map)
    if (gww)
      json["graph_node_widgets"][id] = gww->json_to();

  return json;
}

void GraphTabsWidget::on_copy_buffer_has_changed(const nlohmann::json &new_json)
{
  Logger::log()->trace("GraphTabsWidget::on_copy_buffer_has_changed");

  // redispatch the copy buffer to all the graphs
  for (auto &[_, gww] : this->graph_workspace_widget_map)
    if (gww && gww->get_graph_node_widget())
      gww->get_graph_node_widget()->set_json_copy_buffer(new_json);
}

void GraphTabsWidget::on_has_been_cleared(const std::string &graph_id)
{
  Q_EMIT this->has_been_cleared(graph_id);
  Q_EMIT this->has_changed();
}

void GraphTabsWidget::on_new_node_created(const std::string &graph_id,
                                          const std::string &id)
{
  Logger::log()->trace("GraphTabsWidget::on_new_node_created");

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  // check if it's a Receive node to update its tag list
  auto it_graph = gm->get_graph_nodes().find(graph_id);
  if (it_graph == gm->get_graph_nodes().end())
  {
    Logger::log()->critical("GraphTabsWidget::on_new_node_created: graph {} not found",
                            graph_id);
    return;
  }

  BaseNode *p_node = it_graph->second->get_node_ref_by_id<BaseNode>(id);
  if (p_node)
  {
    if (p_node->get_node_type() == "Receive")
      this->update_receive_nodes_tag_list();

    Q_EMIT this->new_node_created(graph_id, id);
    Q_EMIT this->has_changed();
  }
  else
  {
    Logger::log()->critical(
        "GraphTabsWidget::on_new_node_created: the node just created is nullptr");
  }
}

void GraphTabsWidget::on_node_deleted(const std::string &graph_id, const std::string &id)
{
  Q_EMIT this->node_deleted(graph_id, id);
  Q_EMIT this->has_changed();
}

void GraphTabsWidget::set_show_node_library_pan(bool new_state)
{
  for (auto &[id, gww] : this->graph_workspace_widget_map)
    if (gww)
      gww->set_node_library_visible(new_state);
}

void GraphTabsWidget::set_show_node_settings_widget(bool new_state)
{
  this->show_node_settings_widget = new_state;

  // pass info to each node settings widget
  for (auto &[id, gww] : this->graph_workspace_widget_map)
  {
    if (gww && gww->get_node_settings_widget())
      gww->get_node_settings_widget()->setVisible(new_state);
  }
}

void GraphTabsWidget::set_show_viewer(bool new_state)
{
  this->show_viewer = new_state;

  // pass info to each node settings widget
  for (auto &[id, gww] : this->graph_workspace_widget_map)
  {
    if (gww && gww->get_viewer())
      gww->get_viewer()->setVisible(show_viewer);
  }
}

void GraphTabsWidget::set_viewer_render_type(int new_type)
{
  this->viewer_render_type = new_type;

  for (auto &[id, gww] : this->graph_workspace_widget_map)
    if (gww && gww->get_viewer())
      gww->get_viewer()->set_render_type(new_type);
}

void GraphTabsWidget::set_selected_tab(const std::string &graph_id)
{
  for (int i = 0; i < this->tab_widget->count(); ++i)
  {
    QString tab_label = tab_widget->tabText(i);
    if (tab_label.toStdString() == graph_id)
      this->tab_widget->setCurrentIndex(i);
  }
}

void GraphTabsWidget::setup_connections()
{
  Logger::log()->trace("GraphTabsWidget::setup_connections");

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  // connections / model
  gm->new_broadcast_tag = [safe_this = QPointer(this)](const std::string &)
  {
    if (safe_this)
      safe_this->update_receive_nodes_tag_list();
  };

  gm->remove_broadcast_tag = [safe_this = QPointer(this)](const std::string &)
  {
    if (safe_this)
      safe_this->update_receive_nodes_tag_list();
  };
}

QSize GraphTabsWidget::sizeHint() const { return QSize(1024, 1024); }

void GraphTabsWidget::update_receive_nodes_tag_list()
{
  Logger::log()->trace("GraphTabsWidget::update_receive_nodes_tag_list");

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  // update the tag list for all the Receive nodes of the graphs
  for (auto &[gid, graph] : gm->get_graph_nodes())
    for (auto &[nid, node] : graph->get_nodes())
      if (node->get_label() == "Receive")
      {
        ReceiveNode *p_rnode = graph->get_node_ref_by_id<ReceiveNode>(nid);

        std::vector<std::string> tags = {};
        for (auto &[tag, _] : gm->get_broadcast_params())
          tags.push_back(tag);

        if (p_rnode)
          p_rnode->update_tag_list(tags);
      }
}

void GraphTabsWidget::update_tab_widget()
{
  Logger::log()->trace("GraphTabsWidget::update_tab_widget");

  if (!this->tab_widget)
    return;

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  // backup currently selected tab label
  QString current_tab_label;
  int     current_index = this->tab_widget->currentIndex();
  if (current_index >= 0 && this->tab_widget->count() > 0)
    current_tab_label = this->tab_widget->tabText(current_index);

  // --- Remove tabs for graphs that no longer exist

  for (int i = this->tab_widget->count() - 1; i >= 0; --i)
  {
    QString     tab_label = this->tab_widget->tabText(i);
    std::string id = tab_label.toStdString();

    if (!gm->get_graph_ref_by_id(id))
    {
      QWidget *tab = this->tab_widget->widget(i);
      this->tab_widget->removeTab(i);
      if (tab)
        tab->deleteLater();

      // remove corresponding GraphWorkspaceWidget
      auto it = this->graph_workspace_widget_map.find(id);
      if (it != this->graph_workspace_widget_map.end())
      {
        if (it->second)
          it->second->close();
        this->graph_workspace_widget_map.erase(it);
      }
    }
  }

  // --- Add new tabs for graphs that are not yet in tab_widget

  for (auto &id : gm->get_graph_order())
  {
    if (this->graph_workspace_widget_map.contains(id))
      continue; // widget already exists

    Logger::log()->trace("creating GraphWorkspaceWidget for {}", id);
    GraphNode *p_graph_node = gm->get_graph_ref_by_id(id);
    if (!p_graph_node)
    {
      Logger::log()->error(
          "GraphTabsWidget::update_tab_widget: graph '{}' not found, skipping",
          id);
      continue;
    }

    // Create GraphWorkspaceWidget
    GraphWorkspaceWidget *workspace_widget = new GraphWorkspaceWidget(
        p_graph_node->get_shared());
    this->graph_workspace_widget_map[id] = workspace_widget;

    // a graph added later opens in the viewer mode already chosen
    if (workspace_widget->get_viewer())
      workspace_widget->get_viewer()->set_render_type(this->viewer_render_type);

    // Connect signals
    auto *gnw = workspace_widget->get_graph_node_widget();
    this->connect(gnw,
                  &GraphNodeWidget::graph_edited,
                  this,
                  &GraphTabsWidget::has_changed);
    this->connect(gnw,
                  &GraphNodeWidget::has_been_cleared,
                  this,
                  &GraphTabsWidget::on_has_been_cleared);
    this->connect(gnw,
                  &GraphNodeWidget::new_node_created,
                  this,
                  &GraphTabsWidget::on_new_node_created);
    this->connect(gnw,
                  &GraphNodeWidget::node_deleted,
                  this,
                  &GraphTabsWidget::on_node_deleted);
    this->connect(gnw,
                  &GraphNodeWidget::copy_buffer_has_changed,
                  this,
                  &GraphTabsWidget::on_copy_buffer_has_changed);
    this->connect(gnw,
                  &GraphNodeWidget::update_started,
                  this,
                  [this]() { emit update_started(); });
    this->connect(gnw,
                  &GraphNodeWidget::update_finished,
                  this,
                  [this]() { emit update_finished(); });
    this->connect(workspace_widget,
                  &GraphWorkspaceWidget::node_library_toggle_requested,
                  this,
                  &GraphTabsWidget::node_library_toggle_requested);

    // Create tab container
    QWidget *tab = new QWidget();
    tab->setObjectName("hsdBase");
    auto *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(workspace_widget, 3);
    tab->setLayout(layout);

    this->tab_widget->addTab(tab, QString::fromStdString(id));
  }

  // --- Restore previous tab selection

  if (!current_tab_label.isEmpty())
  {
    for (int i = 0; i < this->tab_widget->count(); ++i)
    {
      if (this->tab_widget->tabText(i) == current_tab_label)
      {
        this->tab_widget->setCurrentIndex(i);
        break;
      }
    }
  }

  this->sync_tab_strips();
}

void GraphTabsWidget::sync_tab_strips()
{
  if (!this->tab_widget)
    return;

  QStringList names;
  for (int i = 0; i < this->tab_widget->count(); ++i)
    names << this->tab_widget->tabText(i);
  const int current = this->tab_widget->currentIndex();

  for (auto &[id, gww] : this->graph_workspace_widget_map)
  {
    if (!gww || !gww->get_tab_strip())
      continue;

    GraphTabStrip *strip = gww->get_tab_strip();
    strip->set_tabs(names, current);

    // Wired once per strip, not on every sync: a sync can run from inside one
    // of these callbacks (the tab menu selects its tab), and reassigning a
    // std::function while it runs destroys it mid-call.
    if (strip->on_selected)
      continue;

    strip->on_selected = [this](int index)
    {
      if (this->tab_widget && index >= 0 && index < this->tab_widget->count())
        this->tab_widget->setCurrentIndex(index);
    };
    strip->on_new = []()
    {
      if (ProjectUI *ui = HSD_APP->get_project_ui_ref())
        if (GraphManagerWidget *manager = ui->get_graph_manager_widget_ref())
          manager->on_new_graph_request();
    };
    strip->on_context_menu = [this](int index, const QPoint &global_pos)
    { this->show_tab_menu(index, global_pos); };
  }
}

void GraphTabsWidget::show_tab_menu(int index, const QPoint &global_pos)
{
  if (!this->tab_widget || index < 0 || index >= this->tab_widget->count())
    return;

  const std::string graph_id = this->tab_widget->tabText(index).toStdString();
  auto              it = this->graph_workspace_widget_map.find(graph_id);
  GraphNodeWidget  *gnw = it != this->graph_workspace_widget_map.end() && it->second
                              ? it->second->get_graph_node_widget()
                              : nullptr;

  HsdMenu  menu(QString::fromStdString(graph_id), this);
  QAction *title = menu.addAction(QString::fromStdString(graph_id));
  title->setEnabled(false);
  menu.addSeparator();

  QAction *settings = menu.addAction("Graph settings…");
  QAction *clear = menu.addAction("Clear graph…");
  menu.addSeparator();
  QAction *add = menu.addAction(HSD_ICON("menu_new_graph"), "New graph");
  menu.addSeparator();
  QAction *remove = menu.addAction("Delete graph…");
  remove->setEnabled(this->tab_widget->count() > 1); // a project keeps one graph
  if (!remove->isEnabled())
    remove->setToolTip("A project needs at least one graph");

  // show the graph being acted on
  this->tab_widget->setCurrentIndex(index);

  QAction *chosen = menu.exec(global_pos);
  if (!chosen)
    return;

  GraphManagerWidget *manager = nullptr;
  if (ProjectUI *ui = HSD_APP->get_project_ui_ref())
    manager = ui->get_graph_manager_widget_ref();

  if (chosen == settings && gnw)
    gnw->on_graph_settings_request();
  else if (chosen == clear && gnw)
    gnw->on_graph_clear_request();
  else if (chosen == add && manager)
    manager->on_new_graph_request();
  else if (chosen == remove && manager)
  {
    MessageDialog box(this->window(),
                      MessageDialog::Kind::Warning,
                      QString("Delete \"%1\"?").arg(QString::fromStdString(graph_id)),
                      "The graph and all its nodes are removed from the project. This "
                      "cannot be undone.");
    box.add_button("Cancel", MessageDialog::Role::Secondary, true, true);
    QPushButton *confirm = box.add_button("Delete graph", MessageDialog::Role::Danger);
    box.exec();
    if (box.clicked_button() == confirm)
      manager->delete_graph(graph_id);
  }
}

void GraphTabsWidget::zoom_to_content()
{
  for (auto &[id, gww] : this->graph_workspace_widget_map)
  {
    if (gww && gww->get_graph_node_widget())
      gww->get_graph_node_widget()->zoom_to_content();
  }
}

} // namespace hesiod
