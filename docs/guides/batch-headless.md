# Automation & Batch / Headless Operation

Hesiod can run without its GUI. Headless operation is useful for:

- **Automation pipelines** — generate terrain assets as part of a build or CI step.
- **Large renders** — queue overnight jobs at resolutions that would be impractical to
  wait on interactively.
- **Seed sweeps and procedural variation** — produce many outputs from one graph by
  varying parameters in a script loop.

Two tools cover this: the **built-in batch mode** bundled with the Hesiod binary, and the
**`hsd` toolkit** — a Python package in this repository that compiles compact JSON specs
into valid `.hsd` files and drives the binary programmatically.

---

## Built-in batch mode

The Hesiod binary accepts a `--batch` flag that loads a `.hsd` graph file and renders it
without opening the GUI.

```
hesiod --batch=graph.hsd [--shape=W,H] [--tiling=X,Y] [--overlap=R]
```

**Flags:**

| Flag | Description |
|---|---|
| `--batch=<file>` | Path to the `.hsd` graph to execute. Required to enter batch mode. |
| `--shape=W,H` | Override the heightmap shape in pixels, e.g. `--shape=2048,2048`. |
| `--tiling=X,Y` | Override tiling, e.g. `--tiling=4,4` to split into 16 tiles. |
| `--overlap=R` | Override the tile overlap ratio (0–1), e.g. `--overlap=0.25`. |

**Output:** export happens **only if the graph defines an export path** (`export_param` —
see [Export configuration](#export-configuration) below). When an export path is
configured, the batch run writes:

- `<path>.png` — 16-bit grayscale heightmap.
- `<path>_preview.png` — TERRAIN colourmap + hillshade composite.

If the graph has no export path set, the graph is computed but no files are written.

**Other modes** (mutually exclusive with `--batch`):

| Flag | Description |
|---|---|
| `--inventory` | Print a JSON inventory of every node type the binary knows about. |
| `--snapshot` | Generate node snapshots (used internally for documentation). |

### Headless display

To suppress the Qt display (required on servers without a display):

```bash
QT_QPA_PLATFORM=offscreen hesiod --batch=graph.hsd --shape=1024,1024
```

### Running from the build directory

The binary must be run from a directory that contains the `data/` folder (node docs,
colour gradients, etc.):

```bash
cd /path/to/Hesiod
QT_QPA_PLATFORM=offscreen build/bin/hesiod --batch=my_graph.hsd
```

---

## Export configuration

The export behaviour in batch mode is controlled by the `export_param` block stored inside
the `.hsd` file. It holds:

| Field | Description |
|---|---|
| `export_path` | Output file path (without extension); the binary appends `.png` and `_preview.png`. |
| `ids` | Which node/port outputs to export. |
| `shape` | Default heightmap resolution. Overridden at runtime by `--shape`. |
| `tiling` | Default tile grid. Overridden by `--tiling`. |
| `overlap` | Default overlap ratio. Overridden by `--overlap`. |

To configure these fields from the GUI, use the Bake & Export dialog
(`Alt+E`, or **File → Bake & Export**). Full details on what the dialog controls and
the resulting export directory structure are in
[Bake and Export](../user_manual/bake_and_export/bake_and_export.md).

---

## The `hsd` toolkit

The `hsd` toolkit is a Python package (`scripts/hsd/`) that lets you author `.hsd` graphs
programmatically — without the GUI and without manually editing the XML-like `.hsd` format.
You describe a graph in a compact JSON **spec**, validate it, and either build a `.hsd` or
build-and-run in one step.

Invoke from the **repo root** with `PYTHONPATH=scripts`:

```bash
PYTHONPATH=scripts python3 -m hsd <subcommand> [args]
```

### Subcommands

| Subcommand | Purpose |
|---|---|
| `nodes --search T` | Search the node catalogue by keyword. |
| `nodes --category C` | List nodes in a category. |
| `nodes --show TYPE` | Show ports and parameters for a node type. |
| `build SPEC -o OUT` | Compile a JSON spec to a `.hsd` file (no render). |
| `validate SPEC` | Check a spec for type errors, unknown nodes, bad links. Prints `ok` or structured errors. |
| `lint FILE` | Style and unknown-param checks beyond hard validation. |
| `run FILE [--shape --tiling --overlap]` | Run a pre-built `.hsd` with the Hesiod binary. |
| `make SPEC -o OUT [--run ...]` | Compile + optionally run in one step. The most common command. |

For rendering (`run` or `make --run`), set:

```bash
export HESIOD_BIN=/path/to/hesiod
export QT_QPA_PLATFORM=offscreen
```

### Spec format

A spec is a JSON object with four top-level keys:

```json
{
  "config": {"shape": [W, H], "tiling": [X, Y], "overlap": R},
  "nodes":  [{"id": "...", "type": "...", "params": {...}}],
  "links":  [["fromNode.fromPort", "toNode.toPort"]],
  "export": [{"node": "...", "port": "...", "path": "out.png"}]
}
```

- **`config`** — optional; defaults to `shape:[1024,1024]`, `tiling:[1,1]`, `overlap:0.0`.
- **`nodes`** — each entry needs `id` (arbitrary string), `type` (exact node type name),
  and optionally `params` (only the values you want to override — omitted params use
  Hesiod's defaults).
- **`links`** — each entry is `["source.port", "dest.port"]` using the `id` strings
  defined in `nodes`.
- **`export`** — required for file output. Without it the graph runs but writes nothing.

### Port data-type rule

Every link must connect ports of the **same data type**. The two types that matter for
terrain work are:

| Type | Meaning | Typical nodes |
|---|---|---|
| `VirtualArray` | Heightmap / scalar field | `NoiseFbm`, `HydraulicParticle`, `ExportHeightmap` |
| `VirtualTexture` | RGBA colour image | `ColorizeGradient`, `ColorizeSolid`, `ExportTexture` |

These are **incompatible** — linking `VirtualArray → VirtualTexture` or vice versa
produces a validation error. The bridge nodes are `ColorizeGradient` and `ColorizeSolid`,
which accept a `VirtualArray` on their `level` input and emit a `VirtualTexture` on their
`texture` output.

**Exporting both heightmap and colour** requires a fork at the `VirtualArray` output:

```
NoiseFbm.output ──► HydraulicParticle.input
HydraulicParticle.output ──► ExportHeightmap.input       (VirtualArray path)
HydraulicParticle.output ──► ColorizeGradient.level
ColorizeGradient.texture ──► ExportTexture.texture       (VirtualTexture path)
```

Always run `hsd nodes --show TYPE` before wiring a node — port names are not always
`input`/`output`. Confirmed port names for the export nodes:

- `ExportHeightmap`: input `input` (VirtualArray), no output port.
- `ExportTexture`: input `texture` (VirtualTexture), no output port.
- `ColorizeGradient`: input `level` (VirtualArray), output `texture` (VirtualTexture).

### Worked example

The verified spec at
`.claude/skills/hesiod-generate/reference/specs/heightmap_export.json`:

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
  "export": [{"node": "ero", "port": "output", "path": "heightmap.png"}]
}
```

Validate, then build and run at a small test resolution:

```bash
# validate first — prints "ok" or structured errors
PYTHONPATH=scripts python3 -m hsd validate \
  .claude/skills/hesiod-generate/reference/specs/heightmap_export.json

# compile + render at 256×256 for a quick sanity check
HESIOD_BIN=/path/to/hesiod \
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  .claude/skills/hesiod-generate/reference/specs/heightmap_export.json \
  -o /tmp/test.hsd --run --shape 256,256 --tiling 1,1
```

On success the toolkit writes:

- `/tmp/test.png` — 16-bit grayscale heightmap
- `/tmp/test_preview.png` — TERRAIN colourmap + hillshade

Scale up by changing `--shape` and `--tiling` without touching the spec file. A verified
4096 × 4096 tiled spec (4 × 4 tiles, 0.25 overlap) is at
`.claude/skills/hesiod-generate/reference/specs/tiled_large.json`.

---

## See also

- [LLM-driven procedural generation](llm-procedural-generation.md) — using the `hsd`
  toolkit with an LLM for seed sweeps, biome exploration, and large-map pipelines.
- The `hesiod-generate` Claude Code skill lives at
  `.claude/skills/hesiod-generate/SKILL.md` — load it when you want Claude to author and
  validate specs for you.
- [Bake and Export](../user_manual/bake_and_export/bake_and_export.md) — the GUI
  counterpart to batch mode; covers export directory structure and variants.
- [Tiling and Overlap](../core_concepts/tiling-overlap.md) — how tile boundaries are
  blended.
