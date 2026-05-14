/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <QHeaderView>
#include <QPushButton>
#include <QToolTip>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/widgets/node_library_widget.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/node_factory.hpp"
#include "hesiod/model/utils.hpp"

namespace hesiod
{

NodeLibraryWidget::NodeLibraryWidget(QWidget *parent) : QWidget(parent)
{
  Logger::log()->trace("NodeLibraryWidget::NodeLibraryWidget");

  this->search_bar = new QLineEdit(this);
  this->tree_widget = new LibraryTreeWidget(this);

  this->setMinimumWidth(256);
  this->setup_layout();
  this->setup_connections();

  this->setToolTip("Double Click: New node\nCtrl + Double Click: Replace selected "
                   "node\nShift + Double Click: Chain to selected node");
}

void NodeLibraryWidget::json_from(nlohmann::json const &json)
{
  Logger::log()->trace("NodeLibraryWidget::json_from");

  std::function<void(QTreeWidgetItem *, const nlohmann::json &)> apply =
      [&](QTreeWidgetItem *item, const nlohmann::json &obj)
  {
    std::string key = item->text(0).toStdString();
    if (!obj.contains(key))
      return;

    const nlohmann::json &entry = obj.at(key);
    item->setExpanded(entry.value("expanded", true));

    if (entry.contains("children"))
      for (int i = 0; i < item->childCount(); ++i)
        apply(item->child(i), entry.at("children"));
  };

  for (int i = 0; i < this->tree_widget->topLevelItemCount(); ++i)
    apply(this->tree_widget->topLevelItem(i), json);
}

nlohmann::json NodeLibraryWidget::json_to() const
{
  Logger::log()->trace("NodeLibraryWidget::json_to");

  nlohmann::json json;

  std::function<void(QTreeWidgetItem *, nlohmann::json &)> collect =
      [&](QTreeWidgetItem *item, nlohmann::json &obj)
  {
    if (item->childCount() == 0)
      return;

    nlohmann::json entry;
    entry["expanded"] = item->isExpanded();
    entry["children"] = nlohmann::json::object();

    for (int i = 0; i < item->childCount(); ++i)
      collect(item->child(i), entry["children"]);

    obj[item->text(0).toStdString()] = entry;
  };

  for (int i = 0; i < this->tree_widget->topLevelItemCount(); ++i)
    collect(this->tree_widget->topLevelItem(i), json);

  return json;
}

void NodeLibraryWidget::setup_connections()
{
  Logger::log()->trace("NodeLibraryWidget::setup_connections");

  // emit signal when the user clicks a leaf node (actual node type, not a category)
  this->connect(
      this->tree_widget,
      &QTreeWidget::itemDoubleClicked,
      this,
      [this](QTreeWidgetItem *item, int /*column*/)
      {
        if (item && item->childCount() == 0) // leaf = node type
        {
          std::string data = item->data(0, Qt::UserRole).toString().toStdString();

          if (QApplication::keyboardModifiers() & Qt::ControlModifier)
            Q_EMIT this->node_type_selected_ctrl(data);
          else if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
            Q_EMIT this->node_type_selected_shift(data);
          else
            Q_EMIT this->node_type_selected(data);
        }
      });

  // filter tree as the user types
  this->connect(this->search_bar,
                &QLineEdit::textChanged,
                this,
                &NodeLibraryWidget::filter_nodes);

  this->connect(this->tree_widget,
                &LibraryTreeWidget::node_type_dragged,
                this,
                &NodeLibraryWidget::node_type_dragged);
}

void NodeLibraryWidget::setup_layout()
{
  Logger::log()->trace("NodeLibraryWidget::setup_layout");

  // --- Search bar

  this->search_bar->setPlaceholderText("Search nodes...");
  this->search_bar->setClearButtonEnabled(true);

  // --- Tree

  this->tree_widget->setHeaderHidden(true);
  this->tree_widget->setIndentation(12);
  this->tree_widget->setAnimated(true);
  this->tree_widget->setSelectionMode(QAbstractItemView::NoSelection);
  this->tree_widget->header()->setStretchLastSection(true);
  this->tree_widget->header()->setSectionResizeMode(0, QHeaderView::Stretch);

  // build a nested map:  category_path -> [ node_types ]
  // e.g. "Math/Arithmetic" -> ["Add", "Subtract"]
  std::map<std::string, std::vector<std::string>> category_nodes;

  std::map<std::string, std::string> inventory = get_node_inventory();
  for (const auto &[node_type, categories_str] : inventory)
  {
    // use the full slash-joined path as the category key
    category_nodes[categories_str].push_back(node_type);
  }

  // retrieve the category → color map from the style settings
  const auto &color_map = HSD_CTX.style_settings.category_color_map;

  // keep track of QTreeWidgetItems per path so we can build the hierarchy
  std::map<std::string, QTreeWidgetItem *> path_to_item;

  auto get_or_create_category =
      [&](const std::string              &categories_str,
          const std::vector<std::string> &categories) -> QTreeWidgetItem *
  {
    if (path_to_item.count(categories_str))
      return path_to_item.at(categories_str);

    QTreeWidgetItem *parent_item = nullptr;
    std::string      current_path;

    for (size_t i = 0; i < categories.size(); ++i)
    {
      if (i > 0)
        current_path += "/";
      current_path += categories[i];

      if (!path_to_item.count(current_path))
      {
        QTreeWidgetItem *cat_item = parent_item ? new QTreeWidgetItem(parent_item)
                                                : new QTreeWidgetItem(this->tree_widget);

        cat_item->setText(0, QString::fromStdString(categories[i]));
        cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);

        // apply category color if available
        auto color_it = color_map.find(categories[i]);
        if (color_it != color_map.end())
        {
          QColor color = color_it->second;
          // cat_item->setForeground(0, QBrush(color));

          // tinted background
          QColor bg = color;
          bg.setAlphaF(0.5f);
          cat_item->setBackground(0, QBrush(bg));
        }

        cat_item->setExpanded(true);
        path_to_item[current_path] = cat_item;
      }

      parent_item = path_to_item.at(current_path);
    }

    return parent_item;
  };

  for (const auto &[categories_str, node_types] : category_nodes)
  {
    std::vector<std::string> categories = split_string(categories_str, '/');
    QTreeWidgetItem *cat_item = get_or_create_category(categories_str, categories);

    for (const std::string &node_type : node_types)
    {
      QTreeWidgetItem *node_item = new QTreeWidgetItem(cat_item);
      node_item->setText(0, QString::fromStdString(node_type));

      // store the canonical type id for retrieval in the signal
      node_item->setData(0, Qt::UserRole, QString::fromStdString(node_type));
      node_item->setFlags(node_item->flags() | Qt::ItemIsSelectable);
    }
  }

  // --- Fold / Expand buttons

  QPushButton *btn_expand = new QPushButton("Expand all", this);
  QPushButton *btn_fold = new QPushButton("Fold all", this);

  btn_expand->setFlat(true);
  btn_fold->setFlat(true);

  this->connect(btn_expand,
                &QPushButton::clicked,
                this->tree_widget,
                &QTreeWidget::expandAll);
  this->connect(btn_fold,
                &QPushButton::clicked,
                this->tree_widget,
                &QTreeWidget::collapseAll);

  QHBoxLayout *btn_layout = new QHBoxLayout();
  btn_layout->setContentsMargins(0, 0, 0, 0);
  btn_layout->setSpacing(4);
  btn_layout->addWidget(btn_expand);
  btn_layout->addWidget(btn_fold);

  // --- Assemble layout

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);
  layout->addWidget(this->search_bar);
  layout->addWidget(this->tree_widget);
  layout->addLayout(btn_layout);
  setLayout(layout);
}

void NodeLibraryWidget::filter_nodes(const QString &text)
{
  // Walk every item; hide category rows that have no visible children
  std::function<bool(QTreeWidgetItem *)> set_visibility =
      [&](QTreeWidgetItem *item) -> bool
  {
    bool any_visible = false;

    if (item->childCount() == 0)
    {
      // leaf: show if name matches (case-insensitive)
      bool match = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
      item->setHidden(!match);
      return match;
    }

    for (int i = 0; i < item->childCount(); ++i)
      any_visible |= set_visibility(item->child(i));

    item->setHidden(!any_visible);
    item->setExpanded(any_visible && !text.isEmpty());
    return any_visible;
  };

  for (int i = 0; i < this->tree_widget->topLevelItemCount(); ++i)
    set_visibility(this->tree_widget->topLevelItem(i));
}

} // namespace hesiod
