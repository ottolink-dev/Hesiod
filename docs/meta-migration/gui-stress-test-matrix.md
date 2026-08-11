# Meta migration — manual GUI stress matrix

> **Amended 2026-08-11 for `meta-integration-wip`.** The compat facade is gone, and the two
> headless gates this matrix leaned on (`--parity-dump`, `--compat-check`) went with it.
> `--parity-dump` is genuinely obsolete — it compared the legacy and Meta backends, and
> there is only one backend now. The corpus decode gate is *not* obsolete, so it was
> re-run externally against this branch: **508 files, zero regressions vs `dev`**, and four
> files that fail on `dev` (`Cloud`, `CloudToPath`, `FloodingFromPoint`, `PathFractalize`
> — natively-migrated nodes that never got legacy decoders) now load correctly through
> `legacy_converter`. Six pre-existing failures are unchanged and environmental (missing
> CSV/image assets, the deprecated `MorphologicalGradient`, the malformed
> `CoastalErosionDiffusion.hsd`, the stray `ZeroedEdges.bak.hsd`).
>
> Changed below: rows 1 and 4 of the coverage table, and **G5**. Worth restoring as a
> CI-able gate: a headless "load every corpus file, fail on error" check — nothing in-tree
> covers legacy decode now.

The headless suite proves **attribute values** are correct across ~295 nodes and that
legacy `.hsd` files decode. It cannot touch what actually crashed twice: **widget
construction, teardown, and live interaction**. This matrix targets exactly that. Ten
graphs, foundational → brutal.

## How to read the steps

- **Data flows left → right.** Place **source** nodes on the left, wire rightward to
  the **sinks** (`ExportHeightmap`, `Preview`) on the right.
- **`A.out → B.in`** means: *drag a wire from node A's output port named `out` to node
  B's input port named `in`.* The arrow always points the way the data travels.
- A **source** (Primitive/*, `CloudRandom`, `Path`, `Brush`, `Receive`) produces output
  with nothing wired in. A **modifier** (Erosion, Filter, Operator, Selector, Export…)
  produces nothing until its main input is fed — so every modifier below gets a wire in.
- Types must match: `ColorizeGradient` emits a **texture**, which can only land on a
  texture input (`Preview.texture`, `ExportTexture`), never a heightmap `input`.

## The 5-point ritual — run on every graph after building it

1. **ADD** — each node computes on placement; clicking it opens its panel with **no abort**.
2. **EDIT** — drag every control full-range: output changes, no clipping, labels legible.
3. **SAVE → RELOAD** — reopen the `.hsd`: topology **and** every value restored exactly.
4. **TOOLBAR** — on a meta node: Backup → Revert → Reset.
5. **TEARDOWN** — with a meta node's panel **open**, delete it *and* switch projects: **no segfault**.

## Coverage matrix — which graph hits which risk surface

| Risk surface (what only the GUI can break) | G1 | G2 | G3 | G4 | G5 | G6 | G7 | G8 | G9 | G10 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Every `hesiod::attributes` builder type round-trips | ● | | | ● | ● | | ● | | ● | ● |
| Paired sliders / `kw` responsive stacking | ● | | | | | | | | ● | ● |
| Custom node-widgets (Thru/Toggle/Export/Debug) | | | | | | ● | | | | ● |
| Brush ArrayEditor (canvas↔model resample, amplitude stability) | | | | | ● | | | | | ● |
| Data-provider widgets live (Saturate hist, Cloud bg) | | | | | | | ● | | | ● |
| Native-Meta nodes (Noise/Saturate/Cloud) | | ● | ● | | | | ● | | | ● |
| Custom editors (gradient, recurve curve, spectral bars) | | | ● | ● | | | | | | ● |
| Multiple wire types (heightmap/texture/path/cloud) + conversions | | | ● | | | | | | | ● |
| Fan-out (one output read many) + selective recompute | | ● | | | | | | | | ● |
| Fully-wired merge node (every input connected) | | | ● | | ● | | | | ● | ● |
| Cross-graph Broadcast/Receive | | | | | | ● | | | | ● |
| **Widget teardown / project-switch (the Event UAF)** | | | | | | ● | | ● | | ● |
| Legacy decode (old file, no `_meta`) → save → reload | | | | | | | | | ● | |

---

## G1 — Densest facade panel

**Tests:** the most complex facade panel (34 attributes) + paired `kw` sliders.

1. Place a `Noise` node.
2. Place a `Rifts` node to its right. *(Rifts is an Erosion modifier — it needs a feed.)*
3. Drag `Noise.out → Rifts.input`.
4. Place an `ExportHeightmap` node.
5. Drag `Rifts.output → ExportHeightmap.input`.
6. Click `Rifts` — its 34-attribute panel opens (Float, Value range, Enumeration, Choice,
   Bool, Random seed, **Vec2Float**, **Wavenumber**). Confirm **no abort**.
7. Drag every slider full-range; toggle every enum/choice/bool — the terrain responds.
8. Drag the graph|settings splitter left to its stop. Confirm the `kw` pair stays fully
   legible — **not** that it stacks. **Corrected 2026-07-30:** the pair cannot stack at
   these label lengths, and that is correct behaviour, not a defect. Instrumented
   `ResponsiveBox::reflow` measured each `kw` slider's minimum at 99px, so the pair needs
   201px side-by-side, while the narrowest width the box is ever handed is 233px — the
   reflow threshold is never crossed, so it rightly stays horizontal.

   The panel bottoms out at `NodeAttributesWidget::minimumSizeHint().width() == 309`,
   which is what makes the pane stop shrinking and shows a horizontal scrollbar. That
   floor is **the 9-button toolbar** in `NodeAttributesWidget::create_toolbar()` — nine
   `QToolButton`s in a plain `QHBoxLayout`, no stretch, no wrap, nothing shrinkable,
   ~34px each. It overrides the 240px floor `node_settings_widget.cpp:28` asks for.
   Next floor below it is the `CollapsibleSection` minimum of 232 + margins ≈ 250, set by
   some other attribute row in the section (not the pair).

   So stacking only becomes *reachable* if the toolbar stops pinning the width — wrap it,
   or move overflow buttons behind a `⋯` menu. Until then, treat a horizontal scrollbar
   at the stop as expected, and check legibility instead of reflow (cleanup item #1).
9. Run the 5-point ritual (save/reload restores all 34 values; teardown with panel open).

*Alternates: dense **sources** needing no feed — `Crater` (28 attrs), `Rift` (26),
`BasaltField` (24). Dense **modifiers** (feed from Noise as above) — `Strata`,
`HydraulicSaleve`, `HydraulicProcedural`.*

## G2 — Wide fan-out (recompute correctness)

**Tests:** one output read by four **independent** branches that never rejoin; only the
touched branch recomputes.

1. Place a `Noise` node.
2. Place `HydraulicParticle`; drag `Noise.out → HydraulicParticle.input`.
3. Place `SelectSlope`; drag `Noise.out → SelectSlope.input`.
4. Place `ColorizeGradient`; drag `Noise.out → ColorizeGradient.level`.
5. Place `ExportHeightmap`; drag `Noise.out → ExportHeightmap.input`.
6. Place a `Preview` node.
7. Drag `HydraulicParticle.output → Preview.elevation`.
8. Drag `SelectSlope.output → Preview.scalar`.
9. Drag `ColorizeGradient.texture → Preview.texture`.

**Verify:**
10. Change the `Noise` seed → all three Preview panes **and** the export update together.
11. Change a `HydraulicParticle` parameter → **only** `Preview.elevation` changes; the
    other three stay put.
12. Then run the 5-point ritual.

## G3 — All Preview types + two conversions

**Tests:** the four wire types `Preview` accepts — **heightmap, texture, path, cloud** —
with a **Path→grid** and a **Cloud→grid** conversion wired into visible slots (nothing
dangles).

1. Place three sources — `Noise`, `CloudRandom`, `Path` — and a `Preview`.
2. Drag `Noise.out → Preview.elevation`.
3. Drag `CloudRandom.cloud → Preview.cloud`.
4. Drag `Path.path → Preview.path`.
5. Place `PathToHeightmap`; drag `Path.path → PathToHeightmap.path` *(Path→grid)*.
6. Drag `PathToHeightmap.heightmap → Preview.scalar` — the converted grid is now visible.
7. Place `CloudToArrayInterp`; drag `CloudRandom.cloud → CloudToArrayInterp.cloud` *(Cloud→grid)*.
8. Place `HeightmapToNormalMap`; drag `CloudToArrayInterp.heightmap → HeightmapToNormalMap.input`.
9. Drag `HeightmapToNormalMap.normal map → Preview.normal map` — the converted cloud is now visible.
10. Place `ColorizeGradient`; drag `Noise.out → ColorizeGradient.level`; then
    `ColorizeGradient.texture → Preview.texture`.
11. Place `FloodingUniformLevel`; drag `Noise.out → FloodingUniformLevel.input`; then
    `FloodingUniformLevel.water_depth → Preview.water_depth`.

**Verify:**
12. All seven `Preview` slots render.
13. The two conversion chains show real content (Path→grid at `scalar`; Cloud→grid behind
    the normal map). Then run the 5-point ritual.

## G4 — Custom editor widgets (round-trip)

**Tests:** the rare editors only 1–2 nodes have — gradient stops, recurve curve,
spectral bars — the highest serialization risk. Two type-correct branches off one `Noise`.

1. Place a `Noise` node.
2. Place `Recurve`; drag `Noise.out → Recurve.input`.
3. Place `SpectralEqualizer`; drag `Recurve.output → SpectralEqualizer.input`.
4. Place `ExportHeightmap`; drag `SpectralEqualizer.output → ExportHeightmap.input`.
5. Place `ColorizeGradient`; drag `Noise.out → ColorizeGradient.level`.
6. Place a `Preview`; drag `ColorizeGradient.texture → Preview.texture`.

**Verify:**
7. Open `ColorizeGradient` — add/move gradient stops.
8. Open `Recurve` — reshape the curve.
9. Open `SpectralEqualizer` — move the bars.
10. Save; reload; **each custom widget restores its exact shape.** Then the ritual.

## G5 — Brush ArrayEditor (canvas ↔ model)

**Rewritten 2026-08-11.** Brush is no longer half-legacy — `add_array` gives it a native
`meta::Array`. What replaces the mixed-backend risk is the **resample round trip**: the
canvas is a fixed 256×256 `[0,1]` paint field, while the model is `node.cfg().shape`
(1024² by default). Every stroke upsamples canvas→model and every sync downsamples
model→canvas, and neither is lossless unless the two shapes match. Two bugs already fixed
in Meta live on this path (`5898e8c` stroke-clobbering, `6c7f196` amplitude inflation), so
this graph guards against their return.

1. Place a `Brush` node — paint straight on its canvas *(its `background` input is optional)*.
2. Place a `Noise` node.
3. Place `Blend`; drag `Brush.out → Blend.input 1`; drag `Noise.out → Blend.input 2`.
4. Place `ExportHeightmap`; drag `Blend.output → ExportHeightmap.input`.

**Verify:**
5. **Strokes land.** Paint a continuous drag — the stroke appears as you move and is not
   erased mid-gesture (regression guard for the R1 sync-contract fix).
6. **Amplitude is stable.** Paint one blob, release, then paint a *second* blob elsewhere.
   The first blob's height must not change when the second is painted (regression guard
   for the normalize-on-sync fix). Watch the 3D preview, not the canvas.
7. **Peak erosion.** Paint one blob, then make five more short strokes far away from it.
   The original blob should keep its height. Expect a slow softening at the default
   1024² model vs 256² canvas — worth measuring, and it disappears entirely if the canvas
   field is set to the model shape (`ui.width`/`ui.height` in `add_array`).
8. Wire `Noise.out → Brush.background` and confirm the thumbnail appears behind the canvas.
9. Adjust Brush's `post_*` (gain/gamma/remap).
10. Save; reload; **both the painting AND the post_* values persist.** Then the ritual.
11. **Resolution sweep:** set the graph shape to 4096² in project settings and reopen
    Brush. The painting is `shape.x * shape.y` floats — 16.7M at 4k, ~67 MB in RAM and
    serialized into the `.hsd` as a JSON number array. Confirm this is acceptable, or that
    the brush should own a fixed resolution independent of the graph.

## G6 — Special node-widgets + cross-graph

**Tests:** the custom `node_widgets` that aborted on panel open (crash #2); Broadcast/Receive.

1. Place a `Noise` node.
2. Place `Thru`; drag `Noise.out → Thru.input`.
3. Place `Toggle`; drag `Thru.output → Toggle.input A`.
4. Place a `Rift` node; drag `Rift.output → Toggle.input B`.
5. Place `ExportHeightmap`; drag `Toggle.output → ExportHeightmap.input`.
6. Place `Debug`; drag `Noise.out → Debug.input`.
7. Place `Broadcast`; drag `Noise.out → Broadcast.input`; set its tag.
8. Place `Receive`; select the same tag. Place a `Preview`; drag `Receive.output → Preview.elevation`.

**Verify:**
9. Each of Thru / Toggle / Export / Debug opens its panel with **no abort**.
10. Flip `Toggle` between A and B — the export output swaps between the Noise and Rift heightmaps.
11. Flip `ExportHeightmap`'s auto-export toggle.
12. Confirm `Receive` shows the broadcast Noise in `Preview`. Then the ritual.

## G7 — Data-provider widgets, live drag

**Tests:** widgets that re-pull provider data mid-drag without being destroyed (the
sync-not-rebuild fix).

1. Place a `Noise` node.
2. Place `Saturate`; drag `Noise.out → Saturate.input`.
3. Place `ExportHeightmap`; drag `Saturate.output → ExportHeightmap.input`.
4. Place a `Cloud` node; drag `Noise.out → Cloud.background` — a MAGMA thumbnail appears
   behind its point editor.
5. Place a `Preview`; drag `Cloud.cloud → Preview.cloud`.

**Verify:**
6. Open `Saturate` — **drag its histogram range handles continuously.** The widget must
   **not** vanish mid-drag; the histogram updates on release.
7. Open `Cloud` — drag points over the MAGMA thumbnail.
8. Save; reload; both restore. Then the ritual.

## G8 — Teardown / project-switch storm

**Tests:** widget lifetime vs model teardown — crash #1's exact mechanism. **The
highest-value guard in the matrix.** This is a *procedure*, not a new graph.

1. Open G1 as project A, G3 as B, G7 as C.
2. Click a meta node so its settings panel is showing.
3. Switch A → B → A → C repeatedly, each time with a panel open.
4. Delete a node while its panel is open.

**Any segfault here is a real regression.**

## G9 — Legacy decode (pre-migration file)

**Tests:** the `legacy_decoders` path — old `.hsd` with no `_meta` (crash-class
"unknown attribute key"). A *procedure*.

1. Open a stock pre-migration file: `Hesiod/data/examples/Rifts.hsd`
   (or `HydraulicParticle.hsd`, `Island.hsd`).
2. Confirm every value populates — no abort.
3. Edit one value.
4. Save (now writes `_meta`).
5. Reload; behaviour identical.

## G10 — Brutal combined (the limit)

**Tests:** everything at once — memory, recompute cost, panel-switching across many meta
nodes. **Build incrementally and confirm stability after each numbered step.**

1. Place `Noise`; place `HydraulicParticle`; drag `Noise.out → HydraulicParticle.input`.
2. Place `HydraulicProcedural` and wire **every** input:
   `HydraulicParticle.output → HydraulicProcedural.input`; two more `Noise` nodes →
   `.noise_x` and `.noise_y`; a `SelectSlope` (fed `HydraulicParticle.output → SelectSlope.input`)
   → `HydraulicProcedural.mask`; a `Noise` → `.angle_shift`; a `Noise` → `.kp_multiplier`.
3. Place `Brush`; place `Blend`; drag `Brush.out → Blend.input 1` and
   `HydraulicProcedural.output → Blend.input 2`.
4. Place `Saturate`; drag `Blend.output → Saturate.input` (its histogram widget).
5. Place a `Preview` and feed it the full type spread as in G3 (heightmap, texture,
   path, cloud, normal map, water_depth).
6. Place a `Broadcast`/`Receive` pair (as in G6).

**Verify:**
7. Heavy-edit several panels; save; reload.
8. Run the G8 teardown storm on this graph. If it survives build + edit + save/reload +
   teardown, the migration holds under load.

---

# Findings — run of 2026-07-30 (Windows VM, `build\meta`)

Recorded from the G1 pass. Nothing here is fixed unless marked so.

## Fixed during this run

- **Unbounded sliders drew an empty box.** Any attribute with `vmax = FLT_MAX` (~98 float)
  or `INT_MAX` (15 int) rendered label + number and no bar, because there is no ratio to
  fill. They now get a handle that follows the drag and recentres on release.
  Meta `3ff903a`, bumped in Hesiod `da2b88a1`.
- **MetaUI widgets ignored the theme.** Both sliders, the range bar, the curve/points/xy/
  vector canvases and the gradient picker read `QPalette`, which nothing populated, so they
  painted near-black text on `#2B2B2B` and used the Windows system accent. The dark theme
  reached only the stylesheet plus a `qsx::Config` block for the *old* QSliderX widgets.
  Hesiod `c175920d` builds the palette from the same `AppSettings::colors`.
- **Quitting deadlocked in `~QApplication`.** The Heightmapper's QtWebEngine profile was
  released with a live page attached. Hesiod `6c2ee8cf`.
- **Multi-viewport teardown crash (Hesiod #537).** `ImGui::GetIO()` ran before
  `SetCurrentContext()`, so closing one viewport crashed the survivors. Submodule bumped to
  QTR `f87aea5` (QTR PR #23, still unmerged upstream) in Hesiod `f522edf8`. **Unblocks
  G6 / G8 / G10.** If PR #23 is squashed on merge, re-bump to the merged SHA.

## Open

1. **9-button toolbar pins the settings panel at 309px.**
   `NodeAttributesWidget::create_toolbar()` puts nine `QToolButton`s in a plain
   `QHBoxLayout` — no stretch, no wrap, nothing shrinkable — so
   `minimumSizeHint().width()` is 309 and the pane cannot honour the 240px floor
   `node_settings_widget.cpp:28` asks for. A horizontal scrollbar appears at the stop.
   Fix: wrap the toolbar, or move overflow buttons behind a `⋯` menu. See G1 step 8;
   this is also why the `kw` pair's stacking is unreachable. Next floor below it is the
   `CollapsibleSection` minimum of 232 + margins ≈ 250, set by some other row.

2. **`meta::register_builtin_types()` is never called — the attribute factory registry is
   empty at runtime.**
   Symptom: `[meta--] [---E---] json_from: unknown attribute type 'bool' for 'ui.state'`,
   once per group box on load (1× `ExportHeightmap`, 4× `NoiseJordan`, 5× `Rifts`), so every
   saved collapsed/expanded state is discarded. Logged at **error** level.

   Root cause is *not* a missing type registration — `META_REGISTER_ATTRIBUTE_TYPE(bool)` is
   there and `type_name<bool>` is `"bool"`. But `register_builtin_types()` (defined in
   `Meta/src/attribute_factory.cpp:35`, declared at `attribute_factory.hpp:105`) is called
   **only** by Meta's own `tests/test_meta/main.cpp:141`. Nothing in Hesiod, and no static
   initialiser, ever calls it — so `AttributeFactory::instance()`'s `registry_` is empty in
   the app and *every* type is unknown on that path.

   It only shows up here because attribute **values** decode into attributes the node's
   `setup` already created, needing no factory; metadata entries persisted in `_meta` that
   `setup` does not recreate must be *constructed*, and that goes through the factory.
   `'bool'` is incidental — it is just `ui.state`'s type.

   Fix: either call `meta::register_builtin_types()` once at Hesiod startup (one line), or
   better, have Meta self-register lazily so no host can forget. Note that adopting the
   values-only serialization of Meta#16 would also make the symptom vanish, by not
   persisting or re-reading metadata at all — but the empty registry would remain a latent
   trap for any genuine factory use.

3. **Stale `_TEXT_` / `_SEPARATOR_` ordered keys drop section headings.**
   `finalize_attributes: node Rifts: ordered key '_TEXT_Base paramaters' not found`
   (also `_TEXT_Edge smoothing`, `_TEXT_Ridge geometry`, `_TEXT_Masking`, and
   `_SEPARATOR_` ×2 on `NoiseJordan`). These legacy layout pseudo-keys no longer resolve
   to anything the Meta facade creates, so the headings and separators vanish from the
   panel. Note the upstream typo in `'_TEXT_Base paramaters'`.

4. **`island.cpp:37` — `Octaves` is pinned to `INT_MAX`.**
   `node.add_attr<IntAttribute>("noise_octaves", "Octaves", 8, INT_MAX)` — the signature is
   `(label, value, vmin = -INT_MAX, vmax = INT_MAX)`, so `INT_MAX` lands in **vmin**,
   giving `vmin == vmax`. The initial 8 displays, but the first edit clamps to `INT_MAX`.
   Pre-existing on `main`; probably meant to be `8, 1, 32`.

5. **ColorizeGradient rendered greyscale once, unreproduced (2026-07-30, G4 build-up).**
   While editing G3 into a G4 graph, `ColorizeGradient`'s output lost all colour at the moment the
   node was clicked (i.e. when its panel was built). Deleting and re-adding the node restored it.
   Not reproducible; the failing state was never saved, so there is no artefact.

   Not diagnosed. What is known: `g3.hsd`'s stored gradient is intact and correctly shaped for
   `ColorGradient::json_from`, so it is not a decode failure of that file. An empty stop list would
   produce exactly this symptom — `colorize_gradient.cpp:60-64` builds `positions` /
   `colormap_colors` from the stops and passes them to `hmap::colorize`, with no guard for empty —
   and nothing logs when it happens.

   Structural suspect: `GradientPicker` holds `std::vector<Stop> &stops_`
   (`gradient_picker.hpp:36,70`), a mutable reference into the attribute's own storage — the widget
   edits the model in place, which is why its `value_changed` handler only notifies and never sets
   a value. Any lifetime or reallocation mismatch under that reference empties or corrupts the model
   silently. Unproven as the cause here.

   **If it recurs: save the project while it is grey.** The failure is in-memory, so the file is the
   evidence — empty `value` under that node's `_meta/gradient` proves the model was emptied; intact
   stops with grey output moves the search downstream into compute or the texture upload.

6. **Saturate's histogram never refreshes after a drag** (G7, 2026-08-03).
   The bar redraws and the terrain recomputes, but the histogram behind it stays as it
   was when the panel was built. No flicker, no vanish — so the sync-not-rebuild fix
   itself holds; this is an ordering bug on top of it.

   `RangeBar::mouseReleaseEvent` (`range_bar.cpp:149-159`) calls `apply_drag()` **first**,
   which emits `value_changed` → the widget emits `edit_started` (setting
   `MetaWidget::editing_ = true`, `meta_widget.cpp:17`) + `value_changed`. Hesiod's
   handler (`node_attributes_widget.cpp:431-440`) then calls `gno->update()`
   **synchronously**, which fires `update_finished` → `NodeSettingsWidget::sync_content`
   → `sync_from_model`. That lambda guards the provider re-pull with
   `if (range_provider && !widget->is_editing())` (`glm_vec2.inl:382`) — and
   `editing_` is *still true*, because `drag_ended` → `edit_ended` is only emitted at
   `range_bar.cpp:156`, **after** `apply_drag()` has returned. So the one sync that
   follows the final recompute is skipped, and nothing syncs again afterwards.

   Net effect: `set_histogram` is only ever reached from the panel-build path
   (`glm_vec2.inl:276-288`). The histogram is frozen at its build-time contents for the
   life of the panel.

   Fix: in the `drag_ended` handler (`glm_vec2.inl:468-471`), re-pull the provider after
   emitting `edit_ended` — by then the recompute has run, so the provider returns fresh
   data. Reordering `mouseReleaseEvent` to emit `drag_ended` before the last `apply_drag`
   would leave the committed value stale, so it is the wrong end to fix.

7. **"Center" is a no-op after "Full", and centres on 0 rather than the domain midpoint.**
   `glm_vec2.inl:486-501`. Center computes `span = value.y - value.x` then
   `lo = clamp(mid - span*0.5, min, max - span)`. After **Full** the span *is* the whole
   domain, so `max - span == min` and `lo` collapses to `min` — the range is already
   where Center would put it. So Full → Center doing nothing is arithmetically correct
   and not itself a defect; `[0, 1]` → Center works because span is then small enough to
   move. Worth a disabled/greyed state so it doesn't read as a dead button.

   The real defect on that line is `const float mid = 0.f; // (min + max) * 0.5f;` —
   "Center" is hardcoded to centre on **zero**, with the domain-midpoint expression
   commented out. The *other* Center button in the same file (the Vec2 canvas one,
   `glm_vec2.inl:210-218`) does use `(min + max) * 0.5f`. Two buttons with the same label
   in the same panel disagree about what "center" means.

8. **Seed +1 arrow dead, -1 arrow works (NoiseFbm).**
   The seed renders through the `"Input"` branch of `WidgetRenderer<int>`
   (`int.inl:52-83`) — a **`QDoubleSpinBox`** with `min = 0`, `max = INT_MAX`, `step = 1`
   (from `meta::presets::seed`, `Meta/src/presets/seed.cpp:22-24`).

   Confirmed defect on that path, whether or not it is this symptom's cause:
   `int.inl:73-76` connects `&QDoubleSpinBox::valueChanged` — signature
   `valueChanged(double)` — to a lambda taking **`int v`**. Qt accepts this (double is
   implicitly convertible to int) and performs a silent **narrowing truncation toward
   zero** on every emission. Any non-integral intermediate value truncates instead of
   rounding, and the `int` never sees the spinbox's real value.

   Also note `spinbox->setValue(std::clamp(value, min, max))` at `int.inl:61` clamps the
   *display* only — it never writes the clamped value back to the attribute, so an
   out-of-range stored seed leaves widget and model disagreeing. `SeedAttribute` is a
   legacy `unsigned int` narrowed by `static_cast<int>` at
   `legacy_compat.hpp:343`, so any stored seed ≥ 2^31 arrives negative and is displayed
   as 0 while the model keeps the negative value.

   **Not yet pinned:** the up/down asymmetry needs the displayed seed number — a
   QDoubleSpinBox disables its up arrow only when `value >= maximum`. Record the value
   shown when it recurs.

9. **Detached viewport goes white on project switch — ROOT-CAUSED, deterministic
   (2026-08-03).** Not #537 resurfacing: the QTR reorder fix **is** present in the built
   tree (`render_3d.cpp:170` sets the context before the `GetIO()` at 174), and there is
   no crash and no dump. A separate defect on the same teardown path.

   **Repro (100%):** open a file → open a detached viewport → select a node so it renders
   → open another file. The detached viewport goes white and never recovers; resizing,
   camera drag and re-selecting nodes do nothing. Its title shows a node from the *newly*
   loaded file.

   **The title is the tell: it is not your viewport.** `GraphNodeWidget::json_from`
   (`graph_node_widget.cpp:313`) does, in order:
   - line 316 `clear_graphic_scene()` → `clear_data_viewers()`, which calls `clear()` then
     **`deleteLater()`** on the open viewer — *deferred*, so it does not die yet;
   - lines 328-350, for each viewer saved in the **new** file, `on_viewport_request()`
     builds a **brand-new** `Viewer3D` and defers its `json_from` with
     `QTimer::singleShot(0, ...)`. The comment on line 342 — *"defer to let OpenGL context
     settle"* — shows this spot already had known GL timing trouble.

   So the window on screen after the switch is a *new* viewer; the original was replaced
   in the same instant. That is why its title tracks the new file and why nothing
   re-binds it.

   **Confirmed by `C:\dev\g8.log`** (17:14:25, and identically at 17:00:49, 17:00:59,
   17:01:08, 17:01:16, 17:19:46 — every switch with a viewport open):

   ```
   GraphNodeWidget::on_viewport_request
   Viewer3D::Viewer3D                      <- new detached viewer
   RenderWidget::RenderWidget
   RenderWidget::initializeGL
   ShaderManager::add_shader_from_code: diffuse_basic   (…7 shaders, all compile)
   RenderWidget::initializeGL: setup ImGui context      <- new viewer fully healthy
   HesiodApplication::notify: Project loaded successfully.
   ShaderManager::~ShaderManager           <- *** OLD viewer's deferred delete, HERE ***
   RenderWidget::clear / reset_textures / set_heightmap_geometry …
   ```

   The old viewer's destructor fires **after** the new viewer has finished
   `initializeGL`, landing in the middle of the new viewer's setup.

   **Mechanism.** `RenderWidget::~RenderWidget` (`render_widget.cpp:65-76`) is the only
   GL-touching method in that file that does not wrap its work in
   `makeCurrent()`/`doneCurrent()` — deliberately, per its comment. It calls
   `ImGui_ImplOpenGL3_Shutdown()` → `ImGui_ImplOpenGL3_DestroyDeviceObjects()`, which
   issues raw `glDeleteBuffers` ×2, `glDeleteProgram` and `glDeleteTextures`
   (`imgui_impl_opengl3.cpp:1028-1033`). Those are plain loader calls with **no context
   guard**, so they execute against whatever context is current — which, per the log
   above, is the freshly-initialised new viewer's.

   Note the asymmetry that makes this ImGui's problem specifically: `ShaderManager`'s own
   shaders are `std::unique_ptr<QOpenGLShaderProgram>` (`shader.hpp:26`), and Qt frees
   those through `QOpenGLSharedResourceGuard`, which *is* context-aware and safe. Only the
   ImGui backend's raw deletes are unguarded.

   `Qt::AA_ShareOpenGLContexts` is **not set anywhere in Hesiod** (grepped, no hits), so
   the viewers' contexts are not guaranteed to share a namespace — meaning the old
   viewer's object *names* can refer to different live objects in the current context.
   Deleting them is exactly "shader program vanishes, widget renders white, never
   recovers".

   **Fix (two independent, either would break the collision; do both):**
   - QTR: make the context current in `~RenderWidget` before the ImGui shutdown —
     `makeCurrent()` … `doneCurrent()`, guarded on `context() && context()->isValid()`.
     The Qt-idiomatic form is cleanup driven from `QOpenGLContext::aboutToBeDestroyed`.
   - Hesiod: in `clear_data_viewers()` (`graph_node_widget.cpp:241-253`) destroy the old
     viewers **synchronously** instead of `deleteLater()`, so teardown completes before
     `json_from` starts building the new project's viewers. This also removes the reason
     for the `singleShot(0)` "let OpenGL context settle" workaround on line 343.

10. **A freshly opened viewport comes up grey and does not mirror the default viewport**
    until a node is selected. `on_viewport_request` (`graph_node_widget.cpp:1311-1314`)
    seeds the new viewer only from `get_selected_node_ids()`, so with no selection it
    starts empty. Minor/cosmetic — flagged only because it is easy to misread as the
    white-viewport bug during G8.

11. **The `RangeBar` histogram is both misdrawn and nearly invisible** (raised during G10,
    on `Saturate` fed from `Blend`). Initially read as "no histogram at all"; the
    screenshot showed it *is* drawn, just barely.

    - **11a — the bins ignore `hist_x_`.** `paintEvent` (`range_bar.cpp:161-184`) spreads
      the bins uniformly across the full widget width (`bw = r.width() / n`, line 173) and
      never reads `hist_x_`, while the handles map through `value_to_canvas` over
      `[min, max]`. `Saturate`'s domain is `[-1, 2]` (`saturate.cpp:53-54`, "legacy
      RangeAttribute domain") but its provider returns bins spanning the *input's*
      `[vmin, vmax]` (`saturate.cpp:74`). Confirmed against the screenshot: handles at
      −0.65 / 1.00 sit at ~12% / ~67% across — correct for `[-1, 2]` — while the bins span
      100% of the width. A `[0, 1]` input is therefore stretched 3× and shifted left. The
      histogram misreports where the data sits relative to the handles being placed
      against it.
    - **11c — the histogram is drawn in the least visible colour available.** Bins use
      `QPalette::Mid` at **alpha 90** (~35%, `range_bar.cpp:174-175`), and
      `apply_global_style.cpp:68` maps `Mid` to `colors.border` — so the histogram is the
      border colour faded to a third, on a dark panel. The track is then filled in that
      **same** colour at full alpha (line 195) directly over it, because the histogram is
      painted first. Invisible by construction, not by theme accident.
    - **11b — an empty provider result is silently indistinguishable from a broken
      widget.** The provider returns an empty `ProviderData` when the port is null *or*
      when `vmin == vmax` (`saturate.cpp:70-71`), and `paintEvent` then skips the block
      entirely (line 164). A flat input and a dead widget look identical. Low priority,
      but it is what sent this investigation down the wrong path initially.

    **Fix 11a and 11c together** — fixing contrast alone yields a clearly visible
    histogram that is still lying about alignment.

12. **`AttributeContainer::compact_insertion_order()` destroys every attribute.**
    `attribute_container.cpp:45` calls `attributes_.clear()` before pruning
    `insertion_order_`, so the predicate on line 50 is always true and both the names *and*
    the attributes are wiped. `ContainerGroup::compact_insertion_order()`
    (`container_group.cpp:35-45`) does the same job correctly and does not touch its
    container — this is a copy-paste slip. **Currently latent:** the only call site is
    `AttributeContainer::clear()` (line 38), which has just emptied the map anyway. But the
    method is public, and the first caller added after a `remove()` silently empties the
    node's panel. Found by inspection, not by a test.

## Progress

**G1** steps 1-7 pass. Step 8 corrected (see above) — behaviour is right, expectation was
wrong. Step 9 passes: save/reload restores all values, teardown clean.

**G2** passes, no findings. Fan-out recompute is correct: a `Noise` change updates all four
branches plus the export together, and a `HydraulicParticle` change updates only its own
branch while the other three stay put.

**G3** passes, no findings. All seven `Preview` slots render, and both conversions produce real
content — Path→grid via `PathToHeightmap` into `scalar`, Cloud→grid via `CloudToArrayInterp`
behind the normal map.

**G4** passes. Gradient stops, recurve curve and spectral bars all round-trip through
save/reload. One incident during build-up — `ColorizeGradient` output went greyscale on clicking
the node, recovered by delete + re-add, not reproducible: see open finding 5.

**G5** passes, no findings. `Brush` — the only mixed-backend node, legacy heightmap attributes
alongside Meta-backed `post_*` ones in one panel — behaves correctly on both sides of the seam.

**G6** passes, no findings — including the teardown surface (widget teardown + project switch)
and multi-viewport creation, which exercises the QTR `#537` ImGui fix bumped in `f522edf8`.

**G7 passes** (2026-08-03), `g7.hsd` saved. The headline test — widgets that re-pull
provider data mid-drag without being destroyed — **holds**: nothing vanished or flickered
during continuous handle dragging, the Cloud point editor drags cleanly over the MAGMA
thumbnail, and both restore through save/reload. The 5-point ritual passes. Four findings
raised alongside it, none of them the sync-not-rebuild failure this graph was built to
catch: 6 (histogram frozen after drag), 7 (Center/Full), 8 (seed +1 arrow), 9 (white
viewport on viewport close).

**G8 passes** (2026-08-03). All four steps run: A→B→A→C rapid project switching with a meta
settings panel open, repeatedly, and node deletion with the panel open. **No segfault, no
abort, no stale-widget artefact** — which is the headline result, because this is crash #1's
exact mechanism and the highest-value guard in the matrix. The one defect the storm surfaced
is finding 9 (detached viewport goes white on project switch), which is a *rendering*
failure in the deferred-teardown path, not a lifetime failure in the widget/model seam G8
targets: the panel side of teardown is clean.

**G9 passes** (2026-08-03), `g9.hsd` saved. The `legacy_decoders` path holds: a stock
pre-migration `.hsd` with no `_meta` block opens with every value populated, no abort on the
crash-class "unknown attribute key", edits apply, and the re-save (now carrying `_meta`)
reloads with identical behaviour. Forward migration of old projects is safe.

**G10 passes** (2026-08-03), `g10.hsd` saved. The full graph built incrementally with no
instability at any step: `HydraulicProcedural` with every input wired, `Brush`+`Blend`,
`Saturate` on the blend output, a `Preview` on the full type spread, and a
`Broadcast`/`Receive` pair. Heavy panel edits, save and reload all clean, and the G8
teardown storm run on top of this graph survives.

**All 10 graphs pass.** The migration holds across every risk surface the matrix targets:
panel density, fan-out recompute, preview types and conversions, custom editors, the mixed
legacy/Meta backend seam, special node-widgets and cross-graph links, live provider drag,
teardown and project-switch, legacy decode, and the combined load. No crash, no abort, no
data loss in any of the ten. Every finding raised is a widget-level defect — none is a
migration-correctness failure.

---

# Fix stack — deferred until the GUI pass completes

Fixes were **stacked, not applied mid-test**, so the binary under test stayed constant
across G1-G10. **The pass is complete (2026-08-03) — this stack is now ready to apply**, in
this order. Nothing below is committed. Note the split across three repos: 9a is QTR;
6, 7a, 7b, 8a, 8b, 11a, 11b, 11c and 12 are Meta; the rest are Hesiod — so separate commits
plus submodule bumps.

| # | Finding | Site | Fix |
|:-:|---|---|---|
| 6 | Histogram frozen after drag | `glm_vec2.inl:468-471` | re-pull the provider in the `drag_ended` handler, after `edit_ended` |
| 8a | `valueChanged(double)` → `int` truncation | `int.inl:73-76` | take `double v`, round explicitly |
| 8b | Display-only clamp desyncs widget from model | `int.inl:61` | write the clamped value back to the attribute |
| 8c | Seed `uint` → `int` narrowing goes negative ≥ 2^31 | `legacy_compat.hpp:343` | widen storage or clamp at the cast |
| 7a | "Center" centres on 0, not the domain midpoint | `glm_vec2.inl:494` | restore `(min + max) * 0.5f`; agree with `glm_vec2.inl:215` |
| 7b | "Center" is a dead button at full span | `glm_vec2.inl:486-501` | disable when `span >= max - min` |
| 2 | `register_builtin_types()` never called | Hesiod startup | one line, or make Meta self-register lazily |
| 3 | Stale `_TEXT_`/`_SEPARATOR_` ordered keys | facade `finalize_attributes` | drop or map the legacy layout pseudo-keys |
| 4 | `island.cpp:37` `INT_MAX` lands in `vmin` | `island.cpp:37` | `8, 1, 32` |
| 1 | 9-button toolbar pins panel at 309px | `NodeAttributesWidget::create_toolbar()` | wrap the row, or overflow behind a `⋯` menu |
| **9a** | **ImGui raw `glDelete*` with no context current** | `render_widget.cpp:65-76` (QTR) | `makeCurrent()`/`doneCurrent()` around the ImGui shutdown |
| **9b** | **Deferred viewer delete lands mid-construction of the new one** | `graph_node_widget.cpp:241-253` | destroy synchronously, not `deleteLater()` |
| 10 | New viewport starts grey until a node is selected | `graph_node_widget.cpp:1311-1314` | seed from the current viewer/pinned node |
| 11a | Histogram bins ignore `hist_x_` — drawn uniformly across the full width while the handles map through `value_to_canvas` over `[min,max]` | `range_bar.cpp:161-184` | map each bin by `value_to_canvas(hist_x_[i])`; the two must share one mapping |
| 11c | Histogram drawn in `QPalette::Mid` (= `colors.border`) at alpha 90, then overpainted by the track in the same colour at full alpha | `range_bar.cpp:174-175`, `:195` | give it its own role (`Text`, or a desaturated `Highlight`), raise the alpha, and draw it *after* the track |
| 11b | Empty provider result is indistinguishable from a broken widget | `glm_vec2.inl:276-288` + `saturate.cpp:70-71` | draw a "no data / flat input" hint instead of nothing |
| 12 | `AttributeContainer::compact_insertion_order()` calls `attributes_.clear()` — destroys every attribute | `attribute_container.cpp:45` | delete the line; compare `ContainerGroup::compact_insertion_order` (`container_group.cpp:35-45`), which is correct |

9a and 9b are the priority — 9 is a deterministic, user-visible break of the core
project-switch workflow, and it is the only finding so far that G8 was specifically
built to catch.

Also worth fixing while in there: **the log is unusable live.** `logger.cpp:16` uses
`spdlog::stdout_color_mt` with no `flush_on`, so redirecting stdout to a file yields a
0-byte log until the process exits (block buffering). Add
`instance->flush_on(spdlog::level::trace)`, or a `--log-file` sink.

Needs a repro before a fix can be written:

- **5 — ColorizeGradient greyscale.** Save the project *while it is grey*.
