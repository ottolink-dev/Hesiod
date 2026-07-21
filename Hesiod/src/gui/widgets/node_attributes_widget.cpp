/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include <QDesktopServices>
#include <QFileDialog>
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

  // These state/preset buttons operate on the legacy AttributesWidget, which is
  // null for meta-backed nodes; those take an else-path onto the Meta container
  // json (snapshot manager for state, json_to/json_from for presets).
  //
  // meta-backed nodes: state/preset operate on the Meta container json
  auto meta_container = [this]() -> meta::AttributeContainer *
  {
    auto gno = this->p_graph_node.lock();
    if (!gno)
      return nullptr;
    BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
    if (!p_node || !p_node->uses_meta())
      return nullptr;
    return &p_node->meta_group().current();
  };

  this->connect(
      bckp_btn,
      &QToolButton::pressed,
      [this, meta_container]()
      {
        // Mixed-backend (Brush): cover BOTH backends independently, not else-if.
        if (this->attributes_widget)
          this->attributes_widget->on_save_state();
        if (auto *c = meta_container())
          c->snapshot_manager().save("user_state", c->json_to());
      });

  this->connect(
      revert_btn,
      &QToolButton::pressed,
      [this, meta_container]()
      {
        // Mixed-backend (Brush): cover BOTH backends independently, not else-if.
        if (this->attributes_widget)
          this->attributes_widget->on_restore_save_state();
        if (auto *c = meta_container())
        {
          if (c->snapshot_manager().has("user_state"))
          {
            c->json_from(c->snapshot_manager().load("user_state"), true);
            this->sync_from_model();
            if (auto gno = this->p_graph_node.lock())
              gno->update(this->node_id);
          }
        }
      });

  // Preset Save/Load stay exclusive (legacy-first, early return) on purpose:
  // each pops a file dialog, so running both backends would show two dialogs.
  // For mixed Brush this covers the legacy "hmap" only; post_* preset is an
  // accepted minor gap (state Backup/Revert/Reset DO cover post_*).
  this->connect(
      load_btn,
      &QToolButton::pressed,
      [this, meta_container]()
      {
        if (this->attributes_widget)
        {
          this->attributes_widget->on_load_preset();
          return;
        }

        auto *c = meta_container();
        if (!c)
          return;

        QString fname = QFileDialog::getOpenFileName(nullptr,
                                                     "preset.json",
                                                     ".",
                                                     "json file (*.json)");

        if (!fname.isNull() && !fname.isEmpty())
        {
          std::ifstream file(fname.toStdString());

          if (file.is_open())
          {
            try
            {
              nlohmann::json json;
              file >> json;
              file.close();
              Logger::log()->trace("JSON successfully loaded from {}",
                                   fname.toStdString());

              c->json_from(json, true);
              this->sync_from_model();
              if (auto gno = this->p_graph_node.lock())
                gno->update(this->node_id);
            }
            catch (const std::exception &e)
            {
              Logger::log()->error("Failed to load preset {}: {}",
                                   fname.toStdString(),
                                   e.what());
            }
          }
          else
            Logger::log()->error("Could not open file {} to load JSON",
                                 fname.toStdString());
        }
      });

  this->connect(
      save_btn,
      &QToolButton::pressed,
      [this, meta_container]()
      {
        if (this->attributes_widget)
        {
          this->attributes_widget->on_save_preset();
          return;
        }

        auto *c = meta_container();
        if (!c)
          return;

        QString fname = QFileDialog::getSaveFileName(nullptr,
                                                     "preset.json",
                                                     ".",
                                                     "json file (*.json)");

        if (!fname.isNull() && !fname.isEmpty())
        {
          std::ofstream file(fname.toStdString());

          if (file.is_open())
          {
            file << c->json_to().dump(4);
            file.close();
          }
          else
            Logger::log()->error("Could not open file {} to save JSON",
                                 fname.toStdString());
        }
      });

  this->connect(
      reset_btn,
      &QToolButton::pressed,
      [this, meta_container]()
      {
        // Mixed-backend (Brush): cover BOTH backends independently, not else-if.
        if (this->attributes_widget)
          this->attributes_widget->on_restore_initial_state();

        auto gno = this->p_graph_node.lock();
        if (!gno)
          return;
        BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
        if (!p_node || !p_node->uses_meta())
          return;

        auto *c = meta_container();
        if (c && !p_node->initial_meta_state().empty())
        {
          c->json_from(p_node->initial_meta_state(), true);
          this->sync_from_model();
          gno->update(this->node_id);
        }
      });

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

void NodeAttributesWidget::sync_from_model()
{
  if (this->meta_widget)
    this->meta_widget->on_sync_meta_widgets_from_model();
  // legacy attr::AttributesWidget: values are the source of truth, no model sync needed.
}

bool NodeAttributesWidget::is_meta_backed() const { return this->meta_widget != nullptr; }

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

  // Mixed-backend aware: a node may have legacy attributes AND/OR a Meta
  // container. Brush is the one mixed node (legacy "hmap" paint canvas + Meta
  // post_* attributes). Pure-meta nodes have an empty legacy attr map; pure-
  // legacy nodes have uses_meta()==false. Build whichever backend(s) are present
  // (legacy panel first so the canvas sits above the post_* Meta panel).
  const bool has_meta = p_node->uses_meta();
  const bool has_legacy = !p_node->get_attributes_ref()->empty();

  // --- main layout (built once)
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setSpacing(4);
  main_layout->setContentsMargins(0, 0, 0, 0);

  if (this->add_toolbar)
    main_layout->addWidget(this->create_toolbar());

  // --- legacy AttributesWidget (canvas / classic attributes)
  if (has_legacy)
  {
    bool        add_save_reset_state_buttons = false;
    std::string window_title = "";
    QWidget    *parent = this;

    this->attributes_widget = new attr::AttributesWidget(
        p_node->get_attributes_ref(),
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

    main_layout->addWidget(this->attributes_widget);
  }
  // else: this->attributes_widget stays nullptr (setup_connections() skips it)

  // --- Meta ContainerGroupWidget (post_* / native Meta attributes)
  if (has_meta)
  {
    this->meta_widget = new meta::qt::ContainerGroupWidget(p_node->meta_group(),
                                                           meta::qt::ContainerRenderOptions{},
                                                           this);

    // Recompute continuously on value_changed: the panel now syncs from the
    // model (sync_from_model()) instead of being rebuilt on update_finished, so
    // recomputing on every value_changed no longer destroys a live-dragged
    // widget mid-drag.
    this->connect(this->meta_widget,
                  &meta::qt::MetaWidget::value_changed,
                  this,
                  [this]()
                  {
                    auto gno = this->p_graph_node.lock();
                    if (!gno)
                      return;
                    gno->update(this->node_id);
                  });

    main_layout->addWidget(this->meta_widget);
  }
  // else: this->meta_widget stays nullptr
}

} // namespace hesiod
