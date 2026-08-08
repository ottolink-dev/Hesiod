/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QLabel>
#include <QPushButton>

#include "hesiod/gui/widgets/data_preview.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/logger.hpp"

namespace hesiod
{

ExportNodeWidget::ExportNodeWidget(std::weak_ptr<BaseNode>   model,
                                   QPointer<GraphNodeWidget> p_gnw,
                                   const std::string        &msg,
                                   QWidget                  *parent)
    : NodeWidget(model, p_gnw, msg, parent)
{
  Logger::log()->trace("ExportNodeWidget::ExportNodeWidget");

  // add export button
  QPushButton *button = new QPushButton("Export!", this);
  this->layout->addWidget(button);

  this->connect(button,
                &QPushButton::pressed,
                [this]()
                {
                  if (auto m = this->model.lock())
                  {
                    // bypass 'auto_export' attribute
                    bool auto_export = m->val<bool>("auto_export");
                    m->set_value<bool>("auto_export", true);
                    m->compute();
                    m->set_value<bool>("auto_export", auto_export);
                  }
                });
}

} // namespace hesiod
