/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QLabel>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/data_preview.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

NodeWidget::NodeWidget(std::weak_ptr<BaseNode>   model,
                       QPointer<GraphNodeWidget> p_gnw,
                       const std::string        &msg,
                       QWidget                  *parent)
    : QWidget(parent), model(model), p_gnw(p_gnw), msg(msg)
{
  Logger::log()->trace("NodeWidget::NodeWidget");

  this->setup_layout();
  this->setup_connections();
}

void NodeWidget::on_compute_finished() { this->data_preview->update_preview(); }

void NodeWidget::on_compute_started() { this->data_preview->clear_preview(); }

void NodeWidget::setup_connections()
{
  if (auto m = this->model.lock())
  {
    this->before_conn = m->update_before_event.subscribe(
        [this](gnode::Node &) {
          QMetaObject::invokeMethod(this, "on_compute_started", Qt::QueuedConnection);
        });

    this->after_conn = m->update_after_event.subscribe(
        [this](gnode::Node &) {
          QMetaObject::invokeMethod(this, "on_compute_finished", Qt::QueuedConnection);
        });
  }
}

void NodeWidget::setup_layout()
{
  if (!this->p_gnw)
    return;

  this->layout = new QVBoxLayout(this);
  this->setLayout(this->layout);
  this->layout->setSpacing(0);
  this->layout->setContentsMargins(0, 0, 0, 0);

  this->data_preview = new DataPreview(model, this);
  this->layout->addWidget(this->data_preview);

  if (HSD_CTX.app_settings.interface.enable_node_settings_in_node_body)
  {
    GraphNode *p_graph = this->p_gnw->get_p_graph_node(); // lifetime safe

    if (p_graph)
    {
      auto m = this->model.lock();
      if (m)
      {
        QWidget *attr_widget = new NodeAttributesWidget(p_graph->get_shared(),
                                                        /* node_id */ m->get_id(),
                                                        this->p_gnw,
                                                        /* add_toolbar */ false);
        this->layout->addWidget(attr_widget);
      }
    }
  }

  if (!this->msg.empty())
  {
    auto *label = new QLabel(msg.c_str());
    resize_font(label, -2);
    this->layout->addWidget(label);
  }
}

QSize NodeWidget::sizeHint() const
{
  AppContext &ctx = HSD_CTX;
  return QSize(ctx.app_settings.node_editor.preview_w,
               ctx.app_settings.node_editor.preview_h);
}

} // namespace hesiod
