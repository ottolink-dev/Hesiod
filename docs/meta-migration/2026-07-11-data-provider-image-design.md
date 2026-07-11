# Meta capability: `ui.data_provider` image consumer (PointsCanvas background) — Design

**Date:** 2026-07-11
**Repo:** `otto-link/Meta` (+ minimal `otto-link/Hesiod` consumer for verification)
**Status:** Design approved; ready for implementation plan.
**Context:** Fast-follow to the G1 histogram work (`docs/meta-migration/2026-07-10-data-provider-*`).
Closes gap **G2** (cloud/points editor with a heightmap background) by reusing the existing
`ui.data_provider` mechanism for its *image* payload, proving "one hook serves both series and
image".

## 1. Goal

Render a live heightmap thumbnail behind Meta's `PointsCanvas` point-cloud editor, fed by the
same `ui.data_provider` callback built in G1 — using the `image_*` fields of `meta::ProviderData`
that already exist. Verify end-to-end by migrating one Hesiod Cloud node.

**In scope:** the `PointsCanvas` background-image consumer + the `std_vector_glm_vec3.inl`
renderer wiring; one Hesiod verification node (`cloud.cpp`).

**Explicitly NOT in scope (already done or deferred):** the `ui.data_provider` mechanism,
`ProviderData` struct, non-serializable storage, and the `json_to` skip (all built in G1); the
broad `_meta` values-only trim; the widget-width fix; refresh approach #1 (sync-not-rebuild).

## 2. Why there is no Meta *core* change

G1 already delivered, in `meta/core/data_provider.hpp`:
- `ProviderData` with `image_width`, `image_height`, `image_channels`, `image_pixels` (row-major,
  channel-interleaved) and `has_image()`.
- `using DataProvider = std::function<ProviderData()>` stored under `meta::keys::ui::data_provider`
  as a non-serializable metadata attribute, with the `AttributeContainer::json_to` skip.

So G2 needs no new type, key, trait, or serialization change — only a new *consumer* widget and
its renderer wiring, plus a Hesiod node to exercise it.

## 3. Meta (`meta_qt`) — PointsCanvas background

### 3.1 `PointsCanvas` (`widgets/points_canvas.{hpp,cpp}`)
- Add `void set_background_image(const std::vector<uint8_t> &pixels, int w, int h, int channels);`
  storing the pixels + dims (empty `pixels` clears it).
- In `paintEvent`, draw the background FIRST (under the points): build a `QImage` from the stored
  pixels (`QImage::Format_RGB888` for 3 channels, `Format_RGBA8888` for 4, `Format_Grayscale8`
  for 1), scale it to the widget rect, and `drawImage`. The existing point/grid drawing follows
  on top. No-op when no image is set (identical to today's look).

### 3.2 `std_vector_glm_vec3.inl` (the `PointsEditor` branch)
After the `PointsCanvas` is constructed, read the provider (mirroring the RangeBar wiring in
`glm_vec2.inl`):
```cpp
if (const auto *mp = attr.metadata().find(meta::keys::ui::data_provider))
  if (const auto *dp = mp->try_cast<meta::Attribute<meta::DataProvider>>())
  {
    const meta::DataProvider &provider = dp->value();
    if (provider)
      try
      {
        meta::ProviderData d = provider();
        if (d.has_image())
          canvas->set_background_image(d.image_pixels, d.image_width,
                                       d.image_height, d.image_channels);
      }
      catch (...) { /* a faulty host provider must not crash the panel */ }
  }
```
Add `#include "meta/core/data_provider.hpp"`. (Confirm the local canvas variable name and the
`attr` handle from the surrounding `PointsEditor` branch at implementation time.)

## 4. Orientation & scaling

The legacy `setup_background_image_for_cloud_attribute.cpp` vertically mirrors the thumbnail
(`QImage(...).mirrored(false, true)`) so the image origin matches the point-canvas coordinate
system (points use a bottom-left-ish origin; QImage is top-left). To keep the thumbnail aligned
with point positions, the **Hesiod provider returns pixels already oriented to the canvas** (i.e.
it applies the same vertical flip the legacy helper did), and the renderer draws them as-is
scaled to the rect. Keeping the flip host-side (not in `PointsCanvas`) preserves Meta's
neutrality — the canvas just blits whatever pixels it's given.

## 5. Hesiod verification node — `cloud.cpp`

`cloud.cpp` is the simplest fit: ports IN `background` (`hmap::VirtualArray`), OUT `cloud`
(`hmap::Cloud`); one `CloudAttribute` `A_CLOUD`; and it already calls
`setup_background_image_for_cloud_attribute(node, A_CLOUD, P_BACKGROUND)`.

Migrate it fully onto Meta (same pattern as `saturate.cpp`):
- `A_CLOUD` → `c.add<std::vector<glm::vec3>>("cloud", {})` (this vector type is registered in
  Meta) + metadata `ui::label`="Cloud", `ui::widget_type`="PointsEditor", `ui::category`="Main".
- Attach a `ui::data_provider` lambda returning `ProviderData` with `image_*` = the colorized
  `background` input (256×256, `hmap::Cmap::MAGMA`, RGB via `to_img_8bit()`), vertically flipped
  to match the canvas — the direct analogue of `setup_background_image_for_cloud_attribute`.
- `compute_cloud_node` reads `c.value<std::vector<glm::vec3>>("cloud")` and builds the
  `hmap::Cloud` output as before.
- Keep `#include "attributes.hpp"`/`using namespace attr;` only if still needed; add
  `meta/core/data_provider.hpp` and `meta/metadata/keys.hpp`.

## 6. Data flow, refresh, error handling

Identical to G1: the provider is called at render; the background refreshes on panel rebuild
(recompute); point edits commit on `edit_ended` (the refresh fix already merged). A faulty
provider is caught at the renderer and yields a plain (no-background) canvas. Absent provider →
unchanged behavior.

## 7. Testing

- **Meta core:** unchanged — no new core code (the G1 `ProviderData`/skip tests still cover it).
  Optionally extend the `has_image()` assertion (already present) — no new test required.
- **MetaUI:** build `meta_qt` (the `PointsCanvas` change + renderer wiring compile). The canvas
  paint is GUI-verified (no headless unit test for Qt widgets).
- **Hesiod end-to-end (controller build + user GUI):** the migrated Cloud node shows the
  heightmap thumbnail behind the point editor, points are editable, the thumbnail refreshes when
  the `background` input recomputes, and there is no crash.

## 8. Branch & pin

Continue on the existing branches: Meta commits on `external/Meta`'s `feature/meta-migration`
(already pushed to otto-link/Meta), Hesiod commits on its `feature/meta-migration` (pushed to
otto-link/Hesiod), bumping the `external/Meta` pin. No PR without explicit request.

## 9. Components summary (isolation)

| Unit | Responsibility | Depends on |
|---|---|---|
| `PointsCanvas::set_background_image` + paint | blit host image under the points | Qt |
| `std_vector_glm_vec3.inl` PointsEditor wiring | read provider → feed the canvas image | PointsCanvas, `data_provider.hpp` |
| Hesiod `cloud.cpp` provider | colorize `background` input → oriented `ProviderData.image_*` | HighMap colorize, all above |
