/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include <memory>
#include <set>

#include <QActionGroup>
#include <QEvent>
#include <QPushButton>

#include "qtr/render_widget.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/menu_chrome.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/viewers/viewport/panel_model.hpp"
#include "hesiod/gui/widgets/viewers/viewport/snap_guide.hpp"
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
// every live toolbar, for settings that apply to all of them
std::set<ViewportControls *> &live_controls()
{
  static std::set<ViewportControls *> controls;
  return controls;
}
} // namespace

ViewportControls::ViewportControls(qtr::RenderWidget        *renderer,
                                   QPointer<GraphNodeWidget> graph,
                                   QObject                  *parent)
    : QObject(parent), renderer(renderer), graph(graph)
{
  if (!renderer)
    return;

  // this UI replaces the renderer's own ImGui window
  renderer->set_settings_window_visible(false);

  this->guide = new SnapGuide(renderer);
  this->rail = new ViewportRail(this, this->guide, renderer);
  live_controls().insert(this);
  this->rebuild_rail();
  this->rail->show();

  renderer->installEventFilter(this);

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
  if (!this->renderer || !content)
    return;

  QPointer<ViewportPanel> &panel = this->panels[ToolPreview];
  if (!panel)
  {
    panel = new ViewportPanel(
        tool_title(ToolPreview),
        std::make_unique<PanelModel>(this->renderer.data(), std::vector<Spec>{}),
        this->renderer);
    panel->on_close = [this]() { this->close_panel(true); };
    panel->set_reset_visible(false);
  }

  content->setObjectName("viewportPreviewRows");
  panel->body()->addWidget(content);
  panel->body()->addStretch(1);
}

bool ViewportControls::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->renderer && event->type() == QEvent::Resize && this->rail)
  {
    this->close_panel(false);
    this->rail->place();
  }

  // a click in the view itself (the panel and the rail keep theirs) puts the
  // open panel away, like any popover
  if (watched == this->renderer && event->type() == QEvent::MouseButtonPress &&
      this->open_tool >= 0)
    this->close_panel(true);

  return QObject::eventFilter(watched, event);
}

void ViewportControls::rebuild_rail()
{
  if (!this->rail)
    return;

  using Item = ViewportRail::Item;
  std::vector<Item> items;
  items.push_back({ToolView2D, "2D view: the heightmap seen from above", false, false});
  items.push_back({ToolView3D, "3D view: the lit terrain", false, false});
  items.push_back({ToolResolution, "Preview resolution", true, true});
  items.push_back(
      {ToolPreview, "Preview: which node outputs the view shows", false, true});

  if (this->render_type == 0)
  {
    items.push_back({ToolLighting2D, "Lighting", true});
    items.push_back({ToolColormap2D, "Colormap", false});
  }
  else
  {
    items.push_back({ToolLighting, "Lighting", true});
    items.push_back({ToolCamera, "Camera", false});
    items.push_back({ToolDisplay, "Display", true});
    items.push_back({ToolMaterial, "Material", false});
    items.push_back({ToolWater, "Water", true});
    items.push_back({ToolSky, "Sky & atmosphere", false});
  }
  items.push_back({ToolMore, "More", true});

  this->rail->set_items(std::move(items));
}

void ViewportControls::set_render_type(int type)
{
  if (type == this->render_type)
    return;
  this->close_panel(false);
  this->render_type = type;
  if (this->rail)
    this->rail->set_view_mode(type, this->rail->isVisible());
  this->rebuild_rail();
}

void ViewportControls::on_tool_activated(int tool, const QRect &item_rect)
{
  if (tool == ToolView2D || tool == ToolView3D)
  {
    // app-wide, so every graph's viewer follows (and new ones start that way)
    const int type = tool == ToolView2D ? 0 : 1;
    if (type != this->render_type)
      HSD_APP->set_viewer_render_type(type);
    return;
  }

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
  if (!this->renderer || !this->rail)
    return;

  this->close_panel(false);

  QPointer<ViewportPanel> &panel = this->panels[tool];

  // the preview panel holds the viewer's rows (set_preview_content)
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
    panel = new ViewportPanel(tool_title(tool), std::move(model), this->renderer);
    panel->on_close = [this]() { this->close_panel(true); };

    // sun dome above the lighting sliders
    if (tool == ToolLighting || tool == ToolLighting2D)
    {
      const bool two_d = tool == ToolLighting2D;
      auto      *dome = new SunDome(
          p_model->float_attr(two_d ? "2d.sun_azimuth" : "light_phi"),
          p_model->float_attr(two_d ? "2d.sun_zenith" : "light_theta"),
          panel);
      panel->body()->addWidget(dome);
    }

    // the settings themselves, in the properties design
    meta::qt::RowContext ctx;
    ctx.theme = &viewport_theme();
    ctx.default_value = [p_model](const std::string &key)
    { return p_model->default_ui(key); };

    meta::qt::ContainerRenderOptions options;
    options.design = properties_panel_design().design;
    options.row_context = ctx;
    options.category_policy = meta::qt::CategoryPolicy::CP_MERGED;
    options.root_category_name = std::string{};

    QWidget *rows = meta::qt::render(p_model->get_container(), options, panel);
    panel->body()->addWidget(rows);

    // one-shot actions
    if (tool == ToolCamera || tool == ToolColormap2D)
    {
      auto *button = new QPushButton(tool == ToolCamera ? "Reset camera" : "Reset view",
                                     panel);
      button->setObjectName("viewportAction");
      button->setCursor(Qt::PointingHandCursor);
      QPointer<qtr::RenderWidget> r = this->renderer;
      QObject::connect(button,
                       &QPushButton::clicked,
                       panel,
                       [r, tool]()
                       {
                         if (!r)
                           return;
                         if (tool == ToolCamera)
                           r->reset_camera();
                         else
                           r->reset_view_2d();
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
  this->rail->raise(); // the rail stays clickable above any panel
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
  if (!this->graph)
    return;

  GraphNode *gno = this->graph->get_p_graph_node();
  const int  current = gno && gno->get_config_ref() ? gno->get_config_ref()->shape.x : 0;

  HsdMenu  menu("Preview Resolution", this->renderer);
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
  // a click on the rail that closes the menu is not replayed onto it (it
  // would open the menu straight again)
  menu.setNoReplayFor(this->rail);
  QAction *chosen = menu.exec(this->renderer->mapToGlobal(anchor));
  this->rail->set_active(this->open_tool);

  if (chosen && chosen->data().isValid() && this->graph)
    this->graph->apply_new_config(chosen->data().toInt());
}

void ViewportControls::show_more_menu(const QRect &item_rect)
{
  HsdMenu menu("Viewport", this->renderer);

  QAction *reset_all = menu.addAction("Reset all viewport settings");
  QAction *reset_camera = nullptr;
  QAction *gizmo = nullptr;
  QAction *void_bg = nullptr;

  if (this->render_type == 1)
  {
    reset_camera = menu.addAction("Reset camera");
    menu.addSeparator();
    gizmo = menu.addAction("Orientation gizmo");
    gizmo->setCheckable(true);
    if (bool *p = this->renderer->bool_setting("show_orientation_gizmo"))
      gizmo->setChecked(*p);
    void_bg = menu.addAction("Void background");
    void_bg->setCheckable(true);
    void_bg->setChecked(this->renderer->get_int_setting("background_mode") == 1);
  }
  else
    menu.addAction("Reset view")->setData(QString("reset_view"));

  this->rail->set_active(ToolMore);
  const QPoint anchor = this->rail->get_edge() == Edge::Left
                            ? item_rect.topRight() + QPoint(8, 0)
                            : item_rect.topLeft() -
                                  QPoint(menu.sizeHint().width() + 8, 0);
  menu.setNoReplayFor(this->rail);
  QAction *chosen = menu.exec(this->renderer->mapToGlobal(anchor));
  this->rail->set_active(this->open_tool);

  if (!chosen || !this->renderer)
    return;

  if (chosen == reset_all)
    this->reset_all();
  else if (chosen == reset_camera)
    this->renderer->reset_camera();
  else if (chosen == gizmo)
  {
    if (bool *p = this->renderer->bool_setting("show_orientation_gizmo"))
    {
      *p = gizmo->isChecked();
      this->renderer->settings_changed();
    }
  }
  else if (chosen == void_bg)
    this->renderer->set_int_setting("background_mode", void_bg->isChecked() ? 1 : 0);
  else if (chosen->data().toString() == "reset_view")
    this->renderer->reset_view_2d();
}

void ViewportControls::reset_all()
{
  if (!this->renderer)
    return;

  // every tool's settings, whether or not its panel was ever opened
  for (const int tool : {ToolLighting,
                         ToolCamera,
                         ToolDisplay,
                         ToolMaterial,
                         ToolWater,
                         ToolSky,
                         ToolLighting2D,
                         ToolColormap2D})
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
  // The orientation gizmo sits in the top-right corner: when the docked rail
  // covers that corner, move the gizmo out of its way (left of a rail on the
  // right edge, below one on the top edge).
  if (this->renderer && this->rail && !this->rail->is_moving())
  {
    const QRectF vr = QRectF(this->renderer->rect());
    const QRectF gizmo_zone(vr.right() - 132, vr.top(), 132, 132);
    const QRectF docked = this->rail->docked_rect();
    const qreal  band = thick_px() + kMargin - 4;

    float top = 0.f, right = 0.f;
    if (docked.intersects(gizmo_zone))
    {
      if (this->rail->get_edge() == Edge::Right)
        right = float(band);
      else if (this->rail->get_edge() == Edge::Top)
        top = float(band);
    }
    this->renderer->set_overlay_insets(top, right);
  }

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
