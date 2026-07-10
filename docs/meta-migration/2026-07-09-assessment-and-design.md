# Meta Migration — Assessment & Design

**Date:** 2026-07-09
**Branch:** `feature/meta-migration` (purpose-built; all Meta work stays off day-to-day branches)
**Status:** Assessment complete; design decisions partially locked; ready for detailed planning.

## 1. Goal

Migrate Hesiod's node attribute layer off `otto-link/Attributes` (and its vendored
`QSliderX`) onto Otto's new `otto-link/Meta` framework, per Otto's Discord proposal:

> Associate each `hesiod::BaseNode` with a `meta::ContainerGroup`. At minimum the
> group holds one `meta::AttributeContainer` (replicating today's design). Nodes with
> multiple parameter sets can hold several containers with one marked *current*, so a
> single node can expose multiple "flavors" (e.g. one `Noise` node instead of
> `Noise`/`NoiseFbm`/`NoiseIq`). Widget rendering is fully automated by Meta.

This document records what we found assessing the design against the real codebase, the
decisions taken, and the proposed shape of the full migration. Detailed sequencing is
left to the implementation plan (writing-plans).

## 2. The three layers Meta touches

The proposal reads as one change but is really three separable concerns:

1. **Storage** — `BaseNode`'s attribute map (`base_node.hpp:118`,
   `std::map<std::string, std::unique_ptr<attr::AbstractAttribute>>` + parallel
   `attr_ordered_key` vector) becomes a `meta::ContainerGroup`.
2. **Widgets** — the attribute→Qt-widget layer (today entirely inside
   `external/Attributes`, which vendors `QSliderX`) becomes `MetaUI/qt`.
3. **Serialization** — the `.hsd` on-disk format, currently produced by
   `attr::AbstractAttribute::json_to/from`, moves onto `meta`'s JSON.

Storage and widgets are coupled: `MetaUI` renders directly from `meta` containers, so a
node's panel must move to `MetaUI` the moment its storage moves to `meta`. Serialization
is independent and is the sharpest risk (see §6).

## 3. Coverage matrix — Hesiod's 15 attribute types → Meta

Usage counts are `add_attr<T>` frequency across `Hesiod/src/model/nodes/nodes_function/`
(304 files). ~96% of all usage falls in the clean-mapping group.

### Clean 1:1 (8 types, ~96% of all usage)

| Hesiod (`attr::`) | Uses | Meta equivalent |
|---|---|---|
| `FloatAttribute` | 825 | `Attribute<float>` + `SliderFloat` (`ui.log_scale`, `ui.plus_minus` supported) |
| `BoolAttribute` | 197 | `Attribute<bool>` + Toggle/Checkbox |
| `IntAttribute` | 100 | `Attribute<int>` + SliderInt/Input |
| `SeedAttribute` | 73 | `presets::seed` (`Attribute<int>`) — see gap: randomize button |
| `EnumAttribute` | 55 | `Attribute<int>` + `EnumComboBox` + `constraints.enum_items` (value→label pairs) |
| `FilenameAttribute` | 14 | `Attribute<std::filesystem::path>` + OpenFile/SaveFile/Directory |
| `ChoiceAttribute` | 13 | `Attribute<std::string>` + `constraints.allowed_values` ComboBox/ButtonGrid (runtime-mutable) |
| `StringAttribute` | 7 | `Attribute<std::string>` |
| `ColorAttribute` | 2 | `glm::vec4` + ColorPicker (Hesiod stores `array<float,4>` RGBA) |
| `ColorGradientAttribute` | 1 | `meta::ColorGradient` + GradientEditor — structurally identical `Stop{position, rgba[4]}` + presets |
| `VecFloatAttribute` / `ArrayAttribute` | 3 | `Attribute<std::vector<float>>` + CurveEditor |

Enum note: Hesiod already holds every label→int map in `enum_mappings.hpp`; each
`add_attr<EnumAttribute>` site passes one. Only `value`+`choice` hit disk, and the maps
are re-injected at node construction — so **enums migrate cleanly**.

### Genuine gaps (7 features, ~4% of usage but high value)

| Hesiod feature | Uses | Meta gap | Candidate owner |
|---|---|---|---|
| `RangeAttribute` + **histogram callback** | 14 | `RangeBar` has no background histogram; runtime input-data→UI binding absent (`setup_histogram_for_range_slider.cpp`) | per-gap decision |
| `CloudAttribute` + **background-image callback** | 2 | `PointsCanvas` has no heightmap preview behind it | per-gap decision |
| `WaveNbAttribute` **linked/lock** | 31 | only `glm::vec2`+LinkedSliders; loses lock semantics | small custom widget |
| **Grouped/nested settings** (`attr_ordered_key` groupbox pseudo-keys `_GROUPBOX_BEGIN_/_END_`, `_SEPARATOR_`, `_TEXT_`) | pervasive | Meta category system is **flat** ("Settings" root, `META_DEFAULT_CATEGORY_POLICY "flat"`); `collapsible_section` widget exists as a starting point | likely upstream to Meta |
| **Tooltips** (`update_attributes_tool_tip`) | all nodes | no `tooltip`/`description` metadata key (only `label`) | likely upstream (add key) |
| **Cross-attribute active-state bindings** (`island.cpp:120`, `cloud_random.cpp:46`, `set_is_active`/`save_initial_state`) | few | `ui.state` key exists but no dependency engine | Hesiod-side logic |
| Seed **randomize button** | 73 | `presets::seed` renders a plain Input; Hesiod has a dedicated randomize widget | small custom widget or upstream |

**Extensibility is the escape hatch:** Meta adds types via `AttributeTraits<T>` +
`TypeName<T>` + a `WidgetRenderer<T>` specialization + a `typeid` branch, so every gap is
closeable **without forking Meta core** — the only question per gap is Hesiod-side
renderer vs upstream PR to Meta.

## 4. What Meta gives us for free (net wins)

Capabilities Hesiod hand-rolls or lacks today:

- **Undo/redo** with slider-drag merging (`meta/undo_redo/`, `SetAttributeCommand<T>` with
  `merge_with`). Hesiod has no attribute undo/redo today.
- **Named snapshot / preset system** (`SnapshotManager`, `PresetComboBox` "Save preset…").
- **Real type-driven attribute factory** (`attribute_factory.cpp`,
  `register_builtin_types()`). Hesiod currently has *no* factory — the on-disk `"type"`
  int is vestigial and types are reconstructed at node construction.
- **FTXUI terminal backend** (`MetaUI/ftxui/`) — a bonus headless UI path.
- **Per-attribute metadata is first-class** (each `meta::Attribute` is itself a
  `MetaObject` carrying its own container of UI hints) — cleaner than the parallel
  `attr_ordered_key` vector.

## 5. Widget-layer swap surface (narrow)

Confirmed: Hesiod owns almost none of the widget code.

- `external/Attributes` owns the `AttributeType` model, the type→widget dispatch
  (`get_attribute_widget`, `attributes_widget.cpp:45`), every concrete widget, and vendors
  `QSliderX` as a **nested** submodule (`external/Attributes/.gitmodules`). Hesiod links
  only `attributes` (`Hesiod/CMakeLists.txt:82`); QSliderX comes transitively. The only
  direct QSliderX reference in Hesiod is a CSS comment (`apply_global_style.cpp:56`).
- Hesiod owns only the thin wrapper `NodeAttributesWidget`
  (`node_attributes_widget.cpp:208` builds `attr::AttributesWidget(...)`) plus the panel
  host `NodeSettingsWidget` and a doc-gen usage (`node_factory.cpp:129`).

Swap touchpoints in Hesiod (≈4): the `AttributesWidget` construction, the
`value_changed`/`update_button_released` signal wiring → `GraphNode::update` (recompute),
the CMake link, and the doc-gen screenshot path. Everything type-specific is replaced
wholesale by `MetaUI/qt` — **provided** Meta covers the widget inventory + the §3 gaps.

## 6. Serialization — the real risk

Current `.hsd` (example `site/examples/Blend.hsd`): a flat per-node object; each attribute
is `{ "type": <int>, "type_string": <str>, "label": <str>, "value": …, "vmin"/"vmax"/… }`
(`abstract_attribute.cpp:34`, `float_attribute.cpp:41`). The **loader is tolerant**
(`base_node.cpp:381`): missing key → keep default; extra key → ignored; parse error →
logged, not fatal.

The dangerous failure mode: if `meta`'s JSON shape differs, the per-attribute
`json.contains(key)` probe misses on **every** attribute → each node loads as
**all-defaults with no error**. Silent data loss, not a crash.

The shape is pinned in **three independent places**, any of which breaks on a shape change:

1. The `attr::AttributeType` enum ints written as `"type"` (`abstract_attribute.hpp:20-42`,
   explicitly "DO NOT change the order").
2. The flat per-node attribute layout assumed by the loader.
3. The **Python `hsd` toolkit** — `scripts/hsd/catalog.py` `TYPE_MAP` hard-codes the numeric
   codes; `scripts/hsd/params.py` emits the literal envelope; `scripts/hsd/data/*.json`
   duplicate enum ints — plus ~150 `site/examples/*.hsd` and the `tests/hsd/` suite.

The `"Hesiod version"` field is the natural hook for a version-gated reader.

**Decision (open, resolved in planning):** the PoC node (§8, Phase B) prototypes **both**
(a) an adapter that keeps the existing `.hsd` shape, and (b) Meta's native shape with a
version-gated reader. We measure the real JSON diff and toolkit blast radius, then lock the
choice. Guarding the silent-all-defaults mode (fail loud on shape mismatch) is mandatory
either way.

## 7. Node architecture & the multi-flavor question

- Registration is a **hand-maintained inventory** (~250 entries, `node_factory.cpp`
  `get_node_inventory()`) + a giant `switch`/`SETUP_NODE` macro dispatching to
  `setup_X_node`/`compute_X_node` free-function pairs, one file per node.
- **No container abstraction exists** — `ContainerGroup` is genuinely new plumbing.
- Compute reads attrs via `get_attr<T>("key")` (`base_node.hpp:94`).
- The Noise family maps cleanly to flavors (shared ports `dx,dy,[control],envelope→out`,
  overlapping core attrs; precedents: `default_noise.cpp` shared setup, `NoiseJordan`
  reusing `noise_iq`). Friction: plain `Noise` has fewer attrs + no `control` port;
  `NoiseParberry` drops `noise_type`; output port name is inconsistent (`out` vs `output`).

**Decision (locked): node-merging is DEFERRED.** The migration gives each `BaseNode` a
`ContainerGroup` with exactly **one** container — a behavior-preserving storage swap. No
node identity changes, so the 7-point catalog/docs blast radius stays untouched:
(1) inventory+switch, (2) `graph_node_widget.cpp` + `node_library_widget.cpp` catalogs,
(3) `dump_node_documentation_stub`, (4) Python doc scripts, (5) per-node `_settings.png`
screenshots, (6) per-node `data/examples/*.hsd`, (7) runtime docs load. The
`ContainerGroup` layer should be built **merge-ready** (single-container today, multi
tomorrow) but merging itself is a separate future project.

## 8. Proposed migration phasing (for the plan)

High-level only; writing-plans owns the detail.

- **Phase A — Foundations.** Add `otto-link/Meta` as a submodule (`external/Meta`), wire
  CMake for `meta` core + `MetaUI/qt`, building **alongside** `Attributes` during
  transition. Establish the branch/submodule-pin strategy (§9).
- **Phase B — Vertical PoC (de-risk).** Migrate ONE node (`Noise`) fully: `ContainerGroup`
  (1 container) storage + `MetaUI/qt` panel + `.hsd` round-trip. Prototype **both**
  serialization strategies → **lock the §6 decision**. Validate a compatibility facade for
  `add_attr/get_attr` so the 304 node files change minimally. Deliverable: findings +
  decisions to share with Otto.
- **Phase C — Storage/widget bulk migration.** Roll the facade across all node files;
  each node moves storage→`meta` and panel→`MetaUI` together. Resolve §3 gaps **per-gap**
  (Hesiod-side renderer vs upstream to Meta) as they're hit.
- **Phase D — Remove `Attributes` + `QSliderX`.** Once every node is on `meta`, drop the
  `external/Attributes` submodule and the CMake link in one cut; delete the thin wrapper.
- **Phase E — Adopt the free wins.** Wire undo/redo + preset/snapshot into the GUI.
- **Later (separate project) — Node-merging.** Multi-container flavors + the 7-point
  catalog/docs reconciliation.

The compatibility-facade choice in Phase B (keep `BaseNode::add_attr<T>`/`get_attr<T>`
signatures, back them with `meta`) vs a bulk codemod of 304 files is a key sub-decision to
settle in the PoC — facade is the lower-risk default.

## 9. Branch & submodule strategy

All work on purpose-built branches, isolated from Hesiod's day-to-day:

- **Hesiod:** `feature/meta-migration` (this branch), off `upstream/dev`. Later phases may
  fork per-phase feature branches off it.
- **Meta:** pin the `external/Meta` submodule to a fixed commit; if Hesiod needs Meta
  changes (gap widgets, tooltip key, nested groups), do them on a Meta feature branch and
  bump the pin — never depend on Meta `main` moving under us.
- No PR/merge to `dev` without explicit request (standing rule). No CI on these branches.

## 10. Open items to resolve in planning

1. **Serialization shape** — adapter vs native; prototype both in Phase B (§6).
2. **Per-gap ownership** — RESOLVED (2026-07-09, Otto issue #15): all gap work lands in
   Meta. Two genuinely-new Meta capabilities remain — the `ui.data_provider` hook (closes
   G1 range-histogram + G2 cloud-background; Meta-neutral return struct) and the
   `ui.tooltip` key (G6). G3 (`ui.locked_xy`) and G5 (`ui.category` + CP_TREE) already exist;
   G4 dropped; G7 reset uses `SnapshotManager` default state. See the gaps doc RESOLUTION
   table. These two Meta capabilities become explicit tasks in the plan.
3. **Facade vs codemod** — RESOLVED (Phase B PoC, §11): favour a **compatibility facade**.
4. **Meta build integration specifics** — dependency reconciliation (nlohmann_json, spdlog,
   glm already in Hesiod; `META_ENABLE_GLM_TYPES`, `META_ENABLE_COLOR_GRADIENT_TYPES` flags).
5. **Coordination with Otto** — which gaps Otto wants upstreamed vs kept Hesiod-side; the
   Phase B findings are the artifact to drive that conversation.

## 11. Phase B PoC outcomes & decisions (2026-07-10)

The vertical PoC (Meta submodule + build + Noise node migrated end-to-end) is implemented on
`feature/meta-migration` (commits `93e42ed3`…`b2659356`). Meta core + `meta_qt` compile and
link in Hesiod's toolchain; the Noise node stores its params in a `meta::ContainerGroup`,
auto-renders via `meta::qt::ContainerGroupWidget`, recomputes on edit, and serializes.

**Serialization — DECISION: native `_meta` shape (not adapter).** `BaseNode::json_to/json_from`
serialize a meta-backed node's container under a `"_meta"` key with a fail-loud guard (absent
`_meta` on a meta node logs an error rather than silently defaulting). The adapter (reproduce
the legacy flat shape) was rejected: Meta stores `noise_type`/`seed` as plain `int` and `kw` as
`glm::vec2`, but legacy `.hsd` tags them semantically (Enumeration / "Random seed number" /
Wavenumber). Meta's value-type does not carry Hesiod's semantic attribute type, so an adapter
cannot faithfully reproduce legacy per-attribute tags without a bespoke per-attribute semantic
map. **Full-migration implication:** the Python `hsd` toolkit + ~150 `site/examples/*.hsd` assume
the legacy shape; a meta-native world needs the toolkit regenerated (or a one-time old→new
translator) and the `"Hesiod version"` field used to gate old-file reads. This is a whole
workstream, tracked for the migration phase.

**Facade vs codemod — DECISION: compatibility facade.** Authoring the Noise node directly against
Meta cost ~5–8 lines per attribute (one `add<T>` + several `metadata().try_add(...)` calls) vs the
legacy 1-line `add_attr<T>(key,label,def,min,max,...)`. Across 304 node files and ~1300 attribute
declarations (825 Float alone), a direct codemod would balloon the node sources and churn every
file. A facade that keeps `BaseNode::add_attr<LegacyType>(...)` signatures and internally maps
each legacy attribute type → (`container.add<value_t>` + the right `ui.*`/`constraints.*` metadata)
localises the translation to one shim, keeps node files ~1 line/attr, and makes the bulk migration
mechanical. The 8 clean types (Float/Bool/Int/Seed/Enum/String/Filename/Color, ~96% of usage) map
directly; the tricky types (Range+histogram, Cloud, WaveNb, etc.) route through the same shim once
their Meta capabilities land (`ui.data_provider` etc., issue #15).

**Widget sizing — FINDING (deferred to Meta-editing phase).** Meta's `SliderFloat`/`LinkedSliders`
set a computed `minimumWidth` that, for the paired-slider `glm::vec2` (`kw`), exceeds Hesiod's
settings panel — which is hard-capped at 360px (`node_settings_widget.cpp:28-29`, `// TODO fix
this`) with horizontal scroll off. The widest widget forces the container wide, so all inputs clip.
Fix belongs with the Meta-editing phase: make Meta widgets shrink to narrow panels and/or fix the
360px cap. (Unrelated: empty toolbar icons launching from `build/bin` are the `qtsvg` plugin-path
launch-env issue, not Meta.)

**Environment note.** A nixpkgs Qt bump (6.11.0→6.11.1) mid-work left `build/` configured against
6.11.0 (stale `CMakeCache`/RUNPATH), causing a `Qt_6_PRIVATE_API` symbol skew at runtime. Fix: `rm
-rf build` + fresh configure under the devshell → coherent 6.11.1 binary. Full rebuilds must cap
parallelism (`-j4`) and run detached — unbounded `-j` OOMs and killed the terminal server.
