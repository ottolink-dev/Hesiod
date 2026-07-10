# Meta Migration — The Genuine Gaps (discussion agenda for Otto)

**Date:** 2026-07-09
**Companion to:** `2026-07-09-meta-migration-design.md`
**Purpose:** A per-point agenda for deciding, with Otto, how each capability that
`otto-link/Attributes` + `QSliderX` provide today but `otto-link/Meta` does not yet, gets
closed — Hesiod-side extension vs upstream into Meta. Everything else (~96% of attribute
usage) maps 1:1 and is out of scope here.

For each gap: **what Hesiod does today** (with evidence), **why it matters**, **what Meta
offers now**, **the precise shortfall**, **options**, and **the question for Otto**.

---

## RESOLUTION (2026-07-09, per Otto on issue #15)

Otto's response collapsed the gap list. Corrected status — **all remaining work lands in
Meta** (the migration moves the attribute + widget system into Meta):

| # | Gap | Resolution |
|---|---|---|
| G1 | Range + live histogram | **Build in Meta:** a non-serialized `std::function` on attribute metadata under `ui.data_provider`, consumed by the RangeBar renderer. |
| G2 | Cloud + heightmap background | **Same `ui.data_provider` mechanism** — consumed by the PointsCanvas renderer as a background image. Return type is a **Meta-neutral struct** (each backend converts; core stays Qt-free so ftxui isn't boxed out). |
| G3 | Wavenumber X/Y lock | **Already in Meta** — `ui.locked_xy` (`glm_vec2.inl:29`). Just set it. |
| G4 | Seed randomize button | **Dropped** — `±1` stepper is functionally equivalent; keep Hesiod's node-level `reseed()` for bulk. |
| G5 | Nested/grouped settings | **Already in Meta** — `ui.category` repertory paths + CP_FLAT/CP_MERGED/CP_TREE + regex default-collapse. Migration expresses old `_GROUPBOX_*` as `ui.category` paths. |
| G6 | Rich tooltips | **Build in Meta:** a `ui.tooltip` key applied centrally in the base render path (HTML/rich text). |
| G7 | Attribute active-state + reset | Active toggle via the **`ui.state`** metadata key (coherent with `VectorEditor`/`LinkedSliders`; replaces the ad-hoc `{-1,0}` RangeBar sentinel, which Otto will retire). Lock via `locked_xy`. Reset uses `SnapshotManager`: store a `default` state right after node setup; user resets to it (replaces the old per-attribute `save_initial_state()`). |

**Net genuinely-new Meta capabilities: two** — the `ui.data_provider` hook (G1+G2) and the
`ui.tooltip` key (G6). Everything else already exists or is a settled idiom. The sections
below are retained for the original reasoning/evidence.

---

## Cross-cutting theme (read first)

Two of the gaps below (G1 Range-histogram, G2 Cloud-background) are the same architectural
question wearing two hats:

> **Can a `MetaUI` widget display host-supplied, runtime-computed data behind/around the
> control — data the widget itself cannot know how to produce?**

In Hesiod the data is derived from a node's *input heightmap* at edit time (a histogram, a
colorized thumbnail). The attribute has no access to the graph; the node injects a callback.
`attr::` models this as a `std::function` stored on the attribute
(`set_histogram_fct`, `set_background_image_fct`). If Meta grows one general mechanism —
"a widget may be given a host callback that returns render data" — both gaps close together,
and it becomes the extension point for future previews (colormap bars, curve-over-heightmap,
etc.). That general hook is the single most valuable thing to align on with Otto. G3–G7 are
smaller and mostly independent.

---

## G1 — Range slider with live input histogram

**Uses:** `RangeAttribute` ×14, plus the remap pattern is pervasive across post-process nodes.

**What Hesiod does today.**
`RangeAttribute` (`external/Attributes/.../range_attribute.hpp:18`) stores `glm::vec2`
(min,max) + `vmin/vmax` bounds + an `is_active` flag + a histogram provider:
```cpp
std::function<PairVec()> histogram_fct;   // PairVec = pair<vector<float> values, vector<float> counts>
void set_histogram_fct(std::function<PairVec()>);
void set_autorange(bool);
```
Hesiod wires it per-node (`setup_histogram_for_range_attribute`,
`setup_histogram_for_range_slider.cpp:17`): a lambda reads the node's input port
(`node.get_value_ref<hmap::VirtualArray>(port_id)`), renders the heightmap to a 256×256
array, bins it into a histogram, and returns it. The range widget draws that histogram
behind the two handles, so the user sets the min/max against the actual data distribution.
`set_autorange(true)` lets it snap the handles to the data range.

**Why it matters.** Choosing a remap/clamp range blind (no histogram) is a real usability
regression — this is one of Hesiod's signature interactions.

**What Meta offers now.** `glm::vec2` + `widget_type="RangeBar"` (`range_bar` widget). It
renders two handles over a plain track — **no background data, no autorange, no host
callback hook**.

**Shortfall.** (a) no channel to feed a per-instance histogram into the widget; (b) no
autorange; (c) `is_active` enable/disable of the whole attribute (see G7).

**Options.**
- **A (Hesiod-side):** subclass/extend `RangeBar` in a Hesiod MetaUI renderer that accepts a
  `std::function<PairVec()>` via attribute metadata; register a `WidgetRenderer` branch.
  Needs Meta to expose the widget base + a metadata slot for a callback (see the general
  hook above).
- **B (upstream):** Meta's `RangeBar` grows an optional "background series" data source +
  autorange, fed through the general host-callback mechanism.

**Question for Otto.** Do you want the generic host-callback-into-widget mechanism in Meta
(then histogram is just a consumer), or should Hesiod carry a bespoke range widget? Is
storing a `std::function` on an attribute's metadata acceptable in Meta's model, or do you
prefer a different binding (e.g. the widget pulls from a registered provider keyed by
attribute name)?

---

## G2 — Point-cloud editor with heightmap background

**Uses:** `CloudAttribute` ×2 (Cloud, CloudRandom-style nodes), plus latent demand for path/point editors.

**What Hesiod does today.**
`CloudAttribute` stores `std::vector<glm::vec3>` (x,y,value points) + a background provider:
```cpp
void set_background_image_fct(std::function<QImage()>);
```
Wired per-node (`setup_background_image_for_cloud_attribute.cpp:18`): the lambda colorizes
the node's input heightmap (MAGMA, 256×256) into a `QImage` drawn behind the point canvas,
so points are placed against the terrain.

**Why it matters.** Placing control points/clouds without seeing the terrain underneath is
guesswork.

**What Meta offers now.** `std::vector<glm::vec3>` + `widget_type="PointsEditor"`
(`points_canvas`, with Clear/Randomize/From-CSV, min/max x/y, z-step, closed flag). **No
background image hook.**

**Shortfall.** Same root cause as G1 — no channel to inject a host-computed background
(`QImage`) into the canvas.

**Options.** Same A/B as G1; ideally both share the general host-callback hook (G1 returns a
data series, G2 returns an image).

**Question for Otto.** Should the general hook be typed per widget (series vs image), or a
single `std::any`/variant the renderer interprets? Does Meta want a Qt dependency (`QImage`)
in the callback signature, or a Meta-neutral image struct that the Qt backend converts?

---

## G3 — Wavenumber attribute with X/Y lock

**Uses:** `WaveNbAttribute` ×31 (very common — every coherent-noise primitive).

**What Hesiod does today.**
`WaveNbAttribute` (`wave_nb_attribute.hpp:16`) stores `glm::vec2` (kx,ky) + `vmin/vmax` + a
`link_xy` bool. When linked, editing one component drives the other so the frequency stays
isotropic; unlinking allows anisotropic `kw`. Its widget exposes the lock toggle.

**Why it matters.** 31 uses; isotropic frequency is the common case and the lock makes it
one-drag. Losing it means every noise node forces two-field editing.

**What Meta offers now.** `glm::vec2` + `LinkedSliders` widget, and a `constraints.aspect_ratio`
key exists — but there is **no lock *toggle*** (runtime link on/off) and no `kw` semantics
(the field is generic 2D).

**Shortfall.** No user-toggleable X/Y link; `aspect_ratio` is a fixed constraint, not a
switch the user flips.

**Options.**
- **A (Hesiod-side):** a small `WidgetRenderer` for `glm::vec2` gated on a
  `ui.widget_type="Wavenumber"` hint + a `ui.link_xy` metadata bool, reusing `LinkedSliders`.
- **B (upstream):** add a "linkable" mode to Meta's `LinkedSliders` driven by a metadata
  bool the user can toggle.

**Question for Otto.** Is a toggleable link something you'd take into `LinkedSliders`
generically (useful beyond terrain), or is `kw` niche enough to keep Hesiod-side?

---

## G4 — Seed field with randomize button

**Uses:** `SeedAttribute` ×73 (every stochastic node).

**What Hesiod does today.** `SeedAttribute` (`seed_attribute.hpp`) is a `uint`, but its widget
adds a dedicated "randomize" (dice) button that rolls a new seed in place. Hesiod also has a
node-level `reseed()`.

**What Meta offers now.** `presets::seed` = `Attribute<int>` with `widget_type="Input"`
(min 0, max INT_MAX). Plain spinbox — **no randomize button.**

**Shortfall.** No one-click reseed at the widget.

**Options.**
- **A (Hesiod-side):** a tiny renderer variant `widget_type="Seed"` = int spinbox + dice
  button that writes a random value through the normal set path (so undo/redo still works).
- **B (upstream):** extend `presets::seed` / add a `SeedWidget` to MetaUI.

**Question for Otto.** Want a first-class `Seed` widget in MetaUI (seeds are common in
generative tools), or leave it to Hesiod? Note: the randomize must go through Meta's
`SetAttributeCommand` so it participates in undo/redo — worth confirming the write path.

---

## G5 — Nested / grouped settings layout

**Uses:** pervasive — many nodes group their attributes; some have several groups
(e.g. `flow_simulation.cpp:71` has *Water Setup / Post-filter / Solver*).

**What Hesiod does today.** Grouping rides on the ordered-key list via pseudo-keys:
`set_attr_ordered_key({"_GROUPBOX_BEGIN_<title>", "attrA", "attrB", "_GROUPBOX_END_", ...})`,
also `_SEPARATOR_`, `_TEXT_`, `_SEPARATOR_TEXT_` (`attributes_widget.cpp:150-222`). Renders as
titled group boxes in the settings panel.

**What Meta offers now.** The category system is explicitly **flat**
(`META_DEFAULT_CATEGORY_POLICY "flat"`, root "Settings"). There is a `constraints`/`ui.category`
key and a `collapsible_section` widget exists, but no nested-group layout policy driving them.

**Shortfall.** No structured grouping (titled boxes / collapsible sections / tabs) from
metadata. A flat migration would dump every node's attributes into one ungrouped list — a
visible UX regression on complex nodes.

**Options.**
- **A (Hesiod-side):** translate the `_GROUPBOX_*` convention into a Hesiod-built panel
  assembler that groups by a `ui.category` value and uses `collapsible_section`. Feasible but
  re-implements layout Meta arguably should own.
- **B (upstream):** Meta gains a "grouped" category policy: `ui.category` (or a `ui.group`
  key) drives titled/collapsible sections in `container_group_widget`.

**Recommendation.** Leans **upstream** — grouping is generic UI, not terrain-specific, and
`collapsible_section` is already half of it.

**Question for Otto.** Is a non-flat category policy on your roadmap? If so, what's the key
model — reuse `ui.category` with a policy switch, or a dedicated nesting key? This also
affects the migration of ~all `set_attr_ordered_key` calls, so it's worth settling early.

---

## G6 — Rich attribute tooltips from node docs

**Uses:** all nodes (`update_attributes_tool_tip`, `base_node.cpp:564`).

**What Hesiod does today.** After construction, Hesiod walks each attribute and, if the node's
documentation JSON has a `parameters.<key>.description`, builds an HTML tooltip
(bold label + secondary-color wrapped description) and calls
`sp_attr->set_description(html)` (`base_node.cpp:589`). `attr::AbstractAttribute` thus carries a
`description` that the widget shows on hover.

**What Meta offers now.** Metadata keys include `ui.label` but **no `tooltip`/`description`/`help`
key**, and no evidence the renderers show a hover tooltip.

**Shortfall.** No per-attribute help text channel; in-app parameter help disappears.

**Options.**
- **A (Hesiod-side):** store the HTML under an ad-hoc metadata key and have a Hesiod renderer
  set the Qt `toolTip`. Works, but every widget type needs the wiring.
- **B (upstream):** add a `ui.tooltip` (or `ui.description`) metadata key that the base
  renderer applies as the widget tooltip for all types.

**Recommendation.** **Upstream** — trivial, generic, one key + one `setToolTip` in the base
render path.

**Question for Otto.** Any objection to a standard `ui.tooltip` key applied centrally? Rich
text (HTML) or plain? Hesiod currently feeds HTML.

---

## G7 — Attribute active-state toggle + initial-state reset

**Uses:** a handful but load-bearing (e.g. `island.cpp:121`, `cloud_random.cpp:46`).

**What Hesiod does today.** `RangeAttribute` has an `is_active` flag. Nodes toggle it and
snapshot the baseline:
```cpp
node.get_attr_ref<RangeAttribute>("post_remap")->set_is_active(false);
node.get_attr_ref<RangeAttribute>("post_remap")->save_initial_state();   // island.cpp:121
```
and compute branches on it:
```cpp
if (node.get_attr_ref<RangeAttribute>("remap")->get_is_active())         // cloud_random.cpp:46
    p_out->remap_values(...);
```
So an attribute can be present-but-disabled (checkbox-gated), with a stored initial state used
for reset-to-default. `save_initial_state()` exists on the `attr::` base for all attributes.

**What Meta offers now.** A `ui.state` metadata key exists (no dependency engine behind it), and
`SnapshotManager` can name/restore JSON snapshots at the container level. But there is **no
per-attribute `is_active`/enabled concept** and no per-attribute "initial state" baseline.

**Shortfall.** (a) No "enabled" bit on an attribute that both the UI (grey out) and compute
(`if active`) read; (b) no per-attribute initial-state to reset to.

**Options.**
- **A (Hesiod-side):** model `is_active` as a companion `Attribute<bool>` per gated attribute,
  and use `SnapshotManager` for reset. Changes the compute-side idiom (`get_is_active()` →
  read a sibling bool) across the affected nodes.
- **B (upstream):** Meta adds an optional `enabled` bit + initial-state to `AbstractAttribute`,
  matching `attr::`'s `is_active` + `save_initial_state()`.

**Question for Otto.** Do you consider enable/disable + reset-to-initial a core attribute
concern (as `attr::` did), or should hosts compose it from a sibling bool + snapshots? This
decides whether ~a-dozen node compute functions change idiom.

---

## Summary table

| # | Gap | Uses | Nearest Meta primitive | Leaning |
|---|---|---|---|---|
| G1 | Range + live histogram | 14 | RangeBar (no data hook) | **general host-callback hook** (upstream) |
| G2 | Cloud + heightmap background | 2 | PointsEditor (no image hook) | same hook as G1 |
| G3 | Wavenumber X/Y lock toggle | 31 | LinkedSliders + aspect_ratio | either; ask Otto |
| G4 | Seed randomize button | 73 | presets::seed (Input) | small; either |
| G5 | Nested/grouped settings | pervasive | flat category + collapsible_section | **upstream** |
| G6 | Rich attribute tooltips | all | no tooltip key | **upstream** (trivial) |
| G7 | Attribute active-state + reset | ~dozen | ui.state + SnapshotManager | ask Otto (idiom-defining) |

**The one decision that unlocks the most:** the **general host-callback-into-widget hook**
(G1+G2). Settle that with Otto first; G3–G7 are smaller and can be triaged per-point.
