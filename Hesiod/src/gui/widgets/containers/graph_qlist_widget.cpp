/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGridLayout>
#include <QLabel>

#include "hesiod/model/utils.hpp"
#include "highmap/colorize.hpp"

#include "hesiod/gui/widgets/containers/graph_manager_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/broadcast_param.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

GraphQListWidget::GraphQListWidget(std::weak_ptr<GraphNode> p_graph_node, QWidget *parent)
    : QWidget(parent), p_graph_node(p_graph_node)
{
  Logger::log()->trace("GraphQListWidget::GraphQListWidget");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  QGridLayout *layout = new QGridLayout(this);
  this->setLayout(layout);

  QLabel *label = new QLabel(gno->get_id().c_str());
  layout->addWidget(label, 0, 0);

  this->combobox = new QComboBox(this);
  this->current_bg_tag = "NONE";
  this->update_combobox();
  layout->addWidget(this->combobox, 0, 1);

  this->connect(this->combobox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &GraphQListWidget::on_combobox_changed);
}

std::string GraphQListWidget::get_current_bg_tag() const { return this->current_bg_tag; }

GraphNode *GraphQListWidget::get_p_graph_node()
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return nullptr;
  return gno.get();
}

void GraphQListWidget::json_from(nlohmann::json const &json)
{
  json_safe_get(json, "current_bg_tag", this->current_bg_tag);
  this->combobox->setCurrentText(this->current_bg_tag.c_str());
  this->on_combobox_changed();
}

nlohmann::json GraphQListWidget::json_to() const
{
  nlohmann::json json;
  json["current_bg_tag"] = this->current_bg_tag;
  return json;
}

void GraphQListWidget::on_combobox_changed()
{
  Logger::log()->trace("GraphQListWidget::on_combobox_changed: current tag: {}",
                       this->current_bg_tag);

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  this->current_bg_tag = this->combobox->currentText().toStdString();

  // retrieve node ID from tag
  std::string graph_id, node_id, port_id;
  bool ret = get_ids_from_broadcast_tag(this->current_bg_tag, graph_id, node_id, port_id);

  if (!ret)
  {
    Q_EMIT this->bg_image_updated(gno->get_id(), QImage());
    Q_EMIT this->has_changed();
    return;
  }

  Logger::log()->trace("GraphQListWidget::on_combobox_changed: node_id={}, port_id={}",
                       node_id,
                       port_id);

  // TODO allow other data types (see data_preview)

  // generate background image
  QImage               image;
  std::vector<uint8_t> img = {};

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(node_id);
  if (!p_node)
    return;

  hmap::VirtualArray *p_h = p_node->get_value_ref<hmap::VirtualArray>(port_id);

  if (p_h)
  {
    // TODO hardcoded
    glm::ivec2  shape_img(256, 256);
    hmap::Array array = p_h->to_array(shape_img, p_node->cfg().cm_cpu);
    img = hmap::colorize(array, array.min(), array.max(), hmap::Cmap::TURBO, true)
              .to_img_8bit();
    image = QImage(img.data(), shape_img.x, shape_img.y, QImage::Format_RGB888);
  }
  else
  {
    // null image
    image = QImage();
  }

  Q_EMIT this->bg_image_updated(gno->get_id(), image);
  Q_EMIT this->has_changed();
}

void GraphQListWidget::update_combobox()
{
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  // backup current tag selection (modified by itemChanged signal of
  // combobox)
  const std::string tag_bckp = this->current_bg_tag;

  this->combobox->clear();
  this->combobox->addItem("NONE");

  Logger::log()->trace("GraphQListWidget::update_combobox, tag: {}", tag_bckp);

  // list of possible data available to show on the canvas as Pixmap
  bool is_current_tag_in_combo = false;

  for (auto &[id, node] : gno->get_nodes())
    for (auto &port : node->get_ports())
    {
      // TODO allow other data types

      // only allow heighmap (for now)
      if (port->get_data_type() == typeid(hmap::VirtualArray).name())
      {
        const std::string tag = node->get_label() + "/" + node->get_id() + "/" +
                                port->get_label();
        this->combobox->addItem(tag.c_str());

        if (tag_bckp == tag)
          is_current_tag_in_combo = true;
      }
    }

  // set selection only if it is still available
  if (is_current_tag_in_combo)
    this->combobox->setCurrentText(tag_bckp.c_str());

  // for image update
  this->on_combobox_changed();
}

} // namespace hesiod
