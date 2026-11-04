/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include <format>
#include <set>

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "hesiod/app/hesiod_application.hpp"
#include "hesiod/gui/project_ui.hpp"
#include "hesiod/gui/widgets/containers/graph_tabs_widget.hpp"
#include "hesiod/gui/widgets/containers/graph_workspace_widget.hpp"
#include "hesiod/gui/widgets/dialogs/node_info_dialog.hpp"
#include "hesiod/gui/widgets/dialogs/node_inspector_widget.hpp"
#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/gui/widgets/utils/gui_utils.hpp"
#include "hesiod/logger.hpp"
#include "hesiod/model/graph/graph_manager.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

// =====================================
// NodeInspectorModel
// =====================================
NodeInspectorModel::NodeInspectorModel(std::weak_ptr<GraphManager> p_graph_manager,
                                       QObject                    *parent)
    : QAbstractTableModel(parent), p_graph_manager(p_graph_manager)
{
}

int NodeInspectorModel::rowCount(const QModelIndex & /* parent */) const
{
  return static_cast<int>(this->items.size());
}

int NodeInspectorModel::columnCount(const QModelIndex & /* parent */) const
{
  return static_cast<int>(Column::COL_COUNT);
}

const NodeRowItem *NodeInspectorModel::get_item(int row) const
{
  if (row < 0 || row >= static_cast<int>(this->items.size()))
    return nullptr;

  return &this->items[static_cast<size_t>(row)];
}

QVariant NodeInspectorModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid())
    return {};

  const int row = index.row();
  if (row < 0 || row >= static_cast<int>(this->items.size()))
    return {};

  const NodeRowItem &item = this->items[static_cast<size_t>(row)];
  const int          col = index.column();

  if (role == Qt::DisplayRole)
  {
    switch (col)
    {
    case Column::COL_GRAPH:
      return QString::fromStdString(item.graph_id);

    case Column::COL_ID:
      return QString::fromStdString(item.node_id);

    case Column::COL_TYPE:
      return QString::fromStdString(item.label);

    case Column::COL_CATEGORY:
      return QString::fromStdString(item.category);

    case Column::COL_TIME_MS:
      return QString::fromStdString(std::format("{:.1f} ms", item.update_time));

    case Column::COL_EVAL_COUNT:
      return static_cast<qulonglong>(item.eval_count);

    case Column::COL_ERROR_COUNT:
      return static_cast<qulonglong>(item.error_count);

    case Column::COL_MEMORY:
      return QString::fromStdString(std::format("{:.2f} MB", item.memory_mb));

    case Column::COL_PORTS:
      return QString::fromStdString(
          std::format("{} in / {} out", item.num_inputs, item.num_outputs));

    case Column::COL_COMMENT:
      return QString::fromStdString(item.comment);

    default:
      break;
    }
  }
  else if (role == Qt::UserRole)
  {
    switch (col)
    {
    case Column::COL_TIME_MS:
      return item.update_time;

    case Column::COL_MEMORY:
      return item.memory_mb;

    default:
      break;
    }
  }
  else if (role == Qt::TextAlignmentRole)
  {
    switch (col)
    {
    case Column::COL_TIME_MS:
    case Column::COL_EVAL_COUNT:
    case Column::COL_ERROR_COUNT:
    case Column::COL_MEMORY:
    case Column::COL_PORTS:
      return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

    default:
      return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
  else if (role == Qt::FontRole)
  {
    return QFont("DejaVu Sans Mono", 9);
  }

  return {};
}

QVariant NodeInspectorModel::headerData(int             section,
                                        Qt::Orientation orientation,
                                        int             role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    switch (section)
    {
    case Column::COL_GRAPH:
      return "Graph";

    case Column::COL_ID:
      return "Node ID";

    case Column::COL_TYPE:
      return "Type";

    case Column::COL_CATEGORY:
      return "Category";

    case Column::COL_TIME_MS:
      return "Exec Time";

    case Column::COL_EVAL_COUNT:
      return "Exec Count";

    case Column::COL_ERROR_COUNT:
      return "Errors";

    case Column::COL_MEMORY:
      return "Memory";

    case Column::COL_PORTS:
      return "Ports";

    case Column::COL_COMMENT:
      return "Comment";

    default:
      break;
    }
  }

  return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags NodeInspectorModel::flags(const QModelIndex &index) const
{
  if (!index.isValid())
    return Qt::NoItemFlags;

  Qt::ItemFlags default_flags = QAbstractTableModel::flags(index);
  if (index.column() == Column::COL_COMMENT)
    default_flags |= Qt::ItemIsEditable;

  return default_flags;
}

bool NodeInspectorModel::setData(const QModelIndex &index,
                                 const QVariant    &value,
                                 int                role)
{
  if (!index.isValid() || role != Qt::EditRole)
    return false;

  const int row = index.row();
  const int col = index.column();

  if (row < 0 || row >= static_cast<int>(this->items.size()))
    return false;

  if (col != Column::COL_COMMENT)
    return false;

  NodeRowItem &item = this->items[static_cast<size_t>(row)];
  std::string  new_comment = value.toString().toStdString();
  if (item.comment == new_comment)
    return false;

  item.comment = new_comment;

  // Persist into BaseNode
  if (auto p_gm = this->p_graph_manager.lock())
  {
    if (GraphNode *graph = p_gm->get_graph_ref_by_id(item.graph_id))
    {
      if (BaseNode *base = graph->get_node_ref_by_id<BaseNode>(item.node_id))
        base->set_comment(new_comment);
    }
  }

  // Mark project as changed
  if (HSD_CTX.project_model)
    HSD_CTX.project_model->on_has_changed();

  Q_EMIT this->dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
  return true;
}

void NodeInspectorModel::update_data(const std::vector<NodeRowItem> &new_items)
{
  this->beginResetModel();
  this->items = new_items;
  this->endResetModel();
}

// =====================================
// NodeInspectorWidget
// =====================================
NodeInspectorWidget::NodeInspectorWidget(std::weak_ptr<GraphManager> p_graph_manager,
                                         QWidget                    *parent)
    : QDialog(parent), p_graph_manager(p_graph_manager)
{
  Logger::log()->trace("NodeInspectorWidget::NodeInspectorWidget");

  this->setWindowTitle("Hesiod - Node Inspector");
  this->setWindowModality(Qt::NonModal);
  this->setWindowFlags(Qt::Window);
  this->setMinimumSize(850, 420);

  this->setup_layout();
  this->setup_connections();
  this->restore_window_state();
  this->refresh_content();
}

NodeInspectorWidget::~NodeInspectorWidget()
{
  Logger::log()->trace("NodeInspectorWidget::~NodeInspectorWidget");
  this->save_window_state();
}

void NodeInspectorWidget::closeEvent(QCloseEvent *event)
{
  this->save_window_state();
  HSD_CTX.app_settings.window.show_node_inspector = false;
  QDialog::closeEvent(event);
}

void NodeInspectorWidget::hideEvent(QHideEvent *event)
{
  this->save_window_state();
  HSD_CTX.app_settings.window.show_node_inspector = false;
  QDialog::hideEvent(event);
}

void NodeInspectorWidget::on_double_clicked(const QModelIndex &index)
{
  if (!index.isValid())
    return;

  // Don't intercept double click on editable comment column
  if (index.column() == NodeInspectorModel::Column::COL_COMMENT)
    return;

  QModelIndex        source_idx = this->proxy_model->mapToSource(index);
  const NodeRowItem *item = this->table_model->get_item(source_idx.row());
  if (!item)
    return;

  ProjectUI *ui = HSD_APP->get_project_ui_ref();
  if (!ui)
    return;

  GraphTabsWidget *tabs = ui->get_graph_tabs_widget_ref();
  if (!tabs)
    return;

  // switch tab to graph
  tabs->set_selected_tab(item->graph_id);

  // locate workspace & graph node widget
  GraphWorkspaceWidget *gww = tabs->get_workspace_widget(item->graph_id);
  if (!gww)
    return;

  GraphNodeWidget *gnw = gww->get_graph_node_widget();
  if (!gnw)
    return;

  // select node and center on it
  if (gnw->scene())
    gnw->scene()->clearSelection();

  gnw->set_node_as_selected(item->node_id);

  if (auto *gfx = gnw->get_graphics_node_by_id(item->node_id))
    gnw->centerOn(gfx);

  // open node info dialog as well for detailed inspection
  gnw->on_node_info(item->node_id);
}

void NodeInspectorWidget::on_filter_changed()
{
  QString text = this->search_edit->text();
  this->proxy_model->setFilterRegularExpression(
      QRegularExpression(QRegularExpression::escape(text),
                         QRegularExpression::CaseInsensitiveOption));
}

void NodeInspectorWidget::refresh_content()
{
  Logger::log()->trace("NodeInspectorWidget::refresh_content");

  auto p_gm = this->p_graph_manager.lock();
  if (!p_gm)
    return;

  // remember currently selected graph filter
  QString current_filter = this->graph_filter_combo->currentText();

  // update graph filter combo options
  std::set<std::string> graph_ids;
  for (const auto &[gid, _] : p_gm->get_graph_nodes())
    graph_ids.insert(gid);

  this->graph_filter_combo->blockSignals(true);
  this->graph_filter_combo->clear();
  this->graph_filter_combo->addItem("All Graphs");
  for (const auto &gid : graph_ids)
    this->graph_filter_combo->addItem(QString::fromStdString(gid));

  int filter_idx = this->graph_filter_combo->findText(current_filter);
  if (filter_idx != -1)
    this->graph_filter_combo->setCurrentIndex(filter_idx);
  else
    this->graph_filter_combo->setCurrentIndex(0);
  this->graph_filter_combo->blockSignals(false);

  std::string target_graph = this->graph_filter_combo->currentText().toStdString();

  // collect node data
  std::vector<NodeRowItem> rows;

  for (const auto &[gid, p_graph_node] : p_gm->get_graph_nodes())
  {
    if (target_graph != "All Graphs" && gid != target_graph)
      continue;

    if (!p_graph_node)
      continue;

    for (const auto &[nid, p_node] : p_graph_node->get_nodes())
    {
      BaseNode *base = dynamic_cast<BaseNode *>(p_node.get());
      if (!base)
        continue;

      NodeRowItem row;
      row.graph_id = gid;
      row.node_id = base->get_id();
      row.label = base->get_label();
      row.category = base->get_category();
      row.is_dirty = base->is_dirty;
      row.memory_mb = base->get_memory_usage();
      row.comment = base->get_comment();

      NodeRuntimeInfo runtime = base->get_runtime_info();
      row.update_time = runtime.update_time;
      row.eval_count = runtime.eval_count;
      row.error_count = runtime.error_count;

      for (int k = 0; k < base->get_nports(); ++k)
      {
        if (base->get_port_type(k) == gnode::PortType::IN)
          row.num_inputs++;
        else
          row.num_outputs++;
      }

      rows.push_back(row);
    }
  }

  this->table_model->update_data(rows);
}

void NodeInspectorWidget::restore_window_state()
{
  const auto &geom = HSD_CTX.app_settings.window.geom_node_inspector;

  if (geom.w > 0 && geom.h > 0)
  {
    this->move(geom.x, geom.y);
    this->resize(geom.w, geom.h);
  }
}

void NodeInspectorWidget::save_window_state() const
{
  const QRect geom = this->geometry();

  HSD_CTX.app_settings.window.geom_node_inspector.x = geom.x();
  HSD_CTX.app_settings.window.geom_node_inspector.y = geom.y();
  HSD_CTX.app_settings.window.geom_node_inspector.w = geom.width();
  HSD_CTX.app_settings.window.geom_node_inspector.h = geom.height();
}

void NodeInspectorWidget::setup_connections()
{
  this->connect(this->search_edit,
                &QLineEdit::textChanged,
                this,
                &NodeInspectorWidget::on_filter_changed);

  this->connect(this->graph_filter_combo,
                &QComboBox::currentIndexChanged,
                this,
                &NodeInspectorWidget::refresh_content);

  this->connect(this->refresh_button,
                &QPushButton::clicked,
                this,
                &NodeInspectorWidget::refresh_content);

  this->connect(this->table_view,
                &QTableView::doubleClicked,
                this,
                &NodeInspectorWidget::on_double_clicked);
}

void NodeInspectorWidget::setup_layout()
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Top control bar
  QHBoxLayout *top_bar = new QHBoxLayout();
  top_bar->setSpacing(6);

  top_bar->addWidget(new QLabel("Filter:", this));

  this->search_edit = new QLineEdit(this);
  this->search_edit->setPlaceholderText("Search by node ID, type, category...");
  this->search_edit->setClearButtonEnabled(true);
  top_bar->addWidget(this->search_edit, 1);

  top_bar->addWidget(new QLabel("Graph:", this));

  this->graph_filter_combo = new QComboBox(this);
  this->graph_filter_combo->addItem("All Graphs");
  top_bar->addWidget(this->graph_filter_combo);

  this->refresh_button = new QPushButton(HSD_ICON("refresh"), "Refresh", this);
  top_bar->addWidget(this->refresh_button);

  layout->addLayout(top_bar);

  // Table model & view setup
  this->table_model = new NodeInspectorModel(this->p_graph_manager, this);

  this->proxy_model = new QSortFilterProxyModel(this);
  this->proxy_model->setSourceModel(this->table_model);
  this->proxy_model->setFilterKeyColumn(-1); // search all columns
  this->proxy_model->setSortRole(Qt::UserRole);

  this->table_view = new QTableView(this);
  this->table_view->setFont(QFont("DejaVu Sans Mono", 9));
  this->table_view->setModel(this->proxy_model);
  this->table_view->setSortingEnabled(true);
  this->table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  this->table_view->setSelectionMode(QAbstractItemView::SingleSelection);
  this->table_view->setAlternatingRowColors(true);
  this->table_view->horizontalHeader()->setStretchLastSection(true);
  this->table_view->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  this->table_view->verticalHeader()->setVisible(false);

  layout->addWidget(this->table_view, 1);
}

void NodeInspectorWidget::showEvent(QShowEvent *event)
{
  QDialog::showEvent(event);
  this->refresh_content();
}

} // namespace hesiod
