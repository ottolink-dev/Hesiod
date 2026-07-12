/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QPointer>
#include <QVBoxLayout>

#include "hesiod/gui/widgets/graph_node_widget.hpp"
#include "hesiod/gui/widgets/node_attributes_widget.hpp"

namespace hesiod
{

class GraphNode; // forward

// =====================================
// NodeSettingsWidget
// =====================================
class NodeSettingsWidget : public QWidget
{
public:
  NodeSettingsWidget(QPointer<GraphNodeWidget> p_graph_node_widget,
                     QWidget                  *parent = nullptr);

private:
  void setup_connections();
  void setup_layout();
  void update_content();
  void sync_content();

  // --- Members ---
  QPointer<GraphNodeWidget> p_graph_node_widget; // own by GraphManagerWidget
  QVBoxLayout              *attr_layout;
  std::vector<std::string>  pinned_node_ids;
  std::vector<std::string>  last_selected_ids; // kept so the pane does not blank
                                               // out when nodes are deselected
  std::vector<QPointer<NodeAttributesWidget>> attr_widgets;
};

} // namespace hesiod
