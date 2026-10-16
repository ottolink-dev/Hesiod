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

Install a package first using the [installation guide](../getting_started/installation.md).
In the examples below, `hesiod` means the executable or launcher for your
installation; a downloaded ZIP does not add it to your shell's `PATH`.

---

## Opening a project from the command line

Independently of batch mode, the binary accepts a project file to open **in the GUI** —
useful for shell aliases, file-manager associations, and scripted workflows that end with
a visual check:

```
hesiod myproject.hsd
hesiod -f myproject.hsd
hesiod --file myproject.hsd
```

The three forms are equivalent; `-f`/`--file` is easier to keep unambiguous in scripts
that pass several flags. The example-selector dialog is skipped and the file behaves as
a regularly opened project (Ctrl+S saves back to it, and it is added to the
**File → Open Recent** menu).

- If the positional argument and `-f/--file` are both given **with different values**,
  Hesiod prints an error explaining the conflict and exits with a non-zero status.
- If the file does not exist, a warning is logged and startup continues as if no file
  had been given.

The **Open Recent** list is stored in the settings JSON (`~/.config/hesiod/hesiod.json`
on Linux) under `global.recent_files`; its length is capped by `global.max_recent_files`
(default 10).

---

## Built-in batch mode

The Hesiod binary accepts a `--batch` flag that loads a `.hsd` graph file and renders it
without opening the GUI.

```
hesiod --batch=graph.hsd [--shape=W,H] [--tiling=X,Y] [--overlap=R] [--force-distributed] [--force-sequential] [--min-memory]
```

**Flags:**

| Flag | Description |
|---|---|
| `--batch=<file>` / `-b <file>` | Path to the `.hsd` graph to execute. Required to enter batch mode. |
| `--shape=W,H` | Override the heightmap shape in pixels, e.g. `--shape=2048,2048`. |
| `--tiling=X,Y` | Override tiling, e.g. `--tiling=4,4` to split into 16 tiles. |
| `--overlap=R` | Override the tile overlap ratio (`0 <= R < 1`), e.g. `--overlap=0.25`. |
| `--force-distributed` | Force multi-threaded distributed computation mode (`VA_DISTRIBUTED`) for both CPU and GPU compute modes. |
| `--force-sequential` | Force single-threaded sequential computation mode (`VA_SEQUENTIAL`) for both CPU and GPU compute modes. |
| `--min-memory` / `--low-memory` | Minimal memory footprint mode: activates on-disk LRU caching (`VA_DISK_LRU_MIN` keeping minimum active tiles in RAM) and forces sequential compute mode (`VA_SEQUENTIAL`). Tiles are processed one-by-one to minimize RAM usage at the cost of higher disk I/O. |

Pass `--shape`, `--tiling` and `--overlap` together when overriding the compute
configuration. Supplying only some of them can reset the omitted values to
batch defaults rather than preserving the saved graph settings.

**Output:** configure file output before saving the graph. There are two routes:

- Export nodes such as `ExportHeightmap` and `ExportTexture` write their configured
  files during graph computation when **Auto Export on Node Update**
  (`auto_export`) is enabled.
- The graph's `export_param` configuration exports a flattened heightmap after
  computation. See [Export configuration](#export-configuration) below.

For a flattened export path of `terrain.png`, the batch run writes:

- `terrain.png` — 16-bit grayscale heightmap.
- `terrain_preview.png` — TERRAIN colourmap + hillshade composite.

Without either export route configured, computing the graph does not produce
terrain files. The batch run also writes timing information to `batch.log`.

**Other modes** (mutually exclusive with `--batch`):

| Flag | Description |
|---|---|
| `--inventory` | Print a JSON inventory of every node type the binary knows about. |
| `--snapshot` | Generate node snapshots (used internally for documentation). |

### Headless display

To suppress the Qt display (required on servers without a display):

```bash
QT_QPA_PLATFORM=offscreen hesiod --batch=graph.hsd --shape=1024,1024 --tiling=1,1 --overlap=0
```

This suppresses the GUI; it does not remove the need for Qt, application data or
an OpenCL runtime for the nodes you use.

### Running a downloaded package

On Linux, run from the extracted folder containing both `hesiod.AppImage` and
`data`. Use an absolute project path if the graph is stored elsewhere:

```bash
QT_QPA_PLATFORM=offscreen ./hesiod.AppImage --batch=/absolute/path/graph.hsd --shape=1024,1024 --tiling=1,1 --overlap=0
```

On Windows, open PowerShell in the extracted folder containing `hesiod.exe`:

```powershell
$env:QT_QPA_PLATFORM = 'offscreen'
.\hesiod.exe --batch="C:\Projects\graph.hsd" --shape=1024,1024 --tiling=1,1 --overlap=0
```

For an experimental macOS app installed in Applications, invoke its executable
directly to receive logs and an exit status in the terminal:

```bash
QT_QPA_PLATFORM=offscreen /Applications/Hesiod.app/Contents/MacOS/Hesiod --batch=/absolute/path/graph.hsd --shape=1024,1024 --tiling=1,1 --overlap=0
```

Use absolute input and export paths for portable scripts. In particular, the
experimental macOS app uses a per-user runtime working directory.

### Running from the build directory

For an unpackaged Linux source build, CMake copies the `data/` folder (node docs,
colour gradients, etc.) into `build/bin`. Run from that directory:

```bash
cd /path/to/Hesiod/build/bin
QT_QPA_PLATFORM=offscreen ./hesiod --batch=/absolute/path/my_graph.hsd --shape=1024,1024 --tiling=1,1 --overlap=0
```

---

## Export configuration

The export behaviour in batch mode is controlled by the `export_param` block stored inside
the `.hsd` file. It holds:

| Field | Description |
|---|---|
| `export_path` | Output file path, preferably ending in `.png`. If no extension is supplied, `.png` is added. The preview inserts `_preview` before the extension. |
| `ids` | Which node/port outputs to export. |
| `shape` | Exported PNG resolution, baked in at build time from the spec's `config.shape`. To change export dimensions, update `config.shape` in the spec and rebuild. |
| `tiling` | Tile grid for the flattened export, separate from the compute grid overridden by `--tiling`. |
| `overlap` | Overlap for the flattened export, separate from the compute overlap overridden by `--overlap`. |

To configure these fields from the GUI, use the Bake & Export dialog
(`Alt+E`, or **File → Bake & Export**). Full details on what the dialog controls and
the resulting export directory structure are in
[Bake and Export](../user_manual/bake_and_export/bake_and_export.md).

---

## The `hsd` toolkit

The `hsd` toolkit is a Python package (`scripts/hsd/`) that lets you author `.hsd` graphs
programmatically — without the GUI and without manually editing the JSON `.hsd` format.
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
| `lint FILE` | Model↔UI consistency check on a compiled `.hsd` file (not a spec JSON). |
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
produces a validation error. The primary bridge node is `ColorizeGradient`, which accepts
a `VirtualArray` on its `level` input and emits a `VirtualTexture` on its `texture` output.
`ColorizeSolid` also outputs a `VirtualTexture`, but it renders a **uniform colour** (set
via its `color` param) and has no `level` input; it accepts an optional `alpha`
(VirtualArray) input only.

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
`bridges/Claude/.claude/skills/hesiod-generate/reference/specs/heightmap_export.json`:

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
  bridges/Claude/.claude/skills/hesiod-generate/reference/specs/heightmap_export.json

# compile + render at 256×256 for a quick sanity check
HESIOD_BIN=/path/to/hesiod \
QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make \
  bridges/Claude/.claude/skills/hesiod-generate/reference/specs/heightmap_export.json \
  -o /tmp/test.hsd --run --shape 256,256 --tiling 1,1
```

On success the toolkit writes files at the path set in the spec's `export[].path` field
(not derived from the `-o` `.hsd` path). For `heightmap_export.json` (export path
`"heightmap.png"`) that is, relative to cwd:

- `heightmap.png` — 16-bit grayscale heightmap
- `heightmap_preview.png` — TERRAIN colourmap + hillshade

To change the output file dimensions, update `config.shape` in the spec and rebuild — the
exported PNG resolution is baked into the `.hsd` at build time; `--shape` overrides the
compute config only. A verified 4096 × 4096 tiled spec (4 × 4 tiles, 0.25 overlap) is at
`bridges/Claude/.claude/skills/hesiod-generate/reference/specs/tiled_large.json`.

---

## See also

- [LLM-driven procedural generation](llm-procedural-generation.md) — using the `hsd`
  toolkit with an LLM for seed sweeps, biome exploration, and large-map pipelines.
- The `hesiod-generate` Claude Code skill lives at
  `bridges/Claude/.claude/skills/hesiod-generate/SKILL.md` — load it when you want Claude to author and
  validate specs for you.
- [Bake and Export](../user_manual/bake_and_export/bake_and_export.md) — the GUI
  counterpart to batch mode; covers export directory structure and variants.
- [Tiling and Overlap](../core_concepts/tiling-overlap.md) — how tile boundaries are
  blended.
