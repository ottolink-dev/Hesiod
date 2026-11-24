/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>
#include <vector>

#include <QDialog>
#include <QLineEdit>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QTableView>

#include "hesiod/model/nodes/base_node.hpp"

class QComboBox;
class QPushButton;

namespace hesiod
{

class GraphManager;

struct NodeRowItem
{
  std::string graph_id;
  std::string node_id;
  std::string label;
  std::string category;
  bool        is_dirty = false;
  float       update_time = 0.f;
  size_t      eval_count = 0;
  size_t      error_count = 0;
  float       memory_mb = 0.f;
  int         num_inputs = 0;
  int         num_outputs = 0;
  std::string comment;
};

// =====================================
// NodeInspectorModel
// =====================================
class NodeInspectorModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Column
  {
    COL_GRAPH = 0,
    COL_ID,
    COL_TYPE,
    COL_CATEGORY,
    COL_TIME_MS,
    COL_EVAL_COUNT,
    COL_ERROR_COUNT,
    COL_MEMORY,
    COL_PORTS,
    COL_COMMENT,
    COL_COUNT
  };

  explicit NodeInspectorModel(std::weak_ptr<GraphManager> p_graph_manager,
                              QObject                    *parent = nullptr);

  int           columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant      data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant      headerData(int             section,
                           Qt::Orientation orientation,
                           int             role = Qt::DisplayRole) const override;
  int           rowCount(const QModelIndex &parent = QModelIndex()) const override;
  bool          setData(const QModelIndex &index,
                        const QVariant    &value,
                        int                role = Qt::EditRole) override;
  const NodeRowItem *get_item(int row) const;

  void update_data(const std::vector<NodeRowItem> &new_items);

private:
  std::weak_ptr<GraphManager> p_graph_manager;
  std::vector<NodeRowItem>    items;
};

// =====================================
// NodeInspectorWidget
// =====================================
class NodeInspectorWidget : public QDialog
{
  Q_OBJECT

public:
  explicit NodeInspectorWidget(std::weak_ptr<GraphManager> p_graph_manager,
                               QWidget                    *parent = nullptr);
  ~NodeInspectorWidget() override;

  void restore_window_state();
  void save_window_state() const;

public slots:
  void refresh_content();

protected:
  void closeEvent(QCloseEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void on_double_clicked(const QModelIndex &index);
  void on_filter_changed();

private:
  void setup_connections();
  void setup_layout();

  std::weak_ptr<GraphManager> p_graph_manager;
  QLineEdit                  *search_edit = nullptr;
  QComboBox                  *graph_filter_combo = nullptr;
  QPushButton                *refresh_button = nullptr;
  QTableView                 *table_view = nullptr;
  NodeInspectorModel         *table_model = nullptr;
  QSortFilterProxyModel      *proxy_model = nullptr;
};

} // namespace hesiod
