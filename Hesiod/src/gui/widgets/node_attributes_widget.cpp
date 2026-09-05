/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QAction>
#include <QLayout>
#include <QMenu>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>

#include "meta_qt/container_group_widget.hpp"
#include "meta_qt/ui/design_registry.hpp"
#include "meta_qt/ui/theme.hpp"
#include "meta_qt/widgets/collapsible_section.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/documentation_popup.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/gui/widgets/properties_panel_design.hpp"
#include "hesiod/gui/widgets/properties_panel_header.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

namespace
{

/** @brief True when any attribute anywhere in the group declares a ui.category.
 *
 * A node that builds its own sections does not want a root one wrapped around
 * them. Every container is checked, not just the current one, because the root
 * category is decided once for the whole group: reading only the current
 * container meant switching to a container whose attributes are all
 * uncategorised put a second card around the sections of the others.
 */
bool has_categorised_attributes(BaseNode &node)
{
  for (const auto &[name, p_container] : node.get_meta_group().containers())
  {
    if (!p_container)
      continue;

    for (const auto &key : p_container->insertion_order())
      if (const auto *p_attr = p_container->find(key))
        if (!meta::common::category(*p_attr).empty())
          return true;
  }

  return false;
}

} // namespace

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
}

QWidget *NodeAttributesWidget::create_toolbar()
{
  Logger::log()->trace("NodeAttributesWidget::create_toolbar");

  QWidget     *toolbar = new QWidget(this);
  QHBoxLayout *layout = new QHBoxLayout(toolbar);
  layout->setContentsMargins(0, 0, 0, 0);
  const PropertiesPanelDesign &panel = properties_panel_design();
  if (panel.has_own_chrome)
  {
    layout->setSpacing(2);
    toolbar->setObjectName("PropertiesPanelActions");
    toolbar->setStyleSheet(properties_action_style(*panel.theme));
    toolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  // Every entry is a QAction rather than a bare button, because the same
  // handful of them now appear in two places: three as buttons on the bar and
  // the rest as rows in the overflow menu. An action carries its own icon and
  // label, so a menu row and a button tooltip cannot drift apart, and each
  // behaviour is still connected exactly once.
  auto make_action = [&](const QIcon &icon, const QString &text)
  { return new QAction(icon, text, toolbar); };

  auto *update_act = make_action(HSD_ICON("refresh"), "Force Update");
  auto *reset_act = make_action(HSD_ICON("settings_backup_restore"), "Reset Settings");
  auto *help_act = make_action(HSD_ICON("help"), "Help");
  auto *info_act = make_action(HSD_ICON("info"), "Node Information");
  auto *bckp_act = make_action(HSD_ICON("bookmark"), "Backup State");
  auto *revert_act = make_action(HSD_ICON("u_turn_left"), "Revert State");
  auto *load_act = make_action(HSD_ICON("file_open"), "Load Preset");
  auto *save_act = make_action(HSD_ICON("save"), "Save Preset");
  auto *doc_act = make_action(HSD_ICON("link"), "Online Documentation");

  // Visible on the bar: the three that get reached while actually working on a
  // node. Nine unlabelled icons of equal weight gave no clue which of them a
  // user wants most of the time, and re-running the node is not the same kind
  // of thing as exporting a preset to disk.
  for (auto *action : {update_act, reset_act, help_act})
  {
    QToolButton *btn = new QToolButton;
    btn->setDefaultAction(action);
    layout->addWidget(btn);
  }

  if (!panel.has_own_chrome) layout->addStretch();

  // Everything else, by name. These are the occasional ones, and a menu row
  // spelling out "Backup State" is easier to find than a bookmark glyph the
  // user has to hover to identify.
  auto *overflow_btn = new QToolButton;
  overflow_btn->setIcon(HSD_ICON("more_horiz"));
  overflow_btn->setToolTip("More Actions");
  overflow_btn->setAccessibleName("More Actions");
  overflow_btn->setPopupMode(QToolButton::InstantPopup);

  auto *overflow_menu = new QMenu(overflow_btn);
  overflow_menu->addAction(info_act);
  overflow_menu->addSeparator();
  overflow_menu->addAction(bckp_act);
  overflow_menu->addAction(revert_act);
  overflow_menu->addSeparator();
  overflow_menu->addAction(load_act);
  overflow_menu->addAction(save_act);
  overflow_menu->addSeparator();
  overflow_menu->addAction(doc_act);

  if (panel.has_own_chrome)
  {
    const auto &theme = *panel.theme;
    overflow_menu->setWindowFlag(Qt::NoDropShadowWindowHint);
    overflow_menu->setFont(meta::qt::ui_font(12));
    overflow_menu->setStyleSheet(
        QString("QMenu { background: %1; color: %2; border: 1px solid %3;"
                " border-radius: 8px; padding: 6px; }"
                "QMenu::item { padding: 7px 18px 7px 10px; border-radius: 4px; }"
                "QMenu::item:selected { background: %4; }"
                "QMenu::separator { height: 1px; background: %3; margin: 5px 8px; }")
            .arg(theme.bar.name(), theme.ink_primary.name(),
                 theme.field_border.name(), theme.section_header_hover.name()));
  }

  overflow_btn->setMenu(overflow_menu);
  layout->addWidget(overflow_btn);
  if (panel.has_own_chrome)
    for (auto *button : toolbar->findChildren<QToolButton *>())
    {
      button->setFixedSize(28, 28);
      button->setIconSize(QSize(16, 16));
      button->setCursor(Qt::PointingHandCursor);
      if (button->defaultAction())
        button->setAccessibleName(button->defaultAction()->text());
    }

  // --- connections

  // use node id + graph_node instead of the node pointer for safety
  // (no lifetime warranty on p_node)
  this->connect(update_act,
                &QAction::triggered,
                this,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  gno->update(this->node_id);
                });

  this->connect(info_act,
                &QAction::triggered,
                this,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;

                  if (this->p_graph_node_widget)
                    this->p_graph_node_widget->on_node_info(this->node_id);
                });

  // State/preset buttons operate on the Meta container json (snapshot manager
  // for state, json_to/json_from for presets).
  auto meta_container = [this]() -> meta::AttributeContainer *
  {
    auto gno = this->p_graph_node.lock();
    if (!gno)
      return nullptr;
    BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
    if (!p_node)
      return nullptr;
    return &p_node->get_meta_group().current();
  };

  this->connect(bckp_act,
                &QAction::triggered,
                this,
                [this, meta_container]()
                {
                  if (auto *c = meta_container())
                    c->snapshot_manager().save("user_state", c->json_to());
                });

  this->connect(revert_act,
                &QAction::triggered,
                this,
                [this, meta_container]()
                {
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

  this->connect(load_act,
                &QAction::triggered,
                this,
                [this, meta_container]()
                {
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

  this->connect(save_act,
                &QAction::triggered,
                this,
                [this, meta_container]()
                {
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

  this->connect(reset_act,
                &QAction::triggered,
                this,
                [this, meta_container]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;
                  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
                  if (!p_node)
                    return;

                  auto *c = meta_container();
                  if (c && !p_node->get_initial_meta_state().empty())
                  {
                    c->json_from(p_node->get_initial_meta_state(), true);
                    this->sync_from_model();
                    gno->update(this->node_id);
                  }
                });

  this->connect(help_act,
                &QAction::triggered,
                this,
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
      doc_act,
      &QAction::triggered,
      this,
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

void NodeAttributesWidget::sync_from_model()
{
  // This slot is delivered through a queued connection (see setup_layout), so it
  // can arrive after the node it mirrors has already been removed from the
  // graph. Meta's sync callbacks capture raw pointers and references into the
  // node's attribute container, so syncing against a dead node dereferences
  // freed memory. Drop the stale event instead.
  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  if (!gno->get_node_ref_by_id<BaseNode>(this->node_id))
    return;

  if (this->meta_widget)
    this->meta_widget->on_sync_widget_from_model();
}

bool NodeAttributesWidget::is_meta_backed() const { return this->meta_widget != nullptr; }

void NodeAttributesWidget::setup_layout()
{
  Logger::log()->trace("NodeAttributesWidget::setup_layout");

  auto gno = this->p_graph_node.lock();
  if (!gno)
    return;

  BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
  if (!p_node)
    return;

  // --- main layout (built once)
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setAlignment(Qt::AlignTop);
  main_layout->setSpacing(4);
  main_layout->setContentsMargins(0, 0, 0, 0);

  if (this->add_toolbar)
    main_layout->addWidget(this->create_toolbar());

  // --- Meta ContainerGroupWidget

  // Registers the designs on first call and validates the configured name.

  const PropertiesPanelDesign &panel = properties_panel_design();

  meta::qt::RowContext row_ctx;
  row_ctx.theme = panel.theme;

  // Only `this` is captured. The defaults live on the node, so reading them
  // through the live node keeps the lookup correct after the node switches
  // container: capturing the container name at build time meant a CoherentNoise
  // switched to Ridged still resolved its defaults against the fbm container,
  // and nothing ever showed as modified. It also avoids copying the whole
  // initial state into the closure, which on a Brush node is a heightmap.
  row_ctx.default_value = [this](const std::string &key) -> std::any
  {
    auto gno = this->p_graph_node.lock();
    if (!gno)
      return {};

    BaseNode *p_current = gno->get_node_ref_by_id<BaseNode>(this->node_id);
    if (!p_current)
      return {};

    const std::string container_name = p_current->get_meta_group()
                                           .current_container_name()
                                           .value_or(std::string{});

    return p_current->get_initial_default(container_name, key);
  };

  auto options = meta::qt::ContainerRenderOptions{
      .design = panel.design,
      .row_context = row_ctx,
      .category_policy = meta::qt::CategoryPolicy::CP_MERGED,
      // Uncategorised attributes would otherwise sit bare under the toolbar
      // with no card behind them, which on a node with a single parameter looks
      // like an unfinished panel rather than a small one.
      //
      // Only when the node has no categories of its own, though: adding a root
      // section to a node that already has some wraps every real section in a
      // second card, which reads as nested boxes rather than grouping.
      .root_category_name = (panel.has_own_chrome && !has_categorised_attributes(*p_node))
                                ? std::string{"Parameters"}
                                : std::string{}};

  this->meta_widget = meta::qt::render(p_node->get_meta_group(),
                                       options,
                                       this,
                                       /* render_single_group_as_a_container */ true);

  if (this->meta_widget && this->meta_widget->layout())
    this->meta_widget->layout()->setAlignment(Qt::AlignTop);

  for (auto *stacked : this->meta_widget->findChildren<QStackedWidget *>())
  {
    for (int i = 0; i < stacked->count(); ++i)
    {
      if (auto *page = stacked->widget(i))
      {
        if (page->layout())
          page->layout()->setAlignment(Qt::AlignTop);
      }
    }
  }

  for (auto *section : this->meta_widget->findChildren<meta::qt::CollapsibleSection *>())
  {
    this->connect(section,
                  &meta::qt::CollapsibleSection::expanded_state_changed,
                  this,
                  [this]()
                  {
                    this->updateGeometry();
                    if (auto *p = this->parentWidget())
                      p->updateGeometry();
                  });
  }

  // Recompute on value_changed or edit_ended depending on app settings.
  auto signal = HSD_CTX.app_settings.node_editor.live_update
                    ? &meta::qt::MetaWidget::value_changed
                    : &meta::qt::MetaWidget::edit_ended;

  this->connect(this->meta_widget,
                signal,
                this,
                [this]()
                {
                  auto gno = this->p_graph_node.lock();
                  if (!gno)
                    return;
                  gno->update(this->node_id);
                });

  if (auto *group_widget = dynamic_cast<meta::qt::ContainerGroupWidget *>(
          this->meta_widget))
  {
    group_widget->setStyleSheet("QTabBar::tab {"
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

    this->connect(group_widget,
                  &meta::qt::ContainerGroupWidget::current_container_changed,
                  this,
                  [this](const std::string &)
                  {
                    auto gno = this->p_graph_node.lock();
                    if (!gno)
                      return;
                    gno->update(this->node_id);
                  });
  }

  this->post_update_conn = p_node->post_update_event.subscribe(
      [this](gnode::Node &)
      { QMetaObject::invokeMethod(this, "sync_from_model", Qt::QueuedConnection); });

  main_layout->addWidget(this->meta_widget);
}

} // namespace hesiod
