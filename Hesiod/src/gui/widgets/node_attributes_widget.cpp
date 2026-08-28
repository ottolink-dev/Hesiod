/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <fstream>

#include <QDesktopServices>
#include <QFileDialog>
#include <QLayout>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>

#include "meta_qt/container_group_widget.hpp"
#include "meta_qt/designs/industrial/industrial.hpp"
#include "meta_qt/ui/design_registry.hpp"
#include "meta_qt/ui/theme.hpp"
#include "meta_qt/widgets/collapsible_section.hpp"

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/documentation_popup.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

namespace
{

/** @brief Look up an attribute's default value from a node's initial state.
 *
 * The panel needs this because "modified" means `|value - default| > 1e-4`,
 * not "changed since the panel opened", and a Meta attribute does not carry
 * its own default. BaseNode captures one in finalize_attributes().
 *
 * The initial state is a ContainerGroup snapshot:
 *   { "current": name, "containers": { <container>: { <attr>: { "value": ... } } } }
 *
 * Returns an empty std::any for anything it cannot resolve -- an unknown
 * default is reported as *not* modified, since a panel that paints every row
 * as modified is worse than one that paints none.
 */
std::any lookup_default(const nlohmann::json    &initial_state,
                        const std::string       &container_name,
                        const std::string       &key,
                        const std::type_index   &type)
{
  if (!initial_state.contains("containers")) return {};

  const auto &containers = initial_state["containers"];
  if (!containers.contains(container_name)) return {};

  const auto &container = containers[container_name];
  if (!container.contains(key)) return {};

  const auto &entry = container[key];
  if (!entry.contains("value")) return {};

  const auto &value = entry["value"];

  try
  {
    // Only the types the design actually renders need converting; anything
    // else falls through to "unknown", which is harmless.
    if (type == std::type_index(typeid(float))) return value.get<float>();
    if (type == std::type_index(typeid(bool))) return value.get<bool>();
    if (type == std::type_index(typeid(int))) return value.get<int>();
  }
  catch (const nlohmann::json::exception &)
  {
    // A snapshot whose shape disagrees with the live attribute is a host bug,
    // not a reason to refuse to draw the row.
  }

  return {};
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

  this->connect(bckp_btn,
                &QToolButton::pressed,
                [this, meta_container]()
                {
                  if (auto *c = meta_container())
                    c->snapshot_manager().save("user_state", c->json_to());
                });

  this->connect(revert_btn,
                &QToolButton::pressed,
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

  this->connect(load_btn,
                &QToolButton::pressed,
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

  this->connect(save_btn,
                &QToolButton::pressed,
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

  this->connect(reset_btn,
                &QToolButton::pressed,
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

  // Designs register once per process; calling this unconditionally is cheap
  // and keeps the registration next to the only thing that consumes it.
  meta::qt::industrial::register_design();

  const AppSettings &settings = HSD_CTX.app_settings;
  const std::string  design = settings.interface.properties_panel_design;
  const meta::qt::Theme &theme = meta::qt::ThemeRegistry::instance().get(
      settings.interface.properties_panel_theme);

  meta::qt::RowContext row_ctx;
  row_ctx.theme = &theme;

  // Captures by value: the row renderer outlives this function, and the widget
  // owns the node only weakly.
  const nlohmann::json initial_state = p_node->get_initial_meta_state();
  const std::string    container_name = p_node->get_meta_group()
                                         .current_container_name()
                                         .value_or(std::string{});

  row_ctx.default_value = [initial_state, container_name, this](
                              const std::string &key) -> std::any
  {
    auto gno = this->p_graph_node.lock();
    if (!gno) return {};

    BaseNode *p_current = gno->get_node_ref_by_id<BaseNode>(this->node_id);
    if (!p_current) return {};

    auto *p_attr = p_current->get_meta_group().current().find(key);
    if (!p_attr) return {};

    return lookup_default(initial_state, container_name, key, p_attr->type());
  };

  auto options = meta::qt::ContainerRenderOptions{
      .category_policy = meta::qt::CategoryPolicy::CP_MERGED,
      .root_category_name = std::string{}};

  // An unregistered design name resolves nothing and every row falls back to
  // the stock renderer, so "stock" is a working value here rather than a error.
  options.row_renderer = [design, row_ctx](meta::AbstractAttribute *p_attr)
  { return meta::qt::render_row(p_attr, design, row_ctx); };

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
