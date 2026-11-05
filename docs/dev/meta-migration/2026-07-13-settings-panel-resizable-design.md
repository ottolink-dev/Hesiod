# Resizable node-settings panel (+ scroll fallback) — Design

**Date:** 2026-07-13
**Repo:** `otto-link/Hesiod` (Hesiod-side only)
**Status:** Design approved; ready for implementation plan.
**Context:** Deferred finding from the Meta migration — Meta node panels clip because the settings
panel is hard-fixed at 360px (`node_settings_widget.cpp:28-29`, `// TODO fix this`) with horizontal
scroll off. Widest offender: the `kw` paired sliders (`LinkedSliders`) exceed 360px and drag the
whole container wide, clipping every input.

## 1. Goal

Make the node-settings panel **user-resizable** with a comfortable default width, and never clip its
contents (scroll as a last resort). This is a Hesiod-side layout fix; it does not touch Meta.

## 2. Scope

**In scope:** the settings panel's width behaviour — a horizontal splitter, removing the hard cap,
and a horizontal-scroll fallback.

**Explicitly out of scope (deferred):** the Meta-side widget sizing (Meta's `SliderFloat`/`SliderInt`
`minimumWidth` making the widgets wide in the first place) — that's Otto's library and a separate
question. This fix resolves the clipping without it. Also out: persisting the splitter position
across sessions (YAGNI).

## 3. Current layout

`GraphEditorWidget::setup_layout` (`graph_editor_widget.cpp`, ~line 130-180) uses a `QGridLayout`:
- `node_library_widget` at grid `(0, 0, 2, 1)`.
- a **vertical** `QSplitter` holding `viewer` + `graph_node_widget` at `(0, row_offset)`, with
  `graph_toolbar` at `(1, row_offset)`.
- `node_settings_widget` at `(0, row_offset + 1, 2, 1)`.

`NodeSettingsWidget` fixes its own width: `setMinimumWidth(360); setMaximumWidth(360);`
(`node_settings_widget.cpp:28-29`). Its internal scroll area is `setWidgetResizable(true)` with
`setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)` (`:75-77`).

## 4. Changes

### 4.1 `graph_editor_widget.cpp` — horizontal splitter
Introduce a **horizontal** `QSplitter` that holds two panes:
- **Pane 1 (graph area):** a container holding the existing vertical viewer/graph splitter and the
  graph toolbar (currently the `row_offset` grid column's contents). Wrap those into one widget/layout
  so they can live in a single splitter pane.
- **Pane 2:** `node_settings_widget`.

Place this horizontal splitter in the grid where the graph column + settings column previously sat
(the grid becomes `[node_library | horizontal_splitter]`). Set initial pane sizes with
`splitter->setSizes({<graph_large>, <settings_default>})` so the graph gets the majority and the
settings panel opens around ~400px, and set stretch factors so the graph pane absorbs most window
resizing (`setStretchFactor(0, 1)` graph, `setStretchFactor(1, 0)` settings).

### 4.2 `node_settings_widget.cpp` — remove the hard cap
Replace `setMinimumWidth(360); setMaximumWidth(360);` with a modest floor and no ceiling:
`setMinimumWidth(320);` (no `setMaximumWidth`). The splitter now governs the actual width.

### 4.3 `node_settings_widget.cpp` — scroll fallback
Change `scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);` →
`Qt::ScrollBarAsNeeded;` so contents wider than the current panel width scroll rather than clip.

## 5. Behaviour / data flow

- The panel opens at its default splitter width (~400px). The user drags the splitter handle to
  widen it for control-heavy nodes (e.g. a node with paired `kw` sliders) or narrow it to reclaim
  graph space.
- If the panel is narrower than a node's widgets need, a horizontal scrollbar appears inside the
  settings scroll area instead of clipping.
- The graph/viewer area keeps its existing vertical splitter behaviour, now nested inside the new
  horizontal splitter.

## 6. Error handling / edge cases

- Dragging the settings pane to its minimum (`320px`) is bounded by `setMinimumWidth`; it cannot
  collapse to zero.
- The node-library column is unchanged (still its own grid column, not in the splitter).
- No persistence: the splitter resets to its default sizes on each launch (acceptable; a
  session-persistence enhancement is deferred).

## 7. Testing

Build; GUI (user):
1. The settings panel opens at a comfortable width (~400px), and a `kw`/`LinkedSliders` node's
   inputs are **not clipped** at the default width.
2. Dragging the splitter handle **widens/narrows** the panel smoothly; the graph area takes the rest.
3. Dragging the panel narrower than its content shows a **horizontal scrollbar** (no clip).
4. The viewer/graph vertical splitter still works; the node library is unaffected; no crash.

## 8. Components summary (isolation)

| Unit | Responsibility | Depends on |
|---|---|---|
| `GraphEditorWidget` layout | host graph-area + settings in a resizable horizontal splitter | QSplitter |
| `NodeSettingsWidget` width | min-only width (no hard cap); splitter governs actual width | the splitter |
| settings scroll area | scroll horizontally when content exceeds the panel width | Qt scroll area |
