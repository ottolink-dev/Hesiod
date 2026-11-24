# Meta panel refresh: sync-not-rebuild + live preview — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refresh meta-backed node panels by syncing live widgets from the model on recompute (keeping them alive) instead of rebuilding, enabling live drag preview while re-pulling data-providers only on genuine upstream changes.

**Architecture:** `MetaWidget` gains an `is_editing()` flag (from its own `edit_started`/`edit_ended`). The RangeBar/PointsEditor sync callbacks re-pull their `ui.data_provider` unless mid-edit. Hesiod keeps `NodeAttributesWidget`s alive and, on `update_finished`, syncs them (pure-meta panels) instead of rebuilding; the meta recompute trigger reverts to continuous `value_changed`.

**Tech Stack:** C++20, Qt6. Libraries: `meta_qt` (Meta Qt backend), Hesiod GUI. Spec: `docs/meta-migration/2026-07-12-panel-refresh-sync-design.md`.

## Global Constraints

- Meta edits commit on `external/Meta`'s `feature/meta-migration`; Hesiod edits on Hesiod's `feature/meta-migration`; bump the `external/Meta` pin in Hesiod. Do NOT push, do NOT open PRs unless explicitly asked.
- The provider re-pull on sync MUST be skipped while the widget `is_editing()` (a self-drag), so the expensive provider does not run every drag frame; it re-pulls when `!is_editing()` (upstream change or drag release).
- Legacy / mixed panels must be untouched: `sync_content()` syncs only when EVERY displayed `NodeAttributesWidget` is meta-backed; otherwise it falls back to a full `update_content()` rebuild.
- Build env (repo precedent): build via `nix develop ~/quixote#cpp-qt-desktop`, cap `-j4`, run detached. GUI verification is user-driven.
- Staging discipline: each commit stages ONLY its named files; the dirty `external/{GNode,GNodeGUI,HighMap}` submodules and untracked files must never be staged. Never `git add -A`/`.`.

---

## Task 1: `MetaWidget::is_editing()` (Meta)

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/meta_widget.hpp`
- Modify: `external/Meta/MetaUI/qt/src/meta_widget.cpp`

**Interfaces:**
- Produces: `bool MetaWidget::is_editing() const;` — true between an `edit_started` and the matching `edit_ended`.

- [ ] **Step 1: Declare `is_editing()` + the flag**

In `meta_widget.hpp`: in `public:` add `bool is_editing() const;`. In `private:` (next to `std::function<void()> sync_from_model_;`) add `bool editing_ = false;`.

- [ ] **Step 2: Self-connect the edit signals in the constructor**

Read `meta_widget.cpp` and `meta_widget.hpp` to find the constructor. If a `MetaWidget(QWidget *parent)` constructor body exists, add to it; if the ctor is implicit/defaulted, add an explicit `MetaWidget::MetaWidget(QWidget *parent) : QWidget(parent) { ... }` (declare it in the header `public:` as `MetaWidget(QWidget *parent = nullptr);` if not already declared). In the ctor body add:
```cpp
  QObject::connect(this, &MetaWidget::edit_started, this, [this]() { this->editing_ = true; });
  QObject::connect(this, &MetaWidget::edit_ended, this, [this]() { this->editing_ = false; });
```
And define the getter in the `.cpp`:
```cpp
bool MetaWidget::is_editing() const { return this->editing_; }
```
(Confirm whether an explicit ctor already exists before adding one, to avoid a duplicate-definition error.)

- [ ] **Step 3: Build `meta_qt` (controller)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target meta_qt 2>&1 | tail -8'
```
Expected: compiles + links.

- [ ] **Step 4: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add MetaUI/qt/include/meta_qt/meta_widget.hpp MetaUI/qt/src/meta_widget.cpp
git commit -m "feat(qt): MetaWidget tracks edit state via is_editing()"
```

---

## Task 2: Re-pull data-providers on sync, guarded by `is_editing()` (Meta)

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widget_renderer_inl/glm_vec2.inl` (RangeBar branch)
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widget_renderer_inl/std_vector_glm_vec3.inl` (PointsEditor branch)

**Interfaces:**
- Consumes: `MetaWidget::is_editing()` (Task 1); the existing `set_histogram` / `set_background_image` (G1/G2); the `ui.data_provider` metadata.
- Produces: on a model sync, a RangeBar/PointsEditor whose attribute has a `ui.data_provider` re-pulls it and refreshes its preview — but only when `!widget->is_editing()`.

- [ ] **Step 1: RangeBar — capture the provider and re-pull it in the sync callback**

In `glm_vec2.inl`, the `RangeBar` branch already reads the provider for the initial `set_histogram`
(the `find` + `try_cast<meta::Attribute<meta::DataProvider>>` block from G1). Refactor so the provider
is stored in a branch-scope copy usable by the sync lambda:
```cpp
      meta::DataProvider range_provider; // empty if none
      if (const auto *mp = attr.metadata().find(meta::keys::ui::data_provider))
        if (const auto *dp = mp->try_cast<meta::Attribute<meta::DataProvider>>())
          range_provider = dp->value();

      if (range_provider)
        try { meta::ProviderData d = range_provider(); if (d.has_series()) bar->set_histogram(d.series_x, d.series_y); }
        catch (...) {}
```
Then in the `widget->set_sync_from_model([...]() { ... })` lambda, add `widget` and `range_provider`
to the capture list, and AFTER the existing `bar->set_value(value)` block add:
```cpp
            if (range_provider && !widget->is_editing())
            {
              try
              {
                meta::ProviderData d = range_provider();
                if (d.has_series())
                  bar->set_histogram(d.series_x, d.series_y);
              }
              catch (...) {}
            }
```

- [ ] **Step 2: PointsEditor — re-pull the image in its existing sync callback**

In `std_vector_glm_vec3.inl`, the PointsEditor branch already has
`widget->set_sync_from_model([...]() { canvas->set_points(value); })` and, from G2, reads the provider
for the initial `set_background_image`. Store the provider in a branch-scope copy the same way:
```cpp
      meta::DataProvider points_provider;
      if (const auto *mp = attr.metadata().find(meta::keys::ui::data_provider))
        if (const auto *dp = mp->try_cast<meta::Attribute<meta::DataProvider>>())
          points_provider = dp->value();
```
(and use it for the existing initial `set_background_image`). Then add `widget` and `points_provider`
to the sync lambda's capture list and, after `canvas->set_points(value);`, add:
```cpp
            if (points_provider && !widget->is_editing())
            {
              try
              {
                meta::ProviderData d = points_provider();
                if (d.has_image())
                  canvas->set_background_image(d.image_pixels, d.image_width, d.image_height, d.image_channels);
              }
              catch (...) {}
            }
```

- [ ] **Step 3: Build `meta_qt` (controller)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target meta_qt 2>&1 | tail -8'
```
Expected: compiles + links.

- [ ] **Step 4: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add MetaUI/qt/include/meta_qt/widget_renderer_inl/glm_vec2.inl MetaUI/qt/include/meta_qt/widget_renderer_inl/std_vector_glm_vec3.inl
git commit -m "feat(qt): re-pull ui.data_provider on sync unless the widget is mid-edit"
```

---

## Task 3: `NodeAttributesWidget` — sync entry point + revert trigger (Hesiod)

**Files:**
- Modify: `Hesiod/include/hesiod/gui/widgets/node_attributes_widget.hpp`
- Modify: `Hesiod/src/gui/widgets/node_attributes_widget.cpp`

**Interfaces:**
- Consumes: `meta::qt::ContainerGroupWidget::on_sync_meta_widgets_from_model()`.
- Produces:
  - `void NodeAttributesWidget::sync_from_model();` — syncs the meta panel from the model (no-op for legacy).
  - `bool NodeAttributesWidget::is_meta_backed() const;`

- [ ] **Step 1: Add the member + method declarations to the header**

In `node_attributes_widget.hpp`: add `#include "meta_qt/container_group_widget.hpp"` (near the other includes). In `public:` add:
```cpp
  void sync_from_model();
  bool is_meta_backed() const;
```
In `private:` add:
```cpp
  meta::qt::ContainerGroupWidget *meta_widget = nullptr;
```

- [ ] **Step 2: Store the meta widget in `setup_layout`**

In `node_attributes_widget.cpp`, in the `if (p_node->uses_meta())` branch, change the local
`auto *meta_widget = new meta::qt::ContainerGroupWidget(...)` to assign the member:
```cpp
    this->meta_widget = new meta::qt::ContainerGroupWidget(p_node->meta_group(),
                                                           meta::qt::ContainerRenderOptions{},
                                                           this);
```
and use `this->meta_widget` in the rest of that branch (the `connect(...)` and `addWidget(...)`).

- [ ] **Step 3: Revert the recompute trigger to `value_changed`**

In that same branch, change the recompute connection signal from
`&meta::qt::MetaWidget::edit_ended` back to `&meta::qt::MetaWidget::value_changed` (the lambda body —
`p_graph_node.lock()` → `gno->update(node_id)` — is unchanged). Update the adjacent comment to note
recompute is continuous again (safe because the panel now syncs instead of rebuilding).

- [ ] **Step 4: Define `sync_from_model()` and `is_meta_backed()`**

In `node_attributes_widget.cpp` add:
```cpp
void NodeAttributesWidget::sync_from_model()
{
  if (this->meta_widget)
    this->meta_widget->on_sync_meta_widgets_from_model();
  // legacy attr::AttributesWidget: values are the source of truth, no model sync needed.
}

bool NodeAttributesWidget::is_meta_backed() const { return this->meta_widget != nullptr; }
```

- [ ] **Step 5: Commit (Hesiod branch) — no build yet (controller builds after Task 4)**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/include/hesiod/gui/widgets/node_attributes_widget.hpp Hesiod/src/gui/widgets/node_attributes_widget.cpp
git commit -m "feat(meta): NodeAttributesWidget exposes sync_from_model + is_meta_backed; recompute on value_changed"
```

---

## Task 4: `NodeSettingsWidget` — sync-not-rebuild + pin bump + build/GUI (Hesiod)

**Files:**
- Modify: `Hesiod/include/hesiod/gui/widgets/node_settings_widget.hpp`
- Modify: `Hesiod/src/gui/widgets/node_settings_widget.cpp`
- Modify: `external/Meta` gitlink (pin bump), committed on Hesiod's branch

**Interfaces:**
- Consumes: `NodeAttributesWidget::sync_from_model()` / `is_meta_backed()` (Task 3); the Meta branch tip (Tasks 1-2).
- Produces: on recompute, pure-meta panels sync in place; selection change and legacy/mixed panels still rebuild.

- [ ] **Step 1: Add the widget list + `sync_content()` declaration**

In `node_settings_widget.hpp`: add `#include "hesiod/gui/widgets/node_attributes_widget.hpp"` (forward-decl is insufficient — we call methods). In `private:` add:
```cpp
  std::vector<QPointer<NodeAttributesWidget>> attr_widgets;
  void                                        sync_content();
```

- [ ] **Step 2: Record each `NodeAttributesWidget` as `update_content` builds it**

In `node_settings_widget.cpp::update_content`, clear the list at the top (right after
`clear_layout(this->attr_layout);`): `this->attr_widgets.clear();`. Where the loop creates
`auto *attr_widget = new NodeAttributesWidget(...)`, after adding it to the layout, record it:
```cpp
    this->attr_widgets.push_back(attr_widget);
```

- [ ] **Step 3: Implement `sync_content()` with the mixed-panel fallback**

Add:
```cpp
void NodeSettingsWidget::sync_content()
{
  Logger::log()->trace("NodeSettingsWidget::sync_content");

  // Rebuild if the panel is empty or contains any legacy (non-meta) widget.
  if (this->attr_widgets.empty())
  {
    this->update_content();
    return;
  }
  for (const auto &w : this->attr_widgets)
    if (!w || !w->is_meta_backed())
    {
      this->update_content();
      return;
    }

  // Pure-meta panel: sync in place, no teardown.
  for (const auto &w : this->attr_widgets)
    if (w)
      w->sync_from_model();
}
```

- [ ] **Step 4: Route `update_finished` to `sync_content` (keep selection → rebuild)**

In `NodeSettingsWidget::setup_connections`, change the second connection: `GraphNodeWidget::update_finished`
should connect to `&NodeSettingsWidget::sync_content` (NOT `update_content`). Leave the
`selection_has_changed → update_content` connection unchanged.

- [ ] **Step 5: Bump the Hesiod submodule pin to the Meta branch tip**

```bash
cd /home/barrulus/dev/Hesiod
git -C external/Meta rev-parse --short HEAD   # Meta tip after Tasks 1-2 (note it)
git add external/Meta
```

- [ ] **Step 6: Build Hesiod (controller, detached)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target hesiod 2>&1 | tail -20; echo EXIT=${PIPESTATUS[0]}'
```
Expected: `Linking CXX executable ... bin/hesiod`, `EXIT=0`. (Run in background per env notes.)

- [ ] **Step 7: Commit (Hesiod branch)**

```bash
cd /home/barrulus/dev/Hesiod
git add external/Meta Hesiod/include/hesiod/gui/widgets/node_settings_widget.hpp Hesiod/src/gui/widgets/node_settings_widget.cpp
git commit -m "feat(meta): sync meta panels on recompute instead of rebuilding; bump Meta pin"
```

- [ ] **Step 8: GUI verification (user-driven)**

Launch `build/bin/hesiod`. Then:
1. `Noise → Saturate`, select Saturate, **drag a range handle** → the terrain preview updates **live while dragging** (not only on release); the histogram behind the handles does not stutter/flash during the drag.
2. Change the upstream `Noise` (seed/type) → the Saturate **histogram refreshes**.
3. `Noise → Cloud` (background) → change the Noise → the Cloud **thumbnail refreshes**.
4. Select a **legacy (non-migrated) node** → its panel behaves exactly as before; switching selection rebuilds correctly.
5. No crash.

---

## Self-Review notes

- **Spec coverage:** §3.1 (Task 1), §3.2 (Task 2), §3.3 + §3.5 (Task 3), §3.4 (Task 4). §4 data flow is realized by Tasks 2-4 together. §6 testing = `meta_qt` builds (Tasks 1-2), Hesiod build + GUI (Task 4).
- **Placeholder scan:** none — concrete code/commands throughout. The one conditional (does `MetaWidget` have an explicit ctor) is directed to confirm against the source, not left vague.
- **Type consistency:** `is_editing()` (Task 1) is used verbatim in Task 2's guards; `sync_from_model()`/`is_meta_backed()` (Task 3) are called verbatim in Task 4's `sync_content`; `meta_widget` member name consistent Task 3 → its usage; `attr_widgets` consistent Task 4.
- **External-API confirmations flagged:** the `MetaWidget` ctor existence (Task 1), the exact RangeBar/PointsEditor sync-lambda capture lists (Task 2), and the `update_content` creation site (Task 4) are each directed to the real source to confirm.
