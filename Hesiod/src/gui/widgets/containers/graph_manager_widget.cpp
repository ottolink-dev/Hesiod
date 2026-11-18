/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include "highmap/colorize.hpp"
#include "highmap/virtual_array/virtual_array.hpp"
#include "highmap/virtual_array/virtual_texture.hpp"

#include "gnode/graph_worker.hpp"
#include "gnodegui/graphics_node.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/containers/graph_manager_widget.hpp"
#include "hesiod/gui/widgets/containers/panel_frame.hpp"
#include "hesiod/gui/widgets/controls/grip_splitter.hpp"
#include "hesiod/gui/widgets/dialogs/string_input_dialog.hpp"
#include "hesiod/gui/widgets/graph/coord_frame_widget.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/graph_config_dialog.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

namespace
{

bool parse_preview_tag(const std::string &tag,
                       std::string       &node_label,
                       std::string       &node_id,
                       std::string       &port_id)
{
  std::vector<std::string> parts = split_string(tag, '/');
  if (parts.size() != 3)
    return false;
  node_label = parts[0];
  node_id = parts[1];
  port_id = parts[2];
  return true;
}

} // namespace

// =====================================
// GraphNodeProxy
// =====================================
GraphNodeProxy::GraphNodeProxy(std::weak_ptr<GraphNode> model, QObject *owner)
    : model(std::move(model))
{
  setParent(owner);
  if (auto g = this->model.lock())
  {
    g->ports_updated = [safe_this = QPointer(this)]()
    {
      if (safe_this)
        Q_EMIT safe_this->ports_changed();
    };
  }
}

std::string GraphNodeProxy::get_id() const
{
  if (auto g = model.lock())
    return g->get_id();
  return {};
}

void GraphNodeProxy::set_id(const std::string &id)
{
  if (auto g = model.lock())
    g->set_id(id);
}

std::string GraphNodeProxy::get_caption() const
{
  if (auto g = model.lock())
    return g->get_id();
  return {};
}

std::string GraphNodeProxy::get_category() const { return "Terrain"; }

std::string GraphNodeProxy::get_comment() const { return {}; }

std::string GraphNodeProxy::get_tool_tip_text() const
{
  if (auto g = model.lock())
    return "Terrain Graph: " + g->get_id();
  return {};
}

int GraphNodeProxy::get_nports() const
{
  if (auto g = model.lock())
    return g->get_nports();
  return 0;
}

std::string GraphNodeProxy::get_port_caption(int index) const
{
  if (auto g = model.lock())
    return g->get_port_label(index);
  return {};
}

gngui::PortType GraphNodeProxy::get_port_type(int index) const
{
  if (auto g = model.lock())
    return static_cast<gngui::PortType>(g->get_port_type(g->get_port_label(index)));
  return gngui::PortType::IN;
}

std::string GraphNodeProxy::get_data_type(int index) const
{
  if (auto g = model.lock())
    return g->gnode::Node::get_data_type(index);
  return {};
}

void *GraphNodeProxy::get_data_ref(int index) const
{
  if (auto g = model.lock())
    return g->get_value_ref_void(index);
  return nullptr;
}

std::unordered_map<std::string, std::string> GraphNodeProxy::consume_port_renames()
{
  if (auto g = model.lock())
    return g->consume_port_renames();
  return {};
}

// =====================================
// GraphPreviewLayerWidget
// =====================================
GraphPreviewLayerWidget::GraphPreviewLayerWidget(std::weak_ptr<GraphNode> p_graph_node,
                                                 QWidget                 *parent)
    : QWidget(parent), p_graph_node(p_graph_node)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(4, 2, 4, 2);
  layout->setSpacing(4);

  auto *label = new QLabel(tr("Layer:"), this);
  layout->addWidget(label);

  this->combobox = new QComboBox(this);
  this->combobox->setMinimumWidth(140);
  layout->addWidget(this->combobox, 1);

  this->update_combobox();

  this->connect(this->combobox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &GraphPreviewLayerWidget::on_combobox_changed);
}

std::string GraphPreviewLayerWidget::get_current_bg_tag() const
{
  return this->current_bg_tag;
}

GraphNode *GraphPreviewLayerWidget::get_p_graph_node()
{
  auto gno = this->p_graph_node.lock();
  return gno ? gno.get() : nullptr;
}

void GraphPreviewLayerWidget::json_from(nlohmann::json const &json)
{
  if (json.is_object())
  {
    json_safe_get(json, "current_bg_tag", this->current_bg_tag);
    this->combobox->setCurrentText(this->current_bg_tag.c_str());
    this->on_combobox_changed();
  }
}

nlohmann::json GraphPreviewLayerWidget::json_to() const
{
  nlohmann::json json;
  json["current_bg_tag"] = this->current_bg_tag;
  return json;
}

void GraphPreviewLayerWidget::on_combobox_changed()
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  this->current_bg_tag = this->combobox->currentText().toStdString();

  std::string node_label, node_id, port_id;
  if (!parse_preview_tag(this->current_bg_tag, node_label, node_id, port_id))
  {
    Q_EMIT this->bg_image_updated(gno->get_id(), QImage());
    Q_EMIT this->has_changed();
    return;
  }

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
  if (!p_node)
    return;

  QImage               image;
  std::vector<uint8_t> img;
  glm::ivec2           shape_img(256, 256);

  if (p_node->has_port<hmap::VirtualArray>(port_id))
  {
    if (auto *p_h = p_node->get_value_ref<hmap::VirtualArray>(port_id))
    {
      hmap::Array array = p_h->to_array(shape_img, p_node->cfg().cm_cpu);
      img = hmap::colorize(array, array.min(), array.max(), hmap::Cmap::TURBO, true)
                .to_img_8bit();
      image = QImage(img.data(), shape_img.x, shape_img.y, QImage::Format_RGB888).copy();
    }
  }
  else if (p_node->has_port<hmap::VirtualTexture>(port_id))
  {
    if (auto *p_tex = p_node->get_value_ref<hmap::VirtualTexture>(port_id))
    {
      if (p_tex->channels() >= 3)
      {
        hmap::Array r = p_tex->channel(0).to_array(shape_img, p_node->cfg().cm_cpu);
        hmap::Array g = p_tex->channel(1).to_array(shape_img, p_node->cfg().cm_cpu);
        hmap::Array b = p_tex->channel(2).to_array(shape_img, p_node->cfg().cm_cpu);
        image = QImage(shape_img.x, shape_img.y, QImage::Format_RGB888);
        for (int j = 0; j < shape_img.y; ++j)
          for (int i = 0; i < shape_img.x; ++i)
          {
            const int cr = std::clamp(static_cast<int>(r(i, j) * 255.f), 0, 255);
            const int cg = std::clamp(static_cast<int>(g(i, j) * 255.f), 0, 255);
            const int cb = std::clamp(static_cast<int>(b(i, j) * 255.f), 0, 255);
            image.setPixel(i, j, qRgb(cr, cg, cb));
          }
      }
    }
  }

  Q_EMIT this->bg_image_updated(gno->get_id(), image);
  Q_EMIT this->has_changed();
}

void GraphPreviewLayerWidget::update_combobox()
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  const std::string tag_bckp = this->current_bg_tag;

  const QSignalBlocker blocker(this->combobox);
  this->combobox->clear();
  this->combobox->addItem("NONE");

  bool is_current_tag_in_combo = false;

  for (const auto &[id, node] : gno->get_nodes())
  {
    for (const auto &port : node->get_ports())
    {
      const std::string dtype = port->get_data_type();
      if (dtype == typeid(hmap::VirtualArray).name() ||
          dtype == typeid(hmap::VirtualTexture).name())
      {
        const std::string tag = node->get_label() + "/" + node->get_id() + "/" +
                                port->get_label();
        this->combobox->addItem(tag.c_str());

        if (tag_bckp == tag)
          is_current_tag_in_combo = true;
      }
    }
  }

  if (is_current_tag_in_combo)
    this->combobox->setCurrentText(tag_bckp.c_str());

  this->on_combobox_changed();
}

// =====================================
// GraphManagerWidget
// =====================================
GraphManagerWidget::GraphManagerWidget(std::weak_ptr<GraphManager> p_graph_manager,
                                       QWidget                    *parent)
    : QWidget(parent), p_graph_manager(p_graph_manager)
{
  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  this->setWindowTitle(tr("Hesiod - Graph Layout Manager"));
  this->setObjectName("hsdBase");
  this->setAttribute(Qt::WA_StyledBackground);

  auto *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(6, 6, 6, 6);
  main_layout->setSpacing(6);

  // --- Split pane: Left = Global Graph Editor, Right = World Editor (CoordFrameWidget)
  auto *splitter = new GripSplitter(Qt::Horizontal, this);

  this->graph_viewer = new gngui::GraphViewer("global_graph", this);
  splitter->addWidget(new PanelFrame(this->graph_viewer));

  this->coord_frame_widget = new CoordFrameWidget(gm, this);
  splitter->addWidget(new PanelFrame(this->coord_frame_widget));

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);
  main_layout->addWidget(splitter, 1);

  // --- Bottom action bar
  auto *btn_layout = new QHBoxLayout();
  auto *new_button = new QPushButton(tr("New Graph"), this);
  auto *layout_button = new QPushButton(tr("Auto Layout"), this);
  auto *zoom_button = new QPushButton(tr("Zoom"), this);
  this->apply_button = new QPushButton(tr("Apply"), this);
  this->apply_button->setStyleSheet("QPushButton:enabled {background-color: red;}");

  btn_layout->addWidget(new_button);
  btn_layout->addWidget(layout_button);
  btn_layout->addWidget(zoom_button);
  btn_layout->addStretch(1);
  btn_layout->addWidget(this->apply_button);
  main_layout->addLayout(btn_layout);

  this->setup_reset_actions();

  // Populate initial graphs
  this->reset();

  // --- Connections: GraphViewer (Left Pane)
  this->connect(this->graph_viewer,
                &gngui::GraphViewer::connection_finished,
                this,
                [this](const std::string &id_out,
                       const std::string &port_id_out,
                       const std::string &to_in,
                       const std::string &port_id_in)
                {
                  auto gm = this->p_graph_manager.lock();
                  if (!gm)
                    return;

                  if (id_out == to_in || gm->is_reachable(to_in, id_out))
                  {
                    Logger::log()->warn("GraphManagerWidget: rejected cyclic link");
                    return;
                  }

                  if (gm->new_link(id_out, port_id_out, to_in, port_id_in))
                  {
                    this->coord_frame_widget->update_frames_z_depth();
                    gm->update(to_in);
                    Q_EMIT this->has_changed();
                  }
                });

  this->connect(this->graph_viewer,
                &gngui::GraphViewer::connection_deleted,
                this,
                [this](const std::string &id_out,
                       const std::string &port_id_out,
                       const std::string &to_in,
                       const std::string &port_id_in,
                       bool /* link_will_be_replaced */)
                {
                  auto gm = this->p_graph_manager.lock();
                  if (!gm)
                    return;

                  if (gm->remove_link(id_out, port_id_out, to_in, port_id_in))
                  {
                    this->coord_frame_widget->update_frames_z_depth();
                    gm->update(to_in);
                    Q_EMIT this->has_changed();
                  }
                });

  this->connect(this->graph_viewer,
                &gngui::GraphViewer::node_double_clicked,
                this,
                [this](const std::string &graph_id)
                { Q_EMIT this->selected_graph_changed(graph_id); });

  this->connect(this->graph_viewer,
                &gngui::GraphViewer::node_deleted,
                this,
                [this](const std::string &graph_id) { this->delete_graph(graph_id); });

  // --- Connections: Buttons & CoordFrameWidget (Right Pane)
  this->connect(this->apply_button,
                &QPushButton::pressed,
                this,
                &GraphManagerWidget::on_apply_changes);

  this->connect(new_button,
                &QPushButton::pressed,
                this,
                &GraphManagerWidget::on_new_graph_request);

  this->connect(layout_button,
                &QPushButton::pressed,
                this,
                [this]()
                {
                  auto gm = this->p_graph_manager.lock();
                  if (!gm)
                    return;

                  auto pts = gm->compute_graph_layout_sugiyama();
                  auto order = gm->get_graph_order();
                  for (size_t i = 0; i < std::min(pts.size(), order.size()); ++i)
                  {
                    if (auto *gn = this->graph_viewer->get_graphics_node_by_id(order[i]))
                      gn->setPos(pts[i].x * 300.f, pts[i].y * 200.f);
                  }
                  this->graph_viewer->zoom_to_content();
                });

  this->connect(this->coord_frame_widget,
                &CoordFrameWidget::has_changed,
                this,
                [this]() { this->set_is_dirty(true); });

  this->connect(zoom_button,
                &QPushButton::pressed,
                this->coord_frame_widget,
                &CoordFrameWidget::on_zoom_to_content);

  this->set_is_dirty(false);
  this->restore_window_state();
}

void GraphManagerWidget::add_graph_item(const std::string &id, QPointF scene_pos)
{
  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  auto *gno = gm->get_graph_ref_by_id(id);
  if (!gno)
    return;

  auto *proxy = new GraphNodeProxy(gno->get_shared(), this);
  this->graph_viewer->add_node(proxy, scene_pos, id);

  auto *preview_widget = new GraphPreviewLayerWidget(gno->get_shared(), nullptr);
  this->preview_widgets[id] = preview_widget;

  if (auto *gn = this->graph_viewer->get_graphics_node_by_id(id))
    gn->set_widget(preview_widget);

  this->connect(preview_widget,
                &GraphPreviewLayerWidget::bg_image_updated,
                this,
                [this](const std::string &gid, QImage image)
                {
                  if (auto *frame = this->coord_frame_widget->get_frame_ref(gid))
                    frame->set_background_image(image);
                });

  this->connect(preview_widget,
                &GraphPreviewLayerWidget::has_changed,
                this,
                [this]() { Q_EMIT this->has_changed(); });

  this->connect(this,
                &GraphManagerWidget::update_finished,
                preview_widget,
                [this, preview_widget]()
                {
                  const QSignalBlocker blocker(this);
                  preview_widget->on_combobox_changed();
                });
}

void GraphManagerWidget::clear()
{
  Logger::log()->trace("GraphManagerWidget::clear");

  this->preview_widgets.clear();
  this->graph_viewer->clear();
  this->coord_frame_widget->clear();
}

void GraphManagerWidget::closeEvent(QCloseEvent *event)
{
  this->save_window_state();
  QWidget::closeEvent(event);
  Q_EMIT this->window_closed();
}

void GraphManagerWidget::hideEvent(QHideEvent *event)
{
  this->save_window_state();
  QWidget::hideEvent(event);
}

void GraphManagerWidget::json_from(nlohmann::json const &json)
{
  if (json.contains("graph_viewer"))
  {
    // Restore node positions without clearing model-created nodes
    const auto &gv_json = json["graph_viewer"];
    if (gv_json.contains("nodes") && gv_json["nodes"].is_array())
    {
      for (const auto &jn : gv_json["nodes"])
      {
        const std::string id = jn.value("id", "");
        if (auto *gn = this->graph_viewer->get_graphics_node_by_id(id))
        {
          if (jn.contains("scene_position") && jn["scene_position"].is_array() &&
              jn["scene_position"].size() >= 2)
          {
            gn->setPos(jn["scene_position"][0].get<double>(),
                       jn["scene_position"][1].get<double>());
          }
        }
      }
    }
  }

  if (json.contains("frames") && json["frames"].is_object())
  {
    for (auto &[id, pw] : this->preview_widgets)
      if (pw && json["frames"].contains(id))
        pw->json_from(json["frames"][id]);
  }
}

nlohmann::json GraphManagerWidget::json_to() const
{
  nlohmann::json json;
  json["graph_viewer"] = this->graph_viewer->json_to();

  for (const auto &[id, pw] : this->preview_widgets)
    if (pw)
      json["frames"][id] = pw->json_to();

  return json;
}

void GraphManagerWidget::on_apply_changes()
{
  Logger::log()->trace("GraphManagerWidget::on_apply_changes");

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  gm->get_worker().stop();

  for (auto &[id, graph] : gm->get_graph_nodes())
  {
    QPointF origin_out;
    QPointF size_out;
    float   angle_out = 0.f;

    if (auto *frame = this->coord_frame_widget->get_frame_ref(id))
    {
      frame->get_frame_parameter(origin_out, size_out, angle_out);
      graph->set_origin(glm::vec2(origin_out.x(), origin_out.y()));
      graph->set_size(glm::vec2(size_out.x(), size_out.y()));
      graph->set_rotation_angle(angle_out);
    }
  }

  gm->update();

  this->set_is_dirty(false);
  Q_EMIT this->has_changed();
}

void GraphManagerWidget::on_new_graph_request()
{
  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  std::vector<std::string> invalid_graph_ids = gm->get_graph_order();

  QString suggestion = "graph";
  for (int k = 2; std::ranges::find(invalid_graph_ids, suggestion.toStdString()) !=
                  invalid_graph_ids.end();
       ++k)
    suggestion = QString("graph_%1").arg(k);

  StringInputDialog id_dialog("New graph",
                              "A name for the graph, its tab shows it.",
                              suggestion,
                              "Next",
                              invalid_graph_ids,
                              this);
  std::string       new_graph_id;

  if (!id_dialog.exec())
    return;
  new_graph_id = id_dialog.get_text_std_string();

  auto              config = std::make_shared<hesiod::GraphConfig>();
  GraphConfigDialog config_editor(*config, this);
  if (!config_editor.exec())
    return;

  auto graph = std::make_shared<hesiod::GraphNode>(new_graph_id, config);
  gm->add_graph_node(graph, new_graph_id);

  this->coord_frame_widget->add_frame(new_graph_id);
  this->add_graph_item(
      new_graph_id,
      QPointF(static_cast<qreal>(invalid_graph_ids.size()) * 280.0, 0.0));

  Q_EMIT this->new_graph_added();
}

void GraphManagerWidget::on_reseed(bool backward)
{
  if (auto gm = this->p_graph_manager.lock())
    gm->reseed(backward);
}

void GraphManagerWidget::reset()
{
  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  this->clear();
  this->coord_frame_widget->reset();

  const auto order = gm->get_graph_order();
  for (size_t i = 0; i < order.size(); ++i)
    this->add_graph_item(order[i], QPointF(static_cast<qreal>(i) * 280.0, 0.0));

  for (const auto &link : gm->get_links())
  {
    auto *p_from = gm->get_node(link.from);
    auto *p_to = gm->get_node(link.to);
    if (p_from && p_to)
    {
      this->graph_viewer->add_link(link.from,
                                   p_from->get_port_label(link.port_from),
                                   link.to,
                                   p_to->get_port_label(link.port_to));
    }
  }
}

void GraphManagerWidget::restore_window_state()
{
  AppContext &ctx = HSD_CTX;
  const auto &geom = ctx.app_settings.window.geom_graph_manager;

  if (geom.x <= 0 && geom.y <= 0)
  {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen)
    {
      QRect screen_geom = screen->availableGeometry();
      int   w = std::min(geom.w > 0 ? geom.w : 1024, screen_geom.width());
      int   h = std::min(geom.h > 0 ? geom.h : 768, screen_geom.height());
      int   x = screen_geom.x() + (screen_geom.width() - w) / 2;
      int   y = screen_geom.y() + (screen_geom.height() - h) / 2;
      this->setGeometry(x, y, w, h);
      return;
    }
  }

  this->setGeometry(geom.x, geom.y, geom.w, geom.h);
}

void GraphManagerWidget::save_window_state() const
{
  AppContext &ctx = HSD_CTX;
  QRect       geom = this->geometry();
  ctx.app_settings.window.geom_graph_manager.x = geom.x();
  ctx.app_settings.window.geom_graph_manager.y = geom.y();
  ctx.app_settings.window.geom_graph_manager.w = geom.width();
  ctx.app_settings.window.geom_graph_manager.h = geom.height();
}

void GraphManagerWidget::set_is_dirty(bool new_is_dirty)
{
  this->is_dirty = new_is_dirty;
  if (this->apply_button)
    this->apply_button->setEnabled(this->is_dirty);
  this->update();
}

void GraphManagerWidget::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  this->restore_window_state();
}

void GraphManagerWidget::setup_reset_actions()
{
  const std::vector<std::pair<QString, QString>> labels = {{"Location", "Alt+L"},
                                                           {"Rotation", "Alt+R"},
                                                           {"Scale", "Alt+S"}};
  for (int k = 0; k < 3; k++)
  {
    auto *action = new QAction(labels[k].first, this);
    action->setShortcut(QKeySequence(labels[k].second));
    action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    this->addAction(action);
    this->reset_actions.push_back(action);

    this->connect(action,
                  &QAction::triggered,
                  this,
                  [this, k]()
                  {
                    const auto selected = this->graph_viewer->get_selected_node_ids();
                    if (selected.empty())
                      return;

                    FrameItem *frame = this->coord_frame_widget->get_frame_ref(
                        selected.front());
                    if (!frame)
                      return;

                    if (k == 0)
                      frame->set_origin(QPointF(0.f, 0.f));
                    else if (k == 1)
                      frame->set_angle(0.f);
                    else
                      frame->set_size(QPointF(1.f, 1.f));
                  });
  }
}

void GraphManagerWidget::delete_graph(const std::string &graph_id)
{
  Logger::log()->trace("GraphManagerWidget::delete_graph: {}", graph_id);

  auto gm = this->p_graph_manager.lock();
  if (!gm)
    return;

  this->preview_widgets.erase(graph_id);
  this->graph_viewer->erase_node(graph_id);
  gm->remove_graph_node(graph_id);
  this->coord_frame_widget->remove_frame(graph_id);

  Q_EMIT this->graph_removed();
}

void GraphManagerWidget::update_combobox(const std::string &graph_id)
{
  Logger::log()->trace("GraphManagerWidget::update_combobox: {}", graph_id);

  auto it = this->preview_widgets.find(graph_id);
  if (it != this->preview_widgets.end() && it->second)
    it->second->update_combobox();
}

} // namespace hesiod
