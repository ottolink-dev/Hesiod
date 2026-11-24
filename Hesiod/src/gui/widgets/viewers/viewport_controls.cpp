/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <memory>
#include <set>

#include <QActionGroup>
#include <QEvent>
#include <QPushButton>

#include "oros/ui/viewport_widget.hpp"

#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/gui/widgets/node_widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/rail/snap_guide.hpp"
#include "hesiod/gui/widgets/shell/menu_chrome.hpp"
#include "hesiod/gui/widgets/viewers/viewport/panel_model.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_panel.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_rail.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_specs.hpp"
#include "hesiod/gui/widgets/viewers/viewport/viewport_style.hpp"
#include "hesiod/gui/widgets/viewers/viewport_controls.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

using namespace viewport;

// =====================================
// ViewportControls
// =====================================

namespace
{
std::set<ViewportControls *> &live_controls()
{
  static std::set<ViewportControls *> controls;
  return controls;
}
} // namespace

ViewportControls::ViewportControls(oros::ui::ViewportWidget *renderer,
                                   QWidget                  *rail_host,
                                   QPointer<GraphNodeWidget> graph,
                                   QObject                  *parent)
    : QObject(parent), renderer(renderer), rail_host(rail_host), graph(graph)
{
  if (!renderer || !rail_host)
    return;

  this->guide = new SnapGuide(rail_host);
  this->rail = new ViewportRail(this, this->guide, rail_host);
  live_controls().insert(this);
  this->rebuild_rail();
  this->rail->show();

  rail_host->installEventFilter(this);

  if (graph)
  {
    auto update_resolution = [this]()
    {
      if (!this->graph || !this->rail)
        return;
      GraphNode *gno = this->graph->get_p_graph_node();
      if (gno && gno->get_config_ref())
        this->rail->set_resolution_text(
            QString::fromStdString(resolution_label(gno->get_config_ref()->shape.x)));
    };
    update_resolution();
    QObject::connect(graph, &GraphNodeWidget::config_changed, this, update_resolution);
  }
}

ViewportControls::~ViewportControls() { live_controls().erase(this); }

void ViewportControls::relayout_all()
{
  for (ViewportControls *controls : live_controls())
    if (controls->rail)
    {
      controls->close_panel(false);
      controls->rail->place();
    }
}

void ViewportControls::set_preview_content(QWidget *content)
{
  if (!this->renderer || !this->rail_host || !content)
    return;

  QPointer<ViewportPanel> &panel = this->panels[ToolPreview];
  if (!panel)
  {
    panel = new ViewportPanel(
        tool_title(ToolPreview),
        std::make_unique<PanelModel>(this->renderer.data(), std::vector<Spec>{}),
        this->rail_host);
    panel->on_close = [this]() { this->close_panel(true); };
    panel->set_reset_visible(false);
  }

  content->setObjectName("viewportPreviewRows");
  panel->body()->addWidget(content);
  panel->body()->addStretch(1);
}

bool ViewportControls::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->rail_host && event->type() == QEvent::Resize && this->rail)
  {
    this->close_panel(false);
    this->rail->place();
  }

  return QObject::eventFilter(watched, event);
}

void ViewportControls::rebuild_rail()
{
  if (!this->rail)
    return;

  using Item = ViewportRail::Item;
  std::vector<Item> items;
  items.push_back({ToolResolution, "Preview resolution", false, true});
  items.push_back(
      {ToolPreview, "Preview: which node outputs the view shows", false, true});
  items.push_back({ToolLighting, "Lighting", true});
  items.push_back({ToolCamera, "Camera", false});
  items.push_back({ToolDisplay, "Display", true});
  items.push_back({ToolMaterial, "Material", false});
  items.push_back({ToolWater, "Water", true});
  items.push_back({ToolSky, "Sky & atmosphere", false});
  items.push_back({ToolMore, "More", true});

  this->rail->set_items(std::move(items));
}

void ViewportControls::on_tool_activated(int tool, const QRect &item_rect)
{
  if (tool == ToolResolution)
  {
    this->show_resolution_menu(item_rect);
    return;
  }
  if (tool == ToolMore)
  {
    this->show_more_menu(item_rect);
    return;
  }

  if (this->open_tool == tool)
    this->close_panel(true);
  else
    this->open_panel(tool);
}

void ViewportControls::open_panel(int tool)
{
  if (!this->renderer || !this->rail_host || !this->rail)
    return;

  this->close_panel(false);

  QPointer<ViewportPanel> &panel = this->panels[tool];

  if (tool == ToolPreview)
  {
    if (!panel)
      return;
    if (this->on_preview_opened)
      this->on_preview_opened();
  }

  if (!panel)
  {
    auto  model = std::make_unique<PanelModel>(this->renderer.data(), specs_for(tool));
    auto *p_model = model.get();
    panel = new ViewportPanel(tool_title(tool), std::move(model), this->rail_host);
    panel->on_close = [this]() { this->close_panel(true); };

    if (tool == ToolLighting)
    {
      auto *dome = new SunDome(p_model->float_attr("sun.azimuth_deg"),
                               p_model->float_attr("sun.elevation_deg"),
                               panel);
      panel->body()->addWidget(dome);
    }

    meta::qt::RowContext ctx;
    ctx.theme = &viewport_theme();
    ctx.default_value = [p_model](const std::string &key)
    { return p_model->default_ui(key); };

    meta::qt::ContainerRenderOptions options;
    options.design = properties_panel_design().design;
    options.row_context = ctx;
    options.category_policy = meta::qt::CategoryPolicy::Merged;
    options.root_category_name = std::string{};

    QWidget *rows = meta::qt::render(p_model->get_container(), options, panel);
    panel->body()->addWidget(rows);

    if (tool == ToolCamera)
    {
      auto *button = new QPushButton("Reset camera", panel);
      button->setObjectName("viewportAction");
      button->setCursor(Qt::PointingHandCursor);
      QPointer<oros::ui::ViewportWidget> r = this->renderer;
      QObject::connect(button,
                       &QPushButton::clicked,
                       panel,
                       [r]()
                       {
                         if (r)
                           r->reset_camera();
                       });
      panel->body()->addWidget(button);
    }

    panel->body()->addStretch(1);
  }

  this->open_tool = tool;
  this->rail->set_active(tool);
  panel->popup(this->rail->docked_rect().toAlignedRect(),
               this->rail->item_rect(tool),
               this->rail->get_edge());
}

void ViewportControls::close_panel(bool animate)
{
  if (this->open_tool >= 0)
    if (auto it = this->panels.find(this->open_tool);
        it != this->panels.end() && it->second)
      it->second->dismiss(animate);

  this->open_tool = -1;
  if (this->rail)
    this->rail->set_active(-1);
}

void ViewportControls::show_resolution_menu(const QRect &item_rect)
{
  if (!this->graph || !this->rail_host)
    return;

  GraphNode *gno = this->graph->get_p_graph_node();
  const int  current = gno && gno->get_config_ref() ? gno->get_config_ref()->shape.x : 0;

  HsdMenu  menu("Preview Resolution", this->rail_host);
  QAction *title = menu.addAction("Preview Resolution");
  title->setEnabled(false);

  auto *group = new QActionGroup(&menu);
  for (const int res : {512, 1024, 2048, 4096})
  {
    QAction *action = menu.addAction(QString("%1 × %1").arg(res));
    action->setCheckable(true);
    action->setChecked(res == current);
    action->setData(res);
    group->addAction(action);
  }

  this->rail->set_active(ToolResolution);
  const QPoint anchor = this->rail->get_edge() == Edge::Left
                            ? item_rect.topRight() + QPoint(8, 0)
                            : item_rect.topLeft() -
                                  QPoint(menu.sizeHint().width() + 8, 0);
  menu.setNoReplayFor(this->rail);
  QAction *chosen = menu.exec(this->rail_host->mapToGlobal(anchor));
  this->rail->set_active(this->open_tool);

  if (chosen && chosen->data().isValid() && this->graph)
    this->graph->apply_new_config(chosen->data().toInt());
}

void ViewportControls::show_more_menu(const QRect &item_rect)
{
  if (!this->rail_host)
    return;

  HsdMenu  menu("Viewport", this->rail_host);
  QAction *reset_all = menu.addAction("Reset all viewport settings");
  QAction *reset_camera = menu.addAction("Reset camera");

  this->rail->set_active(ToolMore);
  const QPoint anchor = this->rail->get_edge() == Edge::Left
                            ? item_rect.topRight() + QPoint(8, 0)
                            : item_rect.topLeft() -
                                  QPoint(menu.sizeHint().width() + 8, 0);
  menu.setNoReplayFor(this->rail);
  QAction *chosen = menu.exec(this->rail_host->mapToGlobal(anchor));
  this->rail->set_active(this->open_tool);

  if (!chosen || !this->renderer)
    return;

  if (chosen == reset_all)
    this->reset_all();
  else if (chosen == reset_camera)
    this->renderer->reset_camera();
}

void ViewportControls::reset_all()
{
  if (!this->renderer)
    return;

  for (const int tool :
       {ToolLighting, ToolCamera, ToolDisplay, ToolMaterial, ToolWater, ToolSky})
  {
    if (auto it = this->panels.find(tool); it != this->panels.end() && it->second)
      it->second->get_model()->reset_to_defaults();
    else
      PanelModel(this->renderer.data(), specs_for(tool)).reset_to_defaults();
  }
}

QMargins ViewportControls::reserved_margins() const
{
  if (!this->rail)
    return QMargins();

  const int band = thick_px() + kMargin + 6;
  switch (this->rail->get_edge())
  {
  case Edge::Left:
    return QMargins(band, 0, 0, 0);
  case Edge::Right:
    return QMargins(0, 0, band, 0);
  case Edge::Top:
    return QMargins(0, band, 0, 0);
  case Edge::Bottom:
    return QMargins(0, 0, 0, band);
  }
  return QMargins();
}

void ViewportControls::notify_layout_changed()
{
  if (this->on_layout_changed)
    this->on_layout_changed();
}

nlohmann::json ViewportControls::json_to() const
{
  return this->rail ? this->rail->json_to() : nlohmann::json::object();
}

void ViewportControls::json_from(const nlohmann::json &json)
{
  if (this->rail)
    this->rail->json_from(json);
}

} // namespace hesiod
