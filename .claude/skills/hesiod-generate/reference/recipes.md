# Hesiod spec recipes

Ready-to-run verified specs in `reference/specs/`. All specs pass `hsd validate`.

For the spec format reference see [`spec-schema.md`](spec-schema.md). For the full skill
workflow — discovery, build, run, iteration — see [`../SKILL.md`](../SKILL.md).

---

## heightmap_export.json — minimal noise → erosion → heightmap

The simplest working pipeline: fractal noise (`NoiseFbm`) feeds hydraulic erosion
(`HydraulicParticle`), and the eroded heightfield is written as a 16-bit grayscale PNG. Use this
as a starting point for any single-layer heightmap, for rapid param iteration, or as the base
graph that more complex specs extend. It produces one heightmap output (`heightmap.png` +
`heightmap_preview.png`).

```json
{
  "config": {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
    {"id": "ero", "type": "HydraulicParticle"},
    {"id": "exp", "type": "ExportHeightmap"}
  ],
  "links": [["noise.output", "ero.input"], ["ero.output", "exp.input"]],
  "export": [{"node": "ero", "port": "output", "path": "heightmap.png"}]
}
```

Run command:

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/heightmap_export.json \
  -o out.hsd --run
```

Files written: `heightmap.png` (16-bit grayscale heightmap) and `heightmap_preview.png`
(TERRAIN colourmap + hillshade preview).

---

## colour_fork.json — fork to heightmap and colourised texture

Extends the minimal pipeline with a fork after erosion: one branch exports the raw heightfield
(`ExportHeightmap`), while the other converts the field through `ColorizeGradient` and exports
an RGBA colour texture (`ExportTexture`). This demonstrates the `VirtualArray → VirtualTexture`
bridge and the pattern required any time you need both a heightmap and a colour image from the
same graph. The `export` block captures the heightfield branch; the colour texture is also
rendered to disk via the `ExportTexture` node's internal export path.

```json
{
  "config": {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
    {"id": "ero", "type": "HydraulicParticle"},
    {"id": "exph", "type": "ExportHeightmap"},
    {"id": "col", "type": "ColorizeGradient"},
    {"id": "expt", "type": "ExportTexture"}
  ],
  "links": [
    ["noise.output", "ero.input"],
    ["ero.output", "exph.input"],
    ["ero.output", "col.level"],
    ["col.texture", "expt.texture"]
  ],
  "export": [{"node": "ero", "port": "output", "path": "terrain.png"}]
}
```

Run command:

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/colour_fork.json \
  -o out.hsd --run
```

Files written: `terrain.png` (16-bit grayscale heightmap) and `terrain_preview.png`
(TERRAIN colourmap + hillshade preview). The `ExportTexture` node (`expt`) also writes its own
output according to Hesiod's internal export path for that node type.

Note: `ColorizeGradient` accepts a `VirtualArray` on its `level` port and emits a
`VirtualTexture` on its `texture` port. Wiring `ero.output` (VirtualArray) directly to
`expt.texture` (VirtualTexture) would be a type error — `col` is the required bridge. See
`SKILL.md §4` and `spec-schema.md` for the port-datatype rule.

---

## tiled_large.json — 4096 × 4096 map with 4 × 4 tiling

A production-scale spec: a 4096 × 4096 logical canvas split into a 4 × 4 grid of tiles with
0.25 overlap for seamless stitching. The higher wavenumber (`kw: [8, 8]`) gives finer detail
appropriate for the larger canvas. Use this spec as the template for any large-map or tiling
workflow; scale down to 512 × 512 with `--shape 512,512` for fast iteration, then run at full
resolution for final output. Tiling is handled entirely by Hesiod at render time — the spec
graph is identical to the minimal case.

```json
{
  "config": {"shape": [4096, 4096], "tiling": [4, 4], "overlap": 0.25},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [8, 8], "seed": 42}},
    {"id": "ero", "type": "HydraulicParticle"},
    {"id": "exp", "type": "ExportHeightmap"}
  ],
  "links": [["noise.output", "ero.input"], ["ero.output", "exp.input"]],
  "export": [{"node": "ero", "port": "output", "path": "large.png"}]
}
```

Run command:

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/tiled_large.json \
  -o out.hsd --run
```

Files written: `large.png` (16-bit grayscale heightmap) and `large_preview.png`
(TERRAIN colourmap + hillshade preview).

To test at reduced resolution without editing the spec:

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/tiled_large.json \
  -o out.hsd --run --shape 512,512 --tiling 2,2 --overlap 0.25
```

---

## fmg_globe.json — tiled equirectangular planet heightmap for Azgaar's Fantasy Map Generator

A 31-node graph producing a 2:1 equirectangular (4096 × 2048) planet heightmap for use as a
globe import in [Azgaar's Fantasy Map Generator (FMG)](https://azgaar.github.io/Fantasy-Map-Generator/).
The map features three distinct continents separated by two carved ocean straits, guaranteed ocean
at all four borders (so FMG sees a complete sphere with no land bleeding across the antimeridian),
symmetric hard-edged polar ice caps that sit above the continental elevation (so FMG's
height→temperature mapping puts them in the glacier zone with no states), and X-only periodicity
for clean longitude tileability. Spec: [`reference/specs/fmg_globe.json`](specs/fmg_globe.json).

### Technique summary (hard-won pattern)

- **Erode first, mask second.** `HydraulicParticle` outputs all-zero when fed a heightmap with
  large exact-zero regions. Always erode the full-domain noise first, then multiply by the land
  mask.
- **Ocean borders via full-domain `Bump` multiply.** A `Bump` centered at (0.5, 0.5) with high
  `gain` acts as a smooth window that drives all four borders to zero without any per-edge
  handling. `gain` controls how tightly land may approach the edge.
- **Vertical straits via paired `Step(angle=0)`.** `Step(angle=0)` is a vertical (x-axis)
  escarpment. A "notch keep-mask" = `max(stepFalling@x1, stepRising@x2)` punches a clean ocean
  channel between two x positions. Wire the keep-mask into the continent field *before* the Warp
  so strait coasts become irregular rather than perfectly straight.
- **`Blend` arithmetic methods are unreliable.** `substract` (and likely `add`) does not produce
  `a − b`; default post-processing corrupts the result. Use only `multiply` and `maximum` for
  mask math. For a notch: `max(falling_step, rising_step)` = land-outside-the-gap, which you
  then multiply into the continent field.
- **Symmetric polar caps via mirrored `Slope`s.** `WaveSine` latitude bands are asymmetric about
  the domain centre. Build caps as `max(Slope(angle=90), Slope(angle=90, post_inverse=true))` →
  tight `Clamp` (e.g. `[0.835, 0.845]`, `remap=true`) → flat max-elevation plateau with a hard
  cliff edge and no slope strip.
- **Caps must be higher than continents.** In FMG, height drives temperature → glacier state. If
  caps sit at the same elevation as land, FMG forms a ring of states around the pole and draws a
  visible seam line at the globe pole. Force cap elevation strictly above the continental range.
- **`MakePeriodic` (X-only) as the final step.** Place it immediately before `ExportHeightmap`
  so the longitude seam is guaranteed to match without affecting the height range or cap symmetry.
- **Verify numerically, not by eye.** The TERRAIN colour preview misleads on flat high plateaus
  (they look "thin" on the colourmap). Use `hsd inspect <png> --edges --landfrac` to confirm
  border columns = 0 and cap symmetry numerically.

### Run commands

Smoke test at low resolution (keeps the 2:1 ratio):

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/fmg_globe.json \
  -o /tmp/fmg.hsd --run --shape 256,128 --tiling 1,1
```

Full production render (matches the spec's `config` block):

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/fmg_globe.json \
  -o fmg_globe.hsd --run --shape 4096,2048 --tiling 4,2 --overlap 0.25
```

Files written: `output/fmg_globe_heightmap_4096x2048.png` (16-bit grayscale) and
`output/fmg_globe_heightmap_4096x2048_preview.png` (TERRAIN colourmap + hillshade).

### Verification

After rendering, verify borders are ocean and caps are symmetric:

```bash
PYTHONPATH=scripts python3 -m hsd inspect output/fmg_globe_heightmap_4096x2048.png \
  --edges --landfrac
```

The `--edges` report should show the far E/W columns at 0.000 at all mid-latitudes. The colour
preview alone is not sufficient — a flat max-elevation plateau reads as a "thin" band on the
TERRAIN colourmap and can look asymmetric even when perfectly balanced.
