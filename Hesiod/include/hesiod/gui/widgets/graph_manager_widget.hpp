/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include <QAction>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include "nlohmann/json.hpp"

#include "hesiod/gui/widgets/coord_frame_widget.hpp"
#include "hesiod/model/graph/broadcast_param.hpp"

#define LIST_ITEM_HEIGHT 64

namespace hesiod
{

// forward
class GraphNode;
class GraphManager; // TODO remove
class GraphManager;

// =====================================
// GraphQListWidget
// =====================================
class GraphQListWidget : public QWidget
{
  Q_OBJECT

public:
  GraphQListWidget(std::weak_ptr<GraphNode> p_graph_node, QWidget *parent = nullptr);

  // --- Accessors ---
  std::string get_current_bg_tag() const;
  GraphNode  *get_p_graph_node();

  // --- Serialization ---
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

signals:
  void bg_image_updated(const std::string &graph_id, const QImage &image);
  void has_changed();

public slots:
  // --- User Actions ---
  void on_combobox_changed();

  // --- GUI ---
  void update_combobox();

private:
  // --- Members ---
  std::weak_ptr<GraphNode> p_graph_node;
  QComboBox               *combobox;
  std::string              current_bg_tag; // == export_tag of GraphNode
};

// =====================================
// GraphManagerWidget
// =====================================
class GraphManagerWidget : public QWidget
{
  Q_OBJECT

public:
  GraphManagerWidget(std::weak_ptr<GraphManager> p_graph_manager,
                     QWidget                    *parent = nullptr);

  void clear();

  // --- Serialization ---
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  // --- State Management ---
  void restore_window_state();
  void save_window_state() const;

signals:
  // --- UI
  void graph_removed();
  void has_changed();
  void list_reordered();
  void new_graph_added();
  void selected_graph_changed(const std::string &graph_id);
  void window_closed();

  // --- Graph
  void update_started();
  void update_finished();

public slots:
  // --- User Actions ---
  void on_apply_changes();
  void on_export();
  void on_item_double_clicked(QListWidgetItem *item);
  void on_list_reordered(const QModelIndex &, int, int, const QModelIndex &, int);
  void on_new_graph_request();
  void delete_graph(const std::string &graph_id); // no confirmation: callers ask
  void on_reseed(bool backward = false);

  // --- Graph Actions ---
  void update_combobox(const std::string &graph_id);

private slots:
  // --- Qt Events ---
  void closeEvent(QCloseEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void showEvent(QShowEvent *event) override;

  // --- Internal State ---
  void reset();
  void set_is_dirty(bool new_is_dirty);

  // --- GUI ---
  void show_context_menu(const QPoint &pos);

private:
  // --- Helper(s) ---
  void add_list_item(const std::string &id);

  // --- Members ---
  std::weak_ptr<GraphManager> p_graph_manager;
  CoordFrameWidget           *coord_frame_widget;
  QListWidget                *list_widget;
  QPushButton                *apply_button;
  bool                        is_dirty = false;
};

} // namespace hesiod