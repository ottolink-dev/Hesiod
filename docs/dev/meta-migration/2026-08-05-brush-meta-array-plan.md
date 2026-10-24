# Brush → native meta::Array — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate the Brush node's `hmap` painting attribute from legacy `attr::ArrayAttribute` to a native `meta::Array` (ArrayEditor/ArrayCanvas widget), making Brush pure-Meta and removing the settings panel's mixed-backend special case.

**Architecture:** Follows the native-Meta node pattern (Noise/Saturate/Cloud): attribute lives in the node's `meta::ContainerGroup`, widget comes from Meta's renderer via metadata, background image via `ui.data_provider` → `ImageData`. A new public `BaseNode::register_legacy_decoder` lets this native node decode old `.hsd` files (the facade's `legacy_decoders_` mechanism, opened up), including mixed-era files that carry BOTH a top-level `"hmap"` and `_meta`.

**Tech Stack:** C++20, Meta @ `0e6e33ad` (meta::Array, ArrayCanvas, `META_ENABLE_ARRAY_TYPES` default ON), nlohmann json, Qt6.

**Spec:** `docs/meta-migration/2026-08-05-brush-meta-array-design.md`

## Global Constraints

- Build: `direnv exec ~/dev/Hesiod bash -c "make -C build -j4 hesiod"` (never unbounded `-j`; OOMs the machine).
- All headless gates need `QT_QPA_PLATFORM=offscreen` and cwd `Hesiod/data`.
- Parity gate: `cd Hesiod/data && QT_QPA_PLATFORM=offscreen ../../build/bin/hesiod --parity-dump=/tmp/parity.json` then `python3 scripts/compat_parity_diff.py docs/meta-migration/fixtures/parity-legacy.json /tmp/parity.json` (run from repo root).
- Corpus gate: `cd Hesiod/data && QT_QPA_PLATFORM=offscreen ../../build/bin/hesiod --compat-check examples` and `--compat-check ../../docs/examples`; both must end `0 failures`.
- Commits: conventional style (`feat(...)`, `fix(...)`), NO Co-Authored-By lines, no Claude footer.
- Model shape stays 512×512; canvas field is 256 via `ui.width`/`ui.height` metadata.
- Branch: `feature/meta-migration`; push branch AND fast-forward `dev` only at the end of the plan (both point at the same tip).

---

### Task 1: BaseNode legacy-decoder API for native nodes + mixed-era decode

**Files:**
- Modify: `Hesiod/include/hesiod/model/nodes/base_node.hpp` (near the `add_attr` template, ~line 95-120)
- Modify: `Hesiod/src/model/nodes/base_node.cpp` (json_from `_meta` branch, ~line 606-612)

**Interfaces:**
- Consumes: existing `std::map<std::string, std::function<void(const nlohmann::json &)>> legacy_decoders_` (base_node.hpp:179).
- Produces: `void BaseNode::register_legacy_decoder(const std::string &key, std::function<void(const nlohmann::json &)> fn)` — public; Task 2 calls it.
- Produces (behavior): when a loaded json has `_meta` AND a registered decoder's key at top level, the decoder runs AFTER the `_meta` load (mixed-era Brush files: `"hmap"` legacy + `_meta` holding only post_*).

- [ ] **Step 1: add the public registration method** in `base_node.hpp`, in the public attribute-management section right after the `add_attr` template:

```cpp
  // Native-Meta nodes (no compat tag) can register a hand-written decoder
  // for a key written by the legacy Attributes serializer, so old .hsd
  // files keep loading after the node stops using add_attr<LegacyType>.
  void register_legacy_decoder(const std::string                           &key,
                               std::function<void(const nlohmann::json &)> fn)
  {
    this->legacy_decoders_[key] = std::move(fn);
  }
```

- [ ] **Step 2: extend json_from for mixed-era files** in `base_node.cpp`. Inside `if (json.contains("_meta"))`, immediately after `this->meta_group().current().json_from(json["_meta"]);` add:

```cpp
        // mixed-era files carry a legacy top-level key (e.g. Brush "hmap")
        // alongside a _meta blob that does not contain it; new-format files
        // store everything inside _meta and never hit these decoders
        for (const auto &[key, decoder] : this->legacy_decoders_)
          if (json.contains(key))
            decoder(json[key]);
```

- [ ] **Step 3: build** (Global Constraints command). Expected: green — this is a pure addition; no caller yet.

- [ ] **Step 4: regression gates** (parity + both corpus commands from Global Constraints). Expected: `0 difference(s)`, `253 files ... 0 failures`, `255 files ... 0 failures` — behavior of existing nodes unchanged.

- [ ] **Step 5: commit**

```bash
git add Hesiod/include/hesiod/model/nodes/base_node.hpp Hesiod/src/model/nodes/base_node.cpp
git commit -m "feat(compat): let native-Meta nodes register legacy .hsd decoders

register_legacy_decoder() exposes the facade's per-key decoder map to
nodes that use the Meta container directly, and json_from now also runs
registered decoders for top-level keys present next to _meta (mixed-era
files, e.g. Brush's legacy hmap beside a post_*-only _meta blob)."
```

---

### Task 2: migrate brush.cpp to meta::Array

**Files:**
- Modify: `Hesiod/src/model/nodes/nodes_function/brush.cpp` (whole file, 86 lines)

**Interfaces:**
- Consumes: `BaseNode::register_legacy_decoder` (Task 1); `meta::Array{glm::ivec2 shape, std::vector<float> vector}` (`meta/ext/array/array.hpp`); `setup_background_image_for_cloud_attribute(BaseNode&, attribute_key, port_id)` (declared `base_node.hpp:230`, meta path is attribute-generic — Path already uses it); `hsd::compat::keys::type_label` (via `compat_attributes.hpp`).
- Produces: Brush node with NO legacy attributes (`get_attributes_ref()->empty()` true) — Task 3 relies on this being true for every node.
- Legacy json format to decode (from `external/Attributes/.../array_attribute.cpp:32-41`): `{"shape.x": int, "shape.y": int, "vector": [float,...]}`.

- [ ] **Step 1: rewrite the setup function.** Replace includes and `setup_brush_node` (keep the ports block):

```cpp
/* Copyright (c) 2023 Otto Link. Distributed under the terms of the GNU General
 * Public License. The full license is in the file LICENSE, distributed with
 * this software. */
#include "meta/core/data_provider.hpp"
#include "meta/ext/array/array.hpp"
#include "meta/metadata/keys.hpp"

#include "hesiod/logger.hpp"
#include "hesiod/model/nodes/base_node.hpp"
#include "hesiod/model/nodes/compat_attributes.hpp"
#include "hesiod/model/nodes/post_process.hpp"

namespace hesiod
{

void setup_brush_node(BaseNode &node)
{
  Logger::log()->trace("setup node {}", node.get_label());

  // port(s)
  node.add_port<hmap::VirtualArray>(gnode::PortType::IN, "background");
  node.add_port<hmap::VirtualArray>(gnode::PortType::OUT, "out", CONFIG(node));

  // attribute(s)
  auto &c = node.meta_group().current();

  auto *a = c.add<meta::Array>(
      "hmap",
      meta::Array{glm::ivec2(512, 512), std::vector<float>(512 * 512, 0.f)});
  a->metadata().try_add(meta::keys::ui::label, std::string("Heightmap"));
  a->metadata().try_add(meta::keys::ui::category, std::string("Main"));
  a->metadata().try_add(meta::keys::ui::width, 256);
  a->metadata().try_add(meta::keys::ui::height, 256);
  a->metadata().try_add(std::string(hsd::compat::keys::type_label),
                        std::string("Array"));

  // legacy .hsd files store the painting as attr::ArrayAttribute json:
  // {"shape.x": int, "shape.y": int, "vector": [float...]}
  node.register_legacy_decoder("hmap",
                               [a](const nlohmann::json &j)
                               {
                                 a->value().shape = glm::ivec2(
                                     j.at("shape.x").get<int>(),
                                     j.at("shape.y").get<int>());
                                 a->value().vector =
                                     j.at("vector").get<std::vector<float>>();
                               });

  setup_post_process_heightmap_attributes(node,
                                          {.add_mix = true, .remap_active_state = true});

  // background thumbnail behind the paint canvas (ImageData data_provider;
  // helper's meta path is attribute-generic despite the cloud name)
  setup_background_image_for_cloud_attribute(node, "hmap", "background");
}
```

Notes for the implementer: `c.add<T>` returns the typed attribute pointer (same pattern as `c.add<float>` in saturate.cpp); `a->value()` is the mutable `meta::Array &` (same accessor the ArrayEditor renderer uses). The legacy `set_attr_ordered_key({"hmap"})` line is deleted (no legacy attrs left). If `meta::keys::ui::width`/`height` expect a specific integer type, match what `array.inl` reads (`try_value<int>`).

- [ ] **Step 2: rewrite the compute function** (only the attribute read changes):

```cpp
void compute_brush_node(BaseNode &node)
{
  Logger::log()->trace("computing node [{}]/[{}]", node.get_label(), node.get_id());

  hmap::VirtualArray *p_out = node.get_value_ref<hmap::VirtualArray>("out");

  // retrieve raw data and convert them to an hmap::Array
  const auto arr = node.meta_group().current().value<meta::Array>("hmap");

  hmap::Array array(hmap::Vec2<int>(arr.shape.x, arr.shape.y));
  array.vector = arr.vector;
  array = array.resample_to_shape_bilinear(node.get_config_ref()->shape);

  // Array -> VirtualArray
  p_out->from_array(array, node.cfg().cm_cpu);

  // post-process
  post_process_heightmap(node, *p_out);
}
```

- [ ] **Step 3: build.** Expected: green. If `hmap::Vec2<int>` construction from the shape doesn't match the local HighMap API, check `external/HighMap/HighMap/include/highmap/array.hpp` for the exact ctor (current legacy code passed a `glm::vec2`, so a converting ctor exists).

- [ ] **Step 4: corpus gates** (both `--compat-check` commands). Expected: `0 failures` — this is the proof that legacy Brush `.hsd` files decode through the new path (both corpora contain Brush files). A Brush-related failure here means the decoder or the mixed-era json_from path is wrong — fix before proceeding.

- [ ] **Step 5: parity gate — expect exactly one node of drift.** Run the parity dump; the diff against the fixture must show changes ONLY for Brush (its `hmap` left the compat-tagged set). Any other node in the diff is a regression — stop and fix.

- [ ] **Step 6: re-baseline the fixture.**

```bash
cd Hesiod/data && QT_QPA_PLATFORM=offscreen ../../build/bin/hesiod --parity-dump=/tmp/parity-new.json
cd ../.. && python3 scripts/compat_parity_diff.py docs/meta-migration/fixtures/parity-legacy.json /tmp/parity-new.json   # eyeball: Brush only
cp /tmp/parity-new.json docs/meta-migration/fixtures/parity-legacy.json
python3 scripts/compat_parity_diff.py docs/meta-migration/fixtures/parity-legacy.json /tmp/parity-new.json   # self-diff: 0 difference(s)
```

- [ ] **Step 7: headless sanity load** of a legacy Brush example:

```bash
cd Hesiod/data && QT_QPA_PLATFORM=offscreen ../../build/bin/hesiod --batch examples/Brush.hsd 2>&1 | grep -i "legacy-format\|error"
```

Expected: the "loading legacy-format parameters" or mixed-era decode path fires with no errors.

- [ ] **Step 8: commit**

```bash
git add Hesiod/src/model/nodes/nodes_function/brush.cpp docs/meta-migration/fixtures/parity-legacy.json
git commit -m "feat(meta): migrate Brush hmap to native meta::Array

Painting is now a meta::Array rendered by Meta's ArrayEditor (canvas
field 256, model 512x512 unchanged). Background input renders behind
the canvas via the ImageData data_provider. Legacy and mixed-era .hsd
files decode via register_legacy_decoder. Parity fixture re-baselined:
Brush leaves the compat-tagged set (all other nodes 0-diff)."
```

---

### Task 3: remove the mixed-backend special case from the settings panel

**Files:**
- Modify: `Hesiod/src/gui/widgets/node_attributes_widget.cpp` (legacy branch in `setup_layout` ~line 366-395; `attributes_widget` references in toolbar handlers ~line 140-200; `is_meta_backed()` line 321)
- Modify: `Hesiod/include/hesiod/gui/widgets/node_attributes_widget.hpp` (drop the `attr::AttributesWidget *attributes_widget = nullptr;` member and any `attributes/...` include)
- Modify (comment only): `Hesiod/src/gui/widgets/node_settings_widget.cpp:188` (mixed-panel wording)

**Interfaces:**
- Consumes: Task 2's guarantee — no node populates the legacy `attr` map, so `has_legacy` is always false.
- Produces: `NodeAttributesWidget` is Meta-only; no references to `attr::AttributesWidget` remain in Hesiod GUI code.

- [ ] **Step 1: prove the precondition.** `grep -rn "add_attr<" Hesiod/src --include="*.cpp" | grep -v "compat"` and confirm no remaining `add_attr<attr::...>`/`add_attr<ArrayAttribute>` caller (facade tag calls like `add_attr<FloatAttribute>` route to Meta and are fine — the check is for types satisfying `std::is_base_of_v<attr::AbstractAttribute, T>`; after Task 2 the only such caller was Brush). Also confirm `grep -rn "AttributesWidget" Hesiod/src Hesiod/include` shows only node_attributes_widget files.

- [ ] **Step 2: delete the legacy branch.** In `setup_layout`, remove the `has_legacy` block that constructs `attr::AttributesWidget` and the `has_legacy` local; simplify the mixed-backend comment to state the panel is Meta-only. In each toolbar handler (backup/revert/reset/preset save/load), remove the `if (this->attributes_widget) { ...; return; }` early-returns, keeping the meta-container logic. Remove `is_meta_backed()`'s reliance if it reduces to `meta_widget != nullptr` only (keep the method, callers elsewhere use it). Remove the member + include from the header.

- [ ] **Step 3: build.** Expected: green, and the `attributes` library is no longer a link-time necessity for these TUs (the submodule still builds until the retirement task).

- [ ] **Step 4: full gates** (parity vs the re-baselined fixture + both corpora). Expected: `0 difference(s)` / `0 failures` / `0 failures`.

- [ ] **Step 5: commit**

```bash
git add Hesiod/src/gui/widgets/node_attributes_widget.cpp Hesiod/include/hesiod/gui/widgets/node_attributes_widget.hpp Hesiod/src/gui/widgets/node_settings_widget.cpp
git commit -m "refactor(gui): drop the mixed-backend settings panel path

Brush was the last node with legacy attributes; the panel is Meta-only
now. Removes the attr::AttributesWidget construction and the per-button
legacy early-returns (the 92eadf5c dual-render fix and its 944ec6aa
companion are obsolete)."
```

---

### Task 4: push + verification hand-off

- [ ] **Step 1: final gate sweep** — build, parity (0-diff vs re-baselined fixture), both corpora (0 failures), and `--batch examples/Brush.hsd` sanity.

- [ ] **Step 2: push** — `git push && git push upstream feature/meta-migration:dev` (fast-forward; verify with `git merge-base --is-ancestor upstream/dev HEAD` first if dev may have moved).

- [ ] **Step 3: user GUI checklist** (blocking for "complete", per spec):
  1. paint feel at field 256 vs legacy (tune `ui.width`/`ui.height` if too soft);
  2. background thumbnail appears when an input is wired to `background`;
  3. live preview updates mid-stroke;
  4. save → reload round-trip preserves the painting;
  5. load a pre-migration painted `.hsd` — painting intact;
  6. **amplitude watch-item:** load a painting, make one small stroke, confirm the rest of the painting's height range did not rescale (if it did → Meta-side normalize bug, file on Otto's branch alongside findings 6/7/8/11 — do not hack around it Hesiod-side).
