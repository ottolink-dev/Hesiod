# Design — `hsd` toolkit: programmatic `.hsd` authoring, validation & execution

Date: 2026-06-06
Status: approved design (foundation phase)
Branch context: `docs-core-concepts` (docs content phase); this introduces the first
non-doc tooling deliverable.

## Goal

Enable automated, validated procedural terrain generation in Hesiod by treating the
`.hsd` graph file as a compile target. Two driving use cases:

1. **Doc example generation** — generate example `.hsd` files and their preview PNGs for
   the documentation, in-repo, via Claude Code.
2. **Game-dev large-map automation** — let a developer (optionally LLM-driven) generate
   many / large terrain graphs procedurally and render them headlessly.

Both share one engine. This spec covers **only the foundation (deliverable B)**. The
LLM-facing skill (C), the MCP, and the manual page (A) are designed at a high level here
as a roadmap but are **separate, later** implementation cycles.

## Decisions (settled during brainstorming)

- **Build order:** foundation (B) → skill (C) → manual page (A).
- **Shape:** hybrid — a **declarative JSON spec** is the public contract; a **Python
  builder library** is the engine that compiles it. The library is also usable directly
  for procedural / loop generation.
- **Output completeness:** generated `.hsd` is **fully GUI-editable** — both the model
  (`graph_manager`) and the UI mirror (`graph_tabs_widget`) are emitted consistently, so
  files open cleanly in the Hesiod GUI and pass `scripts/check_hsd_file_consistency.py`.
- **Authoring:** the LLM/dev writes a compact spec listing node types, only-changed
  params, wires, and export targets. The builder fills everything else.
- **Location:** `scripts/hsd/` (a Python package alongside existing repo tooling).
- **Spec format:** **JSON only** for now (no new dependencies). YAML may be added later.
- **Dependencies:** **stdlib only** (`json`, `argparse`, `pathlib`, `subprocess`,
  `dataclasses`). The model↔UI consistency comparison is implemented in pure stdlib
  (no `deepdiff`, which is not available in base Python).

## Ground truth (verified against the codebase)

### Serialization is attribute-model driven (per Otto Link, project author)

Hesiod's `.hsd` serialization is based on a **generic attribute model**: the param
objects in the file are emitted automatically from the node definitions. Consequences
this design relies on:

- **The catalog self-syncs.** `hesiod --inventory` dumps from the same live attribute
  model, so a regenerated `node_catalog.json` always matches current node definitions.
  `hsd build-catalog` is the only step needed after upstream node changes.
- **The deserializer is tolerant.** Reading a file from a previous version with
  missing / changed / deprecated fields is handled gracefully (defaults filled, unknown
  data ignored). Therefore the builder need not reproduce every field perfectly — it
  emits what the catalog provides and relies on Hesiod to normalize the rest — and
  generated files are robust across Hesiod versions.
- **Round-trip load is the ultimate correctness check.** Building a file, loading it in
  Hesiod (batch or open), and confirming it is accepted/normalized validates more than
  any static schema can. Used as the binary-gated L3 check.

### `.hsd` file structure (JSON)

Top level: `Hesiod version`, `saved_at`, `graph_manager`, `graph_manager_widget`,
`graph_tabs_widget`.

- `graph_manager.export_param`: `export_path`, `ids` (list of `[graph_id, node_id,
  port_id]` whose outputs are flattened/exported), `shape.x/y`, `tiling.x/y`, `overlap`.
- `graph_manager.graph_nodes.<graph_id>`: `id`, `id_count`, `model_config`
  (`shape.x/y`, `tiling.x/y`, `overlap`, `hmap_transform_mode_cpu/gpu`), `links[]`, `nodes[]`.
  - **model link:** `{node_id_from, node_id_to, port_id_from, port_id_to}`.
  - **model node:** `{id, label (= node type), <param_name>: {type, type_string, value, …}}`.
- `graph_manager_widget.frames.<graph_id>.current_bg_tag` (e.g. `"NONE"`).
- `graph_tabs_widget.graph_node_widgets.<graph_id>`: `id`, `comments`, `groups`,
  `current_link_type`, `links[]`, `nodes[]`.
  - **UI link:** `{link_type, node_out_id, port_out_id, node_in_id, port_in_id}`.
  - **UI node:** `{caption (= node type), id, is_widget_visible, scene_position.x,
    scene_position.y}`.

Node ids are strings (e.g. `"5"`, `"13"`). `id_count` is the next free numeric id.

### Headless execution (verified in `Hesiod/src/cli/batch_mode.cpp`)

```
build/bin/hesiod --batch=FILE.hsd [--shape=W,H] [--tiling=X,Y] [--overlap=R]
```

Loads the graph; if `export_param.export_path` is non-empty, flattens the selected
`ids` outputs and writes two files:
- `<export_path>.png` — 16-bit grayscale raw heightmap (`CV_16U`).
- `<export_path>_preview.png` — TERRAIN colormap with hillshading.

CLI shape/tiling/overlap override the file's config for the whole graph. Other modes:
`--inventory` (dumps node inventory + `node_documentation_stub.json` + settings
screenshots) and `--snapshot` (renders example screenshots).

### Node catalog source of truth

- `Hesiod/data/node_documentation.json` (298 nodes): per node `category`, `description`,
  `label`, `parameters` (name → `{type, label, description, …}`), `ports` (name →
  `{caption, data_type, description, type: input|output}`).
- **Full default parameter *value objects*** (`{type, type_string, value, vmin, vmax,
  …}`) are produced by `hesiod --inventory` → `node_documentation_stub.json` (the
  attribute-model dump). The builder merges both into `node_catalog.json`.

### Data-model constraint (hard rule)

A wire is valid only if `out.data_type == in.data_type`. Port data types are one of:
`Array, Cloud, Path, VirtualArray, VirtualTexture, vector<float>`. `VirtualArray`
(heightmap/scalar) and `VirtualTexture` (RGBA colour) are **incompatible**.
`ColorizeGradient` / `ColorizeSolid` convert `VirtualArray` → `VirtualTexture`, so they
feed `ExportTexture`, not `ExportHeightmap`. A heightmap+colour export therefore **forks**.

## Architecture

```
spec.json  ──compile──▶  Graph (builder)  ──to_hsd──▶  .hsd (model + UI mirror)
    ▲                        │                              │
LLM / dev code               ▼                             validate (L1/L2)
                       node catalog                         │
                  (defaults + ports/types)                  ▼
                                              run (hesiod --batch) ──▶ .png + _preview.png
```

### Package layout — `scripts/hsd/`

| Module | Responsibility |
|---|---|
| `catalog.py` | Load/query `node_catalog.json`; expose per-type params (with default value objects), ports (name + `data_type` + direction), category, description. |
| `node_catalog.json` | Committed catalog. Generated by merging `--inventory` stub (default param value objects) with `node_documentation.json` (ports/types/categories/descriptions). |
| `spec.py` | Parse + schema-shape the declarative JSON spec into typed objects. |
| `builder.py` | `Graph` class: `add_node`, `link`, `set_export`, `to_hsd()`. Fills default params, assigns numeric-string ids, builds model + consistent UI mirror, sets `export_param`, `model_config`, version, `saved_at`. |
| `layout.py` | Topological layered positions for `scene_position.x/y`. Deterministic. |
| `validate.py` | L1 (schema), L2 (graph + port-datatype), L3 (binary round-trip/smoke-run); `lint` for existing files incl. model↔UI consistency. |
| `run.py` | Locate binary (`HESIOD_BIN` env, else `build/bin/hesiod`), invoke `--batch`, capture logs, return output PNG paths. |
| `compile.py` | spec → `Graph` (drives builder); `compile_spec()` entry. |
| `cli.py` / `__main__.py` | `hsd` subcommands. |
| `catalog_build.py` | Regenerate `node_catalog.json` from binary + JSON. |

Invocation: `python3 -m scripts.hsd <subcommand>` (add `scripts/__init__.py`), with an
optional thin `scripts/hsd/hsd` wrapper for ergonomics.

### Declarative spec (JSON)

```json
{
  "config": {"shape": [1024, 1024], "tiling": [4, 4], "overlap": 0.5},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
    {"id": "ero",   "type": "HydraulicParticle"},
    {"id": "col",   "type": "ColorizeGradient"}
  ],
  "links": [
    ["noise.output", "ero.input"],
    ["ero.output", "col.input"]
  ],
  "export": [
    {"node": "ero", "port": "output", "path": "terrain.png"}
  ]
}
```

- `id`: friendly string chosen by the author; the builder maps each to a numeric-string
  Hesiod id and keeps the name→id map for links/export and error messages.
- `params`: only values that differ from defaults; everything else is filled from catalog.
- `links`: `"<nodeId>.<port>"` pairs (from → to).
- `export`: each entry adds a `[graph, nodeId, port]` triple to `export_param.ids`. If any
  export entry is present, `export_path` is set so batch mode writes output.

### CLI subcommands

| Command | Behaviour |
|---|---|
| `hsd nodes [--search T] [--category C] [--show TYPE]` | Query catalog. `--show` prints a node's ports (with data_type) + params (with defaults). For LLM authoring. |
| `hsd build SPEC.json -o OUT.hsd` | Compile + L1/L2 validate; write `.hsd`. |
| `hsd validate FILE.hsd` | L1/L2 on a generated/spec-built file. |
| `hsd lint FILE.hsd` | Validate an existing/hand-edited file: L1/L2 + model↔UI consistency. |
| `hsd run FILE.hsd [--shape … --tiling … --overlap …]` | Execute via batch; print output PNG paths. |
| `hsd make SPEC.json -o OUT.hsd [--run]` | build (+validate) then optionally run. |
| `hsd build-catalog` | Regenerate `node_catalog.json` from binary `--inventory` + `node_documentation.json`. |

### Data flow

`spec → compile_spec → Graph(defaults + ids + layout) → validate L1/L2 → write .hsd
→ run (execute) → terrain.png + terrain_preview.png`.

## Validation (layered, structured errors)

Each error is `{level, node_id?, link?, problem, suggestion}` so an LLM can self-correct.

- **L1 — schema:** node `type` exists in catalog; each `params` key is a real param of
  that type; provided value shape matches the param `type`.
- **L2 — graph:** every link endpoint resolves to an existing `node.port`; **port
  `data_type` compatibility** (`out == in`); export targets resolve to a real output
  port; warn on disconnected required inputs where determinable.
- **L3 — round-trip / smoke run (binary-gated, opt-in):** run `--batch` on a tiny
  override (`--shape=64,64 --tiling=1,1`), assert clean exit and that the export PNG
  appears. This exercises Hesiod's tolerant deserializer end-to-end — the strongest
  correctness signal.
- **lint extras:** model↔UI consistency — same nodes (id, type) and same links on both
  sides — implemented in pure stdlib (normalize each side to sorted comparable structures
  and compare), matching what `scripts/check_hsd_file_consistency.py` checks via deepdiff.

## Error handling

- Unknown node type / param → L1 error naming the offending id and the closest valid
  candidates from the catalog.
- Datatype-incompatible link → L2 error stating both port data types and that
  `Colorize*` is the VirtualArray→VirtualTexture bridge.
- Missing catalog file → message: run `hsd build-catalog`.
- Missing binary on `run`/L3 → clear message; static commands still work.
- Runner non-zero exit / hesiod-logged error → surfaced verbatim.

## Testing (pytest; repo already uses it)

- **Builder consistency:** compiled file passes the in-test model↔UI consistency check.
- **Validator:** catches unknown node type, unknown param, datatype-incompatible link,
  dangling export target; passes a known-good spec.
- **Layout determinism:** same spec → identical positions across runs.
- **Golden compile:** a fixture spec → expected `.hsd` (compare ignoring `saved_at`).
- **Catalog:** loads; every node has ports with `data_type`; params have default objects.
- **Integration / round-trip (binary-gated, `skipif` no binary):** `hsd make` a 2-node
  noise→export graph at `--shape=64,64`; assert the PNG + `_preview.png` exist.

## Out of scope (this phase) — roadmap

- **Skill (C):** `.claude/skills/hesiod-generate/` — query catalog → write spec →
  `hsd make --run` → inspect preview PNG → iterate; reference recipes (noise → erosion →
  colorize/export fork) and the data-type rules. Two flows: doc-example generation and
  procedural batch / large-map generation.
- **MCP:** wrap the same CLI subcommands as tools.
- **Manual page (A):** `docs/user_manual/automating_generation/` — `.hsd` format, batch
  CLI, catalog as source of truth, the `hsd` toolkit + worked example.

## Open questions / assumptions

- Assumes `hesiod --inventory` emits full default parameter value objects in
  `node_documentation_stub.json`. To be confirmed when building the catalog; if a param
  lacks a default, Hesiod's tolerant deserializer fills it on load, and we can also mine
  fallbacks from the model node shapes in `site/examples/*.hsd`.
- Assumes arbitrary numeric-string ids are accepted on load (examples use them); the
  builder allocates fresh sequential ids and sets `id_count` accordingly.
- Catalog is committed so static commands work without the binary; regenerated when nodes
  change via `hsd build-catalog`.
