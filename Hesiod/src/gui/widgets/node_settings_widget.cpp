/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QLabel>
#include <QVBoxLayout>

#include "meta_qt/designs/industrial/panel_chrome.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/gui_utils.hpp"
#include "hesiod/gui/widgets/icon_check_box.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/gui/widgets/node_library_widget.hpp"
#include "hesiod/gui/widgets/node_settings_widget.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
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

  // low floor so the pane is freely narrowable; wide content scrolls
  this->setMinimumWidth(240);

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
}

void NodeSettingsWidget::setup_layout()
{
  Logger::log()->trace("NodeSettingsWidget::setup_layout");

  this->setStyleSheet("QTabBar::tab {"
                      "  min-height: 20px;"
                      "  padding: 4px 8px;"
                      "  border-top-left-radius: 4px;"
                      "  border-top-right-radius: 4px;"
                      "  border-bottom-left-radius: 0px;"
                      "  border-bottom-right-radius: 0px;"
                      "}"
                      "QTabBar::tab:selected {"
                      "  margin-bottom: -1px;"
                      "}");

  auto *layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  this->setLayout(layout);

  // --- attributes widget

  {
    // Every bit of panel chrome below is gated on this, so a design that does
    // not bring its own gets exactly the panel it had before.
    const PropertiesPanelDesign &panel = properties_panel_design();

    auto *container = new QWidget();
    this->attr_layout = new QVBoxLayout(container);
    this->attr_layout->setAlignment(Qt::AlignTop);

    // Section headers are meant to span the full width of the panel, and the
    // sections apply their own padding to their contents instead. Only the
    // designs that draw section cards, though: without one, dropping the
    // margin just leaves every row flush against the edge.
    if (panel.has_own_chrome)
      this->attr_layout->setContentsMargins(0, 0, 0, 0);

    this->attr_layout->setSpacing(2);

    auto *scroll = new QScrollArea();

    // As-needed, not off. Some canvases are sized from the array resolution
    // rather than the dock, so on a narrow dock turning this off left the
    // right-hand side of a Brush clipped with no way to reach it.
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    if (panel.has_own_chrome)
    {
      // Reserve the vertical scrollbar permanently. An as-needed bar appearing
      // on expand narrows the viewport and reflows every row, so padding jumps
      // sideways for reasons unrelated to what was clicked. Costs a few pixels
      // and removes a whole class of confusion.
      scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
      scroll->setStyleSheet(
          meta::qt::industrial::scrollbar_stylesheet(*panel.theme));
    }

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
  this->attr_widgets.clear();

  // lifetime safe getter
  GraphNode *p_gno = this->p_graph_node_widget->get_p_graph_node();
  if (!p_gno)
    return;

  this->attr_layout->addWidget(new QLabel()); // space

  // refill based on selected nodes (and pinned nodes)
  std::vector<std::string> selected_ids = this->p_graph_node_widget
                                              ->get_selected_node_ids();

  // remember the last non-empty selection so that deselecting every node does
  // not blank out the settings pane: keep showing the last selected node(s)
  if (!selected_ids.empty())
    this->last_selected_ids = selected_ids;

  const std::vector<std::string> &effective_selected = selected_ids.empty()
                                                           ? this->last_selected_ids
                                                           : selected_ids;

  std::vector<std::string> all_ids = merge_unique(this->pinned_node_ids,
                                                  effective_selected);

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

    this->attr_widgets.push_back(attr_widget);
  }

  this->attr_layout->addStretch();
}

void NodeSettingsWidget::sync_content()
{
  Logger::log()->trace("NodeSettingsWidget::sync_content");

  // Nothing built yet (e.g. no selection): do the initial build.
  if (this->attr_widgets.empty())
  {
    this->update_content();
    return;
  }

  // Sync every widget in place, never tearing the panel down on recompute: the
  // Meta widgets refresh from the model. Because there is no teardown, a
  // live-dragged widget is never destroyed mid-drag.
  for (const auto &w : this->attr_widgets)
    if (w)
      w->sync_from_model();
}

} // namespace hesiod
