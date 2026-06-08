---
name: hesiod-generate
description: Generate Hesiod procedural terrain (.hsd) graphs and large maps programmatically via the hsd toolkit. Use when a developer wants to author, validate, and batch-render terrain heightmaps/textures from code or natural language — especially procedural or large-map generation (seed sweeps, big shapes, tiling).
---

# hesiod-generate — Hesiod terrain graph authoring skill

## 1. What this is

The `hsd` toolkit lets you author, validate, and render Hesiod procedural terrain graphs without touching the GUI. You write a compact JSON **spec** that names nodes, their params, and the wires between them; the toolkit compiles it to a valid `.hsd` file and drives the Hesiod binary headlessly to produce a 16-bit grayscale heightmap PNG and a colourised preview PNG. This skill is scoped to **game-dev / procedural / large-map** workflows: seed sweeps, biome parameter exploration, tiling big terrain sheets, and programmatic generation pipelines.

---

## 2. Prerequisites

All commands run from the **repo root** (`/home/barrulus/dev/Hesiod` or the active worktree root). Prefix every invocation with `PYTHONPATH=scripts`:

```bash
PYTHONPATH=scripts python3 -m hsd <subcommand> [args]
```

For rendering (the `--run` flag or `hsd run`), point at the Hesiod binary and suppress the display:

```bash
export HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod
export QT_QPA_PLATFORM=offscreen
```

Both env vars must be set; without `HESIOD_BIN` the runner raises an error, and without `QT_QPA_PLATFORM=offscreen` it tries to open an X display.

---

## 3. Workflow

1. **Discover nodes.** Find what node types exist before inventing names.

   ```bash
   PYTHONPATH=scripts python3 -m hsd nodes --search Noise      # keyword search
   PYTHONPATH=scripts python3 -m hsd nodes --category Erosion  # browse a category
   PYTHONPATH=scripts python3 -m hsd nodes --show NoiseFbm     # ports + params
   ```

   Always run `--show TYPE` before wiring a node — port names are not guessable (see §4).

2. **Write a JSON spec.** See §5 and `reference/specs/` for the shape.

3. **Validate.**

   ```bash
   PYTHONPATH=scripts python3 -m hsd validate my_spec.json
   ```

   Structured errors name the offending node and suggest a fix. Fix all errors before proceeding.

4. **Lint (optional).** Checks style / unknown params beyond hard validation:

   ```bash
   PYTHONPATH=scripts python3 -m hsd lint my_spec.json
   ```

5. **Build + run in one step.**

   ```bash
   HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod \
   QT_QPA_PLATFORM=offscreen \
   PYTHONPATH=scripts python3 -m hsd make my_spec.json -o out.hsd --run \
     --shape 1024,1024 --tiling 1,1 --overlap 0.0
   ```

   `--shape`, `--tiling`, `--overlap` override the spec's `config` block at run time — useful for testing at low resolution without editing the spec.

6. **Inspect outputs.** `make --run` writes:
   - `out.png` — 16-bit grayscale heightmap
   - `out_preview.png` — TERRAIN colourmap + hillshade

   Open `out_preview.png` to judge the result visually. **Outputs are only written if the spec contains an `export` entry** — a spec without `export` builds and runs but produces no files.

7. **Iterate.** Adjust params, re-validate, re-run. For large shapes or tiling sweeps, work at 512 × 512 first, then scale up.

You can also separate build and run:

```bash
# build only
PYTHONPATH=scripts python3 -m hsd build my_spec.json -o out.hsd

# run a pre-built .hsd
HESIOD_BIN=... QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd run out.hsd --shape 1024,1024
```

---

## 4. The hard rule — port data-type compatibility

Every link must connect ports of the **same `data_type`**. The two types are:

| Type | Meaning | Typical nodes |
|---|---|---|
| `VirtualArray` | Heightmap / scalar field | `NoiseFbm`, `HydraulicParticle`, `ExportHeightmap` |
| `VirtualTexture` | RGBA colour image | `ColorizeGradient`, `ColorizeSolid`, `ExportTexture` |

These are **incompatible**. Wiring `VirtualArray → VirtualTexture` or vice versa produces a validation error.

**The bridge:** `ColorizeGradient` accepts a `VirtualArray` on its `level` input and emits a `VirtualTexture` on its `texture` output. `ColorizeSolid` does the same. These are the only nodes that cross the type boundary.

**Exporting both heightmap and colour** requires a fork:

```
NoiseFbm.output ──► HydraulicParticle.input
HydraulicParticle.output ──► ExportHeightmap.input          (VirtualArray path)
HydraulicParticle.output ──► ColorizeGradient.level
ColorizeGradient.texture ──► ExportTexture.texture          (VirtualTexture path)
```

**Always run `hsd nodes --show TYPE` before wiring.** Port names are often not `input`/`output`. Confirmed examples:

- `ColorizeGradient`: input `level` (VirtualArray), output `texture` (VirtualTexture)
- `ExportHeightmap`: input `input` (VirtualArray) — no output port
- `ExportTexture`: input `texture` (VirtualTexture) — no output port
- `HydraulicParticle`: input `input`, output `output` (both VirtualArray); also `erosion`, `deposition` outputs

---

## 5. Spec format

Full schema and field-by-field reference: [`reference/spec-schema.md`](reference/spec-schema.md)

Quick shape:

```json
{
  "config": {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
    {"id": "ero",   "type": "HydraulicParticle"},
    {"id": "exp",   "type": "ExportHeightmap"}
  ],
  "links": [
    ["noise.output", "ero.input"],
    ["ero.output",   "exp.input"]
  ],
  "export": [{"node": "ero", "port": "output", "path": "terrain.png"}]
}
```

Rules:
- `id` values are arbitrary strings; they scope link endpoints as `"id.port"`.
- Only list params you want to override — omitted params use Hesiod's defaults (tolerant loader).
- `config` can be omitted; defaults are `shape:[1024,1024]`, `tiling:[1,1]`, `overlap:0.0`.
- `export` is required for file output. Without it the graph runs but writes nothing.

---

## 6. Recipes

Full recipe catalogue with commentary: [`reference/recipes.md`](reference/recipes.md)

Verified ready-to-run specs in [`reference/specs/`](reference/specs/):

| File | What it demonstrates |
|---|---|
| `heightmap_export.json` | Minimal noise → erosion → heightmap export |
| `colour_fork.json` | Fork to both `ExportHeightmap` and colourised `ExportTexture` |
| `tiled_large.json` | 4096 × 4096 map, 4 × 4 tiling, 0.25 overlap |
| `fmg_globe.json` | 4096 × 2048 equirectangular planet heightmap for Azgaar's FMG (3 continents, ocean borders, polar ice caps, X-only tiling) |

Run a recipe directly:

```bash
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod \
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/heightmap_export.json \
  -o /tmp/recipe_test.hsd --run
```

---

## 7. Procedural / large-map generation

### Seed sweep — shell loop

```bash
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod
QT_QPA_PLATFORM=offscreen
export HESIOD_BIN QT_QPA_PLATFORM

for seed in 1 2 3 4 5; do
  # write a per-seed spec
  python3 - <<EOF
import json, sys
spec = {
  "config": {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": $seed}},
    {"id": "ero",   "type": "HydraulicParticle"},
    {"id": "exp",   "type": "ExportHeightmap"}
  ],
  "links": [["noise.output","ero.input"],["ero.output","exp.input"]],
  "export": [{"node":"ero","port":"output","path":"/tmp/sweep_$seed.png"}]
}
json.dump(spec, open("/tmp/sweep_$seed.json","w"), indent=2)
EOF
  PYTHONPATH=scripts python3 -m hsd make /tmp/sweep_$seed.json \
    -o /tmp/sweep_$seed.hsd --run
done
```

### Programmatic generation — Python API

The library is importable directly for tighter loops (no subprocess overhead per spec):

```python
import json
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.catalog import Catalog

catalog = Catalog.load()  # loads Hesiod/data/node_documentation.json

seeds = [1, 2, 3]
for seed in seeds:
    spec_data = {
        "config": {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0},
        "nodes": [
            {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": seed}},
            {"id": "ero",   "type": "HydraulicParticle"},
            {"id": "exp",   "type": "ExportHeightmap"}
        ],
        "links": [["noise.output", "ero.input"], ["ero.output", "exp.input"]],
        "export": [{"node": "ero", "port": "output", "path": f"/tmp/terrain_{seed}.png"}]
    }
    spec = Spec.from_dict(spec_data)
    graph = compile_spec(spec, catalog)                    # returns a Graph object
    hsd_path = f"/tmp/terrain_{seed}.hsd"
    with open(hsd_path, "w") as f:
        json.dump(graph.to_hsd(), f, indent=2)
    print(f"wrote {hsd_path}")
# Then drive rendering via subprocess: hsd run <path> --shape ...
```

`compile_spec(spec, catalog)` returns a `Graph`; call `.to_hsd()` to get the JSON dict ready for `json.dump`. Validate the spec first with `hsd.validate` (or `hsd validate` CLI) — `compile_spec` assumes a valid spec and raises `KeyError` on bad node types.

### Large maps with tiling

Tiling splits a large logical map into `tiling.x × tiling.y` tiles at render time. The `overlap` ratio (0–1) controls the blending margin Hesiod uses to stitch tiles seamlessly — use 0.25 for large maps.

```bash
PYTHONPATH=scripts python3 -m hsd make large_spec.json -o large.hsd --run \
  --shape 4096,4096 --tiling 4,4 --overlap 0.25
```

The `--shape`/`--tiling`/`--overlap` flags on `make` and `run` override `config` without editing the spec file — useful for scaling a verified spec up to production resolution.

A verified 4096 × 4096, 4 × 4 tiled spec is at `reference/specs/tiled_large.json`.

---

## 8. Validation discipline

**Always validate before running.** Rendering a broken spec wastes time and can hang.

```bash
PYTHONPATH=scripts python3 -m hsd validate my_spec.json
```

Successful output: `ok`

Failed output names the node and describes the problem:

```
error: node 'col' — unknown port 'input' for type ColorizeGradient
  hint: available inputs: alpha, level, noise
```

Fix every reported error before calling `make --run`. Common pitfalls:

- Wrong port name on a colourize node (`input` instead of `level`).
- Linking a `VirtualTexture` output into a `VirtualArray` input or vice versa.
- Typo in a node `type` string — validator will say "unknown node type".
- Missing `export` block — the graph builds and runs but produces no output files.

After fixing, re-validate until you see `ok`, then run.
