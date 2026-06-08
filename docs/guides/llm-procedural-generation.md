# Driving Procedural Generation with an LLM

An LLM can describe terrain in natural language, translate that description into a
compact JSON spec, validate the spec against the real Hesiod node catalog, render it
headlessly, inspect the preview PNG, and self-correct from structured errors — all in a
tight loop. This page explains the pattern and how the `hesiod-generate` skill encodes it.

For the full CLI reference and spec format, see [Automation & Batch / Headless Operation](batch-headless.md).

---

## The idea

The workflow is a feedback loop with four steps:

1. **Describe** — state what you want in plain language: elevation profile, erosion style,
   noise character, whether it tiles, target resolution.

2. **Spec** — the LLM writes a compact JSON spec: a list of nodes, their parameters, and
   the links between them. The spec stays small and human-readable even for complex graphs.

3. **Validate** — run `hsd validate` before touching the renderer. The validator checks
   every node type, every port name, and every link's data-type compatibility against the
   real Hesiod node catalog. It prints `ok` or a structured error that names the offending
   node and suggests a fix.

4. **Render and inspect** — `hsd make --run` compiles the spec to a `.hsd` file and drives
   the Hesiod binary headlessly. On success it writes a 16-bit heightmap and a colourised
   preview PNG. Open the preview, judge the result, and feed your observations back to the
   LLM to guide the next iteration.

Because validation errors are structured (node name, port name, available alternatives),
the LLM can parse them and self-correct without human intervention. A broken graph rarely
needs more than one correction pass before it validates cleanly.

---

## The `hesiod-generate` skill

The `hesiod-generate` Claude Code skill lives in `.claude/skills/hesiod-generate/` in
this repository. When you open this repo in Claude Code, the skill is auto-discovered and
available immediately — no configuration needed.

The skill encodes:

- **The full workflow** — node discovery, spec authoring, validate → make → inspect,
  iterative refinement.
- **The port data-type rule** — `VirtualArray` (heightmap/scalar) and `VirtualTexture`
  (RGBA) are incompatible; the skill knows the bridge nodes (`ColorizeGradient`,
  `ColorizeSolid`) and confirmed port names for every export node.
- **Verified recipes** — ready-to-run specs for common patterns (minimal heightmap export,
  colour fork, 4096 × 4096 tiled map) in `.claude/skills/hesiod-generate/reference/specs/`.

Invoke it by asking Claude to generate or refine a terrain graph. Claude will validate
before rendering and show you the structured error output if validation fails.

---

## Procedural and large-map patterns

### Seed sweeps

To explore variation, loop over seeds and produce one output per seed. The LLM writes a
parameterised spec template; a shell loop or short Python script substitutes the seed
value, writes a per-seed spec, validates it, and runs it. Each output goes to a distinct
path. The preview PNGs let you pick the most interesting variant quickly.

See the seed-sweep examples in the skill's `reference/recipes.md` and in
[Automation & Batch / Headless Operation](batch-headless.md).

### Scaling resolution

Work at a low resolution (256 × 256 or 512 × 512) while iterating on the graph, then
scale up. The `--shape`, `--tiling`, and `--overlap` flags on `hsd make` and `hsd run`
override the **compute** config (the resolution each graph node processes at), which is
useful for fast iteration. However, the **exported PNG resolution** is set by `config.shape`
baked into the `.hsd` at build time. To change the output file's dimensions, update
`config.shape` in the spec JSON and rebuild:

```bash
# prototype quickly at low compute resolution
hsd make terrain.json -o terrain.hsd --run --shape 512,512

# scale up for production: update config.shape in the spec, then rebuild
# (--shape only affects compute resolution, not the exported file dimensions)
hsd make terrain_4k.json -o terrain.hsd --run \
  --tiling 4,4 --overlap 0.25
```

The `--tiling` flag splits the logical map into a grid of tiles rendered separately;
`--overlap` sets the blending margin (0.25 works well for large maps). See
[Automation & Batch / Headless Operation](batch-headless.md) for flag semantics.

---

## Why this works — guardrails

The loop is robust because every spec passes through a validator that consults the actual
Hesiod node catalog before anything renders:

- **Unknown node types** are caught immediately — the LLM cannot invent a node name.
- **Wrong port names** are caught with a hint listing the available ports for that node type.
- **Type mismatches** (`VirtualArray ↔ VirtualTexture`) are caught at the link level.
- **Missing `export` block** — the validator doesn't catch this as an error, but the
  `hsd make --run` output will produce no files, making the omission obvious.

The Hesiod binary's tolerant loader provides a second check at load time: it accepts
omitted parameters (defaulting silently) but rejects structurally invalid graphs. Between
`hsd validate` and the loader, a spec that reaches the renderer is almost always correct.

---

## Next steps

- **Full CLI and spec reference** — [Automation & Batch / Headless Operation](batch-headless.md)
  covers every subcommand, the spec JSON format, export configuration, headless display
  setup, and worked examples.
- **Tiling concepts** — [Tiling and Overlap](../core_concepts/tiling-overlap.md) explains
  how tile boundaries are blended and what the overlap ratio controls.
- **The skill** — `.claude/skills/hesiod-generate/SKILL.md` has the complete workflow,
  the port data-type rule, all verified recipes, and the Python API for tighter loops.
