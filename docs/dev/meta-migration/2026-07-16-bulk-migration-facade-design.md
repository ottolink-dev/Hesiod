# Bulk migration facade (Phase C) — Design

**Date:** 2026-07-16
**Repos:** `otto-link/Meta` (presets) + `otto-link/Hesiod` (adapter) — both on `feature/meta-migration`
**Status:** Design approved; ready for implementation plan.
**Context:** Phases A/B + G1/G2/R1 proved the Meta stack end-to-end on three hand-migrated nodes
(Noise, Saturate, Cloud). This phase moves the remaining ~300 nodes onto Meta storage via a
compatibility facade, so `external/Attributes` + `QSliderX` become near-dead dependencies.

## 1. Goal

Every node stores its parameters in `meta::ContainerGroup` (rendered by MetaUI), with **zero or
near-zero edits to node .cpp files**. Legacy `.hsd` files keep loading. After this phase, only the
Brush node (ArrayAttribute paint canvas — the one widget Meta lacks) still uses the legacy
attribute map; a small follow-up phase (Meta canvas widget, then submodule removal) finishes the
cut.

## 2. Decisions banked (from brainstorming)

- **Old `.hsd` compatibility:** the facade reads legacy per-key json as a fallback — old projects
  and the ~150 example files load seamlessly; no regen, no offline translator.
- **End state this phase:** all nodes on Meta; `external/Attributes`/`QSliderX` stay *linked* but
  unused except by Brush. Toolbar state/presets re-implemented on Meta.
- **Node edits:** light codemod allowed where impersonation is awkward; target is near-zero
  *semantic* edits (the one systematic mechanical edit is the include swap, §4.2).
- **Verification bar:** automated parity (attribute inventory diff + .hsd corpus round-trip) plus
  spot GUI checks of a representative sample.
- **Approach:** Meta presets + thin Hesiod adapter (the semantic mapping lands in Meta as durable
  `meta::presets`; Hesiod keeps only a traits table, handles, and the legacy-json reader).

## 3. Survey facts the design rests on

- ~1,330 `add_attr<T>` sites across 303 files in `nodes_function/` (Float 815, Bool 191, Int 99,
  Seed 70, Enum 52, Vec2Float 34, WaveNb 30, Filename 14, Choice 12, Range 11, String 7,
  VecFloat 2, Color 2, ColorGradient 1, Cloud 1, Array 1) plus shared setup code
  (`post_process.cpp`, `pre_process_mask.cpp`, `default_noise.cpp`, broadcast/receive nodes).
- No GET macros: compute calls `get_attr<XAttribute>(key)` (~1,360 sites) and
  `get_attr_ref<XAttribute>(key)` (21 sites) directly (`base_node.hpp:89-109`).
- `get_attr_ref` mutators actually used: Range `set_is_active` (5 nodes),
  `set_histogram_fct`/`set_autorange` (via `setup_histogram_for_range_slider.cpp`),
  Choice `set_use_combo_list`/`set_choice_list` (receive), String `set_value` (broadcast),
  ColorGradient (1 site), Array `set_background_image_fct` (Brush only).
- Ordering: `set_attr_ordered_key` in 255 files; `_GROUPBOX_BEGIN_<Title>`/`_GROUPBOX_END_`
  sentinels in 95; shared helpers append via `get_attr_ordered_key_ref()`.
- Legacy serialization: `json[key] = attr->json_to()` flat per node (`base_node.cpp:438-462`);
  Meta nodes write `json["_meta"]`.
- Toolbar Save/Restore-state + Load/Save-preset ride on legacy snapshots
  (`node_attributes_widget.cpp:36-158`; Meta TODO at `:103`).
- Meta preset idiom exists: `Attribute<T> &angle(AttributeContainer&, key, label, value)` setting
  widget_type/format/constraints (`Meta/src/presets/angle.cpp`, `presets/seed.cpp`,
  `include/meta/presets/numeric.hpp`).
- Legacy→Meta widget mapping is 1:1 except ArrayAttribute (no Meta canvas widget).

## 4. Architecture

### 4.1 Layer 1 — `meta::presets` vocabulary (in Meta)

One preset function per semantic attribute kind, same idiom as `presets::angle`: takes
`(AttributeContainer&, key, label, value/bounds/flags…)`, sets `ui.widget_type` + `ui.label` +
`ui.format` + `constraints.*`, returns `Attribute<T>&` so callers can add metadata.

New presets: `slider_float` (vmin/vmax/format/log_scale), `slider_int`, `checkbox` (+
`binary_buttons` two-label variant), `enum_choice` (enum_items), `wavenumber` (LinkedSliders +
`ui.locked_xy` + bounds), `range` (RangeBar + `ui.state` active-toggle + bounds), `xy` (XYCanvas,
per-axis bounds), `points` (PointsEditor), `color` (ColorPicker), `color_gradient`
(GradientEditor), `file` (OpenFile/SaveFile + `constraints.file_filter`), `text` (SingleLineText,
+ read-only), `string_choice` (ComboBox or ButtonGrid + `constraints.allowed_values`), `curve`
(CurveEditor + bounds). `seed` already exists.

### 4.2 Layer 2 — Hesiod compatibility adapter

A compat header + a few .cpp files replace the `attributes/...` includes.

**Tag types + traits.** `hsd::compat::FloatAttribute` etc., aliased into the `attr::` namespace
node files already use. Node files reach the legacy types via the umbrella
`#include "attributes.hpp"` (the external library's header); the one systematic codemod of this
phase is the single-line swap `"attributes.hpp"` → the compat header across `nodes_function/`
(script-generated, trivially auditable — include-path shadowing was rejected as fragile while
Attributes stays linked for Brush, which keeps the legacy include). One `legacy_traits<T>`
specialization per tag:

```cpp
template <> struct legacy_traits<FloatAttribute> {
  using value_type = float;                    // what get_attr returns
  // one create() overload per legacy ctor overload, with IDENTICAL default args
  static meta::Attribute<float> &create(meta::AttributeContainer &c,
      const std::string &key, const std::string &label, float v,
      float vmin = -FLT_MAX, float vmax = FLT_MAX,
      std::string fmt = "{:.3f}", bool log_scale = false)
  { return meta::presets::slider_float(c, key, label, v, vmin, vmax, fmt, log_scale); }
};
```

**BaseNode routing.** `add_attr<T>(key, args…)` → `legacy_traits<T>::create(meta_group().current(),
key, args…)` + registers the key's legacy-json decoder (§5) in a per-node side table.
`get_attr<T>` → `c.value<storage_type>` converted to `value_type`. `get_attr_ref<T>` → a typed
handle (§4.3) for the four types that need one. The legacy attr map / codepath remains only for
allowlisted nodes (`{"Brush"}`); a covered-type node not on the allowlist that somehow reaches the
legacy path is a hard error.

**Value-type conversions (facade-internal; compute code sees legacy types):**
- Seed: legacy `uint` ↔ Meta `int` (all uses feed hmap seeds; int-safe).
- Color: legacy `std::array<float,4>` ↔ Meta `glm::vec4`.
- ColorGradient: legacy `std::vector<Stop>` ↔ Meta's gradient value type (exact type confirmed at
  implementation against the GradientEditor renderer).

**Ordering / groupboxes.** `set_attr_ordered_key` and `get_attr_ordered_key_ref()` keep their
signatures and the vector stays on BaseNode. A `finalize_attributes()` hook, called by the node
factory right after the setup function returns, walks the list and assigns
`ui.category = <Title>` from `_GROUPBOX_BEGIN_/_END_` spans (untagged keys: no category ⇒ flat
render, as today). It also takes the initial snapshot for toolbar Reset (§4.5) and validates:
sentinel imbalance or listed-key-not-in-container ⇒ loud warn.

### 4.3 Handles (the 21 `get_attr_ref` sites)

- **RangeHandle** — `set_is_active(bool)` → `ui.state`; `set_histogram_fct`/`set_autorange` only
  occur inside `setup_histogram_for_range_slider.cpp`, which is rewritten natively onto
  `ui.data_provider` (G1 machinery; PairVec → ProviderData series; autorange folded into that
  wiring).
- **ChoiceHandle** — `set_use_combo_list(true)` → widget_type ComboBox/ButtonGrid;
  `set_choice_list` → update `constraints.allowed_values` (receive node's dynamic tag list).
- **StringHandle** — `set_value` → assign the Meta attribute value + fire its change event
  (broadcast node).
- **ColorGradient** (1 site) — handle or one-line codemod, decided at the site.
- **Array** (Brush) — stays legacy.
- `setup_background_image_for_cloud_attribute.cpp` is likewise rewritten onto `ui.data_provider`
  (G2 machinery).

### 4.4 Shared-setup choke points

`post_process.cpp`, `pre_process_mask.cpp`, `default_noise.cpp`, and the broadcast/receive node
classes go through the same facade — the `post_*` attributes on nearly every heightmap node
convert with zero edits. Their ordered-key appends keep working because the vector is unchanged.

### 4.5 Toolbar snapshots (Meta-backed nodes)

- Save State / Restore State → Meta `SnapshotManager` save/restore on the container.
- Load / Save preset → serialize/restore container **values** to a preset json file (values-only
  by construction; unaffected by the #16 `_meta` trim).
- Reset → restore the initial snapshot taken at `finalize_attributes()` time.

### 4.6 Misc BaseNode branches

- `reseed(±1)` — the Seed traits add a `compat.seed = true` metadata marker; reseed iterates the
  container for marked ints.
- `update_attributes_tool_tip` — writes node-doc HTML into `ui.tooltip` (G1 feature).
- `node_parameters_to_json` (docs/`--inventory`) — Meta branch reads metadata label + storage type
  name, keeping the docs pipeline and the hsd toolkit's catalog source alive.

## 5. Serialization

**Write:** Meta-backed nodes always write `json["_meta"]` (current format; the #16 trim slots in
later without touching this design — the reader tolerates both).

**Read** (`BaseNode::json_from`, Meta-backed nodes):
1. `"_meta"` present → existing Meta restore.
2. No `_meta` → **legacy fallback**: for each container key, decode `json[key]` via the side-table
   decoder registered by `add_attr` (explicit per legacy type — no inference):
   Float/Int/Bool/String/Path/VecFloat read `value`; Enum reads the `choice` **string** and maps it
   through enum_items (unknown ⇒ default + warn); Seed reads `value` uint → int; Range reads
   `value` + `is_active` → `ui.state`; Choice validates the string against the list; Color
   array<float,4> → vec4; ColorGradient stops → Meta gradient type. Missing key ⇒ default + warn
   (legacy tolerance). One info line per node: `loaded legacy-format parameters`.
   Exact legacy field names verified at implementation against each `attributes/*.cpp json_to()`
   and against real example `.hsd` files.
3. Legacy-format files restore **values only** — not per-attribute toolbar state blobs.

## 6. Error handling

- Unknown key at `get_attr` → **throw** (fixes the latent legacy bug where
  `std::invalid_argument` is constructed but never thrown, `base_node.hpp:96`).
- Legacy-json decode mismatch → default + loud per-key warn with node id.
- Groupbox imbalance / unknown ordered key → loud warn at finalize.
- Uncovered attribute type on a non-allowlisted node → hard error at setup (no silent legacy
  fallback).
- Conflicts stay explicit, never silently resolved (project posture).

## 7. Rollout order

1. **Meta presets** (Meta branch) + metadata smoke checks.
2. **Adapter core** (tags, traits, routing, `finalize_attributes`, decoder registration; Brush
   allowlist).
3. **Handles + shared-helper rewrites** (data_provider wiring).
4. **BaseNode branches** (legacy reader, reseed, tooltips, docs json, toolbar snapshots).
5. **The flip + parity harness + fix tail.**
6. Noise/Saturate/Cloud stay native (fold onto the facade only if a pure simplification).

## 8. Verification

- **Parity dump:** headless mode instantiates every registered node and dumps
  `{key → type, default, min, max, label, category, widget}`; run on current dev (legacy) and on
  the facade build; script-diff. Only the declared conversions (uint→int, color repr) may differ.
- **.hsd corpus:** load all ~150 example files through the fallback; assert each restored value
  equals the file's `value` field; re-save + reload the `_meta` form (round-trip). Zero
  non-catalogued warnings.
- **Spot GUI (user):** one node per attribute type (~14) + quirky ones: receive/broadcast, a
  `post_*`-heavy node, an is_active-disabled Range (e.g. cone), Brush (legacy, must still work),
  toolbar state/preset round-trip.

## 9. Out of scope

Brush/ArrayAttribute canvas widget in Meta and submodule removal (follow-up phase); the #16
`_meta` trim; node-merging; any GUI redesign; splitter/panel work (done separately).

## 10. Components summary (isolation)

| Unit | Responsibility | Depends on |
|---|---|---|
| `meta::presets::*` (Meta) | semantic attribute kinds: value + widget + constraints metadata | Meta core |
| `legacy_traits<T>` table (Hesiod) | legacy ctor shapes → preset calls; value-type conversions | presets |
| BaseNode routing (`add_attr`/`get_attr`/`get_attr_ref`) | keep legacy signatures; route to Meta container | traits |
| Handles (Range/Choice/String) | the mutators node code actually calls | metadata keys, data_provider |
| `finalize_attributes()` | groupbox sentinels → `ui.category`; initial snapshot; validation | ordered-key vector |
| Legacy-json reader | old `.hsd` → Meta values via per-key registered decoders | decoder side table |
| Toolbar snapshot wiring | state/preset/reset on Meta SnapshotManager | Meta SnapshotManager |
| Parity harness | inventory diff + corpus round-trip | headless mode |
