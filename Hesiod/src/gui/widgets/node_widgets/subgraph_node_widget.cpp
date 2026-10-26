/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#include "hesiod/gui/widgets/node_widgets/subgraph_node_widget.hpp"

#include <QMouseEvent>

#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/nodes/subgraph_node.hpp"

namespace hesiod
{

SubGraphNodeWidget::SubGraphNodeWidget(std::weak_ptr<BaseNode>   model,
                                       QPointer<GraphNodeWidget> p_gnw,
                                       const std::string        &msg,
                                       QWidget                  *parent)
    : NodeWidget(model, p_gnw, msg, parent)
{
  Logger::log()->trace("SubGraphNodeWidget::SubGraphNodeWidget");

  this->setup_subgraph_ui();
  this->update_node_summary();
}

void SubGraphNodeWidget::setup_subgraph_ui()
{
  this->summary_label = new QLabel(this);
  this->summary_label->setAlignment(Qt::AlignCenter);
  this->summary_label->setStyleSheet(
      "QLabel { color: #cccccc; font-size: 11px; padding: 4px; }");
  this->layout->addWidget(this->summary_label);

  this->open_button = new QPushButton("Dive Into SubGraph", this);
  this->open_button->setStyleSheet(
      "QPushButton { background-color: #2b2b2b; color: #ffffff; border: 1px solid "
      "#444444; "
      "border-radius: 4px; padding: 4px 8px; font-weight: bold; } "
      "QPushButton:hover { background-color: #3d3d3d; border-color: #666666; }");
  this->layout->addWidget(this->open_button);

  this->connect(this->open_button,
                &QPushButton::clicked,
                this,
                [this]()
                {
                  if (auto m = this->model.lock())
                    emit open_subgraph_requested(m->get_id());
                });
}

void SubGraphNodeWidget::update_node_summary()
{
  auto m = this->model.lock();
  if (!m)
    return;

  auto sg_node = std::dynamic_pointer_cast<SubGraphNode>(m);
  if (!sg_node)
    return;

  auto child_graph = sg_node->get_subgraph();
  if (child_graph)
  {
    const size_t count = child_graph->get_nodes().size();
    this->summary_label->setText(QString("Inner Nodes: %1").arg(count));
  }
  else
  {
    this->summary_label->setText("Empty SubGraph");
  }
}

void SubGraphNodeWidget::on_compute_finished()
{
  NodeWidget::on_compute_finished();
  this->update_node_summary();
}

void SubGraphNodeWidget::on_compute_started()
{
  NodeWidget::on_compute_started();
  this->summary_label->setText("Evaluating inner graph...");
}

void SubGraphNodeWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton)
  {
    if (auto m = this->model.lock())
    {
      emit open_subgraph_requested(m->get_id());
      event->accept();
      return;
    }
  }

  NodeWidget::mouseDoubleClickEvent(event);
}

} // namespace hesiod
