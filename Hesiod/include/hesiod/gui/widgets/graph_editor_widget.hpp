/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <memory>

#include "hesiod/gui/widgets/node_library_widget.hpp"
#include "hesiod/model/nodes/base_node.hpp"

namespace hesiod
{

class GraphNode; // forward
class GraphNodeWidget;
class NodeSettingsWidget;
class Viewer3D;

// =====================================
// GraphEditorWidget
// =====================================
class GraphEditorWidget : public QWidget
{
  Q_OBJECT

public:
  // --- Constructor and Setup ---
  GraphEditorWidget(std::weak_ptr<GraphNode> p_graph_node, QWidget *parent = nullptr);

  // --- Serialization ---
  void           json_from(nlohmann::json const &json);
  nlohmann::json json_to() const;

  GraphNodeWidget    *get_graph_node_widget() const;
  NodeSettingsWidget *get_node_settings_widget() const;
  Viewer3D           *get_viewer() const;

private:
  void setup_connections();
  void setup_layout();

  std::weak_ptr<GraphNode> p_graph_node;
  GraphNodeWidget         *graph_node_widget = nullptr;
  NodeSettingsWidget      *node_settings_widget = nullptr;
  NodeLibraryWidget       *node_library_widget = nullptr;
  Viewer3D                *viewer = nullptr;
};

} // namespace hesiod