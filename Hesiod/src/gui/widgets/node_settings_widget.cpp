/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QLabel>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/icon_check_box.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/gui/widgets/node_library_widget.hpp"
#include "hesiod/gui/widgets/node_settings_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

NodeSettingsWidget::NodeSettingsWidget(QPointer<GraphNodeWidget> p_graph_node_widget,
                                       QWidget                  *parent)
    : QWidget(parent), p_graph_node_widget(p_graph_node_widget)
{
  Logger::log()->trace("NodeSettingsWidget::NodeSettingsWidget");

  if (!this->p_graph_node_widget)
    return;

  this->setMinimumWidth(360); // TODO fix this
  this->setMaximumWidth(360);

  this->setup_layout();
  this->setup_connections();
  this->update_content();
}

void NodeSettingsWidget::setup_connections()
{
  Logger::log()->trace("NodeSettingsWidget::setup_connections");

  if (!this->p_graph_node_widget)
    return;

  // connect selection changes -> update UI
  this->connect(this->p_graph_node_widget,
                &gngui::GraphViewer::selection_has_changed,
                this,
                &NodeSettingsWidget::update_content);

  this->connect(this->p_graph_node_widget,
                &GraphNodeWidget::update_finished,
                this,
                &NodeSettingsWidget::update_content);
}

void NodeSettingsWidget::setup_layout()
{
  Logger::log()->trace("NodeSettingsWidget::setup_layout");

  auto *layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  this->setLayout(layout);

  const int margin = 6;

  // --- attributes widget

  {
    auto *container = new QWidget();
    this->attr_layout = new QVBoxLayout(container);
    this->attr_layout->setContentsMargins(margin, 0, margin, 0);
    this->attr_layout->setSpacing(2);

    auto *scroll = new QScrollArea();
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(scroll);
  }
}

void NodeSettingsWidget::update_content()
{
  Logger::log()->trace("NodeSettingsWidget::update_content");

  if (!this->p_graph_node_widget)
    return;

  clear_layout(this->attr_layout);

  // lifetime safe getter
  GraphNode *p_gno = this->p_graph_node_widget->get_p_graph_node();
  if (!p_gno)
    return;

  this->attr_layout->addWidget(new QLabel()); // space

  // refill based on selected nodes (and pinned nodes)
  std::vector<std::string> selected_ids = this->p_graph_node_widget
                                              ->get_selected_node_ids();
  std::vector<std::string> all_ids = merge_unique(this->pinned_node_ids, selected_ids);

  for (auto &node_id : all_ids)
  {
    BaseNode *p_node = p_gno->get_node_ref_by_id<BaseNode>(node_id);
    if (!p_node)
    {
      remove_all_occurrences(this->pinned_node_ids, node_id);
      continue;
    }

    const QString node_caption = QString::fromStdString(p_node->get_caption());

    // pinned checkbox button
    {
      auto *button_pin = new IconCheckBox(this);
      button_pin->set_label(node_caption);
      button_pin->set_icons(HSD_ICON("push_pin"), HSD_ICON("push_pin_accent"));
      button_pin->setCheckable(true);
      button_pin->setChecked(contains(this->pinned_node_ids, node_id));
      this->attr_layout->addWidget(button_pin);

      this->connect(button_pin,
                    &QCheckBox::toggled,
                    this,
                    [this, button_pin, node_id]
                    {
                      if (button_pin->isChecked())
                      {
                        if (!contains(this->pinned_node_ids, node_id))
                          this->pinned_node_ids.push_back(node_id);
                      }
                      else
                      {
                        remove_all_occurrences(this->pinned_node_ids, node_id);
                      }
                    });
    }

    bool add_toolbar = HSD_CTX.app_settings.node_editor.show_node_toolbar_in_settings_pan;

    auto *attr_widget = new NodeAttributesWidget(p_gno->get_shared(),
                                                 node_id,
                                                 this->p_graph_node_widget,
                                                 add_toolbar,
                                                 /* parent */ nullptr);
    if (!attr_widget)
      continue;

    this->attr_layout->addWidget(attr_widget);
    this->attr_layout->addWidget(new QLabel()); // space
  }

  this->attr_layout->addStretch();
}

} // namespace hesiod
