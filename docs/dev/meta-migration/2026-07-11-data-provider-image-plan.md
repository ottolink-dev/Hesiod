# `ui.data_provider` image consumer (PointsCanvas background) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render a live heightmap thumbnail behind Meta's `PointsCanvas`, fed by the existing `ui.data_provider` mechanism's `image_*` payload, verified by migrating one Hesiod Cloud node.

**Architecture:** No Meta *core* change — `ProviderData.image_*` and the non-serializable `DataProvider` storage already exist (G1). Add a `set_background_image` + paint to `PointsCanvas`, read the provider in the `PointsEditor` renderer branch, and migrate `cloud.cpp` to Meta with an image data-provider (colorized `background` input).

**Tech Stack:** C++20, Qt6, nlohmann_json. Libraries: `meta`, `meta_qt`. Spec: `docs/meta-migration/2026-07-11-data-provider-image-design.md`.

## Global Constraints

- No Meta core change: `meta::ProviderData` (with `image_width/height/channels/image_pixels`, `has_image()`), `meta::DataProvider`, `meta::keys::ui::data_provider`, and the `json_to` DataProvider skip already exist from G1. Do NOT re-add them.
- Meta edits commit on `external/Meta`'s `feature/meta-migration` (already exists + pushed); Hesiod edits on Hesiod's `feature/meta-migration`. Bump the `external/Meta` pin in Hesiod. Do NOT push and do NOT open PRs unless explicitly asked.
- Consumer scope: `PointsCanvas` background image only. No other widget.
- Image is Qt-neutral: the host provider returns `image_pixels` (row-major, channel-interleaved) already oriented to the canvas (the Hesiod provider applies the vertical flip the legacy helper did); `PointsCanvas` just blits them scaled to its canvas rect.
- Build env (repo precedent): build via `nix develop ~/quixote#cpp-qt-desktop`, cap `-j4`, run detached (unbounded `-j` OOMs). GUI verification is user-driven.
- Staging discipline: each commit stages ONLY its named files; the dirty `external/{GNode,GNodeGUI,HighMap}` submodules and untracked files must never be staged. Never `git add -A`/`.`.

---

## Task 1: `PointsCanvas::set_background_image` + paint

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widgets/points_canvas.hpp`
- Modify: `external/Meta/MetaUI/qt/src/widgets/points_canvas.cpp` (`paintEvent`, ~line 236)

**Interfaces:**
- Produces: `void PointsCanvas::set_background_image(const std::vector<uint8_t> &pixels, int w, int h, int channels);` — stores the image (empty `pixels` clears it); `paintEvent` blits it under the points, scaled to the canvas rect.

- [ ] **Step 1: Declare the setter + members in the header**

In `points_canvas.hpp`: in `public:` add
```cpp
  void set_background_image(const std::vector<uint8_t> &pixels, int w, int h, int channels);
```
in `private:` (near `points_` / the other members) add
```cpp
  std::vector<uint8_t> bg_pixels_;
  int                  bg_w_ = 0, bg_h_ = 0, bg_channels_ = 0;
```
Ensure `#include <vector>` and `#include <cstdint>` are present (add if missing).

- [ ] **Step 2: Implement the setter**

In `points_canvas.cpp` add:
```cpp
void PointsCanvas::set_background_image(const std::vector<uint8_t> &pixels,
                                       int w, int h, int channels)
{
  this->bg_pixels_ = pixels;
  this->bg_w_ = w;
  this->bg_h_ = h;
  this->bg_channels_ = channels;
  this->update();
}
```

- [ ] **Step 3: Blit the background in `paintEvent`**

In `paintEvent` (`~line 236`), immediately AFTER the existing base fill
`p.fillRect(rect(), palette().color(QPalette::Base));` (~line 244) and BEFORE any point/grid
drawing, add:
```cpp
  if (!this->bg_pixels_.empty() && this->bg_w_ > 0 && this->bg_h_ > 0)
  {
    const QImage::Format fmt = (this->bg_channels_ == 4) ? QImage::Format_RGBA8888
                             : (this->bg_channels_ == 1) ? QImage::Format_Grayscale8
                                                         : QImage::Format_RGB888;
    const QImage img(this->bg_pixels_.data(),
                     this->bg_w_,
                     this->bg_h_,
                     this->bg_w_ * this->bg_channels_, // bytes per line
                     fmt);
    p.drawImage(this->canvas_rect(), img);
  }
```
`QPainter p` and `canvas_rect()` already exist in this method. Add `#include <QImage>` to the
file's includes if not already present (`<QPainter>` is at line 11).

- [ ] **Step 4: Build `meta_qt` (controller runs it)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target meta_qt 2>&1 | tail -10'
```
Expected: `meta_qt` compiles + links (`libmeta_qt.a`).

- [ ] **Step 5: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add MetaUI/qt/include/meta_qt/widgets/points_canvas.hpp MetaUI/qt/src/widgets/points_canvas.cpp
git commit -m "feat(qt): PointsCanvas renders an optional background image behind the points"
```

---

## Task 2: Wire the provider into the PointsEditor renderer

**Files:**
- Modify: `external/Meta/MetaUI/qt/include/meta_qt/widget_renderer_inl/std_vector_glm_vec3.inl`

**Interfaces:**
- Consumes: `PointsCanvas::set_background_image` (Task 1); `meta::keys::ui::data_provider`, `meta::DataProvider`, `meta::ProviderData` (G1).
- Produces: a `PointsEditor`/`PathEditor` `std::vector<glm::vec3>` attribute with a `ui.data_provider` gets its `image_*` blitted behind the points on build.

- [ ] **Step 1: Feed the provider after the canvas is built**

In `std_vector_glm_vec3.inl`, after `layout->addWidget(canvas);` (~line 67, where `canvas` is the
`PointsCanvas*` and `attr` is the attribute handle), add:
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
              if (d.has_image())
                canvas->set_background_image(d.image_pixels,
                                             d.image_width,
                                             d.image_height,
                                             d.image_channels);
            }
            catch (...)
            {
              // a faulty host provider must not crash the panel
            }
          }
        }
      }
```
Add includes to the `.inl`: `#include "meta/core/data_provider.hpp"` and
`#include "meta/metadata/keys.hpp"` (if not already present). (Confirm the actual local names
`canvas` and `attr` from the surrounding branch and use them verbatim.)

- [ ] **Step 2: Build `meta_qt` (controller)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target meta_qt 2>&1 | tail -10'
```
Expected: `meta_qt` compiles + links.

- [ ] **Step 3: Commit (Meta branch)**

```bash
cd /home/barrulus/dev/Hesiod/external/Meta
git add MetaUI/qt/include/meta_qt/widget_renderer_inl/std_vector_glm_vec3.inl
git commit -m "feat(qt): feed ui.data_provider image into the PointsEditor canvas background"
```

---

## Task 3: Migrate Hesiod `cloud.cpp` + pin bump + build/GUI

**Files:**
- Modify: `Hesiod/src/model/nodes/nodes_function/cloud.cpp`
- Modify: `external/Meta` gitlink (pin bump), committed on Hesiod's branch

**Interfaces:**
- Consumes: everything above (Meta branch tip).
- Produces: a Cloud node whose cloud attribute is a Meta `PointsEditor` with a working
  `ui.data_provider` image (colorized `background` input); the submodule pin advanced.

- [ ] **Step 1: Rewrite `setup_cloud_node` against Meta**

Replace the `--- Attributes` + `set_attr_ordered_key` + `setup_background_image_for_cloud_attribute`
block with Meta authoring on `node.meta_group().current()`:
```cpp
  auto &c = node.meta_group().current();

  auto *a = c.add<std::vector<glm::vec3>>(A_CLOUD, {});
  a->metadata().try_add(meta::keys::ui::label, std::string("Cloud"));
  a->metadata().try_add(meta::keys::ui::widget_type, std::string("PointsEditor"));
  a->metadata().try_add(meta::keys::ui::category, std::string("Main"));
  a->metadata().try_add(
      meta::keys::ui::data_provider,
      meta::DataProvider{
          [&node, port_id = std::string(P_BACKGROUND)]() -> meta::ProviderData
          {
            meta::ProviderData d;
            hmap::VirtualArray *p_in = node.get_value_ref<hmap::VirtualArray>(port_id);
            if (!p_in)
              return d;
            const glm::ivec2 shape(256, 256);
            hmap::Array array = p_in->to_array(shape, node.cfg().cm_cpu);
            std::vector<uint8_t> img =
                hmap::colorize(array, array.min(), array.max(), hmap::Cmap::MAGMA, false)
                    .to_img_8bit();
            d.image_width = shape.x;
            d.image_height = shape.y;
            d.image_channels = 3; // to_img_8bit() -> RGB
            // vertical flip so the thumbnail origin matches the canvas (legacy mirrored(false,true))
            const int stride = shape.x * 3;
            d.image_pixels.resize(img.size());
            for (int y = 0; y < shape.y; ++y)
              std::copy_n(img.data() + (shape.y - 1 - y) * stride,
                          stride,
                          d.image_pixels.data() + y * stride);
            return d;
          }});
```
Add includes: `#include "highmap/colorize.hpp"`, `#include "highmap/operator.hpp"`,
`#include "highmap/virtual_array/virtual_array.hpp"`, `#include "meta/core/data_provider.hpp"`,
`#include "meta/metadata/keys.hpp"`, and `#include <algorithm>` (for `std::copy_n`). Drop the
`#include "attributes.hpp"` + `using namespace attr;` if nothing else in the file uses `attr::`.

- [ ] **Step 2: Update `compute_cloud_node` to read from Meta**

Replace `const auto cloud_attr = node.get_attr<CloudAttribute>(A_CLOUD);` with:
```cpp
  const auto cloud_attr = node.meta_group().current().value<std::vector<glm::vec3>>(A_CLOUD);
```
The next line `*p_out = hmap::Cloud(cloud_attr);` is unchanged (`hmap::Cloud` accepts a
`std::vector<glm::vec3>`).

- [ ] **Step 3: Bump the Hesiod submodule pin**

```bash
cd /home/barrulus/dev/Hesiod
git -C external/Meta rev-parse --short HEAD   # Meta branch tip (note it)
git add external/Meta
```

- [ ] **Step 4: Build Hesiod (controller, detached)**

```bash
nix develop ~/quixote#cpp-qt-desktop -c bash -c 'cd /home/barrulus/dev/Hesiod && cmake --build build -j4 --target hesiod 2>&1 | tail -20; echo EXIT=${PIPESTATUS[0]}'
```
Expected: `Linking CXX executable ... bin/hesiod`, `EXIT=0`. (Run in background per env notes.)

- [ ] **Step 5: Commit (Hesiod branch)**

```bash
cd /home/barrulus/dev/Hesiod
git add external/Meta Hesiod/src/model/nodes/nodes_function/cloud.cpp
git commit -m "feat(meta): bump Meta pin; verify ui.data_provider image on Cloud PointsEditor"
```

- [ ] **Step 6: GUI verification (user-driven)**

Launch `build/bin/hesiod`. Add a **Noise** node → connect it to the Cloud node's **background**
input. Confirm:
1. The Cloud node's editor shows the **heightmap thumbnail behind the points**, aligned with the
   point coordinates (not upside-down).
2. It **refreshes** when the upstream Noise recomputes.
3. Points are **editable** (add/drag), and edits commit on release (recompute on `edit_ended`).
4. No crash.

---

## Self-Review notes

- **Spec coverage:** §3.1 PointsCanvas (Task 1), §3.2 renderer wiring (Task 2), §5 Hesiod node + §4 orientation flip (Task 3), §8 pin bump (Task 3). §2 (no core change) is honored — no core files touched. §7 testing = `meta_qt` build (Tasks 1-2) + Hesiod build + GUI (Task 3); no new Meta unit test since core is unchanged.
- **Placeholder scan:** none — every step has concrete code/commands.
- **Type consistency:** `set_background_image(pixels, w, h, channels)` (Task 1) is called with exactly those args from Task 2 and fed `ProviderData.image_pixels/width/height/channels` from Task 3. `A_CLOUD` and `P_BACKGROUND` are the existing `constexpr` names in `cloud.cpp`.
- **External-API confirmations flagged:** the `canvas`/`attr` local names (Task 2), the `paintEvent` insertion point and `QImage` includes (Task 1), and `to_img_8bit()` returning RGB (Task 3) are each directed to the actual source to confirm, not invented.
