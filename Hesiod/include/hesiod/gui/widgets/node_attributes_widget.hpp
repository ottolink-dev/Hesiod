/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include "meta_qt/container_group_widget.hpp"

#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

class BaseNode; // forward decl.
class GraphNodeWidget;

// =====================================
// NodeAttributesWidget
// =====================================
class NodeAttributesWidget : public QWidget
{
  Q_OBJECT

public:
  NodeAttributesWidget(std::weak_ptr<GraphNode>  p_graph_node,
                       const std::string        &node_id,
                       QPointer<GraphNodeWidget> p_graph_node_widget,
                       bool                      add_toolbar = false,
                       QWidget                  *parent = nullptr);

  void sync_from_model();
  bool is_meta_backed() const;

private:
  QWidget *create_toolbar();
  void     setup_layout();

  std::weak_ptr<GraphNode>  p_graph_node;
  std::string               node_id;
  QPointer<GraphNodeWidget> p_graph_node_widget;
  bool                      add_toolbar;

  meta::qt::ContainerGroupWidget    *meta_widget = nullptr;
};

} // namespace hesiod