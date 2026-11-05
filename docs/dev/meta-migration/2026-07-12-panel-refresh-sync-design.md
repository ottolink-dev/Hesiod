# Meta panel refresh: sync-not-rebuild + live preview — Design

**Date:** 2026-07-12
**Repos:** `otto-link/Meta` + `otto-link/Hesiod`
**Status:** Design approved; ready for implementation plan.
**Context:** "Refresh approach #1", deferred from the G1 drag fix (`docs/meta-migration/2026-07-10-*`).
Replaces the pragmatic `edit_ended` workaround with the proper refresh architecture: meta-backed
node panels **sync** their live widgets from the model on recompute instead of being torn down and
rebuilt, enabling **live preview during drag** and refreshing data-providers without re-running
them every frame.

## 1. Goal

When a meta-backed node's settings panel is open and the graph recomputes, refresh the widgets by
**syncing them from the model** (keeping them alive) rather than destroying and rebuilding the whole
panel. This (a) enables live terrain preview while dragging a control, (b) stops the full-rebuild
cost per recompute, and (c) keeps data-provider previews (histogram, thumbnail) fresh on genuine
upstream changes while **not** re-running the expensive provider on every frame of a self-drag.

## 2. Background — why the current path forces a rebuild

- `NodeSettingsWidget::setup_connections` wires **both** `GraphViewer::selection_has_changed` and
  `GraphNodeWidget::update_finished` → `NodeSettingsWidget::update_content`
  (`node_settings_widget.cpp:44-52`), and `update_content` calls `clear_layout(attr_layout)` then
  recreates a `NodeAttributesWidget` per displayed node (`:92`, `:156`). So every recompute rebuilds
  the panel.
- Meta already provides the sync machinery — `MetaWidget::set_sync_from_model(std::function<void()>)`
  / `sync_widget_from_model()`, and `ContainerGroupWidget::on_sync_meta_widgets_from_model()` which
  fans out to each contained `MetaWidget`. **But** the per-widget sync callbacks (e.g. the RangeBar's
  in `glm_vec2.inl`) only `set_value` — they do not re-pull the `ui.data_provider`, so a sync alone
  would leave the histogram/thumbnail stale.
- The G1 fix wired meta recompute to `edit_ended` (commit-on-release) to avoid destroying a
  live-dragged widget. That works but gives no live preview and still rebuilds on every commit.

## 3. Components

### 3.1 Meta core — `MetaWidget` edit state (`meta_qt/meta_widget.{hpp,cpp}`)
- Add `bool editing_ = false;` + `bool is_editing() const;`.
- In the `MetaWidget` constructor, self-connect: `edit_started` → `editing_ = true`;
  `edit_ended` → `editing_ = false`. (These signals are already emitted by the renderer wiring.)
- Purpose: let a sync callback distinguish "this widget is mid-edit (a drag)" from "an upstream
  change triggered the sync".

### 3.2 Meta — provider re-pull on sync, guarded by edit state
- **RangeBar** (`glm_vec2.inl`, the `set_sync_from_model` lambda ~line 298): after the existing
  `bar->set_value(value)`, add — if a `ui.data_provider` is present on `attr` **and**
  `!widget->is_editing()` → call the provider and `bar->set_histogram(d.series_x, d.series_y)` when
  `d.has_series()`, in a try/catch. Capture a copy of the `meta::DataProvider` (read once at render)
  in the sync lambda so no per-sync `find`/`try_cast` is needed.
- **PointsEditor** (`std_vector_glm_vec3.inl`): ensure the branch installs a `set_sync_from_model`
  callback that syncs the points from the model (`canvas->set_points(value)`) and, when
  `!widget->is_editing()` and a provider is present, re-pulls it → `canvas->set_background_image(...)`
  when `d.has_image()`. (If the branch has no sync callback today, add one.)
- Effect: on an upstream change the previews refresh; during a self-drag they are held (cheap).

### 3.3 Hesiod — `NodeAttributesWidget` sync entry point
- Store the meta panel widget as a member (it is currently a local in `setup_layout`):
  `meta::qt::ContainerGroupWidget *meta_widget = nullptr;` (set in the `uses_meta()` branch).
- Add `void sync_from_model();` — if `meta_widget` is non-null,
  `meta_widget->on_sync_meta_widgets_from_model()`; otherwise no-op (legacy `attr::AttributesWidget`
  values are the source of truth and do not change on recompute).
- Add `bool is_meta_backed() const { return this->meta_widget != nullptr; }`.

### 3.4 Hesiod — `NodeSettingsWidget` sync-not-rebuild path
- Keep the created `NodeAttributesWidget`s alive: store them (e.g. `std::vector<QPointer<NodeAttributesWidget>>`)
  as `update_content` builds them.
- Split the refresh:
  - `selection_has_changed` → `update_content` (full rebuild — unchanged).
  - `update_finished` → a **new** `sync_content()`.
- `sync_content()`: if **every** currently-displayed `NodeAttributesWidget` `is_meta_backed()`, call
  `sync_from_model()` on each (no teardown). If **any** is legacy, fall back to `update_content()`
  (full rebuild — preserves current legacy behavior). Settings panels usually show a single node, so
  the pure-meta case (the migration target) is the common one.

### 3.5 Hesiod — revert the recompute trigger
- In `NodeAttributesWidget`'s `uses_meta()` branch, change the recompute connection from
  `meta::qt::MetaWidget::edit_ended` back to `meta::qt::MetaWidget::value_changed` (continuous), so
  dragging streams recompute for live preview. Safe now: sync-not-rebuild keeps the widget alive.

## 4. Data flow

**Dragging a RangeBar handle (live preview):**
1. Drag → RangeBar `value_changed` (continuous) → panel `value_changed` → `gno->update()` (recompute).
2. recompute → `update_finished` → `NodeSettingsWidget::sync_content` → (pure-meta) each
   `NodeAttributesWidget::sync_from_model` → `ContainerGroupWidget::on_sync_meta_widgets_from_model`
   → each widget's sync callback.
3. The dragged RangeBar's sync: `set_value(value)` (no-op — the drag already set it) and, because
   `widget->is_editing()`, **skips** the histogram re-pull. The widget is not destroyed; the terrain
   preview updates live.
4. On release: `edit_ended` → `editing_ = false`. The next sync (or any upstream-triggered recompute)
   has `!is_editing()` → **re-pulls** the provider → fresh histogram.
5. Upstream node changes with no active edit → recompute → sync → `!is_editing()` → provider re-pull
   → histogram/thumbnail refresh.

## 5. Error handling / edge cases

- A displayed node deleted mid-session → `selection_has_changed` fires → full `update_content`; a
  `QPointer` guard skips any widget that became null before `sync_content` runs.
- A faulty/throwing provider during a sync re-pull → caught in the sync callback's try/catch → the
  preview is simply not updated that cycle (no crash).
- Legacy or mixed panels → `sync_content` falls back to `update_content`; no behavior change.
- `is_editing()` defaults false; if `edit_started`/`edit_ended` are ever unbalanced, the worst case is
  a redundant provider re-pull (correct, just not optimal).

## 6. Testing

- **Meta core:** unit assertion — a `MetaWidget` reports `is_editing()==false` initially, `true` after
  `edit_started`, `false` after `edit_ended` (drive the signals directly). Build `meta_qt`.
- **Hesiod:** build. GUI (user):
  1. Drag a `Saturate` range handle → terrain updates **live during the drag** (not only on release);
     the histogram does not stutter/flash while dragging.
  2. Change the upstream node → the Saturate histogram and the Cloud thumbnail **refresh**.
  3. A legacy (non-migrated) node's panel behaves exactly as before.
  4. No crash; selecting a different node still rebuilds correctly.

## 7. Branch & pin

Continue on the existing `feature/meta-migration` branches (Meta commits + Hesiod pin bump). No PR
without explicit request.

## 8. Components summary (isolation)

| Unit | Responsibility | Depends on |
|---|---|---|
| `MetaWidget::is_editing` | expose mid-edit state from edit_started/ended | Qt signals |
| RangeBar/PointsEditor sync re-pull | refresh preview from provider unless mid-edit | is_editing, data_provider |
| `NodeAttributesWidget::sync_from_model` | forward sync to the meta panel | ContainerGroupWidget |
| `NodeSettingsWidget::sync_content` | sync alive widgets on recompute; rebuild only on selection / legacy | NodeAttributesWidget |
| recompute trigger revert | continuous value_changed for live preview | sync-not-rebuild keeping widgets alive |
