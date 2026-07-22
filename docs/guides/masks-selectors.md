# Masks & Selectors — Workflow

A **mask** is a grayscale weight (0–1) that tells a node *where* to act. This
guide shows how to build one and wire it in. For what masks and selectors *are*,
see [Masks & Selectors (concepts)](../core_concepts/masks-selectors.md).

## The pattern

1. Start from a heightmap.
2. Run a **selector** to turn a terrain feature into a 0–1 mask.
3. Optionally **shape** the mask (smooth, remap, combine).
4. Wire the mask into the **mask input port** of a filter or erosion node.

## Selectors

Each picks out a feature as a mask (`Terrain Features/Selector`):

- **`SelectSlope`** — steep vs flat. **`SelectAngle`** — facing direction.
- **`SelectThreshold`** / **`SelectInterval`** / **`SelectMidrange`** — by elevation
  threshold / band / mid-range.
- **`SelectValue`**, **`SelectCavities`**, **`SelectTransitions`**,
  **`SelectValley`**, **`SelectRivers`**, **`SelectInwardOutward`**,
  **`SelectBlobLog`**, **`SelectMultiband3`** — feature-specific selectors.

## Shaping & combining masks

- Smooth or remap the selector output to soften hard edges before use.
- **`CombineMask`** — union/intersect/difference two masks
  (`Terrain Features/Mask Operations`).
- **`ScanMask`** — inspect/adjust mask coverage.

## Applying the mask

![The Hesiod node graph for masked smoothing: NoiseFbm feeds both the SmoothCpulse filter and a SelectSlope selector, and SelectSlope's output is wired into SmoothCpulse's mask input port. The 3D viewport shows the smoothed result and the SmoothCpulse settings panel is open on the right.](../images/guides/masks-applied.png)
*`NoiseFbm → SmoothCpulse`, with `SelectSlope → SmoothCpulse.mask`. Source graph: [`masks-applied.hsd`](graphs/masks-applied.hsd) — and [`masks-unmasked.hsd`](graphs/masks-unmasked.hsd) for the no-mask comparison.*

Feed the mask into a node's mask input port — for example, confine
`HydraulicParticle` to steep terrain by driving its mask from `SelectSlope`. See
[Erosion](erosion.md) for masked-erosion recipes and [Texturing](texturing.md)
for soil masks.
