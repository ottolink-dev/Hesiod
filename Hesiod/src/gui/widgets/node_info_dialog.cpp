/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "highmap/geometry/cloud.hpp"
#include "highmap/geometry/path.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_info_dialog.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/data_info.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

NodeInfoDialog::NodeInfoDialog(QPointer<GraphNodeWidget> p_graph_node_widget,
                               const std::string        &node_id,
                               QWidget                  *parent)
    : QDialog(parent), p_graph_node_widget(p_graph_node_widget), node_id(node_id)
{
  Logger::log()->trace("NodeInfoDialog::NodeInfoDialog");

  if (!this->p_graph_node_widget)
  {
    Logger::log()->error(
        "NodeInfoDialog::NodeInfoDialog: p_graph_node_widget is nullptr");
    return;
  }

  this->setWindowTitle("Hesiod - Node information");
  this->setMinimumWidth(256);
  this->setWindowModality(Qt::NonModal);
  this->setWindowFlags(Qt::Window); // ensures full top-level behavior
  this->setAttribute(Qt::WA_DeleteOnClose);

  this->setup_layout();
  this->setup_connections();
  this->update_content();
}

NodePointers NodeInfoDialog::get_node_pointers() const
{
  if (!this->p_graph_node_widget)
    return NodePointers();

  // there is a lifetime check in this method
  GraphNode *p_graph_node = this->p_graph_node_widget->get_p_graph_node();

  if (!p_graph_node)
    return NodePointers();

  BaseNode            *node = p_graph_node->get_node_ref_by_id<BaseNode>(this->node_id);
  gngui::GraphicsNode *gfx = this->p_graph_node_widget->get_graphics_node_by_id(
      this->node_id);

  return {node, gfx};
}

void NodeInfoDialog::on_comment_text_changed()
{
  auto ptrs = this->get_node_pointers();

  if (!ptrs.node || !ptrs.gfx || !this->editor)
    return;

  ptrs.node->set_comment(this->editor->toPlainText().toStdString());
}

void NodeInfoDialog::setup_connections()
{
  Logger::log()->trace("NodeInfoDialog::setup_connections");

  if (!this->p_graph_node_widget)
    return;

  // GraphNodeWidget -> this (make sure the dialog is closed when
  // the graph is destroyed or if the node is deleted)
  this->connect(this->p_graph_node_widget,
                &QObject::destroyed,
                this,
                [this]()
                {
                  this->p_graph_node_widget = nullptr;
                  this->deleteLater();
                });

  this->connect(
      this->p_graph_node_widget,
      &gngui::GraphViewer::node_deleted,
      this,
      [weak = QPointer<NodeInfoDialog>(this)](const std::string &deleted_node_id)
      {
        if (!weak)
          return;
        if (deleted_node_id == weak->node_id)
          weak->deleteLater();
      });

  if (auto ptrs = this->get_node_pointers(); ptrs.node)
  {
    this->post_update_conn = ptrs.node->post_update_event.subscribe(
        [this](gnode::Node &)
        { QMetaObject::invokeMethod(this, "update_content", Qt::QueuedConnection); });
  }

  this->connect(this->p_graph_node_widget,
                &gngui::GraphViewer::connection_deleted,
                this,
                [this](const std::string &id_out,
                       const std::string &,
                       const std::string &,
                       const std::string &,
                       bool)
                {
                  if (id_out == this->node_id)
                    this->update_content();
                });

  this->connect(this->p_graph_node_widget,
                &gngui::GraphViewer::connection_finished,
                this,
                [this](const std::string &id_out,
                       const std::string &,
                       const std::string &,
                       const std::string &)
                {
                  if (id_out == this->node_id)
                    this->update_content();
                });
}

void NodeInfoDialog::setup_layout()
{
  Logger::log()->trace("NodeInfoDialog::setup_layout");

  // --- safeguards

  if (!this->p_graph_node_widget)
    return;

  auto ptrs = this->get_node_pointers();
  if (!ptrs.node || !ptrs.gfx)
    return;

  // --- layout

  this->layout = new QVBoxLayout(this);
  this->setLayout(this->layout);

  // --- main label

  {
    std::string str = ptrs.node->get_caption() + "/" + ptrs.node->get_id();
    QLabel     *label = new QLabel(str.c_str());
    this->layout->addWidget(label);
  }

  // --- infos

  {
    QWidget    *container = new QWidget(this);
    std::string style = std::format(
        "background-color: {};",
        HSD_CTX.app_settings.colors.bg_deep.name().toStdString());
    container->setStyleSheet(style.c_str());
    this->grid_info = new QGridLayout(container);

    layout->addWidget(container);
  }

  // --- ports

  {
    QLabel *label = new QLabel("Ports");
    layout->addWidget(label);

    QWidget    *container = new QWidget(this);
    std::string style = std::format(
        "background-color: {};",
        HSD_CTX.app_settings.colors.bg_deep.name().toStdString());
    container->setStyleSheet(style.c_str());
    this->grid_ports = new QGridLayout(container);

    layout->addWidget(container);
  }

  // --- comment

  {
    QLabel *label = new QLabel("Comment");
    layout->addWidget(label);

    this->editor = new QPlainTextEdit();
    this->editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    this->editor->setPlainText(ptrs.node->get_comment().c_str());
    this->editor->setMinimumHeight(64);
    layout->addWidget(this->editor);

    this->connect(this->editor,
                  &QPlainTextEdit::textChanged,
                  this,
                  &NodeInfoDialog::on_comment_text_changed);
  }

  // --- exit

  {
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);
    this->connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    this->layout->addWidget(button_box);
  }
}

void NodeInfoDialog::update_content()
{
  Logger::log()->trace("NodeInfoDialog::update_content");
  this->update_ports_content();
  this->update_info_content();
  this->update();
}

void NodeInfoDialog::update_info_content()
{
  Logger::log()->trace("NodeInfoDialog::update_info_content");

  auto ptrs = this->get_node_pointers();
  if (!ptrs.node || !ptrs.gfx || !this->editor)
    return;

  // empty and delete items of current layout
  clear_layout(this->grid_info);

  // fill-in data to display
  struct Row
  {
    std::string caption;
    std::string info;
  };

  NodeRuntimeInfo info = ptrs.node->get_runtime_info();
  auto            cfg = ptrs.node->get_config_ref();

  std::vector<Row> rows = {
      {"Type", ptrs.node->get_caption()},
      {"Category", ptrs.node->get_category()},
      {"ID", ptrs.node->get_id()},
      {"Created", timestamp(info.time_creation)},
      {"Last Update", timestamp(info.time_last_update)},
      {"Update Time", std::format("{:.1f} ms", info.update_time)},
      {"Execution Count", std::to_string(info.eval_count)},
      {"Memory Usage", std::format("~{:.2f} MB", ptrs.node->get_memory_usage())},
      {"Address", ptr_as_string(static_cast<void *>(ptrs.node))},
      {"Config", ""},
      {"- shape", std::format("{}x{}", cfg->shape.x, cfg->shape.y)},
      {"- tiling", std::format("{}x{}", cfg->tiling.x, cfg->tiling.y)},
      {"- overlap", std::format("{}", cfg->overlap)}};

  // build layout
  QFont f("DejaVu Sans Mono", 9);

  int row_idx = 0;
  for (const Row &r : rows)
  {
    int col_idx = 0;
    for (const auto &s : {r.caption, r.info})
    {
      QLabel *label = new QLabel(s.c_str());
      label->setFont(f);
      this->grid_info->addWidget(label, row_idx, col_idx);
      col_idx++;
    }
    row_idx++;
  }
}

void NodeInfoDialog::update_ports_content()
{
  Logger::log()->trace("NodeInfoDialog::update_ports_content");

  auto ptrs = this->get_node_pointers();
  if (!ptrs.node || !ptrs.gfx || !this->editor)
    return;

  // empty and delete items of current layout
  clear_layout(this->grid_ports);

  // fill-in data to display
  struct Row
  {
    std::string caption;
    bool        is_connected;
    std::string type;
    std::string data_type;
    std::string data_info;
  };

  std::vector<Row> rows;

  for (int k = 0; k < ptrs.node->get_nports(); k++)
  {
    Row new_row;
    new_row.caption = ptrs.node->get_port_caption(k);
    new_row.is_connected = ptrs.node->is_port_connected(k);

    std::string str_ct = new_row.is_connected ? "✓" : " ";
    std::string str_in = std::format("→[{}] ", str_ct);
    std::string str_out = std::format(" [{}]→", str_ct);

    new_row.type = (ptrs.node->get_port_type(k) == gngui::PortType::IN) ? str_in
                                                                        : str_out;
    new_row.data_type = map_type_name(ptrs.node->get_data_type(k));

    // data info
    {
      new_row.data_info = "-";

      const std::string type_name = ptrs.node->get_data_type(k);

      if (type_name == typeid(hmap::Array).name())
        new_row.data_info = get_data_info<hmap::Array>(ptrs.node, new_row.caption);
      else if (type_name == typeid(hmap::VirtualArray).name())
        new_row.data_info = get_data_info<hmap::VirtualArray>(ptrs.node, new_row.caption);
      else if (type_name == typeid(hmap::VirtualTexture).name())
        new_row.data_info = get_data_info<hmap::VirtualTexture>(ptrs.node,
                                                                new_row.caption);
      else if (type_name == typeid(hmap::Cloud).name())
        new_row.data_info = get_data_info<hmap::Cloud>(ptrs.node, new_row.caption);
      else if (type_name == typeid(hmap::Path).name())
        new_row.data_info = get_data_info<hmap::Path>(ptrs.node, new_row.caption);
    }

    rows.push_back(new_row);
  }

  // build layout
  QFont f("DejaVu Sans Mono", 9);

  int row_idx = 0;
  for (const Row &r : rows)
  {
    int col_idx = 0;
    for (const auto &s : {r.type, r.caption, r.data_type, r.data_info})
    {
      QLabel *label = new QLabel(s.c_str());
      label->setFont(f);
      this->grid_ports->addWidget(label, row_idx, col_idx);
      col_idx++;
    }
    row_idx++;
  }
}

} // namespace hesiod
