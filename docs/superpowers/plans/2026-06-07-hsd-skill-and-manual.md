# hsd Skill (C) + Automation Manual (A) — Spec & Plan

> **For agentic workers:** execute task-by-task. Verify every node/port/param claim against
> `Hesiod/data/node_documentation.json`; verify every recipe spec against the live `hsd`
> toolkit (`hsd validate`, and `hsd make --run` with the binary). Nothing invented.

**Goal:** Ship the LLM-facing skill (C, scoped to game-dev procedural/large-map generation)
and the automation manual page (A), both building on the `hsd` toolkit foundation already
on this branch.

**Branch context:** `worktree-hsd-toolkit-foundation` (based on `upstream/dev`). The
`docs/guides/` scaffold + "Guides" nav live only on the sibling `docs-topic-guides`
branch; this branch must create `docs/guides/` and a minimal "Guides" nav block. **Merge
note:** `docs/guides/batch-headless.md` and the "Guides" nav section will need a union
merge with `docs-topic-guides` — our full content supersedes their stub.

## Environment facts (verified)
- Toolkit CLI: `PYTHONPATH=scripts python3 -m hsd <nodes|build|validate|lint|run|make> ...`
  (run from the repo root). Stdlib only.
- Binary: `HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod`, run with
  `QT_QPA_PLATFORM=offscreen` for headless. Round-trip confirmed working.
- Docs: MkDocs Material. Build check: `.venv-docs/bin/mkdocs build --strict` (run from repo
  root; the venv lives in the MAIN checkout `/home/barrulus/dev/Hesiod/.venv-docs`).
- Source of truth for nodes: `Hesiod/data/node_documentation.json` (298 nodes).
- Port-datatype rule: a link is valid only if out `data_type` == in `data_type`;
  `VirtualArray` (heightmap) ≠ `VirtualTexture` (colour); `ColorizeGradient`/`ColorizeSolid`
  convert VirtualArray→VirtualTexture (heightmap+colour export forks).
- Verified-valid building blocks: `NoiseFbm.output` (VirtualArray) → `ExportHeightmap.input`;
  `NoiseFbm → HydraulicParticle → ColorizeGradient → ExportTexture` (colour fork);
  `HydraulicParticle.output` (VirtualArray) → `ExportHeightmap`.

## File structure
- Create: `.claude/skills/hesiod-generate/SKILL.md`
- Create: `.claude/skills/hesiod-generate/reference/recipes.md`
- Create: `.claude/skills/hesiod-generate/reference/spec-schema.md`
- Create: `.claude/skills/hesiod-generate/reference/specs/*.json` (verified recipe specs)
- Create: `docs/guides/batch-headless.md` (expanded automation & batch reference)
- Create: `docs/guides/llm-procedural-generation.md` (game-dev LLM narrative)
- Modify: `mkdocs.yml` (add minimal "Guides" nav block)

---

## Task A1: Verified recipe specs (foundation for the skill)

Create 3 recipe spec JSON files under `.claude/skills/hesiod-generate/reference/specs/`
and verify each with the live toolkit. These are the single source the skill recipes embed.

**Files & content:**

`heightmap_export.json` — minimal heightmap:
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

`colour_fork.json` — heightmap + colour fork (uses the Colorize bridge):
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

`tiled_large.json` — large tiled render (same topology as heightmap_export, different config):
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

**Verification (MUST run; fix specs if any fail — do NOT ship an unverified recipe):**
For each file:
```
PYTHONPATH=scripts python3 -m hsd validate .claude/skills/hesiod-generate/reference/specs/<f>.json
```
Then a smoke round-trip at small size (proves the topology runs end-to-end):
```
HESIOD_BIN=/home/barrulus/dev/Hesiod/build/bin/hesiod QT_QPA_PLATFORM=offscreen \
PYTHONPATH=scripts python3 -m hsd make .claude/skills/hesiod-generate/reference/specs/heightmap_export.json \
  -o /tmp/r.hsd --run --shape 128,128 --tiling 1,1
```
Confirm exit 0 and that the PNG + `_preview.png` are written. (Validate all three; round-trip
at least `heightmap_export.json` and `colour_fork.json`.)

**Important:** verify `ExportTexture` exists and its input port is `VirtualTexture`, and
`ColorizeGradient` output is `VirtualTexture`, via
`PYTHONPATH=scripts python3 -m hsd nodes --show ExportTexture` and `--show ColorizeGradient`.
If `ExportTexture`/port names differ, correct `colour_fork.json` to the real names before
shipping. If a node/port doesn't exist as written, FIX it — never ship an invented name.

**Commit:**
```bash
git add .claude/skills/hesiod-generate/reference/specs/
git commit -m "feat(skill): verified hsd recipe specs for procedural generation"
```

---

## Task C1: SKILL.md

Create `.claude/skills/hesiod-generate/SKILL.md`. Frontmatter (YAML):
```yaml
---
name: hesiod-generate
description: Generate Hesiod procedural terrain (.hsd) graphs and large maps programmatically via the hsd toolkit. Use when a developer wants to author, validate, and batch-render terrain heightmaps/textures from code or natural language — especially procedural or large-map generation (seed sweeps, big shapes, tiling).
---
```

Body MUST cover, concisely and accurately:
1. **What this is** — the `hsd` toolkit compiles a compact JSON spec into a valid Hesiod
   `.hsd` graph and runs it headlessly to PNGs. Game-dev / procedural focus.
2. **Prerequisites** — run from repo root; `PYTHONPATH=scripts python3 -m hsd ...`; the
   Hesiod binary for rendering (`HESIOD_BIN` or `build/bin/hesiod`), `QT_QPA_PLATFORM=offscreen`
   for headless.
3. **Workflow** (numbered): discover nodes (`hsd nodes --search T` / `--show TYPE`) → write a
   JSON spec → `hsd validate spec.json` → fix structured errors → `hsd make spec.json -o
   out.hsd --run` → inspect `*_preview.png` → iterate.
4. **The hard rule** — port `data_type` compatibility; VirtualArray vs VirtualTexture; the
   Colorize bridge + export fork. Tell the model to run `hsd nodes --show` to check ports
   before wiring.
5. **Spec format** — point to `reference/spec-schema.md`.
6. **Recipes** — point to `reference/recipes.md`.
7. **Procedural / large-map generation** — generate many specs in a loop (vary `seed`,
   `kw`); render large with `--shape`/`--tiling`/`--overlap`; note tiling for big maps and
   that overlap hides seams. Show a short Python loop sketch using the library
   (`from hsd.spec import Spec; from hsd.compile import compile_spec`) OR repeated
   `hsd make` calls.
8. **Validation discipline** — always `hsd validate` before `run`; structured errors name the
   node and suggest fixes.

Keep it skimmable (headings, short steps). Verify every command actually works by running it.

**Commit:**
```bash
git add .claude/skills/hesiod-generate/SKILL.md
git commit -m "feat(skill): hesiod-generate SKILL.md (procedural generation workflow)"
```

---

## Task C2: reference/spec-schema.md + reference/recipes.md

`reference/spec-schema.md` — document the spec format precisely (match `scripts/hsd/spec.py`):
- Top-level keys: `config` (`shape [w,h]`, `tiling [x,y]`, `overlap` float), `nodes`
  (`id`, `type`, optional `params`), `links` (array of `["fromNode.fromPort",
  "toNode.toPort"]`), `export` (array of `{node, port, path}`).
- `params`: only changed values; scalar types covered by TYPE_MAP (Float, Integer, Bool,
  Random seed number, Wavenumber `[x,y]`, Vec2Float `[x,y]`, Value range `[lo,hi]`, Choice,
  String, Color `[r,g,b,a]`); advanced types take a full value object (dict) — point to
  `hsd nodes --show TYPE`.
- Catalog query cheatsheet (`hsd nodes`, `--search`, `--category`, `--show`).
- Note: omitted params use Hesiod's node defaults (tolerant loader).

`reference/recipes.md` — embed the three verified specs from Task A1 with a one-paragraph
explanation each (what it produces, when to use), and the exact `hsd make ... --run`
command. Only include specs that passed Task A1 verification. Cross-reference SKILL.md.

**Verify:** re-run `hsd validate` on every spec shown in recipes.md to ensure the embedded
JSON matches the verified files.

**Commit:**
```bash
git add .claude/skills/hesiod-generate/reference/
git commit -m "docs(skill): spec-schema + recipes reference (verified specs)"
```

---

## Task A2: Manual page — docs/guides/batch-headless.md + nav

Create `docs/guides/batch-headless.md` (full content per the design: why headless; built-in
`--batch` CLI with exact flags and outputs; export_param config with cross-links to
`../user_manual/bake_and_export/bake_and_export.md`; the `hsd` toolkit — spec format, the
`nodes/build/validate/lint/run/make` subcommands, a worked noise→erosion→export example with
the real commands and outputs; the port-datatype rule + Colorize fork; links to
`llm-procedural-generation.md` and the skill at `.claude/skills/hesiod-generate/SKILL.md`).

Keep the stub's existing "See also" links (`export-formats.md`, bake_and_export). NOTE:
`export-formats.md` lives on the sibling branch; to avoid a broken-link build failure on THIS
branch, link it as plain text or omit until merge, OR add a short note. Prefer: reference it
but verify `mkdocs build --strict` — if it breaks the build, drop that one link with a TODO
comment for merge reconciliation.

Add a minimal **Guides** nav block to `mkdocs.yml` (place after "Getting Started"):
```yaml
  - 'Guides':
    - 'guides/batch-headless.md'
    - 'guides/llm-procedural-generation.md'
```

**Verify:** `/home/barrulus/dev/Hesiod/.venv-docs/bin/mkdocs build --strict` passes from repo
root. Fix any broken links.

**Commit:**
```bash
git add docs/guides/batch-headless.md mkdocs.yml
git commit -m "docs(guides): automation & batch/headless reference (hsd toolkit)"
```

---

## Task A3: docs/guides/llm-procedural-generation.md

Game-dev-focused narrative: how to drive the toolkit with an LLM for procedural/large-map
generation. Cover: the loop (describe terrain → LLM writes spec → validate → render →
inspect), the `hesiod-generate` skill (what it is, where it lives, link to
`../../.claude/skills/hesiod-generate/SKILL.md`), procedural patterns (seed sweeps, large
tiled renders), and a pointer back to `batch-headless.md` for the CLI/spec details. Single-
source the operational detail (link, don't duplicate).

**Verify:** `mkdocs build --strict` passes; links resolve.

**Commit:**
```bash
git add docs/guides/llm-procedural-generation.md
git commit -m "docs(guides): LLM-driven procedural generation guide"
```

---

## Task V1: Final verification
- Run full toolkit suite: `/home/barrulus/dev/Hesiod/.venv-docs/bin/pytest tests/ -q` → green.
- `mkdocs build --strict` → clean.
- Re-validate every recipe spec: `hsd validate` on each → no errors.
- Offer the user a local `mkdocs serve` preview before any push (project rule).

## Self-review checklist
- Every node/port/param named in skill + docs exists in `node_documentation.json`.
- Every recipe spec passed `hsd validate` (and round-trip where run).
- No invented CLI flags — match `scripts/hsd/cli.py`.
- mkdocs builds strict; no broken internal links (cross-branch links handled).
- Skill scoped to game-dev procedural generation (doc-example flow intentionally deferred).
