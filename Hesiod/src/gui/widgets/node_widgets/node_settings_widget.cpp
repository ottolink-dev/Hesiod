/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <algorithm>

#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QVBoxLayout>

#include "meta_qt/designs/industrial/panel_chrome.hpp"
#include "meta_qt/ui/theme.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/controls/icon_check_box.hpp"
#include "hesiod/gui/widgets/node_widgets/node_attributes_widget.hpp"
#include "hesiod/gui/widgets/node_widgets/node_library_widget.hpp"
#include "hesiod/gui/widgets/node_widgets/node_settings_widget.hpp"
#include "hesiod/gui/widgets/node_widgets/post_process_dock.hpp"
#include "hesiod/gui/widgets/node_widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/node_widgets/properties_panel_header.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

namespace
{

QString panel_scrollbar_stylesheet(const meta::qt::Theme &theme)
{
  // The space of the bar is always there; with nothing to scroll, Qt would
  // still draw a handle over the whole height, which reads as content that is
  // cut off.
  return meta::qt::industrial::scrollbar_stylesheet(theme) +
         "QScrollBar[idle=\"true\"]::handle:vertical,"
         "QScrollBar[idle=\"true\"]::handle:vertical:hover { "
         "background: transparent; }";
}

/** @brief Reserve the vertical scrollbar permanently.
 *
 * An as-needed bar appearing on expand narrows the viewport and reflows every
 * row, so padding jumps sideways for reasons unrelated to what was clicked.
 * Costs a few pixels and removes a whole class of confusion.
 */
void reserve_scrollbar(QScrollArea *area, const meta::qt::Theme &theme)
{
  area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  area->setStyleSheet(panel_scrollbar_stylesheet(theme));

  QScrollBar *bar = area->verticalScrollBar();
  bar->setProperty("idle", true); // empty until there is something in it

  QObject::connect(bar,
                   &QScrollBar::rangeChanged,
                   bar,
                   [bar](int, int max)
                   {
                     if (bar->property("idle").toBool() == (max == 0))
                       return;
                     bar->setProperty("idle", max == 0);
                     bar->style()->unpolish(bar);
                     bar->style()->polish(bar);
                   });
}

/** @brief The foot of the pane: as tall as the output chain it holds.
 *
 * Sits under the scrolling list and outside it, so the chain of the node on
 * show stays where it is while the attributes above it scroll, and the row of
 * toggles is always in the same place.
 *
 * It is a scroll area of its own only as a backstop: the chain is given what
 * it asks for up to a share of the pane, and scrolls past that rather than
 * squeezing the attributes out of a short window.
 */
class ChainArea final : public QScrollArea
{
public:
  explicit ChainArea(QWidget *parent) : QScrollArea(parent)
  {
    this->setWidgetResizable(true);
    this->setFrameShape(QFrame::NoFrame);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    if (parent)
      parent->installEventFilter(this);

    // Past the cap, show the end of the chain: that is where the toggles are.
    QScrollBar *bar = this->verticalScrollBar();
    QObject::connect(bar,
                     &QScrollBar::rangeChanged,
                     bar,
                     [bar](int, int max) { bar->setValue(max); });
  }

  QSize sizeHint() const override
  {
    return QSize(QScrollArea::sizeHint().width(), this->fitted_height());
  }

  QSize minimumSizeHint() const override { return QSize(0, this->fitted_height()); }

  bool eventFilter(QObject *watched, QEvent *event) override
  {
    const bool chain_changed = watched == this->widget() &&
                               (event->type() == QEvent::LayoutRequest ||
                                event->type() == QEvent::Resize ||
                                event->type() == QEvent::Show ||
                                event->type() == QEvent::Hide);
    const bool pane_resized = watched == this->parentWidget() &&
                              event->type() == QEvent::Resize;

    const bool handled = QScrollArea::eventFilter(watched, event);

    if ((chain_changed || pane_resized) && !this->fitting &&
        this->height() != this->fitted_height())
    {
      // Now, not on the next pass of the event loop: a stage opens over a
      // dozen frames, and a foot that trails it by one shows the row of
      // toggles pushed out and pulled back on every one of them.
      this->fitting = true;
      this->updateGeometry();
      if (this->parentWidget() && this->parentWidget()->layout())
        this->parentWidget()->layout()->activate();
      this->fitting = false;
    }

    return handled;
  }

private:
  int fitted_height() const
  {
    // a chain hides itself on a node whose current container offers no effect
    if (!this->widget() || this->widget()->isHidden())
      return 0;

    const int wanted = this->widget()->sizeHint().height();
    const int share = this->parentWidget() ? this->parentWidget()->height() * 3 / 5
                                           : wanted;
    return std::max(0, std::min(wanted, share));
  }

  bool fitting = false;
};

} // namespace

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

void NodeSettingsWidget::restyle_all()
{
  for (QWidget *top : QApplication::topLevelWidgets())
    for (NodeSettingsWidget *w : top->findChildren<NodeSettingsWidget *>())
      w->update_content();
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

    this->scroll = new QScrollArea();

    // As-needed, not off. Some canvases are sized from the array resolution
    // rather than the dock, so on a narrow dock turning this off left the
    // right-hand side of a Brush clipped with no way to reach it.
    this->scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    if (panel.has_own_chrome)
      reserve_scrollbar(this->scroll, *panel.theme);

    this->scroll->setWidget(container);
    this->scroll->setWidgetResizable(true);
    this->scroll->setFrameShape(QFrame::NoFrame);
    this->scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(this->scroll);

    // --- output chain of the node on show, attached under the list

    this->chain_area = new ChainArea(this);
    this->chain_area->hide();

    // the same gutter as the list above, or the two would not line up
    if (panel.has_own_chrome)
      reserve_scrollbar(this->chain_area, *panel.theme);

    layout->addWidget(this->chain_area);
  }
}

void NodeSettingsWidget::update_content()
{
  Logger::log()->trace("NodeSettingsWidget::update_content");

  if (!this->p_graph_node_widget)
    return;

  const PropertiesPanelDesign &panel = properties_panel_design();
  if (this->scroll && panel.has_own_chrome)
  {
    this->scroll->setStyleSheet(panel_scrollbar_stylesheet(*panel.theme));
    this->chain_area->setStyleSheet(panel_scrollbar_stylesheet(*panel.theme));
  }

  clear_layout(this->attr_layout);
  this->attr_widgets.clear();

  // the chain at the foot belonged to the widgets that just went
  this->chain_area->hide();
  if (QWidget *old_chain = this->chain_area->takeWidget())
    old_chain->deleteLater();

  // lifetime safe getter
  GraphNode *p_gno = this->p_graph_node_widget->get_p_graph_node();
  if (!p_gno)
    return;

  if (panel.has_own_chrome)
    this->attr_layout->addSpacing(4);
  else
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

    const QString node_caption = QString::fromStdString(p_node->get_label());

    const bool add_toolbar = HSD_CTX.app_settings.node_editor
                                 .show_node_toolbar_in_settings_pan;
    auto *attr_widget = new NodeAttributesWidget(p_gno->get_shared(),
                                                 node_id,
                                                 this->p_graph_node_widget,
                                                 add_toolbar && !panel.has_own_chrome);

    if (panel.has_own_chrome)
    {
      auto *pin = new QToolButton;
      QIcon pin_icon = HSD_ICON("push_pin");
      pin_icon.addPixmap(HSD_ICON("push_pin_accent").pixmap(16, 16),
                         QIcon::Normal,
                         QIcon::On);
      pin->setIcon(pin_icon);
      pin->setCheckable(true);
      pin->setChecked(contains(this->pinned_node_ids, node_id));
      pin->setAccessibleName("Pin " + node_caption);
      pin->setToolTip("Keep this node in the properties panel");
      pin->setCursor(Qt::PointingHandCursor);
      connect(pin,
              &QToolButton::toggled,
              this,
              [this, node_id](bool checked)
              {
                if (checked && !contains(this->pinned_node_ids, node_id))
                  this->pinned_node_ids.push_back(node_id);
                else if (!checked)
                  remove_all_occurrences(this->pinned_node_ids, node_id);
              });
      this->attr_layout->addWidget(new PropertiesPanelHeader(
          node_caption,
          *panel.theme,
          pin,
          add_toolbar ? attr_widget->create_toolbar() : nullptr));
    }

    // pinned checkbox button
    else
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

    this->attr_layout->addWidget(attr_widget);
    if (panel.has_own_chrome)
      this->attr_layout->addSpacing(8);
    else
      this->attr_layout->addWidget(new QLabel()); // space

    this->attr_widgets.push_back(attr_widget);
  }

  this->attr_layout->addStretch();

  // With one node on show, its output chain leaves the list and is attached to
  // the foot of the pane. With several, pinned or selected together, a strip
  // at the foot could not say whose it is, so each chain stays at the end of
  // its own node.
  if (this->attr_widgets.size() == 1 && this->attr_widgets.front())
    if (PostProcessDock *chain = this->attr_widgets.front()->release_post_dock())
    {
      this->chain_area->setWidget(chain);
      chain->setAutoFillBackground(false); // setWidget turns it on; the card is rounded
      this->chain_area->show();
    }
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
