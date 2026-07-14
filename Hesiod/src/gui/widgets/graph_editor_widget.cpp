/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGridLayout>
#include <QSplitter>
#include <QTimer>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_editor_widget.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/graph_toolbar.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_library_widget.hpp"
#include "hesiod/gui/widgets/node_settings_widget.hpp"
#include "hesiod/gui/widgets/viewers/viewer_3d.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

GraphEditorWidget::GraphEditorWidget(std::weak_ptr<GraphNode> p_graph_node,
                                     QWidget                 *parent)
    : QWidget(parent), p_graph_node(p_graph_node)
{
  Logger::log()->trace("GraphEditorWidget::GraphEditorWidget");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  this->setup_layout();
  this->setup_connections();
}

GraphNodeWidget *GraphEditorWidget::get_graph_node_widget() const
{
  return this->graph_node_widget;
}

NodeSettingsWidget *GraphEditorWidget::get_node_settings_widget() const
{
  return this->node_settings_widget;
}

Viewer3D *GraphEditorWidget::get_viewer() const { return this->viewer; }

void GraphEditorWidget::json_from(nlohmann::json const &json)
{
  // GraphNodeWidget
  if (this->graph_node_widget)
  {
    const std::string graph_id = this->graph_node_widget->get_id();
    if (json.contains(graph_id))
      this->graph_node_widget->json_from(json[graph_id]);

    // Viewer3D
    if (this->viewer)
    {
      if (json.contains(graph_id) &&
          json[graph_id].contains("graph_editor_widget.viewer3d"))
      {
        // defer to let OpenGL context settle
        QTimer::singleShot(
            0,
            [this, json, graph_id]()
            { this->viewer->json_from(json[graph_id]["graph_editor_widget.viewer3d"]); });
      }
    }

    // node library
    if (this->node_library_widget)
    {
      if (json.contains(graph_id) &&
          json[graph_id].contains("graph_editor_widget.node_library_widget"))
      {
        this->node_library_widget->json_from(
            json[graph_id]["graph_editor_widget.node_library_widget"]);
      }
    }
  }
}

nlohmann::json GraphEditorWidget::json_to() const
{
  nlohmann::json json;

  // GraphNodeWidget
  if (this->graph_node_widget)
  {
    json = this->graph_node_widget->json_to();

    if (this->viewer)
      json["graph_editor_widget.viewer3d"] = this->viewer->json_to();

    if (this->node_library_widget)
      json["graph_editor_widget.node_library_widget"] = this->node_library_widget
                                                            ->json_to();
  }

  return json;
}

void GraphEditorWidget::setup_connections()
{
  Logger::log()->trace("GraphEditorWidget::setup_connections");

  if (!this->graph_node_widget)
    return;

  // nothing here... for now
}

void GraphEditorWidget::setup_layout()
{
  Logger::log()->trace("GraphEditorWidget::setup_layout");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // --- Layout

  auto *layout = new QGridLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  this->setLayout(layout);

  // optional left pan for node library
  bool show_lib = HSD_CTX.app_settings.node_editor.show_node_library_pan;
  int  row_offset = 0;

  if (show_lib)
  {
    this->node_library_widget = new NodeLibraryWidget();
    layout->addWidget(node_library_widget, 0, 0, 2, 1);
    row_offset++;
  }

  // graph area: viewer/graph vertical splitter + toolbar, wrapped so it can be
  // one pane of the horizontal splitter below.
  QWidget     *graph_container = new QWidget();
  QVBoxLayout *graph_layout = new QVBoxLayout(graph_container);
  graph_layout->setContentsMargins(0, 0, 0, 0);
  graph_layout->setSpacing(0);

  {
    QSplitter *splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);

    this->graph_node_widget = new GraphNodeWidget(gno->get_shared());
    this->graph_node_widget->setMinimumWidth(50); // let the graph pane shrink so the
                                                  // settings pane can be dragged wider

    // skip the 3D viewer (OpenGL) in headless CLI modes (e.g. --snapshot).
    if (!HSD_CTX.headless)
    {
      this->viewer = new Viewer3D(this->graph_node_widget);
      this->viewer->setMinimumHeight(32);
      splitter->addWidget(this->viewer);
    }

    splitter->addWidget(this->graph_node_widget);
    graph_layout->addWidget(splitter);
  }

  {
    auto *graph_toolbar = new GraphToolbar(this->graph_node_widget);
    graph_layout->addWidget(graph_toolbar);
  }

  // settings panel (created after graph_node_widget, which it takes).
  this->node_settings_widget = new NodeSettingsWidget(this->graph_node_widget);
  {
    std::string color = HSD_CTX.app_settings.colors.border.name().toStdString();
    set_style(this->node_settings_widget,
              std::format("border-left: 1px solid {};", color));
    this->node_settings_widget->setVisible(
        HSD_CTX.app_settings.node_editor.show_node_settings_pan);
  }

  // horizontal splitter: [ graph area | settings ] — user-resizable.
  QSplitter *h_splitter = new QSplitter(Qt::Horizontal);
  h_splitter->setChildrenCollapsible(false);
  h_splitter->addWidget(graph_container);
  h_splitter->addWidget(this->node_settings_widget);
  h_splitter->setStretchFactor(0, 1); // graph area absorbs window resizing
  h_splitter->setStretchFactor(1, 0); // settings keeps its width
  h_splitter->setSizes({650, 500});   // default: settings opens wide enough for paired sliders

  layout->addWidget(h_splitter, 0, row_offset, 2, 1);

  // Give the graph/settings splitter column the window's spare width so it always
  // has room to drag (otherwise the split only widens when the whole window grows).
  layout->setColumnStretch(row_offset, 1);

  // --- Connection(s)

  if (this->node_library_widget)
  {
    this->connect(this->node_library_widget,
                  &NodeLibraryWidget::node_type_selected,
                  this->graph_node_widget,
                  [this](const std::string &node_type)
                  {
                    QPointF center = this->graph_node_widget->get_center();
                    this->graph_node_widget->on_new_node_request(node_type, center);
                  });

    this->connect(this->node_library_widget,
                  &NodeLibraryWidget::node_type_selected_shift,
                  this->graph_node_widget,
                  &GraphNodeWidget::on_new_node_request_chain);

    this->connect(this->node_library_widget,
                  &NodeLibraryWidget::node_type_selected_ctrl,
                  this->graph_node_widget,
                  &GraphNodeWidget::on_new_node_request_replace);
  }
}

} // namespace hesiod
