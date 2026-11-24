# `ui.data_provider` (RangeBar histogram) + `ui.tooltip` — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a general Qt-free `ui.data_provider` host-callback mechanism to Meta and use it to render a live histogram behind the `RangeBar` (gap G1), plus a `ui.tooltip` key (G6), verified end-to-end via one Hesiod node.

**Architecture:** The callable is stored as a non-serializable metadata `Attribute<meta::DataProvider>` under `ui.data_provider`; it returns a neutral `meta::ProviderData`. Meta's `RangeBar` gains a `set_histogram` + paint; the `glm::vec2` RangeBar renderer reads the provider and feeds it. `ui.tooltip` is applied centrally in the container-row builder. All Meta edits land on a dedicated Meta branch, pinned into Hesiod.

**Tech Stack:** C++20, CMake ≥3.22, Qt6, nlohmann_json. Libraries: `meta` (core), `meta_qt` (Qt UI). Design: `docs/meta-migration/2026-07-10-data-provider-tooltip-design.md`.

## Global Constraints

- Meta edits are committed on `external/Meta`'s `feature/meta-migration` branch (created off the frozen pin `e71e798`) and kept **local/unpushed** for now (do NOT push to `otto-link/Meta` yet). Meta `main`/`dev` are never touched. Hesiod edits stay on Hesiod's `feature/meta-migration`. No PR/merge to any `dev`/`main` without explicit request. No CI.
- Storage = non-serializable metadata attribute under key `ui.data_provider` (Approach A). Return type = the neutral `meta::ProviderData` struct; one general `DataProvider = std::function<ProviderData()>`.
- The `DataProvider` metadata entry MUST be omitted from serialization (`json_to`), and `json_from` must never try to reconstruct it. Targeted skip for the `DataProvider` type only — NOT the broad `_meta` values-only trim (deferred).
- Tooltips are HTML, applied centrally (one place), not per-renderer.
- Consumer scope: `RangeBar` histogram only. `PointsCanvas` background image is a separate fast-follow (out of scope here).
- Build env (per repo precedent): build via `nix develop ~/quixote#cpp-qt-desktop`, cap `-j4`, run detached (unbounded `-j` OOMs). GUI verification is user-driven. Qt is 6.11.1; if `build/` goes stale, `rm -rf build` + fresh configure.
- Staging discipline: each commit stages ONLY its named files; the dirty `external/{GNode,GNodeGUI,HighMap}` submodules and untracked files must never be staged. Never `git add -A`/`.`.

---

## Task 1: Meta branch + core data contract (`ProviderData`, `DataProvider`, traits, keys)

**Files:**
- `external/Meta` — create branch `feature/meta-migration` (git op)
- Create: `external/Meta/Meta/include/meta/core/data_provider.hpp`
- Modify: `external/Meta/Meta/include/meta/metadata/keys.hpp` (add two keys to `namespace meta::keys::ui`)
- Modify: `external/Meta/tests/test_meta/main.cpp` (append assertions)

**Interfaces produced:**
- `struct meta::ProviderData { std::vector<float> series_x, series_y; int image_width, image_height, image_channels; std::vector<uint8_t> image_pixels; bool has_series() const; bool has_image() const; };`
- `using meta::DataProvider = std::function<meta::ProviderData()>;`
- `AttributeTraits<meta::DataProvider>` (no-op) + `TypeName<meta::DataProvider>` so `Attribute<meta::DataProvider>` compiles.
- `meta::keys::ui::data_provider` = `"ui.data_provider"`, `meta::keys::ui::tooltip` = `"ui.tooltip"`.

- [ ] **Step 1: Create the Meta feature branch off the frozen pin**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git switch -c feature/meta-migration   # created off current HEAD = pinned e71e798
# NOTE: keep this branch LOCAL — do NOT push to otto-link/Meta yet (per user).
git branch --show-current              # expect: feature/meta-migration
```

- [ ] **Step 2: Create `data_provider.hpp` with the struct, alias, and no-op registration**

Create `external/Meta/Meta/include/meta/core/data_provider.hpp`:

```cpp
/* Copyright (c) 2026 Otto Link. Distributed under the terms of the GNU General
   Public License. The full license is in the file LICENSE, distributed with
   this software. */
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "meta/type/attribute_traits.hpp"
#include "meta/type/type_name.hpp"

namespace meta
{

/// Qt-free payload a host supplies to a widget for runtime-computed display data.
struct ProviderData
{
  std::vector<float>   series_x;             ///< e.g. histogram bin centers
  std::vector<float>   series_y;             ///< e.g. histogram counts/heights
  int                  image_width    = 0;
  int                  image_height   = 0;
  int                  image_channels = 0;   ///< 1, 3, or 4
  std::vector<uint8_t> image_pixels;         ///< row-major, channel-interleaved

  bool has_series() const { return !series_y.empty(); }
  bool has_image() const
  {
    return image_width > 0 && image_height > 0 && !image_pixels.empty();
  }
};

/// Host-supplied callback returning fresh display data on each call. Non-serializable.
using DataProvider = std::function<ProviderData()>;

/// No-op traits: a DataProvider carries runtime state that must not be serialized.
template <> struct AttributeTraits<DataProvider>
{
  static std::string   to_string(const DataProvider &) { return "<data_provider>"; }
  static nlohmann::json json_to(const DataProvider &) { return nullptr; }
  static DataProvider   json_from(const nlohmann::json &) { return {}; }
};

} // namespace meta

META_DEFINE_TYPE_NAME(meta::DataProvider);
```

(If `META_DEFINE_TYPE_NAME` with a qualified `meta::DataProvider` argument does not compile as
written — the macro opens `namespace meta` — invoke it with `DataProvider` at the point the
macro expects; confirm the exact form against `tests/test_meta/main.cpp:40`
(`META_DEFINE_TYPE_NAME(Vec2);`) and `meta/type/type_name.hpp`.)

- [ ] **Step 3: Add the two metadata keys**

In `external/Meta/Meta/include/meta/metadata/keys.hpp`, inside `namespace meta::keys::ui`, add:
```cpp
inline constexpr char data_provider[] = "ui.data_provider";
inline constexpr char tooltip[]       = "ui.tooltip";
```

- [ ] **Step 4: Write assertions in the Meta test main**

In `external/Meta/tests/test_meta/main.cpp`, add `#include "meta/core/data_provider.hpp"` and, inside `main()`, append:
```cpp
{
  // ProviderData predicates
  meta::ProviderData d;
  assert(!d.has_series() && !d.has_image());
  d.series_y = {1.f, 2.f};
  assert(d.has_series());

  // Attribute<DataProvider> compiles, to_string is the no-op sentinel
  meta::AttributeContainer c;
  bool called = false;
  c.add<meta::DataProvider>("p", meta::DataProvider{[&]{ called = true; return meta::ProviderData{}; }});
  auto *a = c.find("p");
  assert(a && a->to_string() == "<data_provider>");

  // the stored provider is callable
  auto *typed = a->try_cast<meta::Attribute<meta::DataProvider>>();
  assert(typed && typed->value());
  typed->value()();
  assert(called);

  std::cout << "[data_provider] core OK" << std::endl;
}
```
(Confirm `AttributeContainer::add<T>`, `find`, and `AbstractAttribute::try_cast` signatures
against `meta/core/attribute_container.hpp` / `abstract_attribute.hpp`; they matched the Hesiod
PoC usage.)

- [ ] **Step 5: Build & run the Meta test standalone (controller)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c '
  cd /home/barrulus/dev/Hesiod/external/Meta
  cmake -B build-test -DMETA_ENABLE_TESTS=ON -DMETA_ENABLE_QT_UI=OFF >/dev/null &&
  cmake --build build-test -j4 --target test_meta 2>&1 | tail -15 &&
  ./build-test/tests/test_meta/test_meta'
```
Expected: builds; prints `[data_provider] core OK`; exits 0. (Confirm the test binary path/target
name from `tests/test_meta/CMakeLists.txt`.)

- [ ] **Step 6: Commit (on the Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add Meta/include/meta/core/data_provider.hpp Meta/include/meta/metadata/keys.hpp tests/test_meta/main.cpp
git commit -m "feat: add ui.data_provider mechanism (ProviderData + non-serializable DataProvider) and ui.tooltip key"
```

---

## Task 2: Meta serialization skip for `DataProvider`

**Files:**
- Modify: `external/Meta/Meta/src/attribute_container.cpp` (`AttributeContainer::json_to`, ~line 185)
- Modify: `external/Meta/tests/test_meta/main.cpp` (append assertion)

**Interfaces:**
- Consumes: `meta::DataProvider` (Task 1).
- Produces: `AttributeContainer::json_to()` omits any attribute whose stored type is `meta::DataProvider`; `json_from` (which only updates existing attrs / factory-creates by type name) never reconstructs one because it is absent from the JSON.

- [ ] **Step 1: Add the failing assertion**

In `external/Meta/tests/test_meta/main.cpp` `main()`, append:
```cpp
{
  meta::AttributeContainer c;
  c.add<int>("keep", 7);
  c.add<meta::DataProvider>("skip", meta::DataProvider{[]{ return meta::ProviderData{}; }});
  auto j = c.json_to();
  assert(j.contains("keep"));
  assert(!j.contains("skip"));   // DataProvider omitted from serialization
  std::cout << "[data_provider] serialize-skip OK" << std::endl;
}
```

- [ ] **Step 2: Run to confirm it fails**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod/external/Meta && cmake --build build-test -j4 --target test_meta >/dev/null 2>&1 && ./build-test/tests/test_meta/test_meta'
```
Expected: FAIL — assertion `!j.contains("skip")` fires (the entry is currently serialized as null).

- [ ] **Step 3: Skip `DataProvider`-typed entries in `json_to`**

In `attribute_container.cpp`, the loop at ~line 190-192 is `for (...) j[name] = attr->json_to();`. Guard it:
```cpp
#include "meta/core/data_provider.hpp"   // add to includes
// ...
for (const auto &name : insertion_order_)
{
  const auto *attr = /* existing lookup */;
  if (attr->type() == std::type_index(typeid(meta::DataProvider)))
    continue;                             // non-serializable runtime provider
  j[name] = attr->json_to();
}
```
(Match the exact existing loop form/variable names in the file. `AbstractAttribute::type()`
returns `std::type_index` — verified in `abstract_attribute.hpp`.)

- [ ] **Step 4: Run to confirm it passes**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod/external/Meta && cmake --build build-test -j4 --target test_meta >/dev/null 2>&1 && ./build-test/tests/test_meta/test_meta'
```
Expected: PASS — both `[data_provider] core OK` and `[data_provider] serialize-skip OK`; exit 0.

- [ ] **Step 5: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add Meta/src/attribute_container.cpp tests/test_meta/main.cpp
git commit -m "feat: omit non-serializable DataProvider metadata entries from json_to"
```

---

## Task 3: `RangeBar` histogram (`set_histogram` + paint)

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widgets/range_bar.hpp`
- Modify: `external/Meta/MetaUI/qt/src/widgets/range_bar.cpp` (`paintEvent`)

**Interfaces:**
- Produces: `void RangeBar::set_histogram(const std::vector<float> &x, const std::vector<float> &y);` — stores the series (empty clears); `paintEvent` draws it behind the handles.

- [ ] **Step 1: Declare the setter + members in the header**

In `range_bar.hpp`: in `public:` add
```cpp
  void set_histogram(const std::vector<float> &x, const std::vector<float> &y);
```
in `private:` (near the other members, ~line 56) add
```cpp
  std::vector<float> hist_x_;
  std::vector<float> hist_y_;
```
Ensure `#include <vector>` is present.

- [ ] **Step 2: Implement the setter + histogram draw**

In `range_bar.cpp`, add:
```cpp
void RangeBar::set_histogram(const std::vector<float> &x, const std::vector<float> &y)
{
  this->hist_x_ = x;
  this->hist_y_ = y;
  this->update();
}
```
In `paintEvent`, at the very start of painting (before the existing track/handles drawing), add a bars pass that runs only when data is present:
```cpp
if (!this->hist_y_.empty())
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  const QRect r = this->rect();
  const float ymax = *std::max_element(this->hist_y_.begin(), this->hist_y_.end());
  if (ymax > 0.f)
  {
    const int   n  = static_cast<int>(this->hist_y_.size());
    const float bw = static_cast<float>(r.width()) / static_cast<float>(n);
    QColor c = this->palette().color(QPalette::Mid);
    c.setAlpha(90);
    painter.setPen(Qt::NoPen);
    painter.setBrush(c);
    for (int i = 0; i < n; ++i)
    {
      const float h = (this->hist_y_[i] / ymax) * r.height();
      painter.drawRect(QRectF(r.left() + i * bw, r.bottom() - h, bw, h));
    }
  }
}
```
Add `#include <algorithm>` and ensure `<QPainter>`/`<QPalette>` are included (they are, since paintEvent already paints). The existing handle/track drawing follows and paints on top.

- [ ] **Step 3: Build `meta_qt` (controller)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target meta_qt 2>&1 | tail -12'
```
Expected: `meta_qt` compiles + links (`libmeta_qt.a`). (This uses Hesiod's `build/` which has `META_ENABLE_QT_UI=ON`.)

- [ ] **Step 4: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add MetaUI/qt/include/meta_qt/widgets/range_bar.hpp MetaUI/qt/src/widgets/range_bar.cpp
git commit -m "feat(qt): RangeBar renders an optional histogram behind the handles"
```

---

## Task 4: Wire the provider into the RangeBar renderer + apply `ui.tooltip`

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widget_renderer_inl/glm_vec2.inl` (RangeBar branch, ~line 225)
- Modify: `external/Meta/MetaUI/qt/src/container_widget/container_widget.cpp` (per-attribute row builder)

**Interfaces:**
- Consumes: `RangeBar::set_histogram` (Task 3); `meta::keys::ui::data_provider`/`::tooltip`, `meta::DataProvider` (Task 1).
- Produces: a RangeBar-rendered `glm::vec2` attribute with a `ui.data_provider` gets its histogram fed on build; any attribute with `ui.tooltip` gets a hover tooltip.

- [ ] **Step 1: Feed the provider in the RangeBar branch**

In `glm_vec2.inl`, inside the `widget_type == "RangeBar"` branch, after the `RangeBar *bar = new RangeBar(...)` construction, add:
```cpp
if (const auto *mp = attr.metadata().find(meta::keys::ui::data_provider))
{
  if (const auto *dp = mp->try_cast<meta::Attribute<meta::DataProvider>>())
  {
    const meta::DataProvider &provider = dp->value();
    if (provider)
    {
      try
      {
        meta::ProviderData d = provider();
        if (d.has_series())
          bar->set_histogram(d.series_x, d.series_y);
      }
      catch (...)
      {
        // a faulty host provider must not crash the panel
      }
    }
  }
}
```
Add `#include "meta/core/data_provider.hpp"` to the `.inl`'s includes. (Confirm the local variable
name for the RangeBar and the `attr` handle from the surrounding branch; use them verbatim.)

- [ ] **Step 2: Apply `ui.tooltip` centrally in the row builder**

In `container_widget.cpp`, locate where each attribute's row (label + editor widget) is created.
After the row widget exists, add:
```cpp
if (const std::string tip = meta::common::try_get<std::string>(*attr, meta::keys::ui::tooltip, "");
    !tip.empty())
  row_widget->setToolTip(QString::fromStdString(tip));
```
(Use the actual row/label widget variable and the actual attribute handle from the surrounding
code; `meta::common::try_get<std::string>` is the existing metadata accessor in
`meta_common.hpp`. Ensure `<QString>` is available.)

- [ ] **Step 3: Build `meta_qt` (controller)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target meta_qt 2>&1 | tail -12'
```
Expected: `meta_qt` compiles + links.

- [ ] **Step 4: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add MetaUI/qt/include/meta_qt/widget_renderer_inl/glm_vec2.inl MetaUI/qt/src/container_widget/container_widget.cpp
git commit -m "feat(qt): feed ui.data_provider histogram into RangeBar; apply ui.tooltip centrally"
```

---

## Task 5: Bump Hesiod pin + Hesiod verification node + build/GUI

**Files:**
- Modify: `external/Meta` gitlink (submodule pin) — committed on Hesiod's branch
- Modify: one Hesiod node file under `Hesiod/src/model/nodes/nodes_function/` (chosen in Step 1)

**Interfaces:**
- Consumes: everything above (the Meta branch tip).
- Produces: a Hesiod node whose Range attribute is authored on Meta with a working `ui.data_provider` histogram lambda; the submodule pin advanced to the Meta branch tip.

- [ ] **Step 1: Choose the verification node**

Pick the SIMPLEST node that uses a `RangeAttribute` with a histogram, to fully migrate its
attributes to Meta (like the Noise PoC). Run:
```bash
cd /home/barrulus/dev/Hesiod
grep -rl "setup_histogram_for_range_attribute\|RangeAttribute" Hesiod/src/model/nodes/nodes_function/ | head
```
Prefer a node with few attributes (e.g. a remap/clamp-style node). Record the choice. All of its
attributes must move to the meta container (the panel branches whole-node on `uses_meta()`).

- [ ] **Step 2: Author the node's attributes on Meta, with the range `ui.data_provider`**

Migrate the chosen node's `setup_*` to the meta container (same pattern as `noise.cpp`: `add<T>`
+ `ui.*`/`constraints.*` metadata). For its Range attribute (a `glm::vec2` with
`widget_type = "RangeBar"`), attach the provider — the analogue of
`setup_histogram_for_range_slider.cpp`:
```cpp
auto *range = c.add<glm::vec2>(A_REMAP, glm::vec2(0.f, 1.f));
range->metadata().try_add(meta::keys::ui::widget_type, std::string("RangeBar"));
range->metadata().try_add(meta::keys::constraints::min, 0.f);
range->metadata().try_add(meta::keys::constraints::max, 1.f);
range->metadata().try_add(meta::keys::ui::tooltip, std::string("<b>Remap range</b>"));
range->metadata().try_add(
    meta::keys::ui::data_provider,
    meta::DataProvider{
        [&node, port_id = std::string(P_IN)]() -> meta::ProviderData
        {
          meta::ProviderData d;
          hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(port_id);
          if (!p_in) return d;
          float vmin = p_in->min(node.cfg().cm_cpu);
          float vmax = p_in->max(node.cfg().cm_cpu);
          if (vmin == vmax) return d;
          const int   nbins = 256;
          hmap::Array a = p_in->to_array({256, 256}, node.cfg().cm_cpu);
          d.series_x = hmap::linspace(vmin, vmax, nbins, false);
          d.series_y.assign(nbins, 0.f);
          const float sa = 1.f / (vmax - vmin) * (nbins - 1);
          const float sb = -vmin / (vmax - vmin) * (nbins - 1);
          for (int j = 0; j < a.shape.y; ++j)
            for (int i = 0; i < a.shape.x; ++i)
              d.series_y[static_cast<int>(sa * a(i, j) + sb)] += 1.f;
          return d;
        }});
```
Update the node's `compute_*` to read its params from the meta container (`c.value<T>(...)`),
as in `noise.cpp`. Include `meta/core/data_provider.hpp` and `meta/metadata/keys.hpp`.

- [ ] **Step 3: Bump the Hesiod submodule pin to the Meta branch tip**

```bash
cd /home/barrulus/dev/Hesiod
git -C external/Meta rev-parse --short HEAD    # Meta branch tip (note it)
git add external/Meta                          # stage the advanced gitlink
```

- [ ] **Step 4: Build Hesiod (controller, detached)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target hesiod 2>&1 | tail -20; echo EXIT=${PIPESTATUS[0]}'
```
Expected: `Linking CXX executable ... bin/hesiod`, `EXIT=0`. (Run in background per env notes.)

- [ ] **Step 5: Commit (Hesiod branch)**

```bash
cd /home/barrulus/dev/Hesiod
git add external/Meta Hesiod/src/model/nodes/nodes_function/<chosen_node>.cpp
git commit -m "feat(meta): bump Meta pin; verify ui.data_provider histogram on <chosen_node> RangeBar"
```

- [ ] **Step 6: GUI verification (user-driven)**

Launch `build/bin/hesiod`. Add the chosen node, connect a generator (e.g. Noise) to its input.
Confirm:
1. A **histogram renders behind the RangeBar handles** and reflects the input data.
2. It **updates** when the upstream node recomputes (change the noise → histogram changes).
3. The range attribute's **hover tooltip** shows the HTML text.
4. No crash on panel open / range edit.

---

## Self-Review notes

- **Spec coverage:** §4 core (Task 1 data_provider.hpp/traits/keys; Task 2 serialize skip), §5 MetaUI (Task 3 RangeBar; Task 4 renderer wiring + tooltip), §3 branch/pin (Task 1 branch, Task 5 pin), §6 verification (Task 5), §7 testing (Meta unit asserts Tasks 1-2; build + GUI Tasks 3-5). PointsCanvas image and the broad `_meta` trim are correctly excluded (deferred).
- **Serialization:** the skip is a targeted `typeid(DataProvider)` check in `json_to` (Task 2), matching the spec's "targeted skip, not the broad trim."
- **Type consistency:** `set_histogram(x, y)` (Task 3) is fed exactly as declared from Task 4; `ProviderData.series_x/series_y` names consistent Task 1 → Task 5; `keys::ui::data_provider`/`tooltip` consistent throughout.
- **External-API confirmations flagged:** the `META_DEFINE_TYPE_NAME` form (Task 1), the `json_to` loop variables (Task 2), the RangeBar/`attr` handles in `glm_vec2.inl` and the row-widget in `container_widget.cpp` (Task 4), and the test-binary path (Task 1) are each directed to the specific source to confirm, not invented.
