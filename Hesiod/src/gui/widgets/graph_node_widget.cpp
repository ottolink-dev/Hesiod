/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <exception>

#include <QApplication>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "attributes/widgets/abstract_widget.hpp"
#include "attributes/widgets/attributes_widget.hpp"
#include "attributes/widgets/bool_widget.hpp"
#include "attributes/widgets/filename_widget.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/custom_qmenu.hpp"
#include "hesiod/gui/widgets/graph_config_dialog.hpp"
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
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

GraphNodeWidget::GraphNodeWidget(std::weak_ptr<GraphNode> p_graph_node, QWidget *parent)
    : GraphViewer("", parent), p_graph_node(p_graph_node)
{
  Logger::log()->trace("GraphNodeWidget::GraphNodeWidget: id: {}", this->get_id());

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
  Logger::log()->trace("GraphNodeWidget::add_import_heightmap_node");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // --- Crop and save heightmap file

  const std::filesystem::path path = HSD_CTX.project_model->get_path();
  const glm::ivec2            model_shape = gno->get_config_ref()->shape;

  float aspect_ratio = model_shape.x / model_shape.y;

  // --- Create new import node

  Logger::log()->trace("GraphNodeWidget::add_import_heightmap_node: creating new node");

  // create both model and graphic nodes
  std::string node_id = this->on_new_node_request("ImportHeightmap", this->get_center());

  // setup attributes
  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);

  if (p_node)
  {
    // create filename and save image
    std::filesystem::path fpath = path / ("heightmapper_import_" + node_id + ".png");
    save_heightmap(img, fpath, aspect_ratio);

    // adjust node parameter accordingly
    {
      auto *p_attr = p_node->get_attributes_ref()
                         ->at("fname")
                         ->get_ref<attr::FilenameAttribute>();
      p_attr->set_value(fpath.string());
    }

    {
      auto *p_attr = p_node->get_attributes_ref()
                         ->at("dequantize")
                         ->get_ref<attr::BoolAttribute>();
      p_attr->set_value(true);
    }

    p_node->compute();
  }
  else
  {
    Logger::log()->error(
        "GraphNodeWidget::add_import_heightmap_node: dangling ptr for node_id {}",
        node_id);
  }
}

void GraphNodeWidget::add_import_texture_nodes(
    const std::vector<std::string> &texture_paths)
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  float dx = HSD_CTX.app_settings.node_editor.position_delta_when_duplicating_node;
  float dy = 0.f;

  for (auto &fname : texture_paths)
  {
    Logger::log()->trace("GraphNodeWidget::add_import_texture_nodes: {}", fname);

    // create both model and graphic nodes
    QPointF     pos = this->get_center() + QPointF(dx, dy);
    std::string node_id = this->on_new_node_request("ImportTexture", pos);

    // setup attributes
    BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
    if (p_node)
    {
      auto *p_attr = p_node->get_attributes_ref()
                         ->at("fname")
                         ->get_ref<attr::FilenameAttribute>();
      p_attr->set_value(fname);

      p_node->compute();
    }
    else
    {
      Logger::log()->error(
          "GraphNodeWidget::add_import_texture_nodes: dangling ptr for node_id {}",
          node_id);
    }

    dy += dx;
  }
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

  QTimer::singleShot(0, [this]() { this->zoom_to_content(); });
}

void GraphNodeWidget::backup_selected_ids()
{
  this->selected_ids = this->get_selected_node_ids();
}

void GraphNodeWidget::clear_all()
{
  this->clear_graphic_scene();

  Q_EMIT this->has_been_cleared(this->get_id());
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

bool GraphNodeWidget::is_graph_model_updates_blocked() const
{
  return this->block_graph_model_updates;
}

void GraphNodeWidget::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("GraphNodeWidget::json_from");
  this->clear_graphic_scene();

  // the graph model loading and updating is taken care of by
  // GraphNode (model) and does not need to be updated again when the
  // graphics object are recreated (and are going to trigger signals
  // requesting some model updates...)
  this->set_block_graph_model_updates(true);
  GraphViewer::json_from(json);
  this->set_block_graph_model_updates(false);

  // viewers
  if (json.contains("viewers") && json["viewers"].is_array())
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
        // defer to let OpenGL context settle
        QTimer::singleShot(0,
                           [p_viewer, viewer_json]()
                           { p_viewer->json_from(viewer_json); });
      }
      else
        Logger::log()->error(
            "GraphNodeWidget::json_from: could not retrieve viewer reference");
    }
  }

  // defer
  QTimer::singleShot(0,
                     [this]()
                     {
                       this->update();
                       this->zoom_to_content();
                     });
}

nlohmann::json GraphNodeWidget::json_import(nlohmann::json const &json, QPointF scene_pos)
{
  // import used when copy/pasting only
  Logger::log()->trace("GraphNodeWidget::json_import");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return nlohmann::json();

  // work on a copy of the json to modify the node IDs and return it
  nlohmann::json json_copy = json;

  // nodes
  if (!json_copy["nodes"].is_null())
  {
    // storage of id correspondance storage between original node and it copied version
    std::map<std::string, std::string> copy_id_map = {};

    for (auto &json_node : json_copy["nodes"])
    {
      QPointF pos = scene_pos;
      QPointF delta = QPointF(json_node["scene_position.x"],
                              json_node["scene_position.y"]);

      // create both model and graphic nodes
      std::string node_id = this->on_new_node_request(json_node["caption"], pos + delta);

      // use this new node id and backup it for links
      copy_id_map[json_node["id"].get<std::string>()] = node_id;
      json_node["id"] = node_id;

      // setup attributes
      BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
      p_node->json_from(json_node["settings"]);
      p_node->set_id(node_id);
    }

    // links
    if (!json_copy["links"].is_null())
      for (auto &json_link : json_copy["links"])
      {
        std::string node_id_from = json_link["node_out_id"].get<std::string>();
        std::string node_id_to = json_link["node_in_id"].get<std::string>();

        node_id_from = copy_id_map.at(node_id_from);
        node_id_to = copy_id_map.at(node_id_to);

        std::string port_out_id = json_link["port_out_id"];
        std::string port_in_id = json_link["port_in_id"];

        // add graphics link
        this->add_link(node_id_from, port_out_id, node_id_to, port_in_id);

        // add model link
        gno->new_link(node_id_from, port_out_id, node_id_to, port_in_id);
      }

    this->update_graph_model();
  }

  return json_copy;
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

void GraphNodeWidget::on_connection_deleted(const std::string &id_out,
                                            const std::string &port_id_out,
                                            const std::string &id_in,
                                            const std::string &port_id_in,
                                            bool               prevent_graph_update)
{
  Logger::log()->trace("GraphNodeWidget::on_connection_deleted, {}/{} -> {}/{}",
                       id_out,
                       port_id_out,
                       id_in,
                       port_id_in);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  Logger::log()->debug("BLOCKED? {}",
                       this->is_graph_model_updates_blocked() ? "TRUE" : "FALSE");

  this->set_enabled(false);

  // see GraphNodeWidget::on_node_deleted
  QCoreApplication::processEvents();

  gno->remove_link(id_out, port_id_out, id_in, port_id_in);

  // see GraphNodeWidget::on_node_deleted
  QCoreApplication::processEvents();

  if (!prevent_graph_update)
    this->update_graph_model(id_in);

  this->set_enabled(true);
}

void GraphNodeWidget::on_connection_dropped(const std::string &node_id,
                                            const std::string &port_id,
                                            QPointF /*scene_pos*/)
{
  Logger::log()->trace("GraphNodeWidget::on_connection_dropped: {}/{}", node_id, port_id);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  bool ret = this->execute_new_node_context_menu();

  // if a node has indeed being created, arbitrarily connect the first
  // output with the same type has the output from which the link has
  // been emitted
  if (ret)
  {
    Logger::log()->trace("GraphNodeWidget::on_connection_dropped: auto-connecting nodes");

    const std::string node_to = this->last_node_created_id;

    BaseNode *p_node_from = gno->get_node_ref_by_id<BaseNode>(node_id);
    BaseNode *p_node_to = gno->get_node_ref_by_id<BaseNode>(node_to);

    if (p_node_to)
    {
      int         from_port_index = p_node_from->get_port_index(port_id);
      std::string from_type = p_node_from->get_data_type(from_port_index);

      Logger::log()->trace("GraphNodeWidget::on_connection_dropped: from_type: {}",
                           from_type);

      // connect to the first input that has the same type, if any
      for (int k = 0; k < p_node_to->get_nports(); ++k)
      {
        std::string to_type = p_node_to->get_data_type(k);

        if (to_type == from_type)
        {
          std::string port_to_id = p_node_to->get_port_label(k);

          // GUI
          this->add_link(node_id, port_id, node_to, port_to_id);

          // model
          gno->new_link(node_id, port_id, node_to, port_to_id);
          gno->update(node_to);

          break;
        }
      }
    }
    else
    {
      Logger::log()->trace(
          "GraphNodeWidget::on_connection_dropped: p_node_to is nullptr");
    }
  }
}

void GraphNodeWidget::on_connection_finished(const std::string &id_out,
                                             const std::string &port_id_out,
                                             const std::string &id_in,
                                             const std::string &port_id_in)
{
  Logger::log()->trace("GraphNodeWidget::on_connection_finished, {}/{} -> {}/{}",
                       id_out,
                       port_id_out,
                       id_in,
                       port_id_in);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  gno->new_link(id_out, port_id_out, id_in, port_id_in);

  this->update_graph_model(id_in);
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
      const std::string    node_id = json_node["id"].get<std::string>();
      gngui::GraphicsNode *p_gfx_node = this->get_graphics_node_by_id(node_id);

      // Qt mystery, this needs to be delayed to be effective
      QTimer::singleShot(0,
                         [p_gfx_node]()
                         {
                           if (p_gfx_node)
                             p_gfx_node->setSelected(true);
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
  // GraphicsNodes cannot generated by the GraphViewer instance by
  // itself, it is outsourced to the outer nodes manager (this
  // class). This slot respond to a request for the creation of a
  // GraphicsNodes (only). This is different from
  // GraphNodeWidget::on_new_node_request which generates both the
  // model and the GUI nodes...

  // This one is actually used for serialization, when the graph
  // viewer requests the creation of a graphics node while the base
  // node has aldready been created when the GraphNode has been
  // deserialized

  Logger::log()->trace("GraphNodeWidget::on_new_graphics_node_request: {} {},{}",
                       node_id,
                       scene_pos.x(),
                       scene_pos.y());

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
  auto     *p_proxy = new gngui::TypedNodeProxy<BaseNode>(p_node->get_shared());
  auto *widget = node_widget_factory(p_node->get_caption(), p_node->get_shared(), this);

  this->add_node(p_proxy, scene_pos, node_id);
  this->get_graphics_node_by_id(node_id)->set_widget(widget);
}

std::string GraphNodeWidget::on_new_node_request(const std::string &node_type,
                                                 QPointF            scene_pos)
{
  Logger::log()->trace("GraphNodeWidget::on_new_node_request: node_type {}", node_type);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return "";

  if (node_type == "")
    return "";

  // add control node (compute)
  std::string node_id = gno->add_node(node_type);

  // add corresponding graphics node (GUI)
  this->on_new_graphics_node_request(node_id, scene_pos);

  Q_EMIT this->new_node_created(this->get_id(), node_id);

  this->last_node_created_id = node_id;

  return node_id;
}

std::string GraphNodeWidget::on_new_node_request_chain(const std::string &node_type)
{
  Logger::log()->trace("GraphNodeWidget::on_new_node_request_chain: node_type {}",
                       node_type);

  // --- Safeguards

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return "";

  if (node_type == "")
    return "";

  // get current node selection
  std::vector<std::string> selected_ids = this->get_selected_node_ids();

  // empty selection => skip
  if (selected_ids.empty())
  {
    HSD_APP->notify("Select a node before inserting a new node.");
    return "";
  }

  // --- Backup selected node connections

  const std::string selected_id = selected_ids.back();

  // position
  gngui::GraphicsNode *p_gx_node = this->get_graphics_node_by_id(selected_id);
  if (!p_gx_node)
  {
    Logger::log()->error(
        "GraphNodeWidget::on_new_node_request_replace: p_gx_node is nullptr");
    return "";
  }
  QPointF node_pos = p_gx_node->pos();

  float dx = HSD_CTX.app_settings.node_editor.position_delta_when_duplicating_node;
  node_pos = node_pos + QPointF(dx, 0.f);

  // backup links
  std::vector<gnode::LinkView> link_views = gno->get_link_views(selected_id);

  // --- BLOCK model updates

  this->set_block_graph_model_updates(true);

  // --- Delete downstream links of selected node

  for (const auto &data : link_views)
  {
    if (data.from == selected_id)
    {
      // graphics object first and then the model link
      this->remove_link(data.from, data.port_from, data.to, data.port_to);
      // gno->remove_link(data.from, data.port_from, data.to, data.port_to);
    }
  }

  // --- Create new node

  this->deselect_all();
  std::string new_id = this->on_new_node_request(node_type, node_pos);
  this->set_node_as_selected(new_id);

  // --- Recreate the links if possible

  int link_creation_count = 0;

  for (const auto &data : link_views)
  {
    // change only the output links of the selected node
    if (data.from != selected_id)
      continue;

    gnode::Node *p_new_node = gno->get_node_ref_by_id(new_id);

    // reconnect selected_id (output) => new_id (input)
    {
      std::string from = data.from;
      std::string to = new_id;

      // check the port exists
      int port_id = p_new_node->get_port_index(data.port_label_to);
      if (port_id < 0)
        continue;

      this->add_link(from, data.port_label_from, to, data.port_label_to);
      gno->new_link(from, data.port_label_from, to, data.port_label_to);
      link_creation_count++;
    }

    // connect new_id (output) => some downstream node (input)
    {
      std::string from = new_id;
      std::string to = data.to;

      // check the port exists
      int port_id = p_new_node->get_port_index(data.port_label_from);
      if (port_id < 0)
        continue;

      this->add_link(from, data.port_label_from, to, data.port_label_to);
      gno->new_link(from, data.port_label_from, to, data.port_label_to);
      link_creation_count++;
    }
  }

  // if no link has been created, try to connect the first output of
  // 'selected_id' to the first input of 'new_id'
  if (link_creation_count == 0)
  {
    BaseNode *p_bnode_from = gno->get_node_ref_by_id<BaseNode>(selected_id);
    BaseNode *p_bnode_to = gno->get_node_ref_by_id<BaseNode>(new_id);

    if (p_bnode_from && p_bnode_to)
    {
      // 1st outlet
      int kfrom = -1;

      for (int k = 0; k < p_bnode_from->get_nports(); ++k)
        if (p_bnode_from->get_port_type(k) == gngui::PortType::OUT)
        {
          kfrom = k;
          break;
        }

      // 1st inlet
      if (kfrom != -1)
      {
        std::string data_type = p_bnode_from->get_data_type(kfrom);
        int         kto = -1;
        for (int k = 0; k < p_bnode_to->get_nports(); ++k)
          if (p_bnode_to->get_port_type(k) == gngui::PortType::IN &&
              p_bnode_to->get_data_type(k) == data_type)
          {
            kto = k;
            break;
          }

        if (kto != -1)
        {
          std::string port_label_from = p_bnode_from->get_port_label(kfrom);
          std::string port_label_to = p_bnode_to->get_port_label(kto);

          this->add_link(selected_id, port_label_from, new_id, port_label_to);
          gno->new_link(selected_id, port_label_from, new_id, port_label_to);
        }
      }
    }
  }

  // --- UNBLOCK model updates

  this->set_block_graph_model_updates(false);

  // --- Update and exit

  this->update_graph_model(selected_id);
  return new_id;
}

std::string GraphNodeWidget::on_new_node_request_replace(const std::string &node_type)
{
  Logger::log()->trace("GraphNodeWidget::on_new_node_request_replace: node_type {}",
                       node_type);

  // --- Safeguards

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return "";

  if (node_type == "")
    return "";

  // get current node selection
  std::vector<std::string> selected_ids = this->get_selected_node_ids();

  // empty selection => skip
  if (selected_ids.empty())
  {
    HSD_APP->notify("Select a node before replacing it.");
    return "";
  }

  // --- Backup selected node connections

  const std::string selected_id = selected_ids.back();

  // position
  gngui::GraphicsNode *p_gx_node = this->get_graphics_node_by_id(selected_id);
  if (!p_gx_node)
  {
    Logger::log()->error(
        "GraphNodeWidget::on_new_node_request_replace: p_gx_node is nullptr");
    return "";
  }
  const QPointF node_pos = p_gx_node->pos();

  // backup links
  std::vector<gnode::LinkView> link_views = gno->get_link_views(selected_id);

  // --- BLOCK model updates

  this->set_block_graph_model_updates(true);

  // --- Remove selected node

  this->remove_node(selected_id);             // graphics object first
  this->on_node_deleted_request(selected_id); // then propagate

  // --- Create new node

  this->deselect_all();
  std::string new_id = this->on_new_node_request(node_type, node_pos);
  this->set_node_as_selected(new_id);

  // --- Recreate the links if possible

  for (const auto &data : link_views)
  {
    // replace former ID by new one
    std::string from = (data.from == selected_id) ? new_id : data.from;
    std::string to = (data.to == selected_id) ? new_id : data.to;

    // check if the port to be connected actually exists on the new
    // node before continuing
    std::string port_label = (from == new_id) ? data.port_label_from : data.port_label_to;
    gnode::Node *p_new_node = gno->get_node_ref_by_id(new_id);
    int          port_id = p_new_node->get_port_index(port_label);

    if (port_id < 0)
      continue;

    // add graphics link and then model link
    this->add_link(from, data.port_label_from, to, data.port_label_to);
    gno->new_link(from, data.port_label_from, to, data.port_label_to);
  }

  // --- UNBLOCK model updates

  this->set_block_graph_model_updates(false);

  // --- Update and exit

  this->update_graph_model(new_id);
  return new_id;
}

void GraphNodeWidget::on_node_deleted_request(const std::string &node_id)
{
  Logger::log()->trace("GraphNodeWidget::on_node_deleted_request, node {}", node_id);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // block connection-related updates
  this->set_block_graph_model_updates(true);

  this->set_enabled(false);
  gno->remove_node(node_id);
  this->set_enabled(true);

  this->set_block_graph_model_updates(false);

  Q_EMIT this->node_deleted(this->get_id(), node_id);
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

void GraphNodeWidget::set_block_graph_model_updates(bool new_state)
{
  Logger::log()->trace("GraphNodeWidget::set_block_graph_model_updates: state is now {}",
                       new_state ? "BLOCKED" : "UNBLOCKED");

  this->block_graph_model_updates = new_state;
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
                &gngui::GraphViewer::connection_deleted,
                this,
                &GraphNodeWidget::on_connection_deleted);

  this->connect(this,
                &gngui::GraphViewer::connection_dropped,
                this,
                &GraphNodeWidget::on_connection_dropped);

  this->connect(this,
                &gngui::GraphViewer::connection_finished,
                this,
                &GraphNodeWidget::on_connection_finished);

  this->connect(this,
                &gngui::GraphViewer::new_graphics_node_request,
                this,
                &GraphNodeWidget::on_new_graphics_node_request);

  this->connect(this,
                &gngui::GraphViewer::new_node_request,
                this,
                &GraphNodeWidget::on_new_node_request);

  this->connect(this,
                &gngui::GraphViewer::node_deleted,
                this,
                &GraphNodeWidget::on_node_deleted_request);

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
  Logger::log()->trace("GraphNodeWidget::update_graph_model");

  if (this->is_graph_model_updates_blocked())
  {
    Logger::log()->trace("GraphNodeWidget::update_graph_model: graph model updates are "
                         "blocked, no update");
    return;
  }

  auto gno = this->p_graph_node.lock();
  if (!gno)
  {
    Logger::log()->error(
        "GraphNodeWidget::update_graph_model: graph node model ptr is nullptr");
    return;
  }

  if (node_ids.empty())
    gno->update();
  else
    gno->update(node_ids);
}

void GraphNodeWidget::update_graph_model(const std::string &node_id)
{
  this->update_graph_model(std::vector<std::string>{node_id});
}

} // namespace hesiod
