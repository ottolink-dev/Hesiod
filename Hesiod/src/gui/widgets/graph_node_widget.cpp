/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <exception>
#include <optional>
#include <type_traits>

#include <QApplication>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/graph_editor.hpp"
#include "hesiod/gui/hesiod_node_proxy.hpp"
#include "hesiod/gui/widgets/custom_qmenu.hpp"
#include "hesiod/gui/widgets/graph_config_widgets/graph_config_dialog.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/gui/widgets/node_info_dialog.hpp"
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/gui/widgets/select_string_dialog.hpp"
#include "hesiod/gui/widgets/viewers/viewer_3d.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/legacy/legacy_converter.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/nodes/port_catalog.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{
namespace
{
// Qt event handlers report an edit failure without unwinding through the event loop.
template <typename F> auto perform_graph_edit(F &&edit) -> std::invoke_result_t<F>
{
  try
  {
    return edit();
  }
  catch (const std::exception &error)
  {
    Logger::log()->error("Graph edit failed: {}", error.what());
    if (!HSD_CTX.headless)
      HSD_APP->notify(error.what());
    if constexpr (!std::is_void_v<std::invoke_result_t<F>>)
      return {};
  }
}
} // namespace

GraphNodeWidget::GraphNodeWidget(std::weak_ptr<GraphNode> p_graph_node, QWidget *parent)
    : GraphViewer("", parent), p_graph_node(p_graph_node)
{
  Logger::log()->trace("GraphNodeWidget::GraphNodeWidget: id: {}", this->get_id());

  this->editor = std::make_unique<GraphEditor>(
      p_graph_node,
      *this,
      GraphEditor::NodePresentation{[this](const std::string &id, QPointF pos)
                                    { this->on_new_graphics_node_request(id, pos); },
                                    [this](const std::string &id)
                                    {
                                      this->last_node_created_id = id;
                                      Q_EMIT this->new_node_created(this->get_id(), id);
                                    },
                                    [this](const std::string &id)
                                    { Q_EMIT this->node_deleted(this->get_id(), id); },
                                    [this]() { Q_EMIT this->graph_edited(); }});

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  this->set_id(gno->get_id());
  this->setAttribute(Qt::WA_DeleteOnClose);

  // populate node catalog
  this->set_node_inventory(get_node_inventory());
  this->setup_connections();
}

GraphNodeWidget::~GraphNodeWidget()
{
  Logger::log()->trace("GraphNodeWidget::~GraphNodeWidget");

  // clean-up, viewer are not owned by this
  this->clear_data_viewers();
}

void GraphNodeWidget::add_import_heightmap_node(const QImage &img)
{
  perform_graph_edit(
      [&]()
      {
        auto graph = this->p_graph_node.lock();
        if (!graph)
          return;
        const auto path = HSD_CTX.project_model->get_path();
        const auto shape = graph->get_config_ref()->shape;
        this->editor->add_node(
            "ImportHeightmap",
            this->get_center(),
            [&](BaseNode &node)
            {
              const auto file = path / ("heightmapper_import_" + node.get_id() + ".png");
              save_heightmap(img, file, static_cast<float>(shape.x) / shape.y);
              node.set_value<std::filesystem::path>("fname", file);
              node.set_value<bool>("dequantize", true);
            });
      });
}

void GraphNodeWidget::add_import_texture_nodes(const std::vector<std::string> &paths)
{
  perform_graph_edit(
      [&]()
      {
        GraphEditor::Batch batch(*this->editor);
        const float        delta = HSD_CTX.app_settings.node_editor
                                .position_delta_when_duplicating_node;
        float                    y = 0.f;
        std::vector<std::string> created;
        try
        {
          for (const auto &path : paths)
          {
            created.push_back(this->editor->add_node(
                "ImportTexture",
                this->get_center() + QPointF(delta, y),
                [&](BaseNode &node)
                { node.set_value<std::filesystem::path>("fname", path); }));
            y += delta;
          }
        }
        catch (...)
        {
          this->editor->erase(created);
          throw;
        }
        batch.commit();
      });
}

void GraphNodeWidget::apply_new_config(int new_resolution)
{
  Logger::log()->trace("GraphNodeWidget::apply_new_config: res {}", new_resolution);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  GraphConfig *p_config = gno->get_config_ref();
  if (!p_config)
    return;

  if (new_resolution != p_config->shape.x || new_resolution != p_config->shape.y)
  {
    p_config->shape = {new_resolution, new_resolution};
    this->apply_new_config(*p_config);
  }
}

void GraphNodeWidget::apply_new_config(const GraphConfig &new_config)
{
  Logger::log()->trace("GraphNodeWidget::apply_new_config");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  this->backup_selected_ids();

  this->set_enabled(false);
  gno->change_config_values(new_config);
  this->set_enabled(true);

  this->reselect_backup_ids();

  Q_EMIT this->config_changed();
}

void GraphNodeWidget::automatic_node_layout()
{
  Logger::log()->trace("GraphNodeWidget::automatic_node_layout");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  std::vector<gnode::Point> points = gno->compute_graph_layout_sugiyama();

  AppContext &ctx = HSD_CTX;
  QPointF     delta = QPointF(ctx.app_settings.node_editor.auto_layout_dx,
                          ctx.app_settings.node_editor.auto_layout_dy);
  QRectF      bbox = this->get_bounding_box();
  QPointF     origin = bbox.topLeft();

  size_t k = 0;

  for (auto &[nid, _] : gno->get_nodes())
  {
    if (k > points.size() - 1)
    {
      Logger::log()->error(
          "GraphNodeWidget::automatic_node_layout: computed layout is incoherent "
          "with current graphics node layout");
      return;
    }

    QPointF scene_pos = origin +
                        QPointF(points[k].x * delta.x(), points[k].y * delta.y());
    k++;

    gngui::GraphicsNode *p_gfx_node = this->get_graphics_node_by_id(nid);

    if (p_gfx_node)
      p_gfx_node->setPos(scene_pos);
  }

  QTimer::singleShot(0, this, [this]() { this->zoom_to_content(); });
}

void GraphNodeWidget::backup_selected_ids()
{
  this->selected_ids = this->get_selected_node_ids();
}

void GraphNodeWidget::clear_all()
{
  perform_graph_edit(
      [&]()
      {
        this->clear_data_viewers();
        this->editor->clear();
        Q_EMIT this->has_been_cleared(this->get_id());
      });
}

void GraphNodeWidget::clear_data_viewers()
{
  for (auto viewer : this->data_viewers)
  {
    if (viewer)
      if (Viewer *p_viewer = dynamic_cast<Viewer *>(viewer.get()))
      {
        p_viewer->clear();
        p_viewer->deleteLater();
      }
  }
  this->data_viewers.clear();
}

void GraphNodeWidget::clear_graphic_scene()
{
  this->set_enabled(false);
  this->clear_data_viewers();
  GraphViewer::clear();
  this->set_enabled(true);
}

void GraphNodeWidget::closeEvent(QCloseEvent *event)
{
  this->clear_data_viewers();
  gngui::GraphViewer::closeEvent(event);
}

QScrollArea *GraphNodeWidget::create_attributes_scroll(QWidget *parent, QWidget *widget)
{
  auto *scroll = new QScrollArea(parent);
  scroll->setWidget(widget);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  int max_height = widget->sizeHint().height();

  if (QScreen *screen = this->screen())
  {
    int screen_h = screen->size().height();
    max_height = std::min(max_height, int(0.9 * screen_h));
  }

  scroll->setMinimumHeight(max_height);
  scroll->setMinimumWidth(widget->width() + 16);

  return scroll;
}

bool GraphNodeWidget::get_is_selecting_with_rubber_band() const
{
  return this->is_selecting_with_rubber_band;
}

GraphNode *GraphNodeWidget::get_p_graph_node()
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
  {
    Logger::log()->critical(
        "GraphNodeWidget::get_p_graph_node: model graph_node reference is a "
        "dangling ptr");
    throw std::runtime_error("dangling ptr");
  }

  return gno.get();
}

void GraphNodeWidget::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("GraphNodeWidget::json_from");
  this->clear_graphic_scene();

  nlohmann::json gui_json = convert_legacy_graph_widget_json(json);

  // Loading graphics is presentation-only; edit requests are not emitted.
  GraphViewer::json_from(gui_json);

  // viewers (skipped in headless CLI modes, e.g. --snapshot: no 3D viewer is
  // created there, so there is nothing to restore state into)
  if (!HSD_CTX.headless && json.contains("viewers") && json["viewers"].is_array())
  {
    for (const auto &viewer_json : json["viewers"])
    {
      ViewerType viewer_type = viewer_json["viewer_type"].get<ViewerType>();

      Logger::log()->trace("GraphNodeWidget::json_from: viewer_type: {}",
                           viewer_type_as_string.at(viewer_type));

      // TODO add viewer-type specific handling
      this->on_viewport_request();

      if (auto *p_viewer = dynamic_cast<Viewer3D *>(this->data_viewers.back().get()))
      {
        // defer to let OpenGL context settle. The viewer can be destroyed
        // before the timer fires - closing the viewport, or another load
        // replacing the viewers - so hold it through a QPointer and give the
        // timer a context object, otherwise this fires into freed memory
        QTimer::singleShot(0,
                           this,
                           [viewer = QPointer<Viewer3D>(p_viewer), viewer_json]()
                           {
                             if (viewer)
                               viewer->json_from(viewer_json);
                           });
      }
      else
        Logger::log()->error(
            "GraphNodeWidget::json_from: could not retrieve viewer reference");
    }
  }

  // defer
  QTimer::singleShot(0,
                     this,
                     [this]()
                     {
                       this->update();
                       this->zoom_to_content();
                     });
}

nlohmann::json GraphNodeWidget::json_import(nlohmann::json const &json, QPointF scene_pos)
{
  return perform_graph_edit([&]()
                            { return this->editor->import_nodes(json, scene_pos); });
}

nlohmann::json GraphNodeWidget::json_to() const
{
  Logger::log()->trace("GraphNodeWidget::json_to");
  nlohmann::json json = GraphViewer::json_to();

  // add the viewports
  for (auto widget : this->data_viewers)
    if (widget)
      if (auto *p_viewer = dynamic_cast<Viewer *>(widget.get()))
      {
        json["viewers"].push_back(p_viewer->json_to());
      }

  return json;
}

void GraphNodeWidget::request_connection(const gngui::LinkEndpoints &link)
{
  perform_graph_edit([&]() { this->editor->connect(link); });
}

void GraphNodeWidget::request_deletion(const std::vector<std::string>          &ids,
                                       const std::vector<gngui::LinkEndpoints> &links)
{
  perform_graph_edit([&]() { this->editor->erase(ids, links); });
}

void GraphNodeWidget::on_connection_dropped(const std::string &node_id,
                                            const std::string &port_id,
                                            QPointF /*scene_pos*/)
{
  perform_graph_edit(
      [&]()
      {
        Logger::log()->trace("GraphNodeWidget::on_connection_dropped: {}/{}",
                             node_id,
                             port_id);

        auto gno = this->p_graph_node.lock();
        if (!gno)
          return;

        BaseNode *p_node_from = gno->get_node_ref_by_id<BaseNode>(node_id);
        if (!p_node_from)
          return;

        const int from_index = p_node_from->get_port_index(port_id);
        if (from_index < 0)
          return;

        // The catalog and select_port use documentation names, not typeid names.
        const std::string dragged_type = map_type_name(
            p_node_from->get_data_type(from_index));

        const gnode::PortType dragged_dir = p_node_from->get_port_type(from_index);
        const gnode::PortType wanted_dir = (dragged_dir == gnode::PortType::OUT)
                                               ? gnode::PortType::IN
                                               : gnode::PortType::OUT;

        // Filter GraphViewer's inventory for the duration of its blocking menu.
        const std::map<std::string, std::string> full_inventory = get_node_inventory();
        const PortCatalog catalog = PortCatalog::from_documentation();

        std::map<std::string, std::string> filtered;
        for (const auto &[node_type, category] : full_inventory)
          if (catalog.is_offerable(node_type, dragged_type, wanted_dir))
            filtered[node_type] = category;

        // Fall back to the full inventory if no compatible types are documented.
        const bool use_filtered = !filtered.empty();

        if (use_filtered)
          this->set_node_inventory(filtered);

        GraphEditor::Batch batch(*this->editor);
        this->last_node_created_id.clear();
        const bool created = this->execute_new_node_context_menu();

        if (use_filtered)
          this->set_node_inventory(full_inventory);

        if (!created)
          return;

        const std::string node_to = this->last_node_created_id;
        BaseNode         *p_node_to = gno->get_node_ref_by_id<BaseNode>(node_to);

        if (!p_node_to)
        {
          Logger::log()->trace(
              "GraphNodeWidget::on_connection_dropped: p_node_to is nullptr");
          batch.commit();
          return;
        }

        const std::optional<std::string> port_to = select_port(*p_node_to,
                                                               dragged_type,
                                                               wanted_dir);

        if (!port_to)
        {
          Logger::log()->trace("GraphNodeWidget::on_connection_dropped: node '{}' has no "
                               "{} port of type {}, "
                               "leaving it unconnected",
                               node_to,
                               wanted_dir == gnode::PortType::IN ? "input" : "output",
                               dragged_type);
          batch.commit();
          return;
        }

        const bool dragged_is_output = (dragged_dir == gnode::PortType::OUT);

        const std::string id_out = dragged_is_output ? node_id : node_to;
        const std::string port_out = dragged_is_output ? port_id : *port_to;
        const std::string id_in = dragged_is_output ? node_to : node_id;
        const std::string port_in = dragged_is_output ? *port_to : port_id;

        this->request_connection({id_out, port_out, id_in, port_in});
        batch.commit();
      });
}

void GraphNodeWidget::on_graph_clear_request()
{
  Logger::log()->trace("GraphNodeWidget::on_graph_clear_request");

  QMessageBox::StandardButton reply = QMessageBox::question(
      nullptr,
      "?",
      "This will clear everything. Are you sure?",
      QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes)
    this->clear_all();
}

void GraphNodeWidget::on_graph_import_request()
{
  Logger::log()->trace("GraphNodeWidget::on_graph_import_request");

  // define import path for dialog fiel
  std::filesystem::path path = this->last_import_path.empty()
                                   ? HSD_CTX.project_model->get_path()
                                   : this->last_import_path;

  this->set_enabled(false);
  QString load_fname = QFileDialog::getOpenFileName(this,
                                                    "Load...",
                                                    path.string().c_str(),
                                                    "Hesiod files (*.hsd)");
  this->set_enabled(true);

  if (!load_fname.isNull() && !load_fname.isEmpty())
  {
    std::string    fname = load_fname.toStdString();
    nlohmann::json json = json_from_file(fname);

    // retrieve the number of graph
    size_t n_graph = json["graph_manager"]["graph_nodes"].size();

    std::vector<std::string> graph_id_list = {};
    for (auto [key, _] : json["graph_manager"]["graph_nodes"].items())
      graph_id_list.push_back(key);

    if (n_graph < 1)
    {
      Logger::log()->warn("GraphNodeWidget::on_graph_import_request: no graph to import");
      return;
    }

    std::string import_graph_id;

    if (n_graph == 1)
    {
      import_graph_id = graph_id_list.back();
    }
    else
    {
      SelectStringDialog dialog(graph_id_list, "Select the graph layer to import:");

      if (dialog.exec() != QDialog::Accepted)
      {
        Logger::log()->trace("GraphNodeWidget::on_graph_import_request: aborted");
        return;
      }

      import_graph_id = dialog.selected_value();
    }

    Logger::log()->trace(
        "GraphNodeWidget::on_graph_import_request: importing {} from file {}",
        import_graph_id,
        fname);

    // build a json file that can be imported, see GraphNodeWidget::json_import
    nlohmann::json json_imp;
    json_imp = json["graph_tabs_widget"]["graph_node_widgets"][import_graph_id];

    // add node settings
    for (auto &json_node : json_imp["nodes"])
    {
      const std::string node_id = json_node["id"];

      // retrieve in the model section the corresponding node and
      // its parameters
      for (auto &json_node_model :
           json["graph_manager"]["graph_nodes"][import_graph_id]["nodes"])
      {
        if (json_node_model["id"] == node_id)
          json_node["settings"] = json_node_model;
      }
    }

    nlohmann::json json_mod = this->json_import(json_imp);

    // set selection on copied nodes
    this->deselect_all();

    for (auto &json_node : json_mod["nodes"])
    {
      const std::string node_id = json_node["id"].get<std::string>();
      QTimer::singleShot(0,
                         this,
                         [this, node_id]()
                         {
                           if (auto *node = this->get_graphics_node_by_id(node_id))
                             node->setSelected(true);
                         });
    }

    this->last_import_path = std::filesystem::path(fname).parent_path();
  }
}

void GraphNodeWidget::on_graph_new_request()
{
  Logger::log()->trace("GraphNodeWidget::on_graph_new_request");
  this->clear_all();
}

void GraphNodeWidget::on_graph_reload_request()
{
  Logger::log()->trace("GraphNodeWidget::on_graph_reload_request");
  this->update_graph_model();
}

void GraphNodeWidget::on_graph_settings_request()
{
  Logger::log()->trace("GraphNodeWidget::on_graph_settings_request");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // work on a copy of the model configuration before
  // apllying modifications
  GraphConfig       new_config = *gno->get_config_ref();
  GraphConfigDialog model_config_editor(new_config);

  int ret = model_config_editor.exec();

  if (ret)
    this->apply_new_config(new_config);
}

void GraphNodeWidget::on_new_graphics_node_request(const std::string &node_id,
                                                   QPointF            scene_pos)
{
  // Also used when loading a scene for nodes already present in the model.
  Logger::log()->trace("GraphNodeWidget::on_new_graphics_node_request: {} {},{}",
                       node_id,
                       scene_pos.x(),
                       scene_pos.y());

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
  if (!p_node)
  {
    Logger::log()->warn(
        "GraphNodeWidget::on_new_graphics_node_request: model node '{}' not found",
        node_id);
    return;
  }
  auto *p_proxy = new HesiodNodeProxy(p_node->get_shared(), this);
  auto *widget = node_widget_factory(p_node->get_label(), p_node->get_shared(), this);

  this->add_node(p_proxy, scene_pos, node_id);
  if (auto *gn = this->get_graphics_node_by_id(node_id))
    gn->set_widget(widget);
}

std::string GraphNodeWidget::on_new_node_request(const std::string &node_type,
                                                 QPointF            scene_pos)
{
  const auto id = perform_graph_edit(
      [&]() { return this->editor->add_node(node_type, scene_pos); });
  // A drag-to-create gesture needs the ID before its outer batch commits.
  this->last_node_created_id = id;
  return id;
}

std::string GraphNodeWidget::on_new_node_request_chain(const std::string &node_type)
{
  const auto ids = this->get_selected_node_ids();
  if (ids.empty())
  {
    HSD_APP->notify("Select a node before inserting a new node.");
    return {};
  }
  auto *graphics = this->get_graphics_node_by_id(ids.back());
  if (!graphics)
    return {};
  const auto position = graphics->pos() +
                        QPointF(HSD_CTX.app_settings.node_editor
                                    .position_delta_when_duplicating_node,
                                0.f);
  const auto id = perform_graph_edit(
      [&]() { return this->editor->insert_node(ids.back(), node_type, position); });
  if (!id.empty())
  {
    this->deselect_all();
    this->set_node_as_selected(id);
  }
  return id;
}

std::string GraphNodeWidget::on_new_node_request_replace(const std::string &node_type)
{
  const auto ids = this->get_selected_node_ids();
  if (ids.empty())
  {
    HSD_APP->notify("Select a node before replacing it.");
    return {};
  }
  const auto id = perform_graph_edit(
      [&]() { return this->editor->replace_node(ids.back(), node_type); });
  if (!id.empty())
  {
    this->deselect_all();
    this->set_node_as_selected(id);
  }
  return id;
}

void GraphNodeWidget::on_node_info(const std::string &node_id)
{
  Logger::log()->trace("GraphNodeWidget::on_node_info, node {}", node_id);

  if (!node_id.empty())
  {
    NodeInfoDialog *dialog = new NodeInfoDialog(this, node_id, this);
    dialog->show();
  }
}

void GraphNodeWidget::on_node_pinned(const std::string &node_id, bool state)
{
  Logger::log()->trace("GraphNodeWidget::on_node_pinned, node {}", node_id);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  this->unpin_nodes();

  if (!node_id.empty())
  {
    // TODO make a dedicated GraphViewer method
    BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
    if (!p_node)
      return;

    gngui::GraphicsNode *p_gx_node = this->get_graphics_node_by_id(node_id);
    if (!p_gx_node)
      return;

    p_gx_node->set_is_node_pinned(state);
  }
}

void GraphNodeWidget::on_node_reload_request(const std::string &node_id)
{
  Logger::log()->trace("GraphNodeWidget::on_node_reload_request, node [{}]", node_id);
  this->update_graph_model(node_id);
}

void GraphNodeWidget::on_node_right_clicked(const std::string &node_id, QPointF scene_pos)
{
  Logger::log()->trace("GraphNodeWidget::on_node_right_clicked: id {}", node_id);

  // only show custom menu if clicked inside the top area of the node,
  // outside the embedded widget
  {
    gngui::GraphicsNode *p_gx_node = this->get_graphics_node_by_id(node_id);
    if (!p_gx_node)
      return;

    QPointF item_pos = scene_pos - p_gx_node->scenePos();
    if (item_pos.y() >= p_gx_node->get_geometry().widget_pos.y())
      return;
  }

  // settings widget
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  QWidget *attr_widget = new NodeAttributesWidget(gno->get_shared(),
                                                  node_id,
                                                  /* p_graph_node_widget */ this,
                                                  /* add_toolbar */ true,
                                                  /* parent */ this);
  if (!attr_widget)
    return;

  // create menu
  CustomQMenu *menu = new CustomQMenu();

  {
    auto *scroll = this->create_attributes_scroll(menu, attr_widget);
    auto *scroll_action = new QWidgetAction(menu);
    scroll_action->setDefaultWidget(scroll);
    menu->addAction(scroll_action);
  }

  menu->setWindowFlags(menu->windowFlags() | Qt::Popup);
  menu->setAttribute(Qt::WA_NoMousePropagation, false);
  menu->popup(QCursor::pos());
}

void GraphNodeWidget::on_nodes_copy_request(const std::vector<std::string> &id_list,
                                            const std::vector<QPointF> &scene_pos_list)
{
  Logger::log()->trace("GraphNodeWidget::on_nodes_copy_request");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // dump the nodes data into the copy buffer (for the node positions,
  // save the node position relative to the mouse cursor)
  this->json_copy_buffer.clear();

  QPoint  mouse_view_pos = this->mapFromGlobal(QCursor::pos());
  QPointF mouse_scene_pos = this->mapToScene(mouse_view_pos);

  // dump to a json with a structure that can be read by
  // GraphViewer::json_from

  json_copy_buffer["nodes"] = nlohmann::json::array();

  for (size_t k = 0; k < id_list.size(); k++)
  {
    nlohmann::json json_node;

    gngui::GraphicsNode *p_gfx_node = this->get_graphics_node_by_id(id_list[k]);
    json_node = p_gfx_node->json_to();

    // override absolute positions with relative positions
    QPointF delta = scene_pos_list[k] - mouse_scene_pos;
    json_node["scene_position.x"] = delta.x();
    json_node["scene_position.y"] = delta.y();

    // add attribute settings to set them back when pasting (not
    // required by GraphViewer::json_from)
    BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(id_list[k]);
    json_node["settings"] = p_node->json_to();

    this->json_copy_buffer["nodes"].push_back(json_node);
  }

  // backup links between copied nodes
  this->json_copy_buffer["links"] = nlohmann::json::array();

  for (auto &link : gno->get_links())
  {
    // only keep a link if both nodes are in copy buffer
    if (contains(id_list, link.from) && contains(id_list, link.to))
    {
      nlohmann::json json_link;

      gnode::Node *p_from = gno->get_node_ref_by_id(link.from);
      gnode::Node *p_to = gno->get_node_ref_by_id(link.to);

      json_link["node_out_id"] = link.from;
      json_link["node_in_id"] = link.to;
      json_link["port_out_id"] = p_from->get_port_label(link.port_from);
      json_link["port_in_id"] = p_to->get_port_label(link.port_to);

      this->json_copy_buffer["links"].push_back(json_link);
    }
  }

  Q_EMIT this->copy_buffer_has_changed(this->json_copy_buffer);
}

void GraphNodeWidget::on_nodes_duplicate_request(
    const std::vector<std::string> &id_list,
    const std::vector<QPointF>     &scene_pos_list)
{
  Logger::log()->trace("GraphNodeWidget::on_nodes_duplicate_request");

  std::vector<QPointF> scene_pos_shifted = {};

  AppContext &ctx = HSD_CTX;
  float       dx = ctx.app_settings.node_editor.position_delta_when_duplicating_node;
  QPointF     delta = QPointF(dx, dx);

  for (auto &p : scene_pos_list)
    scene_pos_shifted.push_back(p + delta);

  this->on_nodes_copy_request(id_list, scene_pos_shifted);
  this->on_nodes_paste_request();
}

void GraphNodeWidget::on_nodes_paste_request()
{
  Logger::log()->trace("GraphNodeWidget::on_nodes_paste_request");

  if (!this->json_copy_buffer.is_object())
    return;

  QPointF mouse_scene_pos = this->get_mouse_scene_pos();

  // returned json contains modified node IDs
  nlohmann::json json_mod = this->json_import(this->json_copy_buffer, mouse_scene_pos);

  // set selection on copied nodes
  this->deselect_all();

  for (auto &json_node : json_mod["nodes"])
  {
    const std::string    node_id = json_node["id"].get<std::string>();
    gngui::GraphicsNode *p_gfx_node = this->get_graphics_node_by_id(node_id);
    if (p_gfx_node)
      p_gfx_node->setSelected(true);
  }
}

void GraphNodeWidget::on_viewport_request()
{
  Logger::log()->trace("GraphNodeWidget::on_viewport_request");

  // no independent 3D viewer window in headless CLI modes (e.g. --snapshot):
  // it would receive paint events and crash without a real GUI window context
  if (HSD_CTX.headless)
    return;

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  for (auto &[id, _] : gno->get_nodes())
  {
    gngui::GraphicsNode *p_gfx = this->get_graphics_node_by_id(id);
    if (!p_gfx)
    {
      Logger::log()->critical("GraphNodeWidget::on_graph_settings_request: GraphicsNode "
                              "ref is nullptr for id {}",
                              id);
    }
  }

  this->data_viewers.push_back(new Viewer3D(this)); // no parent, independant window
  this->data_viewers.back()->show();
  Viewer *p_viewer = dynamic_cast<Viewer *>(this->data_viewers.back().get());

  // remove the widget from the widget list if it is closed
  this->connect(p_viewer,
                &Viewer::widget_close,
                [this, p_viewer]()
                {
                  std::erase_if(this->data_viewers,
                                [p_viewer](QWidget *ptr) { return ptr == p_viewer; });
                });

  // set data of the currently selected node, if any
  std::vector<std::string> selected_ids = this->get_selected_node_ids();

  if (selected_ids.size())
    p_viewer->on_node_selected(selected_ids.back());
}

void GraphNodeWidget::reselect_backup_ids()
{
  QTimer::singleShot(
      0,
      this,
      [this]()
      {
        for (size_t k = 0; k < this->selected_ids.size(); ++k)
        {
          const std::string nid = this->selected_ids[this->selected_ids.size() - 1 - k];
          if (gngui::GraphicsNode *p_gfx_node = this->get_graphics_node_by_id(nid))
            p_gfx_node->setSelected(true);
        }
        this->selected_ids.clear();
      });
}

void GraphNodeWidget::set_json_copy_buffer(nlohmann::json const &new_json_copy_buffer)
{
  this->json_copy_buffer = new_json_copy_buffer;
}

void GraphNodeWidget::setup_connections()
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // global actions
  this->connect(this,
                &gngui::GraphViewer::graph_automatic_node_layout_request,
                this,
                &GraphNodeWidget::automatic_node_layout);

  this->connect(this,
                &gngui::GraphViewer::graph_clear_request,
                this,
                &GraphNodeWidget::on_graph_clear_request);

  this->connect(this,
                &gngui::GraphViewer::graph_import_request,
                this,
                &GraphNodeWidget::on_graph_import_request);

  this->connect(this,
                &gngui::GraphViewer::graph_new_request,
                this,
                &GraphNodeWidget::on_graph_new_request);

  this->connect(this,
                &gngui::GraphViewer::graph_reload_request,
                this,
                &GraphNodeWidget::on_graph_reload_request);

  this->connect(this,
                &gngui::GraphViewer::graph_settings_request,
                this,
                &GraphNodeWidget::on_graph_settings_request);

  this->connect(this,
                &gngui::GraphViewer::rubber_band_selection_started,
                [this]() { this->is_selecting_with_rubber_band = true; });

  this->connect(this,
                &gngui::GraphViewer::rubber_band_selection_finished,
                [this]() { this->is_selecting_with_rubber_band = false; });

  // GraphViewer -> GraphNodeWidget
  this->connect(this,
                &gngui::GraphViewer::connection_dropped,
                this,
                &GraphNodeWidget::on_connection_dropped);

  this->connect(this,
                &gngui::GraphViewer::new_graphics_node_request,
                this,
                &GraphNodeWidget::on_new_graphics_node_request);

  this->connect(this,
                &gngui::GraphViewer::new_node_request,
                this,
                &GraphNodeWidget::on_new_node_request);

  this->connect(this,
                &gngui::GraphViewer::node_reload_request,
                this,
                &GraphNodeWidget::on_node_reload_request);

  this->connect(this,
                &gngui::GraphViewer::node_right_clicked,
                this,
                &GraphNodeWidget::on_node_right_clicked);

  this->connect(this,
                &gngui::GraphViewer::nodes_copy_request,
                this,
                &GraphNodeWidget::on_nodes_copy_request);

  this->connect(this,
                &gngui::GraphViewer::nodes_duplicate_request,
                this,
                &GraphNodeWidget::on_nodes_duplicate_request);

  this->connect(this,
                &gngui::GraphViewer::nodes_paste_request,
                this,
                &GraphNodeWidget::on_nodes_paste_request);

  // viewers
  this->connect(this,
                &gngui::GraphViewer::viewport_request,
                this,
                &GraphNodeWidget::on_viewport_request);

  // GraphNodeWidget -> QApplication
  this->connect(this,
                &GraphNodeWidget::update_started,
                this,
                []() { QApplication::setOverrideCursor(Qt::WaitCursor); });

  this->connect(this,
                &GraphNodeWidget::update_finished,
                this,
                []() { QApplication::restoreOverrideCursor(); });

  // GraphNodeWidget -> GFX node
  this->connect(this,
                &GraphNodeWidget::compute_finished,
                this,
                [this](const std::string &node_id)
                {
                  if (HSD_CTX.app_settings.interface.enable_node_settings_in_node_body)
                  {
                    // force update of the graphics node to update the node settings
                    // content
                    gngui::GraphicsNode *p_gfx_node = this->get_graphics_node_by_id(
                        node_id);
                    if (p_gfx_node)
                      p_gfx_node->update();
                  }
                });

  // GraphNode
  gno->update_started = [safe_this = QPointer(this)]()
  {
    if (safe_this)
      Q_EMIT safe_this->update_started();
  };

  gno->update_finished = [safe_this = QPointer(this)]()
  {
    if (safe_this)
      Q_EMIT safe_this->update_finished();
  };

  gno->compute_started = [safe_this = QPointer(this)](const std::string &node_id)
  {
    if (safe_this)
      Q_EMIT safe_this->compute_started(node_id);
  };

  gno->compute_finished = [safe_this = QPointer(this)](const std::string &node_id)
  {
    if (safe_this)
      Q_EMIT safe_this->compute_finished(node_id);
  };
}

void GraphNodeWidget::update_graph_model(const std::vector<std::string> &node_ids)
{
  perform_graph_edit([&]() { this->editor->request_update(node_ids); });
}

void GraphNodeWidget::update_graph_model(const std::string &node_id)
{
  this->update_graph_model(std::vector<std::string>{node_id});
}

} // namespace hesiod
