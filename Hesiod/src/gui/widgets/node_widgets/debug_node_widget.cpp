/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/data_preview.hpp"
#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/node_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

namespace hesiod
{

DebugNodeWidget::DebugNodeWidget(std::weak_ptr<BaseNode>   model,
                                 QPointer<GraphNodeWidget> p_gnw,
                                 const std::string        &msg,
                                 QWidget                  *parent)
    : NodeWidget(model, p_gnw, msg, parent)
{
  Logger::log()->trace("DebugNodeWidget::DebugNodeWidget");

  this->label = new QLabel();

  std::string style = std::format(
      "background-color: {};",
      HSD_CTX.app_settings.colors.bg_deep.name().toStdString());
  QFont f("DejaVu Sans Mono", 7);

  this->label->setStyleSheet(style.c_str());
  this->label->setFont(f);
  this->label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  this->label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  this->label->setMinimumWidth(128);
  this->label->setMinimumHeight(128);

  this->layout->addWidget(this->label);
}

void DebugNodeWidget::on_compute_finished()
{
  this->data_preview->update_preview();

  if (auto m = this->model.lock())
  {
    hmap::VirtualArray *p_in = m->get_value_ref<hmap::VirtualArray>("input");

    std::string msg = "input:\n";

    if (p_in)
    {
      float min = p_in->min(m->cfg().cm_cpu);
      float max = p_in->max(m->cfg().cm_cpu);

      msg += "- addr: " + ptr_as_string((void *)(p_in)) + "\n";
      msg += "- min: " + std::to_string(min) + "\n";
      msg += "- max: " + std::to_string(max) + "\n";
    }
    else
    {
      msg += "addr: nullptr\n";
    }

    this->label->setText(msg.c_str());
  }
  else
  {
    this->label->setText("!!! NO MODEL !!!");
  }
}

} // namespace hesiod
