# hsd Enum-String Resolution (5b "top win") — Spec & Plan

**Goal:** Let spec authors write enum/choice params as human-readable strings
(`"blending_method": "maximum"`) instead of hand-written value-object dicts. The compiler
resolves the string to the exact integer Hesiod serializes, using a committed,
self-syncing `enum_catalog.json`.

**Branch:** `worktree-hsd-toolkit-foundation`. Stdlib only. Run pytest via
`/home/barrulus/dev/Hesiod/.venv-docs/bin/pytest`. No `Co-Authored-By` footer.

## Proven foundation (already validated — do not re-derive)
- `Enumeration` deserialization (`external/Attributes/.../enum_attribute.cpp`) reads the
  integer `value` and the node computes with it (`get_value()`), NOT the `choice` string.
  So we MUST emit the correct integer `value` (plus `choice` for display).
- Reliable string→int requires three sources, all present in the repo:
  1. `Hesiod/include/hesiod/app/enum_mappings.hpp` — 24 `<name>_map` literals mapping
     `choice string → C++ symbol` (e.g. `{"maximum", BlendingMethod::MAXIMUM}`,
     `{"OpenSimplex2", hmap::NoiseType::SIMPLEX2}`).
  2. `scripts/hsd/data/highmap_enum_values.json` (committed) — integer value of every
     HighMap enumerator (e.g. `NoiseType.SIMPLEX2 = 4`), plus the 3 Hesiod-local enums
     (`BlendingMethod`, `ExportFormat`, `MaskCombineMethod`) under `_HesiodLocal`.
  3. Node sources `Hesiod/src/model/nodes/**/*.cpp` — each enum param is wired with
     `node.add_attr<EnumAttribute>(KEY, "label", enum_mappings.<map_name>, [default])`,
     giving the `(node_type, param) → map` association needed to disambiguate (e.g. `"maximum"`
     is `3` via `blending_method_map`/`BlendingMethod` but `1` via
     `stamping_blend_method_map`/`StampingBlendMethod`).
- Validation already run: parsing `enum_mappings.hpp` (regex must allow DIGITS in symbols,
  e.g. `SIMPLEX2`, `F1_SQUARED`) + applying the int tables reproduces **118/123** mined
  ground-truth `(choice→value)` pairs with **0 mismatches**; the 5 remaining are one enum
  (`method`: "Latin Hypercube Sampling"/"Halton sequence") NOT in `enum_mappings.hpp`.

## File structure
- Use (committed): `scripts/hsd/data/highmap_enum_values.json` (already added).
- Create: `scripts/hsd/enum_catalog_build.py` — generator → `scripts/hsd/data/enum_catalog.json`.
- Create (generated, committed): `scripts/hsd/data/enum_catalog.json`.
- Create: `scripts/hsd/enums.py` — loader/resolver over `enum_catalog.json`.
- Modify: `scripts/hsd/params.py` — resolve enum strings via `enums.py`.
- Modify: `scripts/hsd/compile.py` — pass node_type + param name into value-object building.
- Modify: `scripts/hsd/validate.py` — validate enum choice strings (L1).
- Modify: `scripts/hsd/cli.py` — `nodes --show` prints enum choices.
- Tests: `tests/hsd/test_enums.py`, additions to `test_params.py`/`test_compile.py`/`test_validate.py`.

---

## Task E1: enum_catalog generator + committed catalog

**Files:** create `scripts/hsd/enum_catalog_build.py`; generate `scripts/hsd/data/enum_catalog.json`.

The generator (pure stdlib: `json`, `re`, `pathlib`) produces:
```json
{
  "maps": { "blending_method_map": {"add": 0, "maximum": 3, ...}, ... },
  "node_param": { "Blend": {"blending_method": "blending_method_map"},
                  "Stamping": {"blend_method": "stamping_blend_method_map"}, ... }
}
```

Algorithm:
1. Load int tables from `scripts/hsd/data/highmap_enum_values.json`: flatten HighMap enums
   and `_HesiodLocal.*` into one `{EnumTypeName: {ENUMERATOR: int}}` dict (ignore keys
   beginning with `_`).
2. Parse `Hesiod/include/hesiod/app/enum_mappings.hpp`: for each `<name>_map[...]= { ... }`
   literal, extract `{ "choice", Sym::BOL }` entries with regex that allows **digits** in
   the symbol: `\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*([A-Za-z0-9_:]+)\s*\}`. For each, take the
   last two `::`-segments as `(EnumType, ENUMERATOR)`; resolve int from the tables. Build
   `maps[map_name][choice] = int`. If a symbol can't be resolved, record it in a
   `_unresolved` list (should be empty — fail loudly if not).
3. Parse node sources `Hesiod/src/model/nodes/**/*.cpp`: for each file, resolve
   `constexpr const char *A_XXX = "yyy";` constants, then find every
   `add_attr<EnumAttribute>( KEY , "label", enum_mappings.<map_name> ...)` (KEY may be a
   string literal or an `A_XXX` constant; the call may span multiple lines). Map the
   setup function to its node type(s): a function `setup_<snake>_node` corresponds to the
   CamelCase node type whose snake_case equals `<snake>`. Use the 298 node-type names from
   `Hesiod/data/node_documentation.json` to build the camel→snake lookup. Record
   `node_param[node_type][param_key] = map_name`. Calls that reference a map NOT in
   `enum_mappings` (e.g. a node-local map or `hmap::..._as_string`) are skipped (those
   params stay dict-only).
4. Write `scripts/hsd/data/enum_catalog.json` (sorted keys, indent 2).

**Acceptance (run and paste):** after generating, run a validation that re-derives the
mined ground-truth: for every `(node_type, param, choice, value)` Enumeration triple in
`Hesiod/data/examples/*.hsd` + `bootstraps/*.hsd` + `default.hsd` where the catalog has
`node_param[node_type][param]`, assert `maps[that_map][choice] == value`. Require **0
mismatches**. Report how many triples were covered vs. skipped (skipped = node/param not
in catalog, e.g. the `method` sampling enum). Also assert `maps` has 24 entries and no
`_unresolved` symbols.

**Commit:** `git add scripts/hsd/enum_catalog_build.py scripts/hsd/data/enum_catalog.json scripts/hsd/data/highmap_enum_values.json && git commit -m "feat(hsd): generate enum catalog (maps + node-param associations) from sources"`

---

## Task E2: enums loader/resolver + tests

**Files:** create `scripts/hsd/enums.py`; create `tests/hsd/test_enums.py`.

`enums.py` API:
```python
class EnumCatalog:
    @classmethod
    def load(cls, path=None): ...        # default: scripts/hsd/data/enum_catalog.json
    def map_for(self, node_type, param): ...   # -> map_name or None
    def choices(self, node_type, param): ...   # -> sorted list of choice strings, or None
    def resolve(self, node_type, param, choice): ...
        # -> int value; raises EnumError(listing valid choices) if choice invalid;
        #    returns None if (node_type,param) not in catalog (caller falls back to dict)
```
Tests (TDD): `resolve("Blend","blending_method","maximum") == 3`;
`resolve("Stamping","blend_method","maximum") == 1` (disambiguation proof);
`resolve("NoiseFbm","noise_type","OpenSimplex2") == 4`; unknown choice raises `EnumError`
mentioning a valid choice; `(node,param)` not in catalog → `map_for` None.

**Commit:** `feat(hsd): EnumCatalog loader/resolver`

---

## Task E3: integrate into compile + params + validate

**Files:** modify `scripts/hsd/params.py`, `scripts/hsd/compile.py`, `scripts/hsd/validate.py`; extend tests.

- `params.py`: add `value_object(name, type_string, value, *, node_type=None, enum_catalog=None)`.
  For `type_string == "Enumeration"` and a STRING value: resolve via
  `enum_catalog.resolve(node_type, name, value)` → emit
  `{"label": name, "type": 4, "type_string": "Enumeration", "choice": value, "value": <int>}`.
  If catalog has no entry (resolve→None) keep current behaviour (dict passthrough required →
  raise `ParamError` instructing a full dict). Dict values still pass through unchanged.
  For `type_string == "Choice"` and a string value: emit
  `{"label": name, "type": 1, "type_string": "Choice", "value": value}` (the Choice
  attribute stores the string directly; verify against an example .hsd round-trip).
- `compile.py`: load the `EnumCatalog` once; pass `node_type=node.type` and the catalog into
  `value_object` for each param.
- `validate.py` (L1): for an Enumeration param given a string, if the catalog has the
  `(node_type,param)` and the string isn't a valid choice → structured error listing valid
  choices. If value is a dict or the param isn't catalogued, skip (no false positives).
- Tests: a spec using `{"blending_method": "maximum"}` on `Blend` compiles to a node whose
  `blending_method.value == 3` and `.choice == "maximum"`; an invalid choice fails validation
  with the valid list; existing dict-passthrough still works.

**Round-trip proof (binary-gated):** compile a small `Blend`-using spec with a string enum,
render at 64×64 with the binary; assert exit 0 + PNG written (proves the emitted integer is
accepted and computed). Use a wiring known to be valid (e.g. two `NoiseFbm` → `Blend` →
`ExportHeightmap`); confirm exact ports with `hsd nodes --show Blend` first.

**Commit:** `feat(hsd): resolve enum/choice params from strings in the compiler`

---

## Task E4: `nodes --show` prints enum choices + docs

**Files:** modify `scripts/hsd/cli.py`; update `.claude/skills/hesiod-generate/reference/spec-schema.md` and `SKILL.md`.

- `cli.py` `nodes --show TYPE`: for each Enumeration/Choice param, if the catalog knows its
  choices, print them (e.g. `blending_method (Enumeration): add | maximum | multiply | ...`).
- Skill docs: state that enum/choice params accept plain strings now; show the
  `blending_method: "maximum"` example; note `hsd nodes --show TYPE` lists valid choices;
  keep the dict escape hatch documented for the few uncatalogued enums (e.g. sampling
  `method`).

**Commit:** `feat(hsd): nodes --show lists enum choices; document enum-string params`

---

## Task V: final verification
- `/home/barrulus/dev/Hesiod/.venv-docs/bin/pytest tests/ -q` → green.
- `enum_catalog_build.py` acceptance check → 0 mismatches.
- `mkdocs build --strict` clean (docs changes).
- Spot-check: `hsd nodes --show Blend` lists blending choices; a string-enum spec validates
  + (binary) round-trips.

## Self-review checklist
- 0 mismatches vs mined ground truth; 24 maps; no `_unresolved` symbols.
- Disambiguation correct (Blend vs Stamping "maximum").
- Uncatalogued enums (e.g. sampling `method`) fall back to dict with a clear message — no
  silent wrong value.
- Dict passthrough and all prior tests still pass.
