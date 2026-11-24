# Brush → native meta::Array — design

**Date:** 2026-08-05
**Status:** approved (design discussed in-session; field size 256 confirmed by barrulus)
**Depends on:** Meta pin ≥ `0e6e33ad` (meta::Array `048f0df5` + ArrayCanvas `b931b8d5`,
`META_ENABLE_ARRAY_TYPES` defaults ON — no CMake change needed).

## Goal

Migrate the Brush node's `hmap` attribute from the legacy `attr::ArrayAttribute`
(Attributes + QSliderX widget stack) to a native `meta::Array` rendered by Meta's
`ArrayEditor`/`ArrayCanvas`. Brush is the **only remaining mixed-backend node**; after this
change no node uses the legacy attribute widgets, unblocking the retirement of the
Attributes and QSliderX submodules (separate follow-up task).

## Non-goals

- Retiring Attributes/QSliderX (next task, not this one).
- Changing brush painting semantics, model resolution, or the compute path.
- New painting features beyond what ArrayCanvas provides.

## Design

### 1. Attribute (brush.cpp setup)

Replace `node.add_attr<ArrayAttribute>("hmap", "Heightmap", glm::ivec2(512, 512))` with a
`meta::Array` added to the node's meta container:

- value: `meta::Array{glm::ivec2(512, 512), std::vector<float>(512 * 512, 0.f)}` —
  **model shape stays 512×512**, matching legacy.
- metadata: `ui.label` "Heightmap", `ui.category` "Main",
  `ui.width` = `ui.height` = **256** (ArrayCanvas paint-field size; renderer default is 128,
  legacy painted at full 512 — 256 is the agreed starting point, tune by GUI feel; it is a
  one-line metadata change),
  `hsd::compat::keys::type_label` "Array" (panel type chip, consistent with other native
  nodes). `ui.widget_type` may be omitted (renderer defaults to `ArrayEditor`).

The renderer resamples canvas→model bicubic on every stroke and model→canvas bilinear on
sync; compute is unaffected by the field size.

### 2. Background image provider

Same `ui.data_provider` → `meta::qt::ImageData` pattern as Cloud (`cloud.cpp`) /
`setup_background_image_for_cloud_attribute`: colorize the `background` input port to MAGMA
256×256 RGB, vertical flip, return empty `meta::Any` when the port is unwired. Reuse the
existing setup helper if it applies cleanly to the Brush attribute key (it is
attribute-key + port-id generic in its meta path); otherwise inline the lambda as Cloud
does. The legacy `set_background_image_fct` QImage lambda is deleted.

### 3. Compute (brush.cpp compute)

Read `meta::Array` from the container instead of the legacy attr:

```
const auto &arr = <container>.value<meta::Array>("hmap");
hmap::Array array(arr.shape);      // was: get_shape()
array.vector = arr.vector;         // was: get_value()
array = array.resample_to_shape_bilinear(cfg shape);   // unchanged
```

Everything downstream (VirtualArray fill, post_process) is unchanged.

### 4. Legacy .hsd decoding

Register a per-key legacy decoder (the facade's `legacy_decoders_` mechanism on BaseNode)
for `"hmap"` translating the legacy ArrayAttribute json (value vector + shape — confirm
exact field names against the Attributes serializer during implementation) into
`meta::Array`. Old painted Brush projects must load intact; new saves are native `_meta`
only (Meta serializes Array with binary json).

### 5. Settings-panel cleanup

With `hmap` native, Brush is pure-Meta: remove the mixed-backend dual-render special case
(the "Option A" fix, commit `92eadf5c`, plus the UB-init companion `944ec6aa` if its guard
becomes dead) from the node settings/attributes widgets. `brush.cpp` drops its
`attributes.hpp` include.

## Verification gates

1. Build green (Linux dev build).
2. `--parity-dump` vs fixture: Brush's `hmap` **leaves** the parity set (no
   `compat.legacy_type`) — expect a one-node fixture re-baseline, mirroring the other
   native nodes; all other nodes must remain 0-diff.
3. `--compat-check` on `Hesiod/data/examples` and `docs/examples`: 0 failures — this is
   what proves legacy Brush decode (both corpora contain Brush files).
4. User GUI pass: paint feel at field 256 vs legacy; background thumbnail from wired input;
   live preview during a stroke (value_changed path); save → reload round-trip of a
   painting; load of a pre-migration painted .hsd.

## Risks / watch items

- **Amplitude rescale on first stroke (Meta-side suspicion):** the ArrayEditor sync
  normalizes model data to [0,1] for display and writes canvas values back without
  denormalizing. A loaded painting whose values do not span [0,1] may get rescaled by the
  first edit. Targeted test during implementation; if confirmed, the fix is Meta-side and
  goes on Otto's branch alongside stress-matrix findings 6/7/8/11.
- **Per-stroke resample cost:** every stroke bicubic-resamples 256²→512². Judge by feel;
  field size is tunable.
- **Fixture churn:** parity fixture re-baseline must be limited to Brush; any other node
  drifting is a regression, not baseline noise.

## Sequencing

Implementation follows the v0.6.0 release packaging work. Retirement of
Attributes/QSliderX is a separate follow-up spec once Brush is verified on dev.
