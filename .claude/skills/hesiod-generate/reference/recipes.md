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
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod \
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
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod \
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
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod \
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/tiled_large.json \
  -o out.hsd --run
```

Files written: `large.png` (16-bit grayscale heightmap) and `large_preview.png`
(TERRAIN colourmap + hillshade preview).

To test at reduced resolution without editing the spec:

```bash
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod \
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/tiled_large.json \
  -o out.hsd --run --shape 512,512 --tiling 2,2 --overlap 0.25
```
