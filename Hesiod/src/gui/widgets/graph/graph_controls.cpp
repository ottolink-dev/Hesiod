/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <algorithm>

#include <QEvent>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph/graph_controls.hpp"
#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/gui/widgets/rail/snap_guide.hpp"
#include "hesiod/gui/widgets/rail/tool_rail.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_worker.hpp"

namespace hesiod
{

namespace
{

enum GraphTool : int
{
  ToolUpdate,
  ToolStop,
  ToolFit,
  ToolImport,
  ToolSettings,
  ToolViewport
};

const char *icon_name(int tool)
{
  switch (tool)
  {
  case ToolUpdate:
    return "graph_update";
  case ToolStop:
    return "graph_stop";
  case ToolFit:
    return "graph_fit";
  case ToolImport:
    return "graph_import";
  case ToolSettings:
    return "graph_settings";
  case ToolViewport:
    return "graph_viewer";
  default:
    return nullptr;
  }
}

} // namespace

GraphControls::GraphControls(GraphNodeWidget *graph) : QObject(graph), graph(graph)
{
  // children of the view, not of its viewport: it scrolls its children along
  this->guide = new rail::SnapGuide(graph);
  this->rail = new rail::ToolRail(this, this->guide, graph);
  this->rail->icon_painter =
      [this](QPainter &p, int tool, const QRectF &r, const QColor &ink)
  {
    const char *name = icon_name(tool);
    if (!name)
      return;

    // the stop button is lit while the graph is updated, dim otherwise
    QColor color = ink;
    if (tool == ToolStop)
    {
      if (this->updating)
        color = HSD_CTX.app_settings.colors.building;
      else
        color.setAlphaF(color.alphaF() * 0.35);
    }
    rail::paint_svg(p, name, r.center(), std::min(r.width(), r.height()), color);
  };
  this->rail->default_edge = rail::Edge::Left;
  this->rail->default_offset = rail::kMargin;

  using Item = rail::ToolRail::Item;
  this->rail->set_items({Item{ToolUpdate, "Update the graph", false, false},
                         Item{ToolStop, "Stop the update (Esc)", false, false},
                         Item{ToolFit, "Fit the view to the nodes", true, false},
                         Item{ToolImport, "Import a graph", true, false},
                         Item{ToolSettings, "Graph settings", false, false},
                         Item{ToolViewport, "Open a viewer", true, false}});
  this->rail->show();

  graph->installEventFilter(this);

  this->connect(graph,
                &GraphNodeWidget::update_started,
                this,
                [this]() { this->set_updating(true); });
  this->connect(graph,
                &GraphNodeWidget::update_held,
                this,
                [this](bool held)
                {
                  if (held)
                    this->set_updating(false);
                });
  this->connect(graph,
                &GraphNodeWidget::update_finished,
                this,
                [this]() { this->set_updating(false); });
}

void GraphControls::set_updating(bool new_state)
{
  if (new_state == this->updating)
    return;

  this->updating = new_state;
  this->rail->update();
}

bool GraphControls::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->graph && event->type() == QEvent::Resize)
    this->rail->place();

  return QObject::eventFilter(watched, event);
}

nlohmann::json GraphControls::json_to() const { return this->rail->json_to(); }

void GraphControls::json_from(const nlohmann::json &json) { this->rail->json_from(json); }

void GraphControls::on_tool_activated(int tool, const QRect & /* item_rect */)
{
  switch (tool)
  {
  case ToolUpdate:
    Q_EMIT this->graph->graph_reload_request();
    break;
  case ToolStop:
    // as Esc does (see MainWindow::keyPressEvent): what is left goes with the next edit
    if (HSD_CTX.project_model)
      if (GraphManager *p_manager = HSD_CTX.project_model->get_graph_manager_ref())
        p_manager->get_worker().hold();
    break;
  case ToolFit:
    this->graph->zoom_to_content();
    break;
  case ToolImport:
    Q_EMIT this->graph->graph_import_request();
    break;
  case ToolSettings:
    Q_EMIT this->graph->graph_settings_request();
    break;
  case ToolViewport:
    Q_EMIT this->graph->viewport_request();
    break;
  default:
    break;
  }
}

} // namespace hesiod
