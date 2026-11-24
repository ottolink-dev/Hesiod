# Bulk Migration Facade (Phase C) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move all ~300 Hesiod nodes onto Meta storage through a compatibility facade (Meta presets + thin Hesiod adapter), with legacy `.hsd` files still loading.

**Architecture:** Layer 1 = `meta::presets` semantic attribute kinds (in Meta). Layer 2 = Hesiod adapter: compat tag types + `legacy_traits<T>` keep `add_attr`/`get_attr`/`get_attr_ref` signatures and route to the node's Meta container; a per-key decoder side-table reads legacy per-key json when `_meta` is absent. One mechanical codemod: the `#include "attributes.hpp"` swap. Spec: `docs/meta-migration/2026-07-16-bulk-migration-facade-design.md`.

**Tech Stack:** C++20, Qt6, nlohmann::json, glm; Meta (core + MetaUI/qt); nix devshell build.

## Global Constraints

- Branches: `feature/meta-migration` in BOTH repos (Hesiod work tree + `external/Meta` submodule). Commit locally; do NOT push or open PRs unless barrulus explicitly asks.
- Staging discipline: `git add` ONLY the files named in the task. NEVER `git add -A`/`.`. The dirty `external/{GNode,GNodeGUI,HighMap}` submodules and untracked screenshots/docs must never be staged. When committing Hesiod after Meta commits, also stage `external/Meta` (pin bump) explicitly and say so in the commit message.
- No Co-Authored-By lines in commit messages.
- **Hesiod build command** (referred to below as BUILD-HESIOD; run via Bash `run_in_background: true`, never unbounded -j — it OOMs the machine):
  ```bash
  nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target hesiod 2>&1 | tail -15; echo BUILD_EXIT=${PIPESTATUS[0]}'
  ```
  Expected on success: `[100%] ... Linking CXX executable ... bin/hesiod`, `BUILD_EXIT=0`.
- **Meta standalone test build** (referred to as BUILD-META-TESTS; scratch dir, core tests only):
  ```bash
  nix develop ~/quixote#cpp-qt-desktop -c bash -c '
    cmake -S /home/barrulus/dev/Hesiod/external/Meta -B /tmp/claude-1000/meta-tests-build \
      -DMETA_ENABLE_TESTS=ON -DMETA_ENABLE_GLM_TYPES=ON -DMETA_ENABLE_COLOR_GRADIENT_TYPES=ON \
      -DMETA_ENABLE_QT_UI=OFF -DMETA_ENABLE_FTXUI_UI=OFF \
    && cmake --build /tmp/claude-1000/meta-tests-build -j4 --target test_meta \
    && /tmp/claude-1000/meta-tests-build/tests/test_meta/test_meta; echo TEST_EXIT=$?'
  ```
- GUI verification is user-driven (barrulus launches `build/bin/hesiod` from `Hesiod/data/`); never claim GUI behaviour verified yourself.
- Meta CMake globs `Meta/src/*.cpp` recursively; Hesiod CMake globs `Hesiod/src/**/*.cpp` — new files need no CMake edits. New Hesiod compat sources go under `Hesiod/src/model/nodes/compat/` (outside `nodes_function/`, so `HESIOD_MINIMAL_NODE_SET` is unaffected).
- The three hand-migrated nodes (`noise.cpp`, `saturate.cpp`, `cloud.cpp`) stay native-Meta — do not touch them.
- `brush.cpp` stays on the legacy library — never swap its include.
- Legacy JSON field-name reference (verified against `external/Attributes/Attributes/src/*.cpp`): every attribute writes base `{"type": <int>, "type_string": <str>, "label": <str>}` plus: Float `value,vmin,vmax,log_scale`; Int `value,vmin,vmax`; Bool `value,label_true,label_false`; Enum `value(int),choice(str)`; Seed `value(uint)`; Range `value=[x,y],vmin,vmax,is_active`; WaveNb `value=[x,y],vmin,vmax,link_xy`; Vec2Float `value=[x,y],xmin,xmax,ymin,ymax`; Cloud parallel arrays `x,y,values`; Color `value=[r,g,b,a]`; ColorGradient `value=[{position,color[4]},...]`; Filename `value(str),for_saving,filter`; String `value,read_only`; Choice `value(str),choice_list`; VecFloat `value,vmin,vmax`.

---

### Task 1: Meta presets — the compat vocabulary

**Files:**
- Create: `external/Meta/Meta/include/meta/presets/compat.hpp`
- Create: `external/Meta/Meta/src/presets/compat.cpp`
- Modify: `external/Meta/tests/test_meta/main.cpp` (append assertions)

**Interfaces:**
- Consumes: `meta::AttributeContainer::add`, `metadata().try_add`, `meta::keys::*`, `meta::ColorGradient`.
- Produces (later tasks call these exact signatures):

```cpp
namespace meta::presets
{
Attribute<float> &slider_float(AttributeContainer &c, std::string_view key, std::string_view label,
                               float value, float vmin, float vmax,
                               std::string_view format = "{:.3f}", bool log_scale = false);
Attribute<int> &slider_int(AttributeContainer &c, std::string_view key, std::string_view label,
                           int value, int vmin, int vmax, std::string_view format = "{}");
Attribute<bool> &checkbox(AttributeContainer &c, std::string_view key, std::string_view label, bool value);
Attribute<bool> &binary_buttons(AttributeContainer &c, std::string_view key, std::string_view label,
                                std::string_view label_true, std::string_view label_false, bool value);
Attribute<int> &enum_choice(AttributeContainer &c, std::string_view key, std::string_view label,
                            const std::vector<std::pair<int, std::string>> &items, int value);
Attribute<glm::vec2> &wavenumber(AttributeContainer &c, std::string_view key, std::string_view label,
                                 glm::vec2 value, float vmin, float vmax, bool link_xy,
                                 std::string_view format = "{:.2f}");
Attribute<glm::vec2> &range(AttributeContainer &c, std::string_view key, std::string_view label,
                            glm::vec2 value, float vmin, float vmax, bool is_active,
                            std::string_view format = "{:.3f}");
Attribute<glm::vec2> &xy(AttributeContainer &c, std::string_view key, std::string_view label,
                         glm::vec2 value, float xmin, float xmax, float ymin, float ymax);
Attribute<std::vector<glm::vec3>> &points(AttributeContainer &c, std::string_view key,
                                          std::string_view label,
                                          std::vector<glm::vec3> value = {});
Attribute<glm::vec4> &color(AttributeContainer &c, std::string_view key, std::string_view label,
                            glm::vec4 value);
Attribute<ColorGradient> &color_gradient(AttributeContainer &c, std::string_view key,
                                         std::string_view label, ColorGradient value = {});
Attribute<std::filesystem::path> &file(AttributeContainer &c, std::string_view key,
                                       std::string_view label, std::filesystem::path value,
                                       std::string_view filter, bool for_saving);
Attribute<std::string> &text(AttributeContainer &c, std::string_view key, std::string_view label,
                             std::string value, bool read_only = false);
Attribute<std::string> &string_choice(AttributeContainer &c, std::string_view key,
                                      std::string_view label,
                                      const std::vector<std::string> &choices, std::string value,
                                      bool use_combo = true);
Attribute<std::vector<float>> &curve(AttributeContainer &c, std::string_view key,
                                     std::string_view label, std::vector<float> value,
                                     float vmin, float vmax);
} // namespace meta::presets
```

- [ ] **Step 1: Write `compat.hpp`** — the declarations above, following the existing `numeric.hpp` style: `#pragma once`, `#include "meta/core/attribute.hpp"`. glm-typed presets go inside `#ifdef META_ENABLE_GLM_TYPES`; `color_gradient` inside `#ifdef META_ENABLE_COLOR_GRADIENT_TYPES` (with `#include "meta/ext/color_gradient/color_gradient.hpp"` under the same guard). Add `#include <filesystem>`, `#include <vector>`, `#include <utility>`.

- [ ] **Step 2: Write `compat.cpp`** — follow `angle.cpp` structure (`c.add(std::string(key), value)`, then `m.add(...)` metadata). Full implementations:

```cpp
/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"

namespace meta::presets
{

Attribute<float> &slider_float(AttributeContainer &c, std::string_view key,
                               std::string_view label, float value, float vmin, float vmax,
                               std::string_view format, bool log_scale)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "SliderFloat");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::ui::format, std::string(format));
  m.add(keys::constraints::min, vmin);
  m.add(keys::constraints::max, vmax);
  if (log_scale)
    m.add(std::string("ui.log_scale"), true); // read by MetaUI float.inl:36
  return *a;
}

Attribute<int> &slider_int(AttributeContainer &c, std::string_view key, std::string_view label,
                           int value, int vmin, int vmax, std::string_view format)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "SliderInt");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::ui::format, std::string(format));
  m.add(keys::constraints::min, vmin);
  m.add(keys::constraints::max, vmax);
  return *a;
}

Attribute<bool> &checkbox(AttributeContainer &c, std::string_view key, std::string_view label,
                          bool value)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "Checkbox");
  m.add(keys::ui::label, std::string(label));
  return *a;
}

Attribute<bool> &binary_buttons(AttributeContainer &c, std::string_view key,
                                std::string_view label, std::string_view label_true,
                                std::string_view label_false, bool value)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "BinaryButtons");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::ui::label_true, std::string(label_true));
  m.add(keys::ui::label_false, std::string(label_false));
  return *a;
}

Attribute<int> &enum_choice(AttributeContainer &c, std::string_view key, std::string_view label,
                            const std::vector<std::pair<int, std::string>> &items, int value)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "EnumComboBox");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::constraints::enum_items, items);
  return *a;
}

#ifdef META_ENABLE_GLM_TYPES

Attribute<glm::vec2> &wavenumber(AttributeContainer &c, std::string_view key,
                                 std::string_view label, glm::vec2 value, float vmin, float vmax,
                                 bool link_xy, std::string_view format)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "LinkedSliders");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::ui::format, std::string(format));
  m.add(std::string("ui.locked_xy"), link_xy); // read by MetaUI glm_vec2.inl:30
  m.add(keys::constraints::min, vmin);
  m.add(keys::constraints::max, vmax);
  return *a;
}

Attribute<glm::vec2> &range(AttributeContainer &c, std::string_view key, std::string_view label,
                            glm::vec2 value, float vmin, float vmax, bool is_active,
                            std::string_view format)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "RangeBar");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::ui::format, std::string(format));
  m.add(keys::constraints::min, vmin);
  m.add(keys::constraints::max, vmax);
  m.add(std::string("ui.has_active_toggle"), true); // Task 2 renders the checkbox
  m.add(std::string("ui.active"), is_active);
  return *a;
}

Attribute<glm::vec2> &xy(AttributeContainer &c, std::string_view key, std::string_view label,
                         glm::vec2 value, float xmin, float xmax, float ymin, float ymax)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "XYCanvas");
  m.add(keys::ui::label, std::string(label));
  m.add(std::string("ui.min_x"), xmin);
  m.add(std::string("ui.max_x"), xmax);
  m.add(std::string("ui.min_y"), ymin);
  m.add(std::string("ui.max_y"), ymax);
  return *a;
}

Attribute<std::vector<glm::vec3>> &points(AttributeContainer &c, std::string_view key,
                                          std::string_view label, std::vector<glm::vec3> value)
{
  auto *a = c.add(std::string(key), std::move(value));
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "PointsEditor");
  m.add(keys::ui::label, std::string(label));
  return *a;
}

Attribute<glm::vec4> &color(AttributeContainer &c, std::string_view key, std::string_view label,
                            glm::vec4 value)
{
  auto *a = c.add(std::string(key), value);
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "ColorPicker");
  m.add(keys::ui::label, std::string(label));
  return *a;
}

#endif // META_ENABLE_GLM_TYPES

#ifdef META_ENABLE_COLOR_GRADIENT_TYPES

Attribute<ColorGradient> &color_gradient(AttributeContainer &c, std::string_view key,
                                         std::string_view label, ColorGradient value)
{
  auto *a = c.add(std::string(key), std::move(value));
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "GradientEditor");
  m.add(keys::ui::label, std::string(label));
  return *a;
}

#endif // META_ENABLE_COLOR_GRADIENT_TYPES

Attribute<std::filesystem::path> &file(AttributeContainer &c, std::string_view key,
                                       std::string_view label, std::filesystem::path value,
                                       std::string_view filter, bool for_saving)
{
  auto *a = c.add(std::string(key), std::move(value));
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, for_saving ? "SaveFile" : "OpenFile");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::constraints::file_filter, std::string(filter));
  return *a;
}

Attribute<std::string> &text(AttributeContainer &c, std::string_view key, std::string_view label,
                             std::string value, bool read_only)
{
  auto *a = c.add(std::string(key), std::move(value));
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "SingleLineText");
  m.add(keys::ui::label, std::string(label));
  if (read_only)
    m.add(std::string("ui.read_only"), true);
  return *a;
}

Attribute<std::string> &string_choice(AttributeContainer &c, std::string_view key,
                                      std::string_view label,
                                      const std::vector<std::string> &choices, std::string value,
                                      bool use_combo)
{
  auto *a = c.add(std::string(key), std::move(value));
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, use_combo ? "ComboBox" : "ButtonGrid");
  m.add(keys::ui::label, std::string(label));
  m.add(keys::constraints::allowed_values, choices);
  return *a;
}

Attribute<std::vector<float>> &curve(AttributeContainer &c, std::string_view key,
                                     std::string_view label, std::vector<float> value,
                                     float vmin, float vmax)
{
  auto *a = c.add(std::string(key), std::move(value));
  auto &m = a->metadata();
  m.add(keys::ui::widget_type, "CurveEditor");
  m.add(keys::ui::label, std::string(label));
  m.add(std::string("ui.min_y"), vmin);
  m.add(std::string("ui.max_y"), vmax);
  return *a;
}

} // namespace meta::presets
```

Before finalizing, cross-check the ad-hoc key names against the widget renderers (do not trust this plan over the source): `ui.log_scale` (`MetaUI/qt/include/meta_qt/widget_renderer_inl/float.inl:36`), `ui.locked_xy` (`glm_vec2.inl:30`), `ui.min_x/max_x/min_y/max_y` (XYCanvas branch of `glm_vec2.inl` and CurveEditor in `std_vector_float.inl` — if XYCanvas uses different bound keys, use what the source reads), `ui.read_only` (check `std_string.inl` — if unsupported, keep the metadata anyway and note it).

- [ ] **Step 3: Append smoke assertions to `tests/test_meta/main.cpp`** (before the final `return 0;`; the file already includes container/keys headers):

```cpp
  // --- presets::compat smoke
  {
    meta::AttributeContainer pc;
    auto &f = meta::presets::slider_float(pc, "f", "Float", 0.5f, 0.f, 1.f);
    assert(f.value() == 0.5f);
    assert(pc.value<float>("f") == 0.5f);
    assert(f.metadata().value<std::string>(meta::keys::ui::widget_type) == "SliderFloat");
    assert(f.metadata().value<float>(meta::keys::constraints::max) == 1.f);

    std::vector<std::pair<int, std::string>> items = {{0, "a"}, {1, "b"}};
    auto &e = meta::presets::enum_choice(pc, "e", "Enum", items, 1);
    assert(e.value() == 1);

    auto &r = meta::presets::range(pc, "r", "Range", {0.f, 1.f}, -1.f, 2.f, false);
    assert(r.metadata().value<bool>("ui.active") == false);

    auto &ch = meta::presets::string_choice(pc, "c", "Choice", {"x", "y"}, "x");
    assert(ch.value() == "x");

    auto &sf = meta::presets::file(pc, "p", "File", "out.png", "PNG (*.png)", true);
    assert(sf.metadata().value<std::string>(meta::keys::ui::widget_type) == "SaveFile");

    std::cout << "presets::compat smoke OK" << std::endl;
  }
```

Add `#include "meta/presets/compat.hpp"` (and `#include "meta/presets/numeric.hpp"` if not present) at the top of the test file.

- [ ] **Step 4: Run BUILD-META-TESTS** — expected: compile green, `presets::compat smoke OK`, `TEST_EXIT=0`. (Run it once BEFORE writing code too if you want the classic red first — the test won't compile without the header, which is the C++ equivalent of a failing test.)

- [ ] **Step 5: Commit (Meta repo)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add Meta/include/meta/presets/compat.hpp Meta/src/presets/compat.cpp tests/test_meta/main.cpp
git commit -m "feat(presets): compat vocabulary for legacy-attribute kinds (slider/enum/range/wavenumber/points/color/file/text/choice/curve)"
```

---

### Task 2: MetaUI dynamic bits — RangeBar active-toggle, ComboBox list re-pull, container reorder

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widget_renderer_inl/glm_vec2.inl` (RangeBar branch)
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widget_renderer_inl/std_string.inl` (ComboBox branch)
- Modify: `external/Meta/Meta/include/meta/core/attribute_container.hpp` + `external/Meta/Meta/src/attribute_container.cpp` (`set_insertion_order`)
- Modify: `external/Meta/tests/test_meta/main.cpp` (reorder assertion)

**Interfaces:**
- Produces: `bool AttributeContainer::set_insertion_order(const std::vector<std::string> &order)` — reorders `insertion_order_`; returns false (and leaves order unchanged) unless `order` is a permutation of the current keys. Metadata contract for RangeBar: `"ui.has_active_toggle"` (bool) + `"ui.active"` (bool) — the facade's RangeHandle (Task 3) reads/writes `"ui.active"`.

- [ ] **Step 1: `set_insertion_order`** — declaration in `attribute_container.hpp` next to `insertion_order()`; implementation in `attribute_container.cpp`:

```cpp
bool AttributeContainer::set_insertion_order(const std::vector<std::string> &order)
{
  if (order.size() != insertion_order_.size())
    return false;
  for (const auto &k : order)
    if (attributes_.find(k) == attributes_.end())
      return false;
  // reject duplicates
  std::set<std::string> uniq(order.begin(), order.end());
  if (uniq.size() != order.size())
    return false;
  insertion_order_ = order;
  return true;
}
```

(add `#include <set>` to the .cpp if missing). Append to `tests/test_meta/main.cpp`:

```cpp
  // --- set_insertion_order
  {
    meta::AttributeContainer oc;
    oc.add("a", 1); oc.add("b", 2); oc.add("c", 3);
    assert(oc.set_insertion_order({"c", "a", "b"}));
    assert(oc.insertion_order() == (std::vector<std::string>{"c", "a", "b"}));
    assert(!oc.set_insertion_order({"c", "a"}));          // wrong size
    assert(!oc.set_insertion_order({"c", "a", "zzz"}));   // unknown key
    std::cout << "set_insertion_order OK" << std::endl;
  }
```

- [ ] **Step 2: Run BUILD-META-TESTS** — expected `set_insertion_order OK`, `TEST_EXIT=0`.

- [ ] **Step 3: RangeBar active-toggle** in `glm_vec2.inl`'s RangeBar branch. Read the branch first; then: when `meta::common::try_get<bool>(attr, "ui.has_active_toggle", false)` is true, wrap the RangeBar in a row with a `QCheckBox` (no text) to its left, initialized from `try_get<bool>(attr, "ui.active", true)`. On toggle:

```cpp
QObject::connect(active_box, &QCheckBox::toggled, widget,
                 [widget, &attr, range_bar](bool checked)
                 {
                   attr.metadata().try_add(std::string("ui.active"), checked)->value() = checked;
                   range_bar->setEnabled(checked);
                   // is_active affects compute: treat as a value edit
                   widget->notify_edit_started();
                   attr.value_changed.notify(attr.value());
                   widget->notify_value_changed();
                   widget->notify_edit_ended();
                 });
range_bar->setEnabled(initial_active);
```

Match the surrounding widget's actual signal-emission helpers — read how the same .inl emits edit_started/value_changed/edit_ended for slider drags and use the identical mechanism (names above are indicative; the source is authoritative). Also extend the widget's sync-from-model lambda in the same branch to re-read `"ui.active"` and update the checkbox + enabled state (blocking the checkbox's signals while syncing).

- [ ] **Step 4: ComboBox list re-pull** in `std_string.inl`: in the ComboBox branch's sync-from-model callback (the lambda subscribed to `value_changed` / the sync path added in R1 — read the current structure), re-read `meta::common::allowed_values(attr)`; if the item set differs from the combo's current items, repopulate (with the combo's signals blocked), then re-select the model value. This makes the Receive node's dynamic tag list refresh without a panel rebuild.

- [ ] **Step 5: Build the Qt side** — MetaUI compiles only inside a Qt-enabled build; use the Hesiod build for that: run BUILD-HESIOD. Expected `BUILD_EXIT=0` (Hesiod links meta_qt; no Hesiod sources changed yet).

- [ ] **Step 6: Commit (Meta repo)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add Meta/include/meta/core/attribute_container.hpp Meta/src/attribute_container.cpp \
        MetaUI/qt/include/meta_qt/widget_renderer_inl/glm_vec2.inl \
        MetaUI/qt/include/meta_qt/widget_renderer_inl/std_string.inl \
        tests/test_meta/main.cpp
git commit -m "feat(ui): RangeBar active-toggle via ui.active metadata; ComboBox re-pulls allowed_values on sync; AttributeContainer::set_insertion_order"
```

---

### Task 3: Hesiod compat layer — tags, traits, handles, decoders

**Files:**
- Create: `Hesiod/include/hesiod/model/nodes/compat/legacy_compat.hpp` (tags + traits + handles; header-only templates)
- Create: `Hesiod/include/hesiod/model/nodes/compat_attributes.hpp` (node-facing umbrella)
- Create: `Hesiod/src/model/nodes/compat/legacy_compat.cpp` (legacy `type_string` table + non-template helpers)

**Interfaces:**
- Consumes: Task 1 presets (exact signatures above), `meta::Attribute<T>`, `meta::keys`.
- Produces — everything Tasks 4-6 and node code rely on:
  - `hsd::compat::{FloatAttribute, IntAttribute, BoolAttribute, EnumAttribute, SeedAttribute, RangeAttribute, WaveNbAttribute, Vec2FloatAttribute, CloudAttribute, ColorAttribute, ColorGradientAttribute, FilenameAttribute, StringAttribute, ChoiceAttribute, VecFloatAttribute}` tag structs
  - `hsd::compat::Stop` (`{float position; std::array<float,4> color;}`)
  - `hsd::compat::legacy_traits<Tag>` with: `storage` (Meta value type), `legacy_value` (what `get_attr` returns), `static meta::Attribute<storage>& create(meta::AttributeContainer&, const std::string& key, <legacy ctor args>)` (one overload per legacy ctor, identical defaults), `static legacy_value to_legacy(const storage&)`, `static void decode(meta::Attribute<storage>&, const nlohmann::json&)`, `static constexpr const char* type_string`
  - Handles: `RangeHandle{get_is_active(), set_is_active(bool)}`, `ChoiceHandle{get_value(), set_value(str), set_choice_list(vector<string>), set_use_combo_list(bool)}`, `StringHandle{set_value(str)}`, `FilenameHandle{set_value(path)}`, `BoolHandle{set_value(bool)}` — each holds `meta::Attribute<storage>*` and has `Handle* operator->() { return this; }` so legacy `node.get_attr_ref<X>(key)->method()` call sites compile unchanged
  - `template <typename T> concept CompatTag = requires { typename legacy_traits<T>::storage; };`

- [ ] **Step 1: Write `legacy_compat.hpp`.** Skeleton and the full Float specialization as the canonical pattern; then write EVERY other specialization with the same completeness (ctor shapes + defaults per the survey table; legacy field decode per the Global-Constraints json reference):

```cpp
#pragma once
#include <cfloat>
#include <climits>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "meta/core/attribute_container.hpp"
#include "meta/metadata/keys.hpp"
#include "meta/presets/compat.hpp"
#include "meta/presets/numeric.hpp"

#include "hesiod/logger.hpp"

namespace hsd::compat
{

struct Stop
{
  float                position;
  std::array<float, 4> color;
};

// --- tags (template selectors only; never instantiated)
struct FloatAttribute {};
struct IntAttribute {};
struct BoolAttribute {};
struct EnumAttribute {};
struct SeedAttribute {};
struct RangeAttribute {};
struct WaveNbAttribute {};
struct Vec2FloatAttribute {};
struct CloudAttribute {};
struct ColorAttribute {};
struct ColorGradientAttribute {};
struct FilenameAttribute {};
struct StringAttribute {};
struct ChoiceAttribute {};
struct VecFloatAttribute {};

template <typename T> struct legacy_traits; // primary: undefined (unknown tag = compile error)

template <typename T>
concept CompatTag = requires { typename legacy_traits<T>::storage; };

// marker key helpers
inline void add_compat_markers(meta::AbstractAttribute &a, const char *type_string, bool is_seed = false)
{
  a.metadata().try_add(std::string("compat.legacy_type"), std::string(type_string));
  if (is_seed)
    a.metadata().try_add(std::string("compat.seed"), true);
}

// tolerant field read (legacy json_safe_get parity: warn + keep default)
template <typename V>
inline void safe_get(const nlohmann::json &j, const char *field, V &out, const std::string &key)
{
  if (j.contains(field))
  {
    try { out = j.at(field).get<V>(); }
    catch (const std::exception &e)
    { hesiod::Logger::log()->warn("compat decode: key '{}' field '{}': {}", key, field, e.what()); }
  }
  else
    hesiod::Logger::log()->warn("compat decode: key '{}' missing field '{}', keeping default", key, field);
}

inline glm::vec2 vec2_from_json(const nlohmann::json &j, const char *field, glm::vec2 fallback,
                                const std::string &key)
{
  if (j.contains(field) && j.at(field).is_array() && j.at(field).size() == 2)
    return {j.at(field)[0].get<float>(), j.at(field)[1].get<float>()};
  hesiod::Logger::log()->warn("compat decode: key '{}' bad/missing vec2 field '{}'", key, field);
  return fallback;
}

// ---------------------------------------------------------------- Float
template <> struct legacy_traits<FloatAttribute>
{
  using storage = float;
  using legacy_value = float;
  static constexpr const char *type_string = "Float";

  static meta::Attribute<float> &create(meta::AttributeContainer &c, const std::string &key,
                                        const std::string &label, float value,
                                        float vmin = -FLT_MAX, float vmax = FLT_MAX,
                                        const std::string &value_format = "{:.3f}",
                                        bool log_scale = false)
  {
    auto &a = meta::presets::slider_float(c, key, label, value, vmin, vmax, value_format,
                                          log_scale);
    add_compat_markers(a, type_string);
    return a;
  }

  static legacy_value to_legacy(const storage &v) { return v; }

  static void decode(meta::Attribute<float> &a, const nlohmann::json &j, const std::string &key)
  {
    float v = a.value();
    safe_get(j, "value", v, key);
    a.set_from_any(v); // fires value_changed so any open widgets sync
  }
};
```

Now the remaining specializations — write each one out fully in the header (complete `create` overload set, `to_legacy`, `decode`); their exact behaviours:

| Tag | storage | legacy_value | create overloads (defaults verbatim from legacy ctors) | decode |
|---|---|---|---|---|
| Int | `int` | `int` | `(label, value, vmin=-INT_MAX, vmax=INT_MAX, format="{}")` → `slider_int` | `value` |
| Bool | `bool` | `bool` | `(label, value)` → `checkbox`; `(label, label_true, label_false, value)` → `binary_buttons` | `value` |
| Enum | `int` | `int` | `(label, map<string,int>)` → default `map.begin()->second`; `(label, map, choice_string)` → `map.at(choice)` with fallback `map.begin()->second` + warn if absent. Build `items` as `std::vector<std::pair<int,std::string>>` by iterating the map (`{v, k}` per entry, map order) → `enum_choice`. | prefer `"choice"` string mapped through the metadata `enum_items` (unknown → warn + keep); if `"choice"` absent fall back to `"value"` int |
| Seed | `int` | `uint` (`to_legacy` = `static_cast<uint>`) | `()` → label "Seed", value 0; `(label, uint value = 0)` → `meta::presets::seed(c, key, label, (int)value)` then `add_compat_markers(a, "Seed", /*is_seed=*/true)` | `value` as uint → int |
| Range | `glm::vec2` | `glm::vec2` | `(label, is_active = true)` → value {0,1}, vmin -1, vmax 2, format "{:.3f}"; `(label, value, vmin, vmax, is_active=true, format="{:.2f}")` → `range` preset | `value` vec2 + `is_active` → write metadata `"ui.active"` via `try_add(...)->value() =` |
| WaveNb | `glm::vec2` | `glm::vec2` | `()` → ("Wavenumber"?, use label = key) value {2,2}, vmin 0, vmax FLT_MAX, link true, fmt "{:.2f}"; `(label)`; `(label, value, vmin, vmax, link_xy=true, format="{:.2f}")` → `wavenumber` preset. **Verify the 0/1-arg legacy ctor label defaults verbatim in `wave_nb_attribute.cpp` before coding.** | `value` vec2; `link_xy` → metadata `"ui.locked_xy"` |
| Vec2Float | `glm::vec2` | `glm::vec2` | `(label)` → value {0.5,0.5}, bounds [0,1]²; `(label, value, xmin, xmax, ymin, ymax)` → `xy` preset | `value` vec2 |
| Cloud | `std::vector<glm::vec3>` | same | `(label)`; `(label, bool are_points_connected)` (flag → metadata `"ui.closed"`); `(label, vector<glm::vec3> value)` → `points` preset | parallel arrays `x`,`y`,`values` → rebuild vec3 list (all three must exist and be same length, else warn + keep) |
| Color | `glm::vec4` | `std::array<float,4>` | `(label, array<float,4>)`, `(label, r, g, b, a)` → `color` preset (convert to vec4) | `value` array of 4 → vec4 |
| ColorGradient | `meta::ColorGradient` | `std::vector<hsd::compat::Stop>` (`to_legacy` copies stops field-by-field) | `(label)` (default gradient), `(label, vector<Stop>)` (convert into `meta::ColorGradient` via `set_value`) → `color_gradient` preset | `value` array of `{position, color}` → `ColorGradient::set_value` (skip malformed stops, mirroring legacy `contains()` guards) |
| Filename | `std::filesystem::path` | `std::filesystem::path` | `(label, path value, filter = "", for_saving = true)` → `file` preset | `value` string |
| String | `std::string` | `std::string` | `(label, value)`, `(label, value, bool read_only)` → `text` preset | `value` |
| Choice | `std::string` | `std::string` | `(choice_list, value)` (label = key), `(label, choice_list, value)`, `(label, choice_list)` (value = `choice_list.front()`, throw `std::invalid_argument` on empty list — legacy parity) → `string_choice` preset | `value` string, validated against metadata `allowed_values` (absent from list → warn + keep) |
| VecFloat | `std::vector<float>` | `std::vector<float>` | `(label, vector<float> value, vmin, vmax, is_size_variable = true)` → `curve` preset | `value` array |

Notes for the implementer:
- `decode` signatures all take `(meta::Attribute<storage>&, const nlohmann::json&, const std::string& key)`.
- All `decode`s end with `a.set_from_any(<new value>)` EXCEPT where only metadata changed (Range is_active, WaveNb link) — set metadata first, then `set_from_any` the value so one notify covers it.
- `EnumAttribute::decode` reads `enum_items` via `a.metadata().value<std::vector<std::pair<int,std::string>>>(meta::keys::constraints::enum_items)`.
- Where a table row says "Verify … before coding", read the named legacy source file and copy the actual defaults; the legacy source is authoritative over this table.

Then the handles, at the bottom of `legacy_compat.hpp`:

```cpp
// --- handles: faithful stand-ins for the legacy get_attr_ref<T>() mutable pointers.
// Value semantics + operator-> so `node.get_attr_ref<X>(k)->method()` compiles unchanged.

class RangeHandle
{
public:
  explicit RangeHandle(meta::Attribute<glm::vec2> *p) : p_(p) {}
  RangeHandle *operator->() { return this; }

  bool get_is_active() const
  {
    if (const auto *m = p_->metadata().try_value<bool>("ui.active"))
      return *m;
    return true;
  }
  void set_is_active(bool v)
  {
    p_->metadata().try_add(std::string("ui.active"), v)->value() = v;
  }

private:
  meta::Attribute<glm::vec2> *p_;
};

class ChoiceHandle
{
public:
  explicit ChoiceHandle(meta::Attribute<std::string> *p) : p_(p) {}
  ChoiceHandle *operator->() { return this; }

  std::string get_value() const { return p_->value(); }
  void        set_value(const std::string &v) { p_->set_from_any(v); }
  void        set_use_combo_list(bool combo)
  {
    p_->metadata().try_add(std::string(meta::keys::ui::widget_type),
                           std::string(combo ? "ComboBox" : "ButtonGrid"))
        ->value() = combo ? "ComboBox" : "ButtonGrid";
  }
  void set_choice_list(const std::vector<std::string> &choices)
  {
    p_->metadata()
        .try_add(std::string(meta::keys::constraints::allowed_values), choices)
        ->value() = choices;
  }

private:
  meta::Attribute<std::string> *p_;
};

class StringHandle
{
public:
  explicit StringHandle(meta::Attribute<std::string> *p) : p_(p) {}
  StringHandle *operator->() { return this; }
  void          set_value(const std::string &v) { p_->set_from_any(v); }

private:
  meta::Attribute<std::string> *p_;
};

class FilenameHandle
{
public:
  explicit FilenameHandle(meta::Attribute<std::filesystem::path> *p) : p_(p) {}
  FilenameHandle *operator->() { return this; }
  void            set_value(const std::filesystem::path &v) { p_->set_from_any(v); }

private:
  meta::Attribute<std::filesystem::path> *p_;
};

class BoolHandle
{
public:
  explicit BoolHandle(meta::Attribute<bool> *p) : p_(p) {}
  BoolHandle *operator->() { return this; }
  void        set_value(bool v) { p_->set_from_any(v); }

private:
  meta::Attribute<bool> *p_;
};

// which handle a tag's get_attr_ref returns
template <typename T> struct handle_of; // undefined by default
template <> struct handle_of<RangeAttribute>    { using type = RangeHandle; };
template <> struct handle_of<ChoiceAttribute>   { using type = ChoiceHandle; };
template <> struct handle_of<StringAttribute>   { using type = StringHandle; };
template <> struct handle_of<FilenameAttribute> { using type = FilenameHandle; };
template <> struct handle_of<BoolAttribute>     { using type = BoolHandle; };

} // namespace hsd::compat
```

Check `try_value<bool>` exists on the metadata container (it does on `AttributeContainer`: returns `T*` or nullptr) — if the metadata accessor differs, use `find` + `try_cast<meta::Attribute<bool>>()`.

- [ ] **Step 2: Write `compat_attributes.hpp`** (the header node files will include INSTEAD of `"attributes.hpp"`):

```cpp
/* Node-facing compatibility header: legacy attr:: names backed by Meta storage.
   Node files include this instead of the legacy "attributes.hpp".
   NEVER include this in a TU that also includes "attributes.hpp" (brush.cpp,
   base_node.*, node_attributes_widget.*) — the attr:: names would collide. */
#pragma once
#include "hesiod/model/nodes/compat/legacy_compat.hpp"

namespace attr
{
using hsd::compat::FloatAttribute;
using hsd::compat::IntAttribute;
using hsd::compat::BoolAttribute;
using hsd::compat::EnumAttribute;
using hsd::compat::SeedAttribute;
using hsd::compat::RangeAttribute;
using hsd::compat::WaveNbAttribute;
using hsd::compat::Vec2FloatAttribute;
using hsd::compat::CloudAttribute;
using hsd::compat::ColorAttribute;
using hsd::compat::ColorGradientAttribute;
using hsd::compat::FilenameAttribute;
using hsd::compat::StringAttribute;
using hsd::compat::ChoiceAttribute;
using hsd::compat::VecFloatAttribute;
using hsd::compat::Stop;
} // namespace attr
```

- [ ] **Step 3: Write `legacy_compat.cpp`** — only if something can't stay header-only (expected: nothing; create the file with the copyright header and a comment reserving it, or skip creating it). If skipped, remove it from the commit list.

- [ ] **Step 4: Compile check.** The layer is dormant (nothing includes it yet). Force-compile it: temporarily add `#include "hesiod/model/nodes/compat_attributes.hpp"` at the top of `Hesiod/src/model/nodes/base_node.cpp`… do NOT — that TU includes `attributes.hpp` (collision, per the header comment). Instead create a scratch TU `Hesiod/src/model/nodes/compat/compat_compile_check.cpp`:

```cpp
// compile-only exercise of the compat layer (no runtime use)
#include "hesiod/model/nodes/compat_attributes.hpp"

namespace hesiod::compat_check
{
void compile_check(meta::AttributeContainer &c)
{
  using namespace hsd::compat;
  legacy_traits<FloatAttribute>::create(c, "f", "F", 0.5f);
  legacy_traits<FloatAttribute>::create(c, "f2", "F", 0.5f, 0.f, 1.f, "{:.2f}", true);
  legacy_traits<BoolAttribute>::create(c, "b", "B", true);
  legacy_traits<BoolAttribute>::create(c, "b2", "B", "on", "off", false);
  std::map<std::string, int> m = {{"a", 0}, {"z", 3}};
  legacy_traits<EnumAttribute>::create(c, "e", "E", m);
  legacy_traits<EnumAttribute>::create(c, "e2", "E", m, "z");
  legacy_traits<SeedAttribute>::create(c, "s");
  legacy_traits<RangeAttribute>::create(c, "r", "R", false);
  legacy_traits<WaveNbAttribute>::create(c, "w", "W", glm::vec2(2.f, 2.f), 0.f, 64.f);
  legacy_traits<Vec2FloatAttribute>::create(c, "v", "V");
  legacy_traits<CloudAttribute>::create(c, "cl", "C");
  legacy_traits<ColorAttribute>::create(c, "co", "C", 1.f, 0.f, 0.f, 1.f);
  legacy_traits<ColorGradientAttribute>::create(c, "cg", "G");
  legacy_traits<FilenameAttribute>::create(c, "fn", "F", "out.png", "PNG (*.png)", true);
  legacy_traits<StringAttribute>::create(c, "st", "S", "hello");
  legacy_traits<ChoiceAttribute>::create(c, "ch", "C", std::vector<std::string>{"x", "y"});
  legacy_traits<VecFloatAttribute>::create(c, "vf", "V", std::vector<float>(8, 0.5f), 0.f, 1.f);
  RangeHandle h(nullptr);
  (void)h;
}
} // namespace hesiod::compat_check
```

(Adjust the calls if a `create` signature you wrote differs — the point is every specialization instantiates.)

- [ ] **Step 5: Run BUILD-HESIOD** — expected `BUILD_EXIT=0`.

- [ ] **Step 6: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/include/hesiod/model/nodes/compat/legacy_compat.hpp \
        Hesiod/include/hesiod/model/nodes/compat_attributes.hpp \
        Hesiod/src/model/nodes/compat/compat_compile_check.cpp
git commit -m "feat(compat): legacy-attribute tags, traits, handles and decoders over Meta presets (dormant)"
```

---

### Task 4: BaseNode routing, finalize_attributes, factory hook

**Files:**
- Modify: `Hesiod/include/hesiod/model/nodes/base_node.hpp` (templates at lines 89-109; members ~123-126)
- Modify: `Hesiod/src/model/nodes/base_node.cpp` (add `finalize_attributes`; extend `json_from` ~396-436, `reseed` ~566-579, `update_attributes_tool_tip` ~598-633, `node_parameters_to_json` ~464-519)
- Modify: `Hesiod/src/model/nodes/node_factory.cpp` (SETUP_NODE macro lines 19-26; Broadcast/Receive branches ~455-467)

**Interfaces:**
- Consumes: Task 3 (`hsd::compat::legacy_traits`, `CompatTag`, `handle_of`), Task 2 (`set_insertion_order`).
- Produces: `void BaseNode::finalize_attributes()`; `add_attr`/`get_attr`/`get_attr_ref` that accept BOTH real legacy types and compat tags; member `std::map<std::string, std::function<void(const nlohmann::json &)>> legacy_decoders_`; member `nlohmann::json initial_meta_state_` + accessor `const nlohmann::json &initial_meta_state() const`. Tasks 5/6 rely on these exact names.

- [ ] **Step 1: base_node.hpp — include + members.** Add `#include "hesiod/model/nodes/compat/legacy_compat.hpp"` (NOT `compat_attributes.hpp` — no `attr::` aliasing here; this header must coexist with `attributes/abstract_attribute.hpp`). Add members next to `meta_group_`:

```cpp
  // legacy-json fallback decoders, registered by add_attr (compat tags only)
  std::map<std::string, std::function<void(const nlohmann::json &)>> legacy_decoders_;
  // container state captured at finalize time; toolbar "Reset Settings" restores it
  nlohmann::json initial_meta_state_;
```

and public declarations:

```cpp
  void                  finalize_attributes();
  const nlohmann::json &initial_meta_state() const { return this->initial_meta_state_; }
```

- [ ] **Step 2: base_node.hpp — the three templates.** Replace lines 89-105 with:

```cpp
  // --- Attribute Management ---
  // Two storage backends coexist during the Meta migration:
  //  - real attr::* types (is_base_of AbstractAttribute) -> legacy this->attr map (Brush)
  //  - hsd::compat tags -> Meta container via legacy_traits (everything else)
  template <typename T, typename... Args>
  void add_attr(const std::string &key, Args &&...args)
  {
    if constexpr (std::is_base_of_v<attr::AbstractAttribute, T>)
    {
      this->attr[key] = std::make_unique<T>(std::forward<Args>(args)...);
    }
    else
    {
      static_assert(hsd::compat::CompatTag<T>,
                    "add_attr<T>: T is neither a legacy attribute nor a compat tag");
      auto &a = hsd::compat::legacy_traits<T>::create(this->meta_group().current(),
                                                      key,
                                                      std::forward<Args>(args)...);
      this->legacy_decoders_[key] = [&a, key](const nlohmann::json &j)
      { hsd::compat::legacy_traits<T>::decode(a, j, key); };
    }
  }

  template <typename T> auto get_attr(const std::string &key) const -> decltype(auto)
  {
    if constexpr (std::is_base_of_v<attr::AbstractAttribute, T>)
    {
      if (!this->attr.contains(key))
        throw std::invalid_argument("unknown attribute key: " + key); // was silently not thrown
      return this->attr.at(key)->get_ref<T>()->get_value();
    }
    else
    {
      static_assert(hsd::compat::CompatTag<T>);
      using traits = hsd::compat::legacy_traits<T>;
      return traits::to_legacy(
          this->meta_group().current().value<typename traits::storage>(key));
    }
  }

  template <typename T> auto get_attr_ref(const std::string &key) const
  {
    if constexpr (std::is_base_of_v<attr::AbstractAttribute, T>)
    {
      return this->attr.at(key)->get_ref<T>();
    }
    else
    {
      using storage = typename hsd::compat::legacy_traits<T>::storage;
      // legacy get_attr_ref was const-returning-mutable; mirror that
      auto &c = const_cast<BaseNode *>(this)->meta_group().current();
      auto *p = c.find(key);
      if (!p)
        throw std::invalid_argument("unknown attribute key: " + key);
      auto *typed = p->template try_cast<meta::Attribute<storage>>();
      if (!typed)
        throw std::runtime_error("wrong attribute type for key: " + key);
      return typename hsd::compat::handle_of<T>::type(typed);
    }
  }
```

Note `get_attr`'s legacy branch behaviour changes: the never-thrown exception now throws. `value<T>` already throws `std::out_of_range` on the Meta side. `<stdexcept>` include if missing.

- [ ] **Step 3: `finalize_attributes()` in base_node.cpp:**

```cpp
void BaseNode::finalize_attributes()
{
  if (!this->uses_meta())
    return;

  auto &c = this->meta_group().current();

  // 1) _GROUPBOX_ sentinels in the ordered-key list -> ui.category metadata
  //    + build the sanitized display order
  std::vector<std::string> order;
  std::string              category = "";

  for (const auto &key : this->attr_ordered_key)
  {
    if (key.starts_with("_GROUPBOX_BEGIN_"))
    {
      category = key.substr(std::string("_GROUPBOX_BEGIN_").size());
      continue;
    }
    if (key.starts_with("_GROUPBOX_END"))
    {
      category = "";
      continue;
    }
    auto *p = c.find(key);
    if (!p)
    {
      Logger::log()->warn("finalize_attributes: node {}: ordered key '{}' not found", this->get_label(), key);
      continue;
    }
    if (!category.empty())
      p->metadata().try_add(std::string(meta::keys::ui::category), category)->value() = category;
    order.push_back(key);
  }

  // 2) render order = legacy ordered-key order, unlisted keys appended
  if (!order.empty())
  {
    for (const auto &key : c.insertion_order())
      if (std::find(order.begin(), order.end(), key) == order.end())
        order.push_back(key);
    if (!c.set_insertion_order(order))
      Logger::log()->warn("finalize_attributes: node {}: set_insertion_order rejected", this->get_label());
  }

  // 3) initial state for toolbar Reset
  this->initial_meta_state_ = c.json_to();
}
```

(`try_add(...)->value() =` needs the returned attribute; category is `std::string` so the string overload applies. Add `#include <algorithm>` if missing.)

- [ ] **Step 4: factory hook.** In `node_factory.cpp` extend the macro:

```cpp
#define SETUP_NODE(NodeType, node_type)                                                  \
  case str2int(#NodeType):                                                               \
    setup_##node_type##_node(*sptr);                                                     \
    sptr->set_compute_fct(&compute_##node_type##_node);                                  \
    sptr->update_attributes_tool_tip();                                                  \
    sptr->finalize_attributes();                                                         \
    break;
```

and add `sptr->finalize_attributes();` after `setup_broadcast_node(*sptr);`/`setup_receive_node(*sptr);` in the two specialized branches (after their `set_compute_fct` lines).

- [ ] **Step 5: json_from legacy fallback.** In `BaseNode::json_from`, replace the `else` error branch of the `uses_meta()` block with:

```cpp
      if (json.contains("_meta"))
      {
        this->meta_group().current().json_from(json["_meta"]);
      }
      else if (!this->legacy_decoders_.empty())
      {
        // legacy-format file: decode per-key values written by the old
        // Attributes library into the Meta container
        Logger::log()->info("BaseNode::json_from: node '{}' loading legacy-format parameters",
                            this->get_id());
        for (const auto &[key, decoder] : this->legacy_decoders_)
        {
          if (json.contains(key))
            decoder(json[key]);
          else
            Logger::log()->warn("Missing JSON key for attribute: {}, using default", key);
        }
      }
      else
      {
        Logger::log()->error(
            "BaseNode::json_from: node '{}' uses Meta storage but neither '_meta' nor "
            "legacy decoders are available — parameters NOT restored",
            this->get_id());
      }
```

(The three hand-migrated nodes have no decoders — they keep today's fail-loud path via the final else.)

- [ ] **Step 6: reseed meta branch.** Append to `BaseNode::reseed` after the legacy loop:

```cpp
  if (this->uses_meta())
  {
    for (const auto &key : this->meta_group().current().insertion_order())
    {
      auto *p = this->meta_group().current().find(key);
      if (!p)
        continue;
      if (const bool *is_seed = p->metadata().try_value<bool>("compat.seed"); is_seed && *is_seed)
        if (auto *typed = p->try_cast<meta::Attribute<int>>())
        {
          int increment = backward ? -1 : 1;
          typed->set_from_any(typed->value() + increment);
        }
    }
  }
```

(If `try_value` on the metadata container has a different name, use the same accessor as the handles in Task 3.)

- [ ] **Step 7: update_attributes_tool_tip meta branch.** After the legacy `for (auto &[key, sp_attr] : this->attr)` loop, add an equivalent loop for the Meta container: iterate `insertion_order()`, look up documentation the same way (`documentation["parameters"][key]["description"]`), build the identical HTML string (copy the existing string-building block verbatim; label comes from `p->metadata().try_value<std::string>(meta::keys::ui::label)` falling back to `key`), then store it: `p->metadata().try_add(std::string(meta::keys::ui::tooltip), description)->value() = description;`.

- [ ] **Step 8: node_parameters_to_json meta branch.** In the "Attribute information" section, after the legacy loop add:

```cpp
    if (this->uses_meta())
      for (const auto &key : this->meta_group().current().insertion_order())
      {
        const auto *p = this->meta_group().current().find(key);
        if (!p)
          continue;
        nlohmann::json param_info;
        param_info["key"] = key;
        const std::string *lbl = p->metadata().try_value<std::string>(meta::keys::ui::label);
        param_info["label"] = lbl ? *lbl : key;
        const std::string *lt = p->metadata().try_value<std::string>("compat.legacy_type");
        param_info["type"] = lt ? *lt : std::string(p->type().name());
        auto json_ptr = nlohmann::json::json_pointer("/parameters/" + key + "/description");
        param_info["description"] = this->documentation.value(json_ptr, "No description");
        params_json[key] = param_info;
      }
```

so the docs pipeline (`--inventory` → `node_documentation.json`, hsd toolkit catalog) emits identical records post-flip. The `type_string` values in Task 3's traits MUST match the legacy `attr::attribute_type_map` strings exactly — copy them from `external/Attributes/Attributes/src/abstract_attribute.cpp` (or wherever `attribute_type_map` is defined) when writing Task 3 if not already done; known: "Float", "Wavenumber". Skip container entries whose metadata contains `compat.legacy_type` absent AND belong to the hand-migrated native nodes — they'll emit `p->type().name()`; acceptable.

- [ ] **Step 9: Run BUILD-HESIOD** — expected `BUILD_EXIT=0` (all still dormant: no node includes the compat umbrella yet).

- [ ] **Step 10: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/include/hesiod/model/nodes/base_node.hpp Hesiod/src/model/nodes/base_node.cpp \
        Hesiod/src/model/nodes/node_factory.cpp
git commit -m "feat(compat): BaseNode dual routing (legacy types + compat tags), finalize_attributes, legacy-json fallback, meta reseed/tooltip/docs branches"
```

---

### Task 5: Shared-helper rewrites onto ui.data_provider

**Files:**
- Modify: `Hesiod/src/model/nodes/setup_histogram_for_range_slider.cpp`
- Modify: `Hesiod/src/model/nodes/setup_background_image_for_cloud_attribute.cpp`

**Interfaces:**
- Consumes: `meta::DataProvider`/`meta::ProviderData` (G1/G2 machinery), `node.uses_meta()`, `meta::keys::ui::data_provider`.
- Produces: same two function signatures, now branching `if (node.uses_meta())` to Meta wiring; legacy path preserved verbatim for Brush-era callers.

- [ ] **Step 1: histogram helper.** Keep the existing histogram lambda maths byte-for-byte (it computes `hist.first`/`hist.second`). Add at the top of the function:

```cpp
  if (node.uses_meta())
  {
    auto provider = [&node, port_id]() -> meta::ProviderData
    {
      meta::ProviderData data;
      // (same maths as the legacy lambda below, writing to data.series_x / data.series_y)
      ...
      return data;
    };

    auto &c = node.meta_group().current();
    auto *p = c.find(attribute_key);
    if (!p)
    {
      Logger::log()->error("setup_histogram_for_range_attribute: meta key '{}' not found", attribute_key);
      return;
    }
    p->metadata().try_add(std::string(meta::keys::ui::data_provider), meta::DataProvider(provider));
    return;
  }
```

Port the provider body from the already-working pattern in `Hesiod/src/model/nodes/nodes_function/saturate.cpp` (the G1 Saturate histogram provider — same VirtualArray → 256-bin fill, INCLUDING its clamped bin index `bin = bin < 0 ? 0 : (bin >= nbins ? nbins - 1 : bin);` which the legacy lambda lacks). The legacy `set_autorange(true)` has no Meta equivalent yet — drop it on the meta path (the provider recomputes bounds each pull, which is what autorange achieved).

- [ ] **Step 2: cloud background helper.** Same structure: `if (node.uses_meta())` branch building a `meta::ProviderData` image provider — port the provider body from `Hesiod/src/model/nodes/nodes_function/cloud.cpp` (the G2 pattern: colorize the port to MAGMA 256×256 RGB, vertical flip, fill `image_width/height/channels/pixels`), attach via `try_add(ui::data_provider, ...)`; legacy QImage path kept below.

- [ ] **Step 3: Run BUILD-HESIOD** — expected `BUILD_EXIT=0`.

- [ ] **Step 4: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/src/model/nodes/setup_histogram_for_range_slider.cpp \
        Hesiod/src/model/nodes/setup_background_image_for_cloud_attribute.cpp
git commit -m "feat(compat): shared histogram/background helpers gain ui.data_provider path for meta-backed nodes"
```

---

### Task 6: GUI edges — toolbar snapshots, screenshots dump, direct-writer codemods

**Files:**
- Modify: `Hesiod/src/gui/widgets/node_attributes_widget.cpp` (toolbar lambdas ~103-120)
- Modify: `Hesiod/src/model/nodes/node_factory.cpp` (`dump_node_settings_screenshots` ~114-137)
- Modify: `Hesiod/src/gui/widgets/graph_node_widget.cpp` (import-drop writers ~98-143)
- Modify: `Hesiod/src/model/nodes/broadcast_node.cpp` + `Hesiod/src/model/nodes/receive_node.cpp`

**Interfaces:**
- Consumes: Task 4 (`get_attr_ref` handle path, `initial_meta_state()`), `NodeAttributesWidget::sync_from_model()`, `meta::AttributeContainer::json_to/json_from/snapshot_manager`.
- Produces: nothing new for later tasks; closes out every direct legacy-map writer outside `nodes_function/`.

- [ ] **Step 1: toolbar meta branches.** In `node_attributes_widget.cpp`, extend each of the 5 lambdas with an else-path. Add a private helper first (this file already resolves `p_node` in `setup_layout`; store `BaseNode *p_node` lookup inside the lambdas the same way `setup_layout` does — via `p_graph_node.lock()` + `get_node_ref_by_id<BaseNode>`):

```cpp
  // meta-backed nodes: state/preset operate on the Meta container json
  auto meta_container = [this]() -> meta::AttributeContainer *
  {
    auto gno = this->p_graph_node.lock();
    if (!gno)
      return nullptr;
    BaseNode *p_node = gno->get_node_ref_by_id<BaseNode>(this->node_id);
    if (!p_node || !p_node->uses_meta())
      return nullptr;
    return &p_node->meta_group().current();
  };
```

Then per button:
- **Backup State**: `else if (auto *c = meta_container()) c->snapshot_manager().save("user_state", c->json_to());`
- **Revert State**: `else if (auto *c = meta_container()) { if (c->snapshot_manager().has("user_state")) { c->json_from(c->snapshot_manager().load("user_state"), true); this->sync_from_model(); if (auto gno = this->p_graph_node.lock()) gno->update(this->node_id); } }`
- **Reset Settings**: same shape, restoring `p_node->initial_meta_state()` (fetch `p_node` inside the lambda; guard `!initial_meta_state().empty()`).
- **Save Preset**: QFileDialog::getSaveFileName (same args as legacy: `"preset.json", ".", "json file (*.json)"`), write `c->json_to().dump(4)`.
- **Load Preset**: QFileDialog::getOpenFileName, parse, `c->json_from(json, true)`, then `sync_from_model()` + `gno->update(node_id)`.

Wrap file IO in the same error-logged patterns as the legacy `AttributesWidget::on_load_preset/on_save_preset`. Note: `snapshot_manager` state (`user_state`) will serialize into `_meta` on save — acceptable for now ((#16 trim later); `initial_meta_state_` deliberately lives OUTSIDE the container so Reset state never bloats files.

- [ ] **Step 2: screenshots dump meta branch.** In `dump_node_settings_screenshots`, where the legacy `attr::AttributesWidget` is built, branch:

```cpp
    QWidget *widget = nullptr;
    if (p_base_node->uses_meta())
      widget = new meta::qt::ContainerGroupWidget(p_base_node->meta_group(),
                                                  meta::qt::ContainerRenderOptions{},
                                                  nullptr);
    else
      widget = new attr::AttributesWidget(p_base_node->get_attributes_ref(),
                                          p_base_node->get_attr_ordered_key_ref());
```

(then the existing grab/save logic uses `widget`). Add the `meta_qt` include used by `node_attributes_widget.cpp`.

- [ ] **Step 3: import-drop writers.** In `graph_node_widget.cpp` replace the three `get_attributes_ref()->at(...)->get_ref<attr::X>()->set_value(...)` sites with facade calls (these compile against BOTH backends only via compat tags — but this TU handles generic nodes, post-flip all meta; ImportHeightmap/ImportTexture will be meta-backed):

```cpp
p_node->get_attr_ref<hsd::compat::FilenameAttribute>("fname")->set_value(fname.toStdString());
p_node->get_attr_ref<hsd::compat::BoolAttribute>("dequantize")->set_value(true);
```

Use the `hsd::compat::` qualified names directly (this TU must NOT include `compat_attributes.hpp` because other GUI TUs include `attributes.hpp`; qualified tags avoid any `attr::` collision). `base_node.hpp` already brings `legacy_compat.hpp`.

- [ ] **Step 4: broadcast/receive rewrite.** In `broadcast_node.cpp`: swap `#include "attributes.hpp"` → `#include "hesiod/model/nodes/compat_attributes.hpp"`, and change `generate_broadcast_tag` to:

```cpp
  this->get_attr_ref<attr::StringAttribute>("tag")->set_value(this->broadcast_tag);
```

In `receive_node.cpp`: same include swap; `get_current_tag` already uses `get_attr<attr::ChoiceAttribute>` (compiles against the tag unchanged); rewrite `update_tag_list`:

```cpp
void ReceiveNode::update_tag_list(const std::vector<std::string> &new_tags)
{
  auto handle = this->get_attr_ref<attr::ChoiceAttribute>("tag");

  std::vector<std::string> new_tags_mod = new_tags;
  new_tags_mod.emplace(new_tags_mod.begin(), "NO TAG");

  handle->set_choice_list(new_tags_mod);

  std::string current_tag = handle->get_value();

  Logger::log()->trace("ReceiveNode::update_tag_list: current tag value: {}", current_tag);

  if (std::find(new_tags_mod.begin(), new_tags_mod.end(), current_tag) == new_tags_mod.end())
    handle->set_value(new_tags_mod.front());
}
```

Check both files' `setup_*` functions for other legacy-type uses (`setup_broadcast_node`/`setup_receive_node` add the `tag` attribute — they live in these same files or `nodes_function/`; whatever TU declares them must be include-swapped consistently in this task if it's these two files, else it happens in Task 9). The `receive.cpp` nodes_function file's `set_use_combo_list(true)` site swaps in Task 9.

- [ ] **Step 5: Run BUILD-HESIOD** — expected `BUILD_EXIT=0`. NOTE: after Step 4, Broadcast/Receive are the first meta-backed-by-facade nodes (their setup adds attrs via compat tags once their setup TU is swapped — if their setup functions live in `nodes_function/broadcast.cpp`/`receive.cpp` (NOT yet swapped), the handles in Step 4 will throw at runtime against legacy-stored attrs. In that case guard: do Step 4's include swap but ALSO swap `nodes_function/broadcast.cpp` + `nodes_function/receive.cpp` now (they become the first facade canaries).

- [ ] **Step 6: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/src/gui/widgets/node_attributes_widget.cpp Hesiod/src/model/nodes/node_factory.cpp \
        Hesiod/src/gui/widgets/graph_node_widget.cpp Hesiod/src/model/nodes/broadcast_node.cpp \
        Hesiod/src/model/nodes/receive_node.cpp
# plus nodes_function/broadcast.cpp receive.cpp if swapped per Step 5
git commit -m "feat(compat): toolbar state/preset on Meta container json, meta screenshots dump, direct-writer sites onto facade handles"
```

---

### Task 7: Parity dump CLI + legacy reference fixture

**Files:**
- Modify: `Hesiod/src/cli/batch_mode.cpp` (new `--parity-dump=<file>` flag)
- Modify: `Hesiod/src/model/nodes/node_factory.cpp` + its header decl (new `dump_node_attribute_parity`)
- Modify: `Hesiod/src/model/nodes/base_node.cpp/.hpp` (new `nlohmann::json BaseNode::attribute_parity_record() const`)
- Create: `scripts/compat_parity_diff.py`
- Create (fixture, generated): `docs/meta-migration/fixtures/parity-legacy.json`

**Interfaces:**
- Produces: per-node parity records, normalized identically for both backends:
  `{ "<key>": {"type": <type_string>, "label": <str>, "value": <json>, "bounds": [min,max]|null, "category": <str|"">}, ... , "__order": [keys...] }`

- [ ] **Step 1: `BaseNode::attribute_parity_record()`.** Legacy path: iterate `attr` map; `type` = `attr::attribute_type_map.at(get_type())`, `label` = `get_label()`, `value` = the `"value"` member of `attr->json_to()` (plus `is_active` for Range folded as `{"value": ..., "is_active": ...}`), `bounds` from json fields `vmin`/`vmax` when present else null, `category` derived by walking `attr_ordered_key` groupbox sentinels (same parse as `finalize_attributes`). `__order` = sanitized ordered keys (or attr-map keys if no order list). Meta path: iterate `insertion_order()`; `type` = `compat.legacy_type` metadata; `label` = `ui.label`; `value` = the `"value"` member of the attribute's `json_to()` (fold `ui.active` into the Range record as `is_active`); `bounds` = `[constraints.min, constraints.max]` when both present else null; `category` = `ui.category` metadata or `""`. Write both paths in one function so the normalization stays in one place. Skip `compat.seed`? No — seeds appear as `type "Seed"` both sides.

- [ ] **Step 2: `dump_node_attribute_parity(fname, config)`** in node_factory.cpp — clone `dump_node_documentation_stub`'s loop, but `json[name] = p_base_node->attribute_parity_record();`.

- [ ] **Step 3: CLI flag.** In `batch_mode.cpp`, add `args::ValueFlag<std::string> parity_dump(parser, "file", "dump attribute parity records", {"parity-dump"});` following the existing flags' style, and a dispatch branch before the others:

```cpp
    else if (parity_dump)
    {
      auto config = std::make_shared<hesiod::GraphConfig>();
      hesiod::dump_node_attribute_parity(args::get(parity_dump), config);
      return 0;
    }
```

- [ ] **Step 4: `scripts/compat_parity_diff.py`:**

```python
#!/usr/bin/env python3
"""Diff two --parity-dump outputs (legacy reference vs facade build).

Usage: compat_parity_diff.py ref.json new.json
Exit 0 = parity (allowed conversions only), 1 = differences found.
"""
import json, math, sys

ALLOWED_TYPE_CHANGES = {}  # facade must emit identical type strings

def norm(v):
    if isinstance(v, float):
        return round(v, 5)
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v

def main(ref_path, new_path):
    ref = json.load(open(ref_path))
    new = json.load(open(new_path))
    fails = []
    for node, rrec in ref.items():
        if node not in new:
            fails.append(f"{node}: missing from new dump"); continue
        nrec = new[node]
        for key, r in rrec.items():
            if key == "__order":
                if norm(r) != norm(nrec.get("__order")):
                    fails.append(f"{node}: order {r} != {nrec.get('__order')}")
                continue
            n = nrec.get(key)
            if n is None:
                fails.append(f"{node}.{key}: missing"); continue
            for field in ("type", "label", "value", "bounds", "category"):
                if norm(r.get(field)) != norm(n.get(field)):
                    fails.append(f"{node}.{key}.{field}: {r.get(field)!r} != {n.get(field)!r}")
    for node in new:
        if node not in ref:
            fails.append(f"{node}: extra node in new dump")
    print(f"{len(fails)} difference(s)")
    for f in fails[:200]:
        print(" ", f)
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
```

- [ ] **Step 5: Build (BUILD-HESIOD), then capture the reference** (all nodes still legacy — this IS the reference):

```bash
mkdir -p /home/barrulus/dev/Hesiod/docs/meta-migration/fixtures
nix develop ~/quixote#cpp-qt-desktop -c bash -c \
  'cd /home/barrulus/dev/Hesiod/Hesiod/data && ../../build/bin/hesiod --parity-dump=../../docs/meta-migration/fixtures/parity-legacy.json'
python3 -c "import json; d=json.load(open('/home/barrulus/dev/Hesiod/docs/meta-migration/fixtures/parity-legacy.json')); print(len(d), 'nodes')"
```

Expected: ~260 nodes. Self-check: `python3 scripts/compat_parity_diff.py fixtures ref against itself` → `0 difference(s)`. If the dump needs a QApplication (it shouldn't — no widgets), run under `QT_QPA_PLATFORM=offscreen`.

- [ ] **Step 6: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/src/cli/batch_mode.cpp Hesiod/src/model/nodes/node_factory.cpp \
        Hesiod/include/hesiod/model/nodes/base_node.hpp Hesiod/src/model/nodes/base_node.cpp \
        scripts/compat_parity_diff.py docs/meta-migration/fixtures/parity-legacy.json
git commit -m "feat(compat): --parity-dump CLI + normalized attribute records + diff script + legacy reference fixture"
```

(Also check `node_factory`'s header for where `dump_node_documentation_stub` is declared and declare the new function next to it.)

---

### Task 8: Canary flip — first facade-backed nodes

**Files:**
- Modify: 2-3 canary node files in `Hesiod/src/model/nodes/nodes_function/` (include swap only)

**Interfaces:** none new. Gate task: proves the whole stack before the bulk swap.

- [ ] **Step 1: Pick canaries** — nodes that do NOT call the shared setup helpers (their TUs stay legacy until Task 9, and a node must not mix backends):

```bash
cd /home/barrulus/dev/Hesiod/Hesiod/src/model/nodes/nodes_function
grep -L -E 'setup_post_process_heightmap_attributes|setup_pre_process_mask_attributes|setup_default_noise|setup_histogram_for_range_attribute|setup_background_image_for_cloud_attribute' *.cpp | head -30
```

From the result pick: one Float/Bool/Enum-rich node, one with `WaveNbAttribute` or `SeedAttribute`, one with `FilenameAttribute` (e.g. an export node). Confirm each compiles standalone by reading its `add_attr` calls against the traits you wrote.

- [ ] **Step 2: Swap** `#include "attributes.hpp"` → `#include "hesiod/model/nodes/compat_attributes.hpp"` in the chosen files (leave everything else in them untouched).

- [ ] **Step 3: Run BUILD-HESIOD** — fix any compile fallout in the traits (canary files are the first real consumers; typical issues: a ctor-overload default you mirrored wrong). Expected `BUILD_EXIT=0`.

- [ ] **Step 4: Parity check on the canaries:**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c \
  'cd /home/barrulus/dev/Hesiod/Hesiod/data && ../../build/bin/hesiod --parity-dump=/tmp/claude-1000/parity-canary.json'
python3 /home/barrulus/dev/Hesiod/scripts/compat_parity_diff.py \
  /home/barrulus/dev/Hesiod/docs/meta-migration/fixtures/parity-legacy.json /tmp/claude-1000/parity-canary.json
```

Expected: `0 difference(s)` (non-canary nodes unchanged; canary nodes must match their legacy records exactly). Fix traits/presets until clean.

- [ ] **Step 5: USER GUI CHECK (pause here).** Ask barrulus to launch and verify per canary: widgets render with correct labels/grouping/order, values edit + recompute, save a graph → reload → values persist, load an OLD example containing a canary node → values restore (watch the log for `loading legacy-format parameters`), toolbar Backup/Revert/Load/Save/Reset behave, reseed buttons work if a canary has a seed. Do not proceed until the user reports.

- [ ] **Step 6: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/src/model/nodes/nodes_function/<canary1>.cpp <canary2> <canary3>
git commit -m "feat(compat): canary nodes on the facade (include swap only)"
```

---

### Task 9: Bulk include-swap + fix tail

**Files:**
- Create: `scripts/compat_include_swap.sh`
- Modify: ~295 files in `nodes_function/` + `post_process.cpp`, `pre_process_mask.cpp`, `default_noise.cpp` (include line only)

- [ ] **Step 1: the codemod script:**

```bash
#!/usr/bin/env bash
# Swap the legacy attributes umbrella include for the Meta compat header.
# brush.cpp is excluded (stays on the legacy library: ArrayAttribute).
set -euo pipefail
cd "$(dirname "$0")/.."

FILES=$(grep -rl '#include "attributes.hpp"' \
  Hesiod/src/model/nodes/nodes_function/ \
  Hesiod/src/model/nodes/post_process.cpp \
  Hesiod/src/model/nodes/pre_process_mask.cpp \
  Hesiod/src/model/nodes/default_noise.cpp \
  | grep -v 'nodes_function/brush.cpp')

for f in $FILES; do
  sed -i 's|#include "attributes.hpp"|#include "hesiod/model/nodes/compat_attributes.hpp"|' "$f"
done
echo "swapped: $(echo "$FILES" | wc -l) files"
```

- [ ] **Step 2: Run it** (`bash scripts/compat_include_swap.sh`). Expected: ~295 files. Then `git diff --stat | tail -3` to confirm only include lines changed (`git diff | grep '^[+-]' | grep -v '^[+-]#include' | grep -v '^[+-][+-]'` should output nothing).

- [ ] **Step 3: Run BUILD-HESIOD.** Expect a compile-error tail: node files using legacy APIs beyond the facade surface (e.g. `attr::` members not aliased, direct attribute-object calls the survey missed, `Stop` usages). Fix each: prefer extending the facade (alias, handle method) over editing node files; edit a node file only when the facade genuinely can't express it, and keep such edits one-line. Track every node-file edit for the commit message. Iterate until `BUILD_EXIT=0`.

- [ ] **Step 4: Full parity diff** (same commands as Task 8 Step 4, output `/tmp/claude-1000/parity-facade.json`). Work the difference list to `0 difference(s)` — every line is a real transcription bug (wrong default, wrong bounds, missing category…). This step is the heart of the migration; budget for iteration.

- [ ] **Step 5: Sanity headless load:**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c \
  'cd /home/barrulus/dev/Hesiod/Hesiod/data && QT_QPA_PLATFORM=offscreen timeout 300 ../../build/bin/hesiod -f examples/GammaCorrection.hsd -b examples/GammaCorrection.hsd --shape=256,256 2>&1 | tail -20'
```

(match `run_batch_mode`'s real flag syntax — read `parse_args` first; the point is one legacy example loads + computes headlessly without errors).

- [ ] **Step 6: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add scripts/compat_include_swap.sh Hesiod/src/model/nodes/nodes_function/ \
        Hesiod/src/model/nodes/post_process.cpp Hesiod/src/model/nodes/pre_process_mask.cpp \
        Hesiod/src/model/nodes/default_noise.cpp
# stage nodes_function by directory ONLY after `git status` confirms nothing unexpected inside it
git commit -m "feat(compat): bulk include swap — all nodes (except Brush) on Meta-backed facade"
```

If facade files needed fixes in Step 3/4, commit those separately first (`fix(compat): ...`).

---

### Task 10: Legacy .hsd corpus check

**Files:**
- Modify: `Hesiod/src/cli/batch_mode.cpp` (new `--compat-check=<dir>` flag)
- Create: `Hesiod/src/cli/compat_check.cpp` (+ decl where batch_mode's helpers are declared)

**Interfaces:**
- Consumes: node json location in .hsd files: `root["graph_manager"]["graph_nodes"][<graph_id>]["nodes"]` = LIST of node objects, each `{"id", "label" (= node type), <attr_key>: {...}, ...}` — verified against `Hesiod/data/examples/GammaCorrection.hsd`. Older files may lack keys (tolerated).
- Produces: exit-0/1 corpus report.

- [ ] **Step 1: `compat_check.cpp`** — no graph construction, no compute; node-level only:

```cpp
// For every .hsd under dir: for every node json, instantiate the node type,
// run json_from (exercising the legacy fallback decoders), then compare the
// Meta container's values against the legacy per-key json; then round-trip
// through _meta and compare again.
int run_compat_check(const std::string &dir)
{
  auto config = std::make_shared<hesiod::GraphConfig>();
  int  files = 0, nodes = 0, failures = 0;

  for (const auto &entry : std::filesystem::recursive_directory_iterator(dir))
  {
    if (entry.path().extension() != ".hsd")
      continue;
    files++;
    nlohmann::json root;
    { std::ifstream f(entry.path()); f >> root; }

    if (!root.contains("graph_manager") || !root["graph_manager"].contains("graph_nodes"))
      continue;

    for (auto &[graph_id, graph] : root["graph_manager"]["graph_nodes"].items())
    {
      if (!graph.contains("nodes"))
        continue;
      for (auto &node_json : graph["nodes"])
      {
        const std::string type = node_json.value("label", "");
        nodes++;
        std::shared_ptr<gnode::Node> p_node;
        try { p_node = hesiod::node_factory(type, config); }
        catch (const std::exception &)
        { hesiod::Logger::log()->warn("compat-check: unknown node type '{}' in {}", type, entry.path().string()); continue; }

        auto *p_base = dynamic_cast<hesiod::BaseNode *>(p_node.get());
        if (!p_base || !p_base->uses_meta())
          continue; // Brush / non-meta

        p_base->json_from(node_json);

        // 1) decoded values vs legacy fields
        nlohmann::json parity = p_base->attribute_parity_record();
        for (auto &[key, rec] : parity.items())
        {
          if (key == "__order" || !node_json.contains(key))
            continue;
          if (!node_json[key].contains("value"))
            continue;
          if (!values_equivalent(rec["value"], node_json[key], rec["type"]))
          {
            hesiod::Logger::log()->error("compat-check: {} {}[{}]: decoded {} vs legacy {}",
                                         entry.path().filename().string(), type, key,
                                         rec["value"].dump(), node_json[key]["value"].dump());
            failures++;
          }
        }

        // 2) _meta round-trip
        nlohmann::json saved = p_base->json_to();
        auto p_node2 = hesiod::node_factory(type, config);
        auto *p_base2 = dynamic_cast<hesiod::BaseNode *>(p_node2.get());
        p_base2->json_from(saved);
        if (p_base2->attribute_parity_record() != parity)
        {
          hesiod::Logger::log()->error("compat-check: {} {}: _meta round-trip mismatch",
                                       entry.path().filename().string(), type);
          failures++;
        }
      }
    }
  }

  hesiod::Logger::log()->info("compat-check: {} files, {} nodes, {} failures", files, nodes, failures);
  return failures ? 1 : 0;
}
```

Write `values_equivalent(decoded, legacy_attr_json, type_string)` alongside: float compare with 1e-5 tolerance; Enum compares decoded int against legacy `"value"` int AND (when `choice` present) trusts the choice-derived int; Seed compares as integers; Range/WaveNb/Vec2Float compare 2-element arrays; Cloud rebuilds vec3 triplets from `x/y/values`; ColorGradient compares stop lists; everything else `==` after `norm`-style float rounding.

- [ ] **Step 2: wire the flag** in `batch_mode.cpp` (same pattern as `--parity-dump`): `--compat-check=<dir>` → `return run_compat_check(args::get(compat_check));`.

- [ ] **Step 3: Build + run over the corpus:**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c \
  'cd /home/barrulus/dev/Hesiod/Hesiod/data && QT_QPA_PLATFORM=offscreen ../../build/bin/hesiod --compat-check=examples 2>&1 | tail -30'
```

Expected: `... 253 files, <N> nodes, 0 failures`. Every failure is a decoder bug — fix in `legacy_compat.hpp` and re-run. Then run once more over `docs/examples/` (255 files) as a second corpus.

- [ ] **Step 4: Commit**

```bash
cd /home/barrulus/dev/Hesiod
git add Hesiod/src/cli/batch_mode.cpp Hesiod/src/cli/compat_check.cpp
git commit -m "feat(compat): --compat-check corpus mode — legacy .hsd decode + _meta round-trip verification (253 examples green)"
```

---

### Task 11: Final review, GUI spot checks, wrap-up

- [ ] **Step 1: Full-branch review.** Dispatch a code-reviewer subagent over every commit since the plan started (both repos), focused on: facade transcription fidelity vs the survey table, const-correctness of the `get_attr_ref` path, decoder exception-safety (a malformed .hsd must never crash load), snapshot/preset json paths, and any node-file semantic edits from Task 9 Step 3. Fix findings; re-run parity + corpus after any fix.

- [ ] **Step 2: Meta pin bump + ledger.** Commit the `external/Meta` pin bump in Hesiod (if not already carried by earlier commits); update `.superpowers/sdd/progress.md` and `docs/meta-migration/` ledger with outcomes.

- [ ] **Step 3: USER GUI SPOT CHECKS (hand the session to barrulus).** Checklist to send:
  1. One node per attribute type: e.g. Gain (float), GammaCorrection (float+post_*), SelectAngle (enum), Noise stays native, GaborWaveFbm (wavenumber), Rift (range is_active), Ridgelines/CloudRandom (cloud/seed), ColorizeGradient (gradient), ExportHeightmap (filename), a curve node (VecFloat).
  2. Quirky set: Receive (dynamic tag combo refreshes when Broadcasts change), Broadcast (tag text read-only), a `post_*`-heavy node's groupbox rendering + histogram on `post_remap` (drag live-updates), Cone (post_remap starts INACTIVE via the new checkbox), Brush (still legacy: paints, saves, loads).
  3. Toolbar on a facade node: Backup → change → Revert; Reset; Save preset → change → Load preset.
  4. Old file: open a pre-migration personal .hsd → values restore, one info log line per node.
  5. Save + reload a new graph (writes `_meta`).
- [ ] **Step 4: After user sign-off** — update memory (`meta-migration-assessment.md`: Phase C status), and STOP. Do not push, do not PR (both need explicit ask).

---

## Self-Review notes

- **Spec coverage:** §4.1 presets → Task 1; §4.2 traits/routing/umbrella/include-swap → Tasks 3, 4, 9; §4.3 handles + helper rewrites → Tasks 3, 5 (ColorGradient's 1 `get_attr_ref` site: handled in Task 9's fix tail — no `handle_of<ColorGradientAttribute>`, add one if that site needs it); §4.4 choke points → Task 9 (same swap); §4.5 toolbar → Task 6; §4.6 misc branches → Task 4; §5 serialization → Tasks 4 (reader), 10 (corpus proof); §6 error handling → Tasks 3 (safe_get warn), 4 (throw on unknown key); §7 rollout order preserved; §8 verification → Tasks 7, 8, 10, 11; §9 out-of-scope respected (Brush excluded everywhere).
- **Deviation from spec, flagged:** §4.5 said Reset restores "the initial snapshot taken at finalize time" via SnapshotManager — implemented instead as the `initial_meta_state_` plain member so the initial state never serializes into `.hsd` files (SnapshotManager contents ride along in `_meta`); Backup/Revert still use SnapshotManager. Same behaviour, less file bloat.
- **Known limitations, accepted:** legacy `RangeAttribute::autorange` becomes provider-recompute semantics (Task 5); `ui.read_only` may be inert until MetaUI supports it (1 site, Broadcast tag — verify in GUI step, escalate to a small MetaUI change if the tag becomes editable); panel *within-category* order fixed via `set_insertion_order`.
- **Placeholder scan:** Task 3's non-Float traits are specified by an exhaustive table (types, overloads, defaults, decode fields) rather than 14 more verbatim blocks — the Float block is the complete pattern and the table rows carry every varying fact, including "read the legacy source first" directives where a default needs verbatim confirmation. Task 5's provider bodies intentionally say "port from saturate.cpp/cloud.cpp" — those working implementations ARE in the repo and are the ground truth to copy.
- **Type consistency:** `legacy_traits<T>::decode(attr, json, key)` 3-arg form used in Task 3 Step 1 and Task 4 Step 2's lambda — consistent; handle names (`RangeHandle`, `set_choice_list`, …) match between Tasks 3, 6; `attribute_parity_record` name matches Tasks 7 & 10; `finalize_attributes`/`initial_meta_state` match Tasks 4, 6.
