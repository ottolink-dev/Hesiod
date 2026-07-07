## Usage

The `flux` output is a smooth, artifact-free estimate of how much water passes
through each cell. With **Log Scale** enabled (the default) it compresses the
orders-of-magnitude flux into a `[0, 1]` field that can be used directly as a
river / water mask, or as a moisture input to downstream nodes.

Two optional inputs unlock non-uniform transport:

- **source** — a per-cell spawn weight. Feed a rainfall or moisture map to bias
  accumulation toward wetter regions.
- **decay** — a per-cell attenuation rate. Non-zero decay turns the pure
  accumulation into a *transport-with-loss* field (evaporating moisture,
  finite sediment settling distance, snow drift).

Because flow is globally coupled, a single tile (`tiling = 1 × 1`) produces the
most coherent basin-scale drainage; larger tilings approximate it per tile.
Raise **Samples** to reduce per-cell variance (noise falls roughly as `1 / N`).
