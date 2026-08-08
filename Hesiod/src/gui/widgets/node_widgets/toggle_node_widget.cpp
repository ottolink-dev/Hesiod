/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/data_preview.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_node.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

ToggleNodeWidget::ToggleNodeWidget(std::weak_ptr<BaseNode>   model,
                                   QPointer<GraphNodeWidget> p_gnw,
                                   const std::string        &msg,
                                   QWidget                  *parent)
    : NodeWidget(model, p_gnw, msg, parent)
{
  Logger::log()->trace("ToggleNodeWidget::ToggleNodeWidget");

  auto m = this->model.lock();
  if (!m)
    return;

  this->button_a = new QPushButton("A");
  this->button_b = new QPushButton("B");

  this->button_a->setCheckable(true);
  this->button_b->setCheckable(true);

  bool checked = m->val<bool>("toggle");
  this->button_a->setChecked(checked);
  this->button_b->setChecked(!checked);

  this->connect(this->button_a,
                &QPushButton::toggled,
                [this](bool is_checked)
                {
                  if (this->sync_guard)
                    return;

                  this->sync_guard = true;
                  this->button_b->setChecked(!is_checked);
                  this->sync_guard = false;

                  if (auto m = this->model.lock())
                  {
                    m->set_value<bool>("toggle", is_checked);
                    m->get_p_graph()->update(m->get_id());
                  }
                });

  this->connect(this->button_b,
                &QPushButton::toggled,
                [this](bool is_checked)
                {
                  if (this->sync_guard)
                    return;

                  this->sync_guard = true;
                  this->button_a->setChecked(!is_checked);
                  this->sync_guard = false;

                  if (auto m = this->model.lock())
                  {
                    m->set_value<bool>("toggle", !is_checked);
                    m->get_p_graph()->update(m->get_id());
                  }
                });

  this->layout->addWidget(button_a);
  this->layout->addWidget(button_b);
}

void ToggleNodeWidget::on_compute_finished()
{
  this->data_preview->update_preview();

  // align attribute state and widget state (it can have being changed
  // by another UI element)
  if (auto m = this->model.lock())
  {
    bool checked = m->val<bool>("toggle");
    this->sync_guard = true;
    this->button_a->setChecked(checked);
    this->button_b->setChecked(!checked);
    this->sync_guard = false;
  }
}

} // namespace hesiod
