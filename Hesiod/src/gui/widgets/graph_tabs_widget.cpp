/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QHBoxLayout>

#include "gnodegui/style.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_editor_widget.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/graph_tabs_widget.hpp"
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

  this->main_layout = new QHBoxLayout(this);
  this->main_layout->setContentsMargins(2, 2, 2, 2);
  this->main_layout->setSpacing(0);
  this->setLayout(this->main_layout);

  this->tab_widget = new QTabWidget(this);
  this->tab_widget->setTabPosition(QTabWidget::West);
  this->main_layout->addWidget(this->tab_widget);
  this->update_tab_widget();

  this->setup_connections(); // "permanent" ones
}

void GraphTabsWidget::clear()
{
  Logger::log()->trace("GraphTabsWidget::clear");

  // clear tabs
  while (this->tab_widget->count() > 0)
    this->tab_widget->removeTab(0);

  for (auto &[id, gew] : this->graph_editor_widget_map)
  {
    if (gew && gew->get_graph_node_widget())
    {
      gew->get_graph_node_widget()->clear_graphic_scene();
      gew->get_graph_node_widget()->close();
    }
  }

  this->graph_editor_widget_map.clear();
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

  QPointer<GraphEditorWidget> gew = this->graph_editor_widget_map.at(graph_id);
  if (gew && gew->get_graph_node_widget())
    gew->get_graph_node_widget()->add_import_heightmap_node(img);
  else
    Logger::log()->error("GraphTabsWidget::on_heightmap_download_ready: dangling ptr");
}

void GraphTabsWidget::on_textures_request(const std::vector<std::string> &texture_paths)
{
  Logger::log()->trace("GraphTabsWidget::on_textures_request");

  if (texture_paths.empty())
    return;

  std::string graph_id = this->get_selected_graph_id();

  QPointer<GraphEditorWidget> gew = this->graph_editor_widget_map.at(graph_id);
  if (gew && gew->get_graph_node_widget())
    gew->get_graph_node_widget()->add_import_texture_nodes(texture_paths);
  else
    Logger::log()->error("GraphTabsWidget::on_textures_request: dangling ptr");
}

void GraphTabsWidget::json_from(nlohmann::json const &json)
{
  this->update_tab_widget();

  if (json.contains("graph_node_widgets"))
    for (auto &[id, gew] : this->graph_editor_widget_map)
      if (gew)
        gew->json_from(json["graph_node_widgets"]);
}

nlohmann::json GraphTabsWidget::json_to() const
{
  nlohmann::json json;

  for (auto &[id, gew] : this->graph_editor_widget_map)
    if (gew)
      json["graph_node_widgets"][id] = gew->json_to();

  return json;
}

void GraphTabsWidget::on_copy_buffer_has_changed(const nlohmann::json &new_json)
{
  Logger::log()->trace("GraphTabsWidget::on_copy_buffer_has_changed");

  // redispatch the copy buffer to all the graphs
  for (auto &[_, gew] : this->graph_editor_widget_map)
    if (gew && gew->get_graph_node_widget())
      gew->get_graph_node_widget()->set_json_copy_buffer(new_json);
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

void GraphTabsWidget::set_show_node_settings_widget(bool new_state)
{
  this->show_node_settings_widget = new_state;

  // pass info to each node settings widget
  for (auto &[id, gew] : this->graph_editor_widget_map)
  {
    if (gew && gew->get_node_settings_widget())
      gew->get_node_settings_widget()->setVisible(new_state);
  }
}

void GraphTabsWidget::set_show_viewer(bool new_state)
{
  this->show_viewer = new_state;

  // pass info to each node settings widget
  for (auto &[id, gew] : this->graph_editor_widget_map)
  {
    if (gew && gew->get_viewer())
      gew->get_viewer()->setVisible(show_viewer);
  }
}

void GraphTabsWidget::set_show_node_library_pan(bool new_state)
{
  for (auto &[id, gew] : this->graph_editor_widget_map)
    if (gew)
      gew->set_node_library_visible(new_state);
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

      // remove corresponding GraphEditorWidget
      auto it = this->graph_editor_widget_map.find(id);
      if (it != this->graph_editor_widget_map.end())
      {
        if (it->second)
          it->second->close();
        this->graph_editor_widget_map.erase(it);
      }
    }
  }

  // --- Add new tabs for graphs that are not yet in tab_widget

  for (auto &id : gm->get_graph_order())
  {
    if (this->graph_editor_widget_map.contains(id))
      continue; // widget already exists

    Logger::log()->trace("creating GraphEditorWidget for {}", id);
    GraphNode *p_graph_node = gm->get_graph_ref_by_id(id);
    if (!p_graph_node)
    {
      Logger::log()->error(
          "GraphTabsWidget::update_tab_widget: graph '{}' not found, skipping",
          id);
      continue;
    }

    // Create GraphEditorWidget
    GraphEditorWidget *editor_widget = new GraphEditorWidget(p_graph_node->get_shared());
    this->graph_editor_widget_map[id] = editor_widget;

    // Connect signals
    auto *gnw = editor_widget->get_graph_node_widget();
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
    this->connect(editor_widget,
                  &GraphEditorWidget::node_library_toggle_requested,
                  this,
                  &GraphTabsWidget::node_library_toggle_requested);

    // Create tab container
    QWidget *tab = new QWidget();
    auto    *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(editor_widget, 3);
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
}

void GraphTabsWidget::zoom_to_content()
{
  for (auto &[id, gew] : this->graph_editor_widget_map)
  {
    if (gew && gew->get_graph_node_widget())
      gew->get_graph_node_widget()->zoom_to_content();
  }
}

} // namespace hesiod
