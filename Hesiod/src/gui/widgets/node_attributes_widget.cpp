/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QDesktopServices>
#include <QLayout>
#include <QStyle>
#include <QToolButton>

#include "meta_qt/container_group_widget.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/documentation_popup.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

NodeAttributesWidget::NodeAttributesWidget(std::weak_ptr<GraphNode>  p_graph_node,
                                           const std::string        &node_id,
                                           QPointer<GraphNodeWidget> p_graph_node_widget,
                                           bool                      add_toolbar,
                                           QWidget                  *parent)
    : QWidget(parent), p_graph_node(p_graph_node), node_id(node_id),
      p_graph_node_widget(p_graph_node_widget), add_toolbar(add_toolbar)
{
  Logger::log()->trace("NodeAttributesWidget::NodeAttributesWidget: node {}", node_id);

  this->setAttribute(Qt::WA_DeleteOnClose);

  this->setup_layout();
  this->setup_connections();
}

QWidget *NodeAttributesWidget::create_toolbar()
{
  Logger::log()->trace("NodeAttributesWidget::create_toolbar");

  QWidget     *toolbar = new QWidget(this);
  QHBoxLayout *layout = new QHBoxLayout(toolbar);
  layout->setContentsMargins(0, 0, 0, 0);

  auto make_button = [&](const QIcon &icon, const QString &tooltip)
  {
    QToolButton *btn = new QToolButton;
    btn->setToolTip(tooltip);
    btn->setIcon(icon);
    // btn->setStyleSheet("border: 0px;");
    return btn;
  };

  auto *update_btn = make_button(HSD_ICON("refresh"), "Force Update");
  auto *info_btn = make_button(HSD_ICON("info"), "Node Information");
  auto *bckp_btn = make_button(HSD_ICON("bookmark"), "Backup State");
  auto *revert_btn = make_button(HSD_ICON("u_turn_left"), "Revert State");
  auto *load_btn = make_button(HSD_ICON("file_open"), "Load Preset");
  auto *save_btn = make_button(HSD_ICON("save"), "Save Preset");
  auto *reset_btn = make_button(HSD_ICON("settings_backup_restore"), "Reset Settings");
  auto *help_btn = make_button(HSD_ICON("help"), "Help!");
  auto *doc_btn = make_button(HSD_ICON("link"), "Online Documentation");

  for (auto *btn : {update_btn,
                    info_btn,
                    bckp_btn,
                    revert_btn,
                    load_btn,
                    save_btn,
                    reset_btn,
                    help_btn,
                    doc_btn})
    layout->addWidget(btn);

  // layout->addStretch();

  // --- connections

  // use node id + graph_node instead of the node pointer for safety
  // (no lifetime warranty on p_node)
  this->connect(update_btn,
                &QToolButton::pressed,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  gno->update(this->node_id);
                });

  this->connect(info_btn,
                &QToolButton::pressed,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  if (this->p_graph_node_widget)
                    this->p_graph_node_widget->on_node_info(this->node_id);
                });

  this->connect(bckp_btn,
                &QToolButton::pressed,
                [this]() { this->attributes_widget->on_save_state(); });
  this->connect(revert_btn,
                &QToolButton::pressed,
                [this]() { this->attributes_widget->on_restore_save_state(); });
  this->connect(load_btn,
                &QToolButton::pressed,
                [this]() { this->attributes_widget->on_load_preset(); });
  this->connect(save_btn,
                &QToolButton::pressed,
                [this]() { this->attributes_widget->on_save_preset(); });
  this->connect(reset_btn,
                &QToolButton::pressed,
                [this]() { this->attributes_widget->on_restore_initial_state(); });

  this->connect(help_btn,
                &QToolButton::pressed,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  if (auto *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id))
                  {
                    auto *popup = new DocumentationPopup(
                        p_node->get_label(),
                        p_node->get_documentation_html());
                    popup->setAttribute(Qt::WA_DeleteOnClose);
                    popup->show();
                  }
                });

  this->connect(
      doc_btn,
      &QToolButton::pressed,
      [this]()
      {
        auto gno = this->p_graph_node.lock();
        if (!gno)
          return;

        if (auto *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id))
        {
          std::string
              url = "https://hesioddoc.readthedocs.io/en/latest/node_reference/nodes/" +
                    p_node->get_label();
          QDesktopServices::openUrl(QUrl(url.c_str()));
        }
      });

  return toolbar;
}

attr::AttributesWidget *NodeAttributesWidget::get_attributes_widget_ref()
{
  return this->attributes_widget;
}

void NodeAttributesWidget::setup_connections()
{
  Logger::log()->trace("NodeAttributesWidget::setup_connections");

  if (!this->attributes_widget)
    return;

  this->connect(this->attributes_widget,
                &attr::AttributesWidget::value_changed,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  gno->update(this->node_id);
                });

  this->connect(this->attributes_widget,
                &attr::AttributesWidget::update_button_released,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  gno->update(this->node_id);
                });
}

void NodeAttributesWidget::setup_layout()
{
  Logger::log()->trace("NodeAttributesWidget::setup_layout");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
  if (!p_node)
    return;

  if (p_node->uses_meta())
  {
    // Meta-backed node: render with meta_qt, skip the legacy AttributesWidget path.
    this->attributes_widget = nullptr; // so setup_connections() skips the legacy wiring

    auto *meta_widget = new meta::qt::ContainerGroupWidget(p_node->meta_group(),
                                                           meta::qt::ContainerRenderOptions{},
                                                           this);

    this->connect(meta_widget,
                  &meta::qt::MetaWidget::value_changed,
                  this,
                  [this]()
                  {
                    auto gno = this->p_graph_node.lock();
                    if (!gno)
                      return;
                    gno->update(this->node_id);
                  });

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(4);
    main_layout->setContentsMargins(0, 0, 0, 0);
    if (this->add_toolbar)
      main_layout->addWidget(this->create_toolbar());
    main_layout->addWidget(meta_widget);
    return;
  }

  // generate a fresh widget
  bool        add_save_reset_state_buttons = false;
  std::string window_title = "";
  QWidget    *parent = this;

  this->attributes_widget = new attr::AttributesWidget(p_node->get_attributes_ref(),
                                                       p_node->get_attr_ordered_key_ref(),
                                                       window_title,
                                                       add_save_reset_state_buttons,
                                                       parent);

  // change the attribute widget layout spacing a posteriori
  QLayout *retrieved_layout = attributes_widget->layout();
  if (retrieved_layout)
  {
    retrieved_layout->setSpacing(4);
    retrieved_layout->setContentsMargins(4, 0, 4, 0);

    for (int i = 0; i < retrieved_layout->count(); ++i)
    {
      QWidget *child = retrieved_layout->itemAt(i)->widget();
      if (!child)
        continue;

      if (auto *inner_layout = child->layout())
      {
        inner_layout->setSpacing(4);
        inner_layout->setContentsMargins(4, 0, 4, 0);
      }
    }
  }

  // --- main layout
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setSpacing(4);
  main_layout->setContentsMargins(0, 0, 0, 0);

  if (this->add_toolbar)
    main_layout->addWidget(this->create_toolbar());

  main_layout->addWidget(this->attributes_widget);
}

} // namespace hesiod
