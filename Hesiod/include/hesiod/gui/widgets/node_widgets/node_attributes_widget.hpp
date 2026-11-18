/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include "meta_qt/container_group_widget.hpp"

#include "hesiod/gui/widgets/graph/graph_node_widget.hpp"
#include "hesiod/model/graph/graph_node.hpp"

namespace hesiod
{

class BaseNode; // forward decl.
class GraphNodeWidget;
class PostProcessDock;

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

  Q_INVOKABLE void sync_from_model();
  bool             is_meta_backed() const;

  /// Build the node actions for either the attributes view or its panel header.
  QWidget *create_toolbar();

  /** @brief Hand the node's output chain over to the caller, or nullptr.
   *
   * The chain is built at the end of this widget. A panel showing one node can
   * take it out and attach it to its own bottom edge instead, where it stays
   * put while the attributes scroll. It keeps reporting its edits here; the
   * caller owns it from then on and has to delete it along with this widget.
   */
  PostProcessDock *release_post_dock();

private:
  void setup_layout();

  std::weak_ptr<GraphNode>  p_graph_node;
  std::string               node_id;
  QPointer<GraphNodeWidget> p_graph_node_widget;
  bool                      add_toolbar;
  gnode::EventConnection    post_update_conn;

  meta::qt::MetaWidget *meta_widget = nullptr;
  nlohmann::json        state_before_edit;
  bool                  is_in_edit = false;
  std::string           current_container_name;
  bool                  is_switching_container_programmatically = false;

  // nullptr on a node with no output chain; guarded because a panel that took
  // it over with release_post_dock() deletes it on its own schedule
  QPointer<PostProcessDock> post_dock;
};

} // namespace hesiod
