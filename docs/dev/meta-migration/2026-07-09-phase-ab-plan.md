# Meta Migration — Phase A + B Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring `otto-link/Meta` into Hesiod's build and migrate the single `Noise` node end-to-end onto it (storage, auto-rendered widgets, `.hsd` round-trip) as an isolated vertical slice that de-risks the full migration and settles the serialization approach.

**Architecture:** Meta is added as a submodule and built alongside the existing `Attributes` library (both present during transition). `BaseNode` gains a `meta::ContainerGroup` *alongside* its current `attr` map, opt-in per node via a flag. Only `Noise` flips the flag: its parameters live in a `meta::AttributeContainer`, its panel is rendered by `meta_qt`, and its serialization is prototyped in two shapes to pick the format. Every other node is untouched and keeps using `Attributes`.

**Tech Stack:** C++20, CMake ≥3.22, Qt6 (Core, Widgets), nlohmann_json, spdlog, glm — all already present in Hesiod. Meta core (`meta`), Meta Qt backend (`meta_qt`).

## Global Constraints

- All Phase A/B work happens on the single Hesiod branch `feature/meta-migration` (off `upstream/dev`), fully isolated — it never merges to Hesiod `dev`/`main` until a deliberate cutover. No CI on this branch. See the **Branch Strategy** section below.
- Phase A/B **only consumes** Meta — it does not modify it. The `external/Meta` submodule is pinned to a **frozen commit SHA** (immutable; cannot drift). No Meta branch is needed until the later phase that first edits Meta.
- Meta CMake options for Hesiod's build: `META_ENABLE_QT_UI=ON`, `META_ENABLE_GLM_TYPES=ON`, `META_ENABLE_COLOR_GRADIENT_TYPES=ON`, `META_ENABLE_FTXUI_UI=OFF`, `META_ENABLE_TESTS=OFF`.
- Meta library targets: core = `meta` (STATIC, public include dir `Meta/include`), Qt backend = `meta_qt` (links `Qt6::Core`, `Qt6::Widgets`; pulls in `MetaUI/common`).
- The migration is behavior-preserving: one `AttributeContainer` per node's `ContainerGroup`. No node identities change. Node-merging is out of scope.
- Otto's gap resolutions (issue #15) are authoritative: `kw` uses `ui.locked_xy`; grouping uses `ui.category` repertory paths (not `_GROUPBOX_*`); reset uses a `SnapshotManager` `default` state; no seed randomize widget. `ui.data_provider`/`ui.tooltip` are NOT needed by the Noise PoC.
- Build env quirks (this machine): Qt6 via Nix; run the GUI from the `data/` dir with `qtsvg` on `QT_PLUGIN_PATH`; if a wide recompile hits `qwebenginedownloadrequest.h`, add the base include via `NIX_CFLAGS_COMPILE`. The user builds and runs the GUI and pastes output — GUI verification steps are handed to the user.
- Verification model: this codebase has no per-node unit-test harness. Tasks verify by (a) a successful build/link, (b) a headless `.hsd` round-trip via the CLI where possible, and (c) explicit user-run GUI checks. TDD applies where a headless test is feasible (Task B4).

---

## Phase A — Foundations (build integration)

### Task A1: Add Meta as a submodule (frozen pin)

**Files:**
- Modify: `.gitmodules`
- Create: `external/Meta` (submodule)

**Interfaces:**
- Produces: the `external/Meta` source tree pinned to a frozen commit SHA, containing `Meta/` (core) and `MetaUI/qt/` (Qt backend). Read-only for this plan — no Meta branch (see Branch Strategy).

- [ ] **Step 1: Confirm you are on Hesiod's branch**

Run:
```bash
cd /home/barrulus/dev/Hesiod
git branch --show-current
```
Expected: `feature/meta-migration`. If not, `git switch feature/meta-migration` first.

- [ ] **Step 2: Add the submodule (pins to current Meta HEAD as a frozen SHA)**

```bash
git submodule add git@github.com:otto-link/Meta.git external/Meta
```

- [ ] **Step 3: Verify the expected layout is present**

Run:
```bash
ls external/Meta/Meta/include/meta/core/container_group.hpp external/Meta/MetaUI/qt/CMakeLists.txt external/Meta/CMakeLists.txt
```
Expected: all three paths listed (no "No such file").

- [ ] **Step 4: Record the pinned commit**

Run:
```bash
git submodule status external/Meta
```
Expected: a line beginning with the pinned SHA and `external/Meta`. Note the SHA in the commit message (this is the frozen base; a Meta branch, if later needed, is cut from it).

- [ ] **Step 5: Commit**

```bash
git add .gitmodules external/Meta
git commit -m "build(meta): add otto-link/Meta submodule (frozen pin <SHA>)"
```

### Task A2: Wire Meta into the CMake build and prove it links

**Files:**
- Modify: `external/CMakeLists.txt` (after the `Attributes` block, ~line 22)
- Modify: `Hesiod/CMakeLists.txt` (the `target_link_libraries(...)` block at line 71, add near the `attributes` entry at line 82)
- Modify: `Hesiod/src/main.cpp` (temporary smoke use — reverted in Step 6)

**Interfaces:**
- Consumes: the `external/Meta` tree from Task A1.
- Produces: `meta` and `meta_qt` as link targets available to the `hesiod` target; Hesiod builds and links against Meta.

- [ ] **Step 1: Add Meta to the external build with the required options**

In `external/CMakeLists.txt`, after the `Attributes` block (the two lines `set(ATTRIBUTES_ENABLE_TESTS OFF)` / `add_subdirectory(Attributes)`), add:

```cmake
# Meta (transitional: built alongside Attributes)
set(META_ENABLE_TESTS OFF)
set(META_ENABLE_GLM_TYPES ON)
set(META_ENABLE_COLOR_GRADIENT_TYPES ON)
set(META_ENABLE_QT_UI ON)
set(META_ENABLE_FTXUI_UI OFF)
add_subdirectory(Meta)
```

- [ ] **Step 2: Link Meta into the Hesiod target**

In `Hesiod/CMakeLists.txt`, in the `target_link_libraries(` block, add `meta` and `meta_qt` adjacent to the existing `attributes` entry (line 82):

```cmake
          attributes
          meta
          meta_qt
```

- [ ] **Step 3: Add a temporary smoke use to force a real link**

In `Hesiod/src/main.cpp`, near the top of `main()`, add (temporarily):

```cpp
#include "meta/core/attribute_container.hpp"
// ... inside main(), first lines:
{
  meta::AttributeContainer smoke;
  smoke.add<float>("smoke", 1.0f);
  spdlog::info("[meta smoke] container size = {}", smoke.size());
}
```

- [ ] **Step 4: Configure and build**

Run (from the repo's build dir; use the project's usual configure flags):
```bash
cmake -B build -S . && cmake --build build -j --target hesiod 2>&1 | tail -30
```
Expected: build completes; link succeeds; no unresolved `meta::` symbols. (If a wide recompile hits `qwebenginedownloadrequest.h`, set `NIX_CFLAGS_COMPILE` per the Global Constraints note and rebuild.)

- [ ] **Step 5: Run and confirm the smoke line**

Have the user launch the built binary and confirm the log line `[meta smoke] container size = 1` appears at startup. Expected: line present → Meta core is linked and functional.

- [ ] **Step 6: Revert the smoke use, keep the build wiring**

Remove the temporary block and include from `Hesiod/src/main.cpp` (Step 3). Rebuild to confirm still green:
```bash
cmake --build build -j --target hesiod 2>&1 | tail -5
```
Expected: build succeeds.

- [ ] **Step 7: Commit**

```bash
git add external/CMakeLists.txt Hesiod/CMakeLists.txt Hesiod/src/main.cpp
git commit -m "build(meta): compile and link meta + meta_qt alongside Attributes"
```

---

## Phase B — Vertical PoC on the Noise node

### Task B1: Add an opt-in `meta::ContainerGroup` to BaseNode

**Files:**
- Modify: `Hesiod/include/hesiod/model/nodes/base_node.hpp` (add member + accessors near the attr API, lines 87-118)
- Modify: `Hesiod/src/model/nodes/base_node.cpp` (accessor definitions)

**Interfaces:**
- Consumes: `meta::ContainerGroup` from Task A1/A2.
- Produces:
  - `bool BaseNode::uses_meta() const;`
  - `meta::ContainerGroup &BaseNode::meta_group();` (creates the group + a `"main"` container on first use, sets it current, flips the flag)
  - `const meta::ContainerGroup &BaseNode::meta_group() const;`
  These let a single node opt into Meta storage while all others keep the `attr` map.

- [ ] **Step 1: Add the member and accessors to the header**

In `base_node.hpp`, add the include `#include "meta/core/container_group.hpp"` near the `attributes/abstract_attribute.hpp` include (line 14). In the `private:` members (after line 118) add:

```cpp
  std::unique_ptr<meta::ContainerGroup> meta_group_; // opt-in Meta storage (nullptr = legacy attr map)
```

In the public Attribute-Management section (near line 106) declare:

```cpp
  bool                       uses_meta() const;
  meta::ContainerGroup      &meta_group();       // lazily creates group + "main" container
  const meta::ContainerGroup &meta_group() const;
```

- [ ] **Step 2: Define the accessors**

In `base_node.cpp` add:

```cpp
bool BaseNode::uses_meta() const { return this->meta_group_ != nullptr; }

meta::ContainerGroup &BaseNode::meta_group()
{
  if (!this->meta_group_)
  {
    this->meta_group_ = std::make_unique<meta::ContainerGroup>();
    this->meta_group_->add("main");
    this->meta_group_->set_current("main");
  }
  return *this->meta_group_;
}

const meta::ContainerGroup &BaseNode::meta_group() const { return *this->meta_group_; }
```

- [ ] **Step 3: Build**

Run:
```bash
cmake --build build -j --target hesiod 2>&1 | tail -10
```
Expected: builds clean (no node uses the new path yet).

- [ ] **Step 4: Commit**

```bash
git add Hesiod/include/hesiod/model/nodes/base_node.hpp Hesiod/src/model/nodes/base_node.cpp
git commit -m "feat(meta): add opt-in ContainerGroup storage to BaseNode"
```

### Task B2: Author the Noise node's parameters against Meta

**Files:**
- Modify: `Hesiod/src/model/nodes/nodes_function/noise.cpp` (setup + compute, whole file)

**Interfaces:**
- Consumes: `BaseNode::meta_group()` (Task B1); `meta::AttributeContainer::add<T>`, `try_add`, `value<T>`; `meta::Attribute<T>::metadata().try_add(key, value)`; metadata keys from `meta/metadata/keys.hpp` (`ui.widget_type`, `ui.category`, `constraints.min`, `constraints.max`, `constraints.enum_items`, plus ad-hoc `ui.locked_xy`).
- Produces: a `Noise` node whose parameters live in `meta_group().current()` with full UI metadata; compute reads them via `value<T>`.

- [ ] **Step 1: Read the metadata-key names to use exact strings**

Run:
```bash
sed -n '1,80p' external/Meta/Meta/include/meta/metadata/keys.hpp
```
Note the exact constants for `widget_type`, `category`, `min`, `max`, `enum_items` (namespaces `meta::keys::ui::*` and `meta::keys::constraints::*`). Use these constants, not raw strings, in the code below.

- [ ] **Step 2: Rewrite `setup_noise_node` against Meta**

Replace the `--- Attributes` and `--- Attribute(s) order` sections (noise.cpp:49-70) with Meta authoring. Ports stay unchanged. Use the noise_type label→int list from `enum_mappings.noise_type_map` to build `enum_items` (a `std::vector<std::pair<int,std::string>>`). Example shape (adjust key constants per Step 1):

```cpp
  auto &c = node.meta_group().current();

  // noise_type: int-backed enum dropdown
  {
    auto *a = c.add<int>(A_NOISE_TYPE, /*default*/ 0);
    a->metadata().try_add(meta::keys::ui::label, std::string("Type"));
    a->metadata().try_add(meta::keys::ui::widget_type, std::string("EnumComboBox"));
    a->metadata().try_add(meta::keys::ui::category, std::string("Main Parameters"));
    std::vector<std::pair<int, std::string>> items;
    for (auto &[name, val] : enum_mappings.noise_type_map) items.emplace_back(val, name);
    a->metadata().try_add(meta::keys::constraints::enum_items, items);
  }

  // kw: 2D wavenumber with X/Y lock
  {
    auto *a = c.add<glm::vec2>(A_KW, glm::vec2(2.f, 2.f));
    a->metadata().try_add(meta::keys::ui::label, std::string("Spatial Frequency"));
    a->metadata().try_add(meta::keys::ui::widget_type, std::string("LinkedSliders"));
    a->metadata().try_add(std::string("ui.locked_xy"), true);
    a->metadata().try_add(meta::keys::constraints::min, 0.f);
    a->metadata().try_add(meta::keys::constraints::max, 64.f);
    a->metadata().try_add(meta::keys::ui::category, std::string("Main Parameters"));
  }

  // seed
  {
    auto *a = c.add<int>(A_SEED, 1);
    a->metadata().try_add(meta::keys::ui::label, std::string("Seed"));
    a->metadata().try_add(meta::keys::constraints::min, 0);
    a->metadata().try_add(meta::keys::ui::category, std::string("Main Parameters"));
  }

  // periodic
  {
    auto *a = c.add<bool>(A_PERIODIC, false);
    a->metadata().try_add(meta::keys::ui::label, std::string("Periodic (tileable)"));
    a->metadata().try_add(meta::keys::ui::widget_type, std::string("Checkbox"));
    a->metadata().try_add(meta::keys::ui::category, std::string("Tiling"));
  }
```

Note: `set_attr_ordered_key(...)` is dropped — order is insertion order, grouping is `ui.category`. Keep the `setup_post_process_heightmap_attributes(...)` call for now; the post-process attributes still use the legacy `attr` map (they render in a separate legacy panel section, acceptable for the PoC — call this out in the Task B3 GUI check).

- [ ] **Step 3: Update `compute_noise_node` to read from Meta**

Replace the param reads (noise.cpp:96-101) with:

```cpp
  auto &c = node.meta_group().current();
  const auto noise_type = hmap::NoiseType(c.value<int>(A_NOISE_TYPE));
  const glm::vec2 kw    = c.value<glm::vec2>(A_KW);
  const auto seed       = static_cast<uint>(c.value<int>(A_SEED));
  const auto periodic   = c.value<bool>(A_PERIODIC);
```

The compute body below (noise.cpp:103-145) is unchanged.

- [ ] **Step 4: Build**

Run:
```bash
cmake --build build -j --target hesiod 2>&1 | tail -20
```
Expected: builds clean. Fix type mismatches against the real key-constant names from Step 1 if the compiler complains.

- [ ] **Step 5: Commit**

```bash
git add Hesiod/src/model/nodes/nodes_function/noise.cpp
git commit -m "feat(meta): author Noise node parameters on meta::ContainerGroup"
```

### Task B3: Render the Noise panel via `meta_qt`

**Files:**
- Modify: `Hesiod/src/gui/widgets/node_attributes_widget.cpp` (the `setup_layout()` path that builds `attr::AttributesWidget`, line 208)
- Read (for exact API): `external/Meta/MetaUI/qt/include/meta_qt/container_group_widget.hpp`

**Interfaces:**
- Consumes: `BaseNode::uses_meta()`, `BaseNode::meta_group()` (Task B1); `meta_qt::ContainerGroupWidget(meta::ContainerGroup&, …, QWidget* parent)` or the free `meta_qt::render(meta::ContainerGroup&, …, QWidget*)` returning a `MetaWidget*` with a `value_changed`-style signal.
- Produces: for a meta-backed node the settings panel is built by `meta_qt`; attribute edits trigger `GraphNode::update(node_id)` (recompute), same as the legacy path.

- [ ] **Step 1: Read the exact widget constructor + change signal**

Run:
```bash
sed -n '1,70p' external/Meta/MetaUI/qt/include/meta_qt/container_group_widget.hpp
```
Note the exact constructor parameters (the 2nd param between `group` and `parent`) and the signal(s) emitted on edit. Use these exact names below.

- [ ] **Step 2: Branch the panel builder on `uses_meta()`**

In `node_attributes_widget.cpp::setup_layout()`, before the `new attr::AttributesWidget(...)` construction (line 208), add:

```cpp
  if (p_node->uses_meta())
  {
    auto *meta_widget = new meta_qt::ContainerGroupWidget(p_node->meta_group(),
                                                          /*2nd param per Step 1*/,
                                                          this);
    // wire edit -> recompute (signal name per Step 1)
    connect(meta_widget, &meta_qt::ContainerGroupWidget:://*value_changed*/,
            this,
            [this]()
            {
              if (auto gno = this->p_graph_node.lock())
                gno->update(this->node_id);
            });
    this->layout->addWidget(meta_widget);
    return; // skip the legacy AttributesWidget path for meta nodes
  }
```

(Match the existing recompute lambda already used at `node_attributes_widget.cpp:168-177`; reuse the same weak-ptr guard and `update` call.)

- [ ] **Step 3: Build**

Run:
```bash
cmake --build build -j --target hesiod 2>&1 | tail -20
```
Expected: builds clean.

- [ ] **Step 4: User GUI check — panel renders and recomputes**

Hand to the user: launch the GUI, add a `Noise` node, select it. Confirm:
1. The settings panel shows Type (dropdown), Spatial Frequency (linked X/Y sliders with the lock active), Seed, Periodic — grouped under "Main Parameters" / "Tiling" sections (`ui.category`).
2. Editing Spatial Frequency with the lock on moves both components together.
3. Editing any value re-renders the terrain preview (recompute fires).
4. (Known PoC seam) the post-process attributes may appear in a separate legacy section — acceptable for now.

Expected: 1-3 hold. Capture any discrepancy as a follow-up note.

- [ ] **Step 5: Commit**

```bash
git add Hesiod/src/gui/widgets/node_attributes_widget.cpp
git commit -m "feat(meta): render meta-backed node panels via meta_qt"
```

### Task B4: Serialization — prototype both shapes and decide (TDD)

**Files:**
- Modify: `Hesiod/src/model/nodes/base_node.cpp` (`json_to` line 407, `json_from` line 381 — branch on `uses_meta()`)
- Create: `Hesiod/data/examples/_poc_noise.hsd` (a saved graph with one Noise node, produced in Step 3)

**Interfaces:**
- Consumes: `meta::AttributeContainer::json_to()/json_from()`; the existing tolerant loader path.
- Produces: a decision (recorded in the plan's companion spec) between (a) **adapter** — `json_to/from` translate the meta container to/from the existing flat `.hsd` attribute shape, and (b) **native** — emit `meta`'s container JSON under a versioned key. Both prototypes must **fail loud** on shape mismatch (no silent all-defaults).

- [ ] **Step 1: Write the failing headless round-trip test**

Use the CLI batch mode to save+reload. Create a throwaway script `scratch/poc_roundtrip.sh` (not committed):

```bash
#!/usr/bin/env bash
set -euo pipefail
BIN=./build/Hesiod/hesiod   # adjust to actual binary path
# 1. Save a graph containing a single Noise node with non-default params
$BIN --load Hesiod/data/examples/_poc_noise.hsd --save /tmp/_poc_out.hsd --headless
# 2. Compare the Noise node's serialized params before/after
python3 - <<'PY'
import json
a = json.load(open("Hesiod/data/examples/_poc_noise.hsd"))
b = json.load(open("/tmp/_poc_out.hsd"))
# locate the Noise node in both and assert its attribute values match
# (fields: noise_type, kw, seed, periodic)
print("ROUNDTRIP_OK" if a == b else "ROUNDTRIP_MISMATCH")
PY
```

- [ ] **Step 2: Run it to confirm it fails (no meta serialization yet)**

Run:
```bash
bash scratch/poc_roundtrip.sh
```
Expected: FAIL — either the save errors (meta container not serialized) or `ROUNDTRIP_MISMATCH` (Noise params lost/defaulted on reload).

- [ ] **Step 3: Create the fixture graph**

Have the user build a graph with one `Noise` node, set non-default params (e.g. Type=perlin, kw=(4,4), seed=42, periodic=true), and save as `Hesiod/data/examples/_poc_noise.hsd`. (Or produce it headlessly once save works.)

- [ ] **Step 4: Implement prototype (a) — adapter (keep flat `.hsd` shape)**

In `base_node.cpp::json_to()`, branch: if `uses_meta()`, iterate `meta_group().current()` and emit each attribute in the existing flat shape (`{label, type, value, vmin, vmax}`) so the file is byte-compatible with today's format. In `json_from()`, when `uses_meta()`, read the flat keys and `set` them into the container via `value<T>(key) = ...` (or `set_from_any`). **Guard:** if a container key is absent from the JSON, log an error and count it; if any are missing, emit a single loud warning `[meta serialize] N attrs defaulted for node <id>` (do not silently default).

```cpp
// json_to (uses_meta branch): emit flat shape
for (const auto &name : node.meta_group().current().insertion_order())
{
  const auto *attr = node.meta_group().current().find(name);
  json[name] = { {"label", /*ui.label from metadata*/}, {"value", attr->to_any_json_value()} };
  // include vmin/vmax when present in metadata
}
```

- [ ] **Step 5: Run the round-trip; expect PASS for adapter**

Run:
```bash
bash scratch/poc_roundtrip.sh
```
Expected: `ROUNDTRIP_OK`. If mismatch, fix the field mapping until identical.

- [ ] **Step 6: Implement prototype (b) — native meta shape under a versioned key**

Add a second code path (behind a local `constexpr bool kUseNativeMetaJson`) that instead writes `json["_meta"] = meta_group().json_to()` and, on load, detects `_meta` and calls `meta_group().json_from(...)`. Bump the node JSON with a `"meta_schema": 1` marker. **Guard:** if `uses_meta()` but neither the flat keys nor `_meta` are present, throw/log loudly rather than defaulting.

- [ ] **Step 7: Measure and record the decision**

Compare the two prototypes on: diff vs the current `.hsd` (does the Python `hsd` toolkit + `site/examples/*.hsd` still parse?), code complexity, and how the silent-default guard behaves. Record the chosen approach + rationale in `docs/superpowers/specs/2026-07-09-meta-migration-design.md` §6 (replace the "open" note with the decision). Keep the losing prototype's code out of the final commit.

- [ ] **Step 8: Commit the chosen serialization + fixture**

```bash
git add Hesiod/src/model/nodes/base_node.cpp Hesiod/data/examples/_poc_noise.hsd
git commit -m "feat(meta): serialize meta-backed Noise node (<chosen> shape) with fail-loud guard"
```

### Task B5: Facade-vs-codemod decision for the remaining 303 nodes

**Files:**
- Create/Modify: `docs/superpowers/specs/2026-07-09-meta-migration-design.md` §8 + §10 item 3 (record the decision)

**Interfaces:**
- Consumes: the ergonomics observed authoring `noise.cpp` against Meta directly (Task B2).
- Produces: a decision — (a) **facade**: keep `BaseNode::add_attr<T>`/`get_attr<T>` signatures, back them with `meta` + a metadata-mapping shim, so the 303 node files change minimally; vs (b) **codemod**: rewrite each node's setup against the Meta API directly. No bulk code in this plan.

- [ ] **Step 1: Sketch the facade shim signature**

Based on Task B2, write (in the spec, not code) how a facade `add_attr<FloatAttribute>(key, label, def, min, max, fmt, log)` would translate to `container.add<float>(key, def)` + metadata `try_add`s. Note which of the 15 attribute types translate mechanically and which need bespoke handling (per the coverage matrix).

- [ ] **Step 2: Estimate effort per approach**

Record: facade = 1 shim + ~0 changes per node file (but a translation layer to maintain); codemod = ~304 file rewrites (mechanical for the ~96% clean types). Recommend one, with the reasoning tied to what B2 actually felt like.

- [ ] **Step 3: Commit the decision**

```bash
git add docs/superpowers/specs/2026-07-09-meta-migration-design.md
```
(Note: `docs/superpowers` is gitignored in this repo — if the commit is desired, force-add or relocate per the user's preference; otherwise keep the spec update local.)

---

## Self-Review notes

- **Spec coverage:** Phase A (foundations) + Phase B (vertical PoC, serialization decision, facade decision) cover the design doc's Phase A + B. Phases C–E and node-merging are explicitly out of scope for this plan (separate plans).
- **Serialization:** Task B4 prototypes BOTH shapes with a fail-loud guard against the silent-all-defaults failure mode identified in the spec §6 — the plan's highest-risk item, made headlessly testable via the CLI round-trip.
- **Gap resolutions folded in:** `ui.locked_xy` (B2), `ui.category` grouping (B2), no seed widget (B2), `SnapshotManager` reset noted for later (not needed by Noise). `ui.data_provider`/`ui.tooltip` correctly deferred (Noise doesn't exercise Range/Cloud/tooltip-critical paths).
- **External-API honesty:** where Meta's exact call signatures aren't fully known (panel widget ctor 2nd param, edit signal name, metadata key constants), the plan directs reading the specific header first (B2 Step 1, B3 Step 1) rather than inventing calls.

---

## Branch Strategy

Phase A/B touches **one repo**: `otto-link/Hesiod`. All work is on the single branch `feature/meta-migration` (already checked out, off `upstream/dev`), fully isolated — it never merges to Hesiod `dev`/`main` until a deliberate cutover. Push target for the branch: `otto-link/Hesiod` (the `barrulus` fork is retiring; Hesiod's local `upstream` remote = otto-link — confirm before first push).

**Meta is consumed, not modified, in this plan.** The `external/Meta` submodule is pinned to a frozen commit SHA — immutable, so it cannot drift regardless of what happens on Meta's `main`. No Meta branch, no `.gitmodules` branch tracking, no cross-repo submodule dance is needed for A/B.

**When Meta first needs editing (a later phase — `ui.data_provider`, `ui.tooltip`):** create `feature/meta-migration` inside `external/Meta` off the then-pinned commit, commit Meta changes there, push to `otto-link/Meta`, and bump Hesiod's pin. That coordination is deferred to the phase that actually needs it, not front-loaded here.

Other submodules (`Attributes`, `HighMap`, `GNode`, `GNodeGUI`, `QTerrainRenderer`, `QTextureDownloader`) are untouched and stay at their current pins. (`Attributes` removal is a later phase.)

**Cutover (out of scope for this plan):** when the migration is proven and complete, Hesiod's `feature/meta-migration` branch merges to its `dev`. If Meta has by then diverged onto its own branch, merge Meta to its `dev` first, then bump Hesiod's pin, then merge Hesiod.

> Branch name settled: `feature/meta-migration` (Hesiod is on it). Any Meta branch created in a later phase uses the same name for consistency.
