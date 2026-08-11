/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QGridLayout>
#include <QLabel>

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

ThruNodeWidget::ThruNodeWidget(std::weak_ptr<BaseNode>   model,
                               QPointer<GraphNodeWidget> p_gnw,
                               const std::string        &msg,
                               QWidget                  *parent)
    : NodeWidget(model, p_gnw, msg, parent)
{
  Logger::log()->trace("ThruNodeWidget::ThruNodeWidget");

  auto m = this->model.lock();
  if (!m)
    return;

  bool state = m->val<bool>("block_update");

  this->button = new QPushButton("BLOCK UPDATE");
  this->button->setCheckable(true);
  this->button->setChecked(state);
  this->button->setStyleSheet("QPushButton:checked { background-color: red; }");

  this->connect(this->button,
                &QPushButton::toggled,
                [this](bool checked)
                {
                  auto m = this->model.lock();
                  if (!m)
                    return;

                  m->set_value<bool>("block_update", checked);
                  m->get_p_graph()->update(m->get_id());
                });

  this->layout->addWidget(this->button);
}

void ThruNodeWidget::on_compute_finished()
{
  this->data_preview->update_preview();

  // align attribute state and widget state (it can have being changed
  // by another UI element)
  if (auto m = this->model.lock())
    this->button->setChecked(m->val<bool>("block_update"));
}

} // namespace hesiod
