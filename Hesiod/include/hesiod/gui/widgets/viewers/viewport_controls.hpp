/* Copyright (c) 2025 Otto Link. Distributed under the terms of the GNU General Public
   License. The full license is in the file LICENSE, distributed with this software. */
#pragma once
#include <functional>
#include <map>
#include <memory>

#include <QMargins>
#include <QObject>
#include <QPointer>

#include "nlohmann/json.hpp"

#include "hesiod/gui/widgets/menu_chrome.hpp"

namespace qtr
{
class RenderWidget;
}

namespace hesiod
{

class GraphNodeWidget;
class ViewportRail;
class ViewportPanel;
class SnapGuide;

// =====================================
// ViewportControls
// =====================================

// The viewport's own toolbar and settings panels, replacing the renderer's
// built-in ImGui "Render settings" window.
//
// - A rail of tool buttons floats over the viewport, docked to one of its
//   edges. It can be dragged anywhere: while dragging it folds into a single
//   settings square, a ghost shows where it will land, and on release it
//   glides to the nearest edge and unfolds there (vertical on the left and
//   right edges, horizontal on the top and bottom ones).
// - Each tool opens a panel next to the rail: lighting (with a sun dome to
//   drag the sun around), camera, display layers, material, water and sky in
//   3D; lighting and colormap in 2D. Panels are Meta containers rendered with
//   the application's properties design, so their rows are the same animated
//   sliders, switches and collapsible sections as the node settings.
// - The first button is the graph's preview resolution.
//
// Parented to the renderer; everything it creates is a child of it.
class ViewportControls : public QObject
{
public:
  ViewportControls(qtr::RenderWidget        *renderer,
                   QPointer<GraphNodeWidget> graph,
                   QObject                  *parent = nullptr);
  ~ViewportControls() override;

  /// 0: 2D viewer, 1: 3D renderer. Rebuilds the rail for that mode.
  void set_render_type(int type);

  nlohmann::json json_to() const;
  void           json_from(const nlohmann::json &json);

  // Space the docked rail takes at each viewport edge, so other overlays
  // (the output selectors) can keep clear of it; on_layout_changed fires
  // whenever it changes.
  QMargins              reserved_margins() const;
  std::function<void()> on_layout_changed;

  // The viewer's own preview rows (which node output feeds each layer, and
  // each layer's visibility), shown in the Preview panel. Takes ownership.
  void                  set_preview_content(QWidget *content);
  std::function<void()> on_preview_opened; // refresh the rows before showing

  /// Re-lay out every viewport toolbar, e.g. after the toolbar size changed.
  static void relayout_all();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  friend class ViewportRail;

  void open_panel(int tool);
  void close_panel(bool animate = true);
  void on_tool_activated(int tool, const QRect &item_rect);
  void show_resolution_menu(const QRect &item_rect);
  void show_more_menu(const QRect &item_rect);
  void rebuild_rail();
  void reset_all();
  void notify_layout_changed();

  // A click on a tool whose menu is open closes the menu, and Qt replays the
  // press onto the rail: that press must not open the menu again.
  PopupReplayGuard menu_guard;

  QPointer<qtr::RenderWidget>            renderer;
  QPointer<GraphNodeWidget>              graph;
  ViewportRail                          *rail = nullptr;
  SnapGuide                             *guide = nullptr;
  std::map<int, QPointer<ViewportPanel>> panels;
  int                                    open_tool = -1;
  int                                    render_type = 1;
};

} // namespace hesiod
