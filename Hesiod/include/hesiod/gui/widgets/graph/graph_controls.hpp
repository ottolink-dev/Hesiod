/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QObject>

#include "nlohmann/json.hpp"

#include "hesiod/gui/widgets/rail/rail_owner.hpp"

namespace hesiod
{

class GraphNodeWidget;

namespace rail
{
class ToolRail;
class SnapGuide;
} // namespace rail

// =====================================
// GraphControls
// =====================================

// The node editor's toolbar, in place of GNodeGUI's own: a ToolRail, as the 3D
// view has, with the graph's tools. It floats over the editor, docked to one
// of its edges, and can be dragged to any other.
class GraphControls : public QObject, public rail::RailOwner
{
public:
  explicit GraphControls(GraphNodeWidget *graph);

  nlohmann::json json_to() const; // where the user put it
  void           json_from(const nlohmann::json &json);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void close_panel(bool) override {} // its tools open no panel
  void notify_layout_changed() override {}
  void on_tool_activated(int tool, const QRect &item_rect) override;
  void set_updating(bool new_state); // the stop button is lit meanwhile

  GraphNodeWidget *graph;
  rail::ToolRail  *rail = nullptr;
  rail::SnapGuide *guide = nullptr;
  bool             updating = false;
};

} // namespace hesiod
