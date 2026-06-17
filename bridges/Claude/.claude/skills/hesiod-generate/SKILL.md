---
name: hesiod-generate
description: Generate Hesiod procedural terrain (.hsd) graphs and large maps programmatically via the hsd toolkit. Use when a developer wants to author, validate, and batch-render terrain heightmaps/textures from code or natural language — especially procedural or large-map generation (seed sweeps, big shapes, tiling).
---

# hesiod-generate — Hesiod terrain graph authoring skill

## 1. What this is

The `hsd` toolkit lets you author, validate, and render Hesiod procedural terrain graphs without touching the GUI. You write a compact JSON **spec** that names nodes, their params, and the wires between them; the toolkit compiles it to a valid `.hsd` file and drives the Hesiod binary headlessly to produce a 16-bit grayscale heightmap PNG and a colourised preview PNG. This skill is scoped to **game-dev / procedural / large-map** workflows: seed sweeps, biome parameter exploration, tiling big terrain sheets, and programmatic generation pipelines.

---

## 2. Prerequisites

All commands run from the **repository root** (the directory containing `scripts/` and `Hesiod/`). Prefix every invocation with `PYTHONPATH=scripts`:

```bash
PYTHONPATH=scripts python3 -m hsd <subcommand> [args]
```

For rendering (the `--run` flag or `hsd run`), the runner auto-locates the Hesiod binary and you must suppress the display:

- **In-tree build** (`build/bin/hesiod` exists under the repo root): no extra config needed — the runner finds it automatically.
- **Binary elsewhere**: set `HESIOD_BIN` before running:
  ```bash
  export HESIOD_BIN=/path/to/hesiod
  ```
- **Always** set `QT_QPA_PLATFORM=offscreen` for headless rendering:
  ```bash
  export QT_QPA_PLATFORM=offscreen
  ```

Without `QT_QPA_PLATFORM=offscreen` the binary tries to open an X display. Without a binary (either in-tree or via `HESIOD_BIN`), the runner raises an error.

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

4. **Lint (optional).** Checks model↔UI consistency in a compiled `.hsd` file (not a
   spec JSON). Run it on the `.hsd` output of `hsd build` or `hsd make`:

   ```bash
   PYTHONPATH=scripts python3 -m hsd lint out.hsd
   ```

5. **Build + run in one step.**

   ```bash
   QT_QPA_PLATFORM=offscreen \
   PYTHONPATH=scripts python3 -m hsd make my_spec.json -o out.hsd --run \
     --shape 1024,1024 --tiling 1,1 --overlap 0.0
   ```

   `--shape`, `--tiling`, `--overlap` override the **compute** config used by each
   graph node at render time. They do **not** change the exported PNG resolution —
   that comes from `export_param.shape` baked into the `.hsd` at build time from the
   spec's `config.shape`. To change the output file's dimensions, set `config.shape`
   in the spec JSON and rebuild.

6. **Inspect outputs.** `make --run` writes files at the path(s) set in the spec's
   `export[].path` field (not derived from `-o`). For `heightmap_export.json`
   (export path `"heightmap.png"`) that is:
   - `heightmap.png` — 16-bit grayscale heightmap (relative to cwd)
   - `heightmap_preview.png` — TERRAIN colourmap + hillshade (relative to cwd)

   Open `out_preview.png` for a visual gut-check, but **verify numerically — the colourmap misleads** (a flat max plateau can look thin, value ranges are remapped). Use the built-in inspector on the raw 16-bit `out.png`:

   ```bash
   PYTHONPATH=scripts python3 -m hsd inspect out.png              # dims, bitdepth, min/max/mean
   PYTHONPATH=scripts python3 -m hsd inspect out.png --edges      # border means + left/right wrap-seam match
   PYTHONPATH=scripts python3 -m hsd inspect out.png --landfrac 0.5   # fraction of pixels above a threshold
   PYTHONPATH=scripts python3 -m hsd inspect out.png --profile col --profile-n 16  # column-mean profile
   ```

   This catches what the preview hides — e.g. confirming borders are 0, caps are symmetric, or a wrap seam matches. **Outputs are only written if the spec contains an `export` entry** — a spec without `export` builds and runs but produces no files.

7. **Iterate.** Adjust params, re-validate, re-run. For large shapes or tiling sweeps, work at 512 × 512 first, then scale up.

You can also separate build and run:

```bash
# build only
PYTHONPATH=scripts python3 -m hsd build my_spec.json -o out.hsd

# run a pre-built .hsd
QT_QPA_PLATFORM=offscreen \
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

**The bridge:** `ColorizeGradient` accepts a `VirtualArray` on its `level` input and
emits a `VirtualTexture` on its `texture` output. `ColorizeSolid` also outputs a
`VirtualTexture` but its inputs differ — it takes an optional `alpha` (VirtualArray)
and renders a **uniform colour** set via its `color` param; it has no `level` input.
Use `ColorizeGradient` when you need to map heightmap values to a colour gradient.

**Exporting both heightmap and colour** requires a fork:

```
NoiseFbm.output ──► HydraulicParticle.input
HydraulicParticle.output ──► ExportHeightmap.input          (VirtualArray path)
HydraulicParticle.output ──► ColorizeGradient.level
ColorizeGradient.texture ──► ExportTexture.texture          (VirtualTexture path)
```

**Always run `hsd nodes --show TYPE` before wiring.** Port names are often not `input`/`output`. Confirmed examples:

- `ColorizeGradient`: inputs `level`, `alpha`, `noise` (all VirtualArray), output `texture` (VirtualTexture)
- `ColorizeSolid`: input `alpha` (VirtualArray, optional), output `texture` (VirtualTexture); no `level` port
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
- **Enumeration / Choice params take a plain string.** The toolkit resolves it to the correct
  integer. Run `hsd nodes --show TYPE` to see valid choices listed inline next to the param name.
  Example — `"blending_method": "maximum"` for a `Blend` node. A handful of uncatalogued enums
  still need a full value-object dict; `--show` will print `(pass a full value-object dict)` for
  those, and validation will flag invalid strings.

  ```json
  {"id": "blend", "type": "Blend", "params": {"blending_method": "maximum"}}
  ```

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
| `multi_graph_world.json` | 2 × 2 region graphs over a broadcast base continent, single flattened export |

Run a recipe directly:

```bash
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  bridges/Claude/.claude/skills/hesiod-generate/reference/specs/heightmap_export.json \
  -o /tmp/recipe_test.hsd --run
```

---

## 7. Procedural / large-map generation

### Seed sweep — shell loop

```bash
export QT_QPA_PLATFORM=offscreen
# If hesiod is not at build/bin/hesiod under the repo root, also set:
# export HESIOD_BIN=/path/to/hesiod

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

`compile_spec(spec, catalog)` returns a `Project`; call `.to_hsd()` to get the JSON dict ready for `json.dump`. Validate the spec first with `hsd.validate` (or `hsd validate` CLI) — `compile_spec` assumes a valid spec and raises `KeyError` on bad node types.

### Large maps with tiling

Tiling splits a large logical map into `tiling.x × tiling.y` tiles at render time. The `overlap` ratio (0–1) controls the blending margin Hesiod uses to stitch tiles seamlessly — use 0.25 for large maps.

```bash
PYTHONPATH=scripts python3 -m hsd make large_spec.json -o large.hsd --run \
  --shape 4096,4096 --tiling 4,4 --overlap 0.25
```

The `--shape`/`--tiling`/`--overlap` flags on `make` and `run` override the **compute**
config for each node. Note that the exported PNG resolution is controlled by `config.shape`
baked into the `.hsd` at build time; to change the output file dimensions, update
`config.shape` in the spec and rebuild.

A verified 4096 × 4096, 4 × 4 tiled spec is at `reference/specs/tiled_large.json`.

### Multi-graph regional maps

Tiling scales **resolution**; multi-graph scales **variety**. When different parts of a
large map need different pipelines (mountain erosion here, dune fields there) while
sharing one underlying continent shape, describe the map as a project of spatially-placed
graphs instead of one big graph:

- a top-level `graphs` array, each graph placed by `origin`/`size` or by `cell` on a
  `grid` (the world is **y-up**: row 0 = south);
- `broadcasts` entries (`["base.noise.output", "region.recv"]`) share heightmaps across
  graphs — the compiler inserts and wires the Broadcast/Receive nodes and tags for you;
  the destination graph uses `recv.output` like any local port. Frames must overlap:
  a Receive only gets data where its rectangle intersects the source graph's;
- one object-form `export` flattens selected outputs from all regions into a single
  image (keep its `shape` proportional to the world bbox — there is no aspect
  correction).

Full format: [`reference/spec-schema.md`](reference/spec-schema.md) §Multi-graph specs.
Verified example: `reference/specs/multi_graph_world.json` (recipe + recorded numbers in
[`reference/recipes.md`](reference/recipes.md)).

---

## 8. Validation discipline

**Always validate before running.** Rendering a broken spec wastes time and can hang.

```bash
PYTHONPATH=scripts python3 -m hsd validate my_spec.json
```

Successful output: `ok`

Failed output names the node and describes the problem. Each line has a level prefix
(`L1` for node/param errors, `L2` for link/port errors), an optional `[node_id]` tag,
and a suggestion on the next line when one is available. For example:

```
L1 [n1]: unknown param 'badparam' on node type 'NoiseFbm'
      -> run `hsd nodes --show NoiseFbm` to list its params
L2: 'col.input' is not an input port
```

Fix every reported error before calling `make --run`. Common pitfalls:

- Wrong port name on a colourize node (`input` instead of `level`).
- Linking a `VirtualTexture` output into a `VirtualArray` input or vice versa.
- Typo in a node `type` string — validator will say "unknown node type".
- Missing `export` block — the graph builds and runs but produces no output files.

After fixing, re-validate until you see `ok`, then run.
