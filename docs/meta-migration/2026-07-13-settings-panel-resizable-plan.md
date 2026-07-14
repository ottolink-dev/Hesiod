# Resizable node-settings panel (+ scroll fallback) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Hesiod's node-settings panel user-resizable (horizontal splitter, ~400px default) and never clip its contents, replacing the hard-coded 360px cap.

**Architecture:** Wrap the graph area (the existing vertical viewer/graph splitter + the graph toolbar) and the settings panel in a new horizontal `QSplitter`; drop `NodeSettingsWidget`'s `min==max==360` to a min-only floor; set its scroll area to `ScrollBarAsNeeded`. Hesiod-side only; Meta untouched.

**Tech Stack:** C++20, Qt6 (QSplitter, QVBoxLayout, QGridLayout). Spec: `docs/meta-migration/2026-07-13-settings-panel-resizable-design.md`.

## Global Constraints

- Hesiod-side only (`otto-link/Hesiod`, branch `feature/meta-migration`). No Meta change, no submodule pin change. Do NOT push / open PRs unless explicitly asked.
- Deferred (do NOT do here): Meta-side widget `minimumWidth` sizing; splitter-position persistence.
- Build env (repo precedent): build via `nix develop ~/quixote#cpp-qt-desktop`, cap `-j4`, run detached. GUI verification is user-driven.
- Staging discipline: stage ONLY the two named files; never `git add -A`/`.`; the dirty `external/{GNode,GNodeGUI,HighMap}` submodules and untracked files must never be staged.

---

## Task 1: Resizable settings panel via a horizontal splitter

**Files:**
- Modify: `Hesiod/src/gui/widgets/graph_editor_widget.cpp` (`setup_layout`, ~lines 140-180)
- Modify: `Hesiod/src/gui/widgets/node_settings_widget.cpp` (width cap ~lines 28-29; scroll policy ~line 75)

**Interfaces:** none consumed/produced across tasks (single self-contained layout change).

- [ ] **Step 1: Remove the hard width cap in `node_settings_widget.cpp`**

Replace the fixed cap (currently `this->setMinimumWidth(360); this->setMaximumWidth(360);`, ~line 28-29) with a min-only floor:
```cpp
  this->setMinimumWidth(320); // splitter governs the actual width; never collapses below this
```
(Delete the `setMaximumWidth(360)` line entirely.)

- [ ] **Step 2: Set the scroll-area horizontal policy to AsNeeded**

In `node_settings_widget.cpp`, change (~line 75):
```cpp
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
```
to:
```cpp
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
```

- [ ] **Step 3: Restructure `graph_editor_widget.cpp` into a horizontal splitter**

Read `setup_layout` (~lines 122-180). Replace the three blocks — `// left pan with splitter`, `// right pan`, and `// bottom toolbar` (the current lines ~140-180) — with a graph container + a horizontal splitter. Keep the `// optional left pan for node library` block above unchanged. New code:
```cpp
  // graph area: viewer/graph vertical splitter + toolbar, wrapped so it can be
  // one pane of the horizontal splitter below.
  QWidget     *graph_container = new QWidget();
  QVBoxLayout *graph_layout = new QVBoxLayout(graph_container);
  graph_layout->setContentsMargins(0, 0, 0, 0);
  graph_layout->setSpacing(0);

  {
    QSplitter *splitter = new QSplitter(Qt::Vertical);
    splitter->setChildrenCollapsible(false);

    this->graph_node_widget = new GraphNodeWidget(gno->get_shared());

    // skip the 3D viewer (OpenGL) in headless CLI modes (e.g. --snapshot).
    if (!HSD_CTX.headless)
    {
      this->viewer = new Viewer3D(this->graph_node_widget);
      this->viewer->setMinimumHeight(32);
      splitter->addWidget(this->viewer);
    }

    splitter->addWidget(this->graph_node_widget);
    graph_layout->addWidget(splitter);
  }

  {
    auto *graph_toolbar = new GraphToolbar(this->graph_node_widget);
    graph_layout->addWidget(graph_toolbar);
  }

  // settings panel (created after graph_node_widget, which it takes).
  this->node_settings_widget = new NodeSettingsWidget(this->graph_node_widget);
  {
    std::string color = HSD_CTX.app_settings.colors.border.name().toStdString();
    set_style(this->node_settings_widget,
              std::format("border-left: 1px solid {};", color));
    this->node_settings_widget->setVisible(
        HSD_CTX.app_settings.node_editor.show_node_settings_pan);
  }

  // horizontal splitter: [ graph area | settings ] — user-resizable.
  QSplitter *h_splitter = new QSplitter(Qt::Horizontal);
  h_splitter->setChildrenCollapsible(false);
  h_splitter->addWidget(graph_container);
  h_splitter->addWidget(this->node_settings_widget);
  h_splitter->setStretchFactor(0, 1); // graph area absorbs window resizing
  h_splitter->setStretchFactor(1, 0); // settings keeps its width
  h_splitter->setSizes({900, 400});   // default: graph large, settings ~400px

  layout->addWidget(h_splitter, 0, row_offset, 2, 1);
```
Ensure `#include <QVBoxLayout>` is present at the top of the file (`<QSplitter>` is already included at line 5); add it if missing. Confirm `graph_node_widget`, `viewer`, `node_settings_widget` are members (they are — assigned via `this->`), and that nothing after `setup_layout`'s replaced region referenced the old local `graph_toolbar` (it was a block-local; safe).

- [ ] **Step 4: Build Hesiod (controller, detached)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target hesiod 2>&1 | tail -20; echo EXIT=${PIPESTATUS[0]}'
```
Expected: `Linking CXX executable ... bin/hesiod`, `EXIT=0`. (Run in background per env notes.)

- [ ] **Step 5: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/src/gui/widgets/graph_editor_widget.cpp Hesiod/src/gui/widgets/node_settings_widget.cpp
git commit -m "feat(gui): resizable node-settings panel via horizontal splitter (drop 360px cap)"
```

- [ ] **Step 6: GUI verification (user-driven)**

Launch `build/bin/hesiod`. Confirm:
1. The settings panel opens at a comfortable width (~400px); add a `Noise` node and select it — the **Spatial Frequency (`kw`) linked sliders are not clipped** at the default width.
2. **Drag the splitter handle** between the graph and the settings panel → the panel **widens/narrows** smoothly; the graph area takes the remaining space.
3. Drag the panel **narrower** than its content → a **horizontal scrollbar** appears in the settings area (no clipping).
4. The **viewer/graph vertical splitter** still works; the **node library** pane is unaffected; nodes still add/select/compute; no crash.

---

## Self-Review notes

- **Spec coverage:** §4.1 horizontal splitter (Step 3), §4.2 min-only cap (Step 1), §4.3 scroll AsNeeded (Step 2), §7 testing (Steps 4-6). §2 out-of-scope (Meta widget sizing, persistence) correctly excluded.
- **Placeholder scan:** none — concrete code and the `{900, 400}` default sizes are explicit.
- **Type consistency:** single task; `graph_container`/`h_splitter` are local; `graph_node_widget`/`viewer`/`node_settings_widget` remain the existing members, created in the same order (graph_node_widget before node_settings_widget) as today.
- **External-API confirmations flagged:** the `<QVBoxLayout>` include and that no code after the replaced region uses the old block-local `graph_toolbar` are directed to confirm against the source.
- **One task, by design:** the cap removal (Steps 1-2) and the splitter (Step 3) are coupled — removing the cap without the splitter leaves the panel width ungoverned — so they ship and verify together.
