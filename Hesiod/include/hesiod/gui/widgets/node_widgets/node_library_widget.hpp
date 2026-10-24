/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QLineEdit>
#include <QPointer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "nlohmann/json.hpp"

namespace hesiod
{

// =====================================
// LibraryTreeWidget
// =====================================

// specialized QTreeWidget for drag-and-drop
class LibraryTreeWidget : public QTreeWidget
{
  Q_OBJECT
public:
  explicit LibraryTreeWidget(QWidget *parent = nullptr);

signals:
  void node_type_dragged(const std::string &node_type);

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;

private:
  QPoint drag_start_pos;
};

// =====================================
// NodeLibraryWidget
// =====================================
class NodeLibraryWidget : public QWidget
{
  Q_OBJECT

public:
  // --- Constructor ---
  NodeLibraryWidget(QWidget *parent = nullptr);

  // --- Serialization ---
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

signals:
  // --- UI ---
  void node_type_selected(const std::string &node_type);
  void node_type_selected_ctrl(const std::string &node_type);
  void node_type_selected_shift(const std::string &node_type);
  void node_type_dragged(const std::string &node_type);

private:
  void filter_nodes(const QString &text);

  // --- Setup ---
  void setup_connections();
  void setup_layout();

  // --- Members ---
  LibraryTreeWidget *tree_widget = nullptr;
  QLineEdit         *search_bar = nullptr;
};

} // namespace hesiod
