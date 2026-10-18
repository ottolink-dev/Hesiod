/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <QRect>

namespace hesiod::rail
{

// What a ToolRail asks of the controls it belongs to: the 3D view's
// (ViewportControls), or the node editor's (GraphControls).
class RailOwner
{
public:
  virtual ~RailOwner() = default;

  virtual void close_panel(bool animate) = 0; // before the rail moves
  virtual void notify_layout_changed() = 0;   // the docked rail moved
  virtual void on_tool_activated(int tool, const QRect &item_rect) = 0;
};

} // namespace hesiod::rail
