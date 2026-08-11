/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QDialog>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QPointer>
#include <QVBoxLayout>
#include <QWidget>

#include "hesiod/gui/widgets/graph_node_widget.hpp"

namespace hesiod
{

struct NodePointers
{
  BaseNode            *node = nullptr;
  gngui::GraphicsNode *gfx = nullptr;
};

// =====================================
// NodeInfoDialog
// =====================================
class NodeInfoDialog : public QDialog
{
  Q_OBJECT

public:
  NodeInfoDialog() = default;
  NodeInfoDialog(QPointer<GraphNodeWidget> p_graph_node_widget,
                 const std::string        &node_id,
                 QWidget                  *parent = nullptr);

public slots:
  void update_content();

private:
  void setup_connections();
  void setup_layout();
  void update_info_content();
  void update_ports_content();

  NodePointers get_node_pointers() const;
  void         on_comment_text_changed();

  // members
  QPointer<GraphNodeWidget> p_graph_node_widget;
  std::string               node_id;
  gnode::EventConnection    after_conn;

  // UI
  QVBoxLayout    *layout;
  QGridLayout    *grid_ports;
  QGridLayout    *grid_info;
  QPlainTextEdit *editor;
};

} // namespace hesiod