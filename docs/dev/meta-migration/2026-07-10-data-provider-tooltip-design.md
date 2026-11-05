# Meta capability: `ui.data_provider` (RangeBar histogram) + `ui.tooltip` — Design

**Date:** 2026-07-10
**Repo:** `otto-link/Meta` (+ minimal `otto-link/Hesiod` consumer for verification)
**Status:** Design approved; ready for implementation plan.
**Context:** First slice of the Meta-editing phase after the Phase A+B PoC
(`docs/meta-migration/2026-07-09-*`). Closes gap **G1** (range slider with live input
histogram) from the gaps doc / `otto-link/Meta#15`; lays the mechanism that also closes
**G2** (cloud/points background image) in a fast follow. Adds **G6** (`ui.tooltip`).

## 1. Goal

Give Meta a general, Qt-free mechanism for a host to feed **runtime-computed data** into a
widget that cannot produce it itself — a `ui.data_provider` metadata callback — and use it to
render a **histogram behind the `RangeBar`** handles (Hesiod's signature range-editing UX).
Add a `ui.tooltip` metadata key applied centrally. Verify end-to-end via one Hesiod node.

**In scope:** the `data_provider` mechanism + the **RangeBar histogram** consumer; the
`ui.tooltip` key; a targeted serialization skip for the non-serializable provider; one
Hesiod-side consumer node for verification.

**Explicitly out of scope (deferred):** the `PointsCanvas` background-image consumer (G2 —
fast follow, same mechanism); the broad `_meta` values-only serialization trim; the widget
sizing fix; migrating the fleet of Range-using nodes.

## 2. Decisions (locked in brainstorming)

- **Storage = metadata attribute (Approach A).** The callable lives in the attribute's
  metadata container under the `ui.data_provider` key, matching Otto's #15 framing ("an
  attribute that cannot be serialized"). Reuses the existing `meta::common::try_get` plumbing.
- **Return type = one neutral struct** (`meta::ProviderData`), Qt-free; each backend converts.
  One general `ui.data_provider`, not typed per-widget.
- **Refresh = compose with existing panel rebuild.** No new refresh signal; Hesiod rebuilds
  the settings panel on `update_finished`, which re-invokes the renderer → re-calls the provider.
- **Consumer scope = RangeBar histogram first**, PointsCanvas image as a later fast follow.
- **Tooltip = HTML**, applied centrally (per Otto #15).

## 3. Branch & cross-repo setup

This is the first work that edits Meta, so the deferred Meta branch is created now:
- Inside `external/Meta`, create `feature/meta-migration` off the current frozen pin
  (`e71e798`), commit all Meta changes there, push to `otto-link/Meta`.
- Bump Hesiod's `external/Meta` submodule pin to the new Meta commit; commit the pin on
  Hesiod's `feature/meta-migration` branch. Meta `main`/`dev` are never touched.
- Standing rules hold: no PR/merge to any `dev`/`main` without explicit request; no CI.

## 4. Meta core (`meta` library)

### 4.1 `meta/core/data_provider.hpp` (new)

```cpp
#pragma once
#include <functional>
#include <vector>
#include <cstdint>

namespace meta
{
/// Qt-free payload a host supplies to a widget for runtime-computed display data.
/// A provider populates the series fields (e.g. a histogram) and/or the image fields
/// (e.g. a heightmap thumbnail); each UI backend converts to its native types.
struct ProviderData
{
  std::vector<float> series_x;          ///< e.g. histogram bin centers
  std::vector<float> series_y;          ///< e.g. histogram counts/heights

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

/// A host-supplied callback returning fresh display data on each call.
using DataProvider = std::function<ProviderData()>;
} // namespace meta
```

### 4.2 Type registration (so `Attribute<DataProvider>` compiles, serializes to null)

`DataProvider` is non-serializable. Provide no-op specializations:
- `TypeName<meta::DataProvider>::name = "data_provider"`.
- `AttributeTraits<meta::DataProvider>`: `to_string` → `"<data_provider>"`; `json_to` →
  `nullptr` (JSON null); `json_from` → no-op (leave the value untouched).

These let `metadata().try_add(keys::ui::data_provider, DataProvider{...})` create an
`Attribute<DataProvider>` through the existing plumbing without a compile error.

### 4.3 Metadata keys (`meta/metadata/keys.hpp`)

Add to `namespace meta::keys::ui`:
```cpp
inline constexpr char data_provider[] = "ui.data_provider";
inline constexpr char tooltip[]       = "ui.tooltip";
```

### 4.4 Serialization skip (targeted)

The provider lives in the serializable metadata container, so `serialize_metadata` /
`AttributeContainer::json_to` must **omit** the `DataProvider` entry (its `json_to` yields
null and the factory cannot reconstruct it on load). Skip metadata attributes whose stored
type is `meta::DataProvider` (or, equivalently, whose serialized value is JSON null from the
no-op trait). This is a *targeted* skip for the provider type only — the broad values-only
`_meta` trim remains a separate deferred task. `json_from` correspondingly does not attempt to
create it; the host re-sets the provider in node setup on every construction.

## 5. MetaUI Qt (`meta_qt` library)

### 5.1 `RangeBar` — histogram behind the handles

- Add `void set_histogram(const std::vector<float> &x, const std::vector<float> &y);`
  storing the series (empty clears it).
- In `paintEvent`, before drawing the handles, draw the series as bars/an area normalized to
  the widget rect (y scaled to max). No-op when the series is empty (identical to today's look).

### 5.2 `glm_vec2.inl` — wire the provider in the `RangeBar` branch

In the `widget_type == "RangeBar"` branch, after constructing the `RangeBar`:
```cpp
if (auto *mp = attr.metadata().find(meta::keys::ui::data_provider))
{
  if (auto *dp = mp->try_cast<meta::Attribute<meta::DataProvider>>())
  {
    const meta::DataProvider &provider = dp->value();
    if (provider)
    {
      meta::ProviderData d = provider();
      if (d.has_series())
        bar->set_histogram(d.series_x, d.series_y);
    }
  }
}
```
(Exact accessor confirmed against `attribute_container.hpp`/`abstract_attribute.hpp` at
implementation time; `try_get` is for value types, so a `find` + `try_cast` is used for the
function-typed metadata entry.)

### 5.3 `ui.tooltip` — central application

Where `container_widget` builds each attribute's row (label + widget), read
`ui.tooltip` from the attribute metadata (a `std::string`, HTML allowed) and
`setToolTip(QString::fromStdString(...))` on the row/label widget. No per-renderer wiring.

## 6. Hesiod-side verification (minimal consumer)

Wire the provider into **one** Hesiod node that has a Range attribute, authored on Meta:
`a->metadata().try_add(meta::keys::ui::data_provider, meta::DataProvider{[&node, port_id]{ …read input VirtualArray, bin a 256-bin histogram, return ProviderData with series_x/series_y… }})`
— the direct analogue of today's `setup_histogram_for_range_slider.cpp`. This is a *verification*
use, not a Range-node migration sweep. GUI check: the histogram renders behind the range
handles and refreshes when the upstream node recomputes.

## 7. Testing

**Meta core (`Meta/tests`):**
- `ProviderData::has_series`/`has_image` predicates.
- `Attribute<DataProvider>` round-trips through serialization as null: `json_to` on a
  container holding a provider omits the entry; `json_from` leaves node behaviour intact; the
  host-set provider remains callable in-memory.
- `keys::ui::data_provider` / `keys::ui::tooltip` present and correctly valued.

**MetaUI:**
- `RangeBar::set_histogram` with a series then `paintEvent` runs without error; empty series
  restores the plain look.
- `glm_vec2` RangeBar renderer with a metadata provider feeds the bar; with none, unchanged.

**Hesiod end-to-end (controller/user, per repo precedent):**
- Build via the nix devshell (capped `-j4`, detached).
- GUI (user): histogram appears behind the range handles on the verification node and
  updates on recompute; `ui.tooltip` shows on hover; no crash.

## 8. Error handling / edge cases

- Provider absent → widgets render exactly as today (no series, no tooltip).
- Provider present but returns empty `ProviderData` → treated as "no data", plain look.
- Provider throws → caught at the renderer call site, logged, plain look (a bad host callback
  must not crash the panel).
- Loading a `.hsd` whose author stored a provider (shouldn't happen — skipped on save) → the
  skip on `json_to` guarantees no provider key on disk; `json_from` never reconstructs one.

## 9. Components summary (isolation)

| Unit | Responsibility | Depends on |
|---|---|---|
| `meta::ProviderData` / `DataProvider` | Qt-free data contract | std only |
| `AttributeTraits/TypeName<DataProvider>` | make it a non-serializable metadata attribute | attribute traits |
| serialize-skip | keep provider out of `.hsd` | metadata serialization |
| `RangeBar::set_histogram` + paint | draw series behind handles | Qt |
| `glm_vec2.inl` RangeBar wiring | read provider → feed RangeBar | RangeBar, metadata |
| `ui.tooltip` in `container_widget` | central tooltip application | Qt, keys |
| Hesiod verification node | prove it end-to-end | all of the above |
