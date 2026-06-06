# hsd Toolkit (Foundation) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a stdlib-only Python toolkit (`scripts/hsd/`) that compiles a compact JSON spec into a full, GUI-editable Hesiod `.hsd` graph file, validates it against the node catalog, and runs it headlessly to produce terrain PNGs.

**Architecture:** A declarative JSON spec is parsed (`spec.py`), compiled (`compile.py`) by a builder library (`builder.py` + `params.py` + `layout.py`) into a `.hsd` dict containing both the model (`graph_manager`) and a consistent UI mirror (`graph_tabs_widget`), validated layer-by-layer (`validate.py`) against a catalog loaded from `Hesiod/data/node_documentation.json` (`catalog.py`), and executed via `hesiod --batch` (`run.py`). A `hsd` CLI (`cli.py`) ties it together.

**Tech Stack:** Python 3 standard library only (`json`, `argparse`, `pathlib`, `subprocess`, `dataclasses`). Tests with pytest (`.venv-docs/bin/pytest`, already installed). No new dependencies.

---

## Key facts (verified against the codebase — do not re-derive)

- **`.hsd` is JSON.** Top level: `Hesiod version`, `saved_at`, `graph_manager`, `graph_manager_widget`, `graph_tabs_widget`.
- **Model node** (`graph_manager.graph_nodes.graph.nodes[]`): `{id, label, <param_name>: <value_object>}`. Node ids are strings (`"1"`, `"2"`, …). `id_count` = next free id.
- **Model link**: `{node_id_from, node_id_to, port_id_from, port_id_to}`.
- **UI node** (`graph_tabs_widget.graph_node_widgets.graph.nodes[]`): `{caption, id, is_widget_visible, "scene_position.x", "scene_position.y"}`. `caption` == model `label`.
- **UI link**: `{link_type, node_out_id, port_out_id, node_in_id, port_in_id}`. `link_type` is an int (use `2`, the value seen in example files).
- **export_param**: `{export_path, ids: [[graph_id,node_id,port_id], …], "shape.x", "shape.y", "tiling.x", "tiling.y", overlap}`.
- **model_config** (per graph): `{"hmap_transform_mode_cpu":0, "hmap_transform_mode_gpu":2, overlap, "shape.x", "shape.y", "tiling.x", "tiling.y"}`.
- **graph_manager_widget**: `{frames: {graph: {current_bg_tag: "NONE"}}}`.
- **Catalog source**: `Hesiod/data/node_documentation.json` — per node `category`, `description`, `label`, `parameters` (name → `{type (string), label, description}`), `ports` (name → `{caption, data_type, description, type: "input"|"output"}`).
- **Port datatypes**: `Array, Cloud, Path, VirtualArray, VirtualTexture, vector<float>`. A link is valid only if out `data_type` == in `data_type`. `VirtualArray` ≠ `VirtualTexture`; `ColorizeGradient`/`ColorizeSolid` convert VirtualArray→VirtualTexture.
- **Param type string → (numeric code, type_string)** — mined from `Hesiod/data/examples/*.hsd` (tracked), empirically confirmed:
  | doc `type` string | code | value shape |
  |---|---|---|
  | Float | 6 | number |
  | Integer | 10 | int |
  | Bool | 0 | bool |
  | Random seed number | 12 | int |
  | Wavenumber | 17 | `[x, y]` |
  | Vec2Float | 16 | `[x, y]` |
  | Value range | 11 | `[lo, hi]` (also `is_active: true`) |
  | Choice | 1 | string |
  | String | 13 | string |
  | Color | 2 | `[r,g,b,a]` |
  | Enumeration | 4 | (advanced — pass full value object) |
  | Color gradient | 3 | (advanced — pass full value object) |
  | Cloud | 8 | (advanced — pass full value object) |
  | Path | 9 | (advanced — pass full value object) |
- **Tolerant deserializer (Otto Link):** missing/deprecated param fields are filled on load. Therefore the builder emits **only overridden params**; un-overridden params are omitted and Hesiod uses node defaults. This is the v1 strategy and is confirmed by the round-trip test (Task 11).
- **Headless run:** `build/bin/hesiod --batch=FILE.hsd [--shape=W,H] [--tiling=X,Y] [--overlap=R]`. With a non-empty `export_path`, writes `<path>.png` (16-bit grayscale) and `<path>_preview.png` (TERRAIN colormap + hillshade).
- **Test convention:** existing tests add `scripts/` to `sys.path` then import modules. Run with `.venv-docs/bin/pytest`. The toolkit code uses **system `python3`** (stdlib only).

---

## File Structure

- Create: `scripts/hsd/__init__.py` — package marker + version.
- Create: `scripts/hsd/catalog.py` — load/query `node_documentation.json`; `TYPE_MAP`.
- Create: `scripts/hsd/spec.py` — parse JSON spec → dataclasses.
- Create: `scripts/hsd/params.py` — build param value objects from `TYPE_MAP`.
- Create: `scripts/hsd/layout.py` — deterministic topological node positions.
- Create: `scripts/hsd/builder.py` — `Graph` → model + UI mirror → `.hsd` dict.
- Create: `scripts/hsd/compile.py` — `compile_spec(spec, catalog) → Graph`.
- Create: `scripts/hsd/validate.py` — L1/L2 validation + lint existing files.
- Create: `scripts/hsd/run.py` — locate binary, run `--batch`, return PNG paths.
- Create: `scripts/hsd/cli.py` + `scripts/hsd/__main__.py` — `hsd` CLI.
- Create tests under `tests/hsd/` (one file per module).
- Create: `tests/hsd/fixtures/first_terrain.json` — sample spec used by several tests.

---

## Task 1: Package skeleton + catalog loader

**Files:**
- Create: `scripts/hsd/__init__.py`
- Create: `scripts/hsd/catalog.py`
- Test: `tests/hsd/test_catalog.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_catalog.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog, TYPE_MAP


def test_catalog_loads_default_path():
    cat = Catalog.load()
    assert cat.has_node("NoiseFbm")
    assert not cat.has_node("NoSuchNode")


def test_catalog_ports_have_datatypes():
    cat = Catalog.load()
    out = cat.port("NoiseFbm", "output")
    assert out["type"] == "output"
    assert out["data_type"] == "VirtualArray"


def test_catalog_params_expose_type_strings():
    cat = Catalog.load()
    params = cat.params("NoiseFbm")
    assert params["kw"]["type"] == "Wavenumber"


def test_type_map_has_confirmed_codes():
    assert TYPE_MAP["Float"] == (6, "Float")
    assert TYPE_MAP["Wavenumber"] == (17, "Wavenumber")
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_catalog.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/__init__.py
"""hsd toolkit: compile/validate/run Hesiod .hsd graphs."""
__version__ = "0.1.0"
```

```python
# scripts/hsd/catalog.py
import json
from pathlib import Path

# doc param "type" string -> (numeric type code, type_string), confirmed from
# Hesiod/data/examples/*.hsd. Entries here are safe for scalar override values; types
# NOT listed require a full value object passed through verbatim.
TYPE_MAP = {
    "Float": (6, "Float"),
    "Integer": (10, "Integer"),
    "Bool": (0, "Bool"),
    "Random seed number": (12, "Random seed number"),
    "Wavenumber": (17, "Wavenumber"),
    "Vec2Float": (16, "Vec2Float"),
    "Value range": (11, "Value range"),
    "Choice": (1, "Choice"),
    "String": (13, "String"),
    "Color": (2, "Color"),
}

_DEFAULT_CATALOG = (
    Path(__file__).resolve().parents[2] / "Hesiod" / "data" / "node_documentation.json"
)


class Catalog:
    def __init__(self, data):
        self._data = data

    @classmethod
    def load(cls, path=None):
        path = Path(path) if path else _DEFAULT_CATALOG
        if not path.exists():
            raise FileNotFoundError(
                f"node catalog not found at {path}; expected "
                "Hesiod/data/node_documentation.json"
            )
        with open(path) as f:
            return cls(json.load(f))

    def node_types(self):
        return sorted(self._data)

    def has_node(self, node_type):
        return node_type in self._data

    def category(self, node_type):
        return self._data[node_type].get("category", "")

    def description(self, node_type):
        return self._data[node_type].get("description", "")

    def params(self, node_type):
        return self._data[node_type].get("parameters") or {}

    def ports(self, node_type):
        return self._data[node_type].get("ports") or {}

    def port(self, node_type, port_id):
        return self.ports(node_type).get(port_id)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_catalog.py -v`
Expected: PASS (4 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/__init__.py scripts/hsd/catalog.py tests/hsd/test_catalog.py
git commit -m "feat(hsd): catalog loader over node_documentation.json + TYPE_MAP"
```

---

## Task 2: Spec parser

**Files:**
- Create: `scripts/hsd/spec.py`
- Create: `tests/hsd/fixtures/first_terrain.json`
- Test: `tests/hsd/test_spec.py`

- [ ] **Step 1: Create the fixture spec**

```json
{
  "config": {"shape": [1024, 1024], "tiling": [4, 4], "overlap": 0.5},
  "nodes": [
    {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
    {"id": "ero", "type": "HydraulicParticle"},
    {"id": "col", "type": "ColorizeGradient"}
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

Save as `tests/hsd/fixtures/first_terrain.json`.

- [ ] **Step 2: Write the failing test**

```python
# tests/hsd/test_spec.py
import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.spec import Spec, SpecError

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_parse_fixture():
    spec = Spec.from_file(FIX)
    assert spec.config["shape"] == [1024, 1024]
    assert [n.id for n in spec.nodes] == ["noise", "ero", "col"]
    assert spec.nodes[0].type == "NoiseFbm"
    assert spec.nodes[0].params == {"kw": [4, 4], "seed": 1}
    # links parsed into (from_node, from_port, to_node, to_port)
    assert spec.links[0] == ("noise", "output", "ero", "input")
    assert spec.exports[0] == ("ero", "output", "terrain.png")


def test_malformed_link_raises():
    bad = {"nodes": [{"id": "a", "type": "X"}], "links": [["a", "b.in"]]}
    try:
        Spec.from_dict(bad)
        assert False, "expected SpecError"
    except SpecError as e:
        assert "endpoint" in str(e).lower()


def test_missing_node_id_raises():
    try:
        Spec.from_dict({"nodes": [{"type": "X"}]})
        assert False, "expected SpecError"
    except SpecError:
        pass
```

- [ ] **Step 3: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_spec.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.spec'`.

- [ ] **Step 4: Write minimal implementation**

```python
# scripts/hsd/spec.py
import json
from dataclasses import dataclass, field


class SpecError(Exception):
    pass


@dataclass
class NodeSpec:
    id: str
    type: str
    params: dict = field(default_factory=dict)


@dataclass
class Spec:
    config: dict
    nodes: list          # list[NodeSpec]
    links: list          # list[(from_node, from_port, to_node, to_port)]
    exports: list        # list[(node, port, path)]

    @classmethod
    def from_file(cls, path):
        with open(path) as f:
            return cls.from_dict(json.load(f))

    @classmethod
    def from_dict(cls, d):
        config = d.get("config", {}) or {}

        nodes = []
        for raw in d.get("nodes", []) or []:
            if "id" not in raw:
                raise SpecError(f"node is missing 'id': {raw}")
            if "type" not in raw:
                raise SpecError(f"node '{raw.get('id')}' is missing 'type'")
            nodes.append(NodeSpec(raw["id"], raw["type"], raw.get("params", {}) or {}))

        links = [
            (*_endpoint(a), *_endpoint(b))
            for a, b in (d.get("links", []) or [])
        ]

        exports = [
            (e["node"], e["port"], e["path"])
            for e in (d.get("export", []) or [])
        ]

        return cls(config, nodes, links, exports)


def _endpoint(s):
    if not isinstance(s, str) or s.count(".") != 1:
        raise SpecError(f"link endpoint must be 'node.port', got: {s!r}")
    node, port = s.split(".")
    if not node or not port:
        raise SpecError(f"link endpoint must be 'node.port', got: {s!r}")
    return node, port
```

- [ ] **Step 5: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_spec.py -v`
Expected: PASS (3 passed).

- [ ] **Step 6: Commit**

```bash
git add scripts/hsd/spec.py tests/hsd/test_spec.py tests/hsd/fixtures/first_terrain.json
git commit -m "feat(hsd): JSON spec parser with structural validation"
```

---

## Task 3: Param value-object builder

**Files:**
- Create: `scripts/hsd/params.py`
- Test: `tests/hsd/test_params.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_params.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.params import value_object, ParamError


def test_float_value_object():
    assert value_object("angle", "Float", 45.0) == {
        "label": "angle", "type": 6, "type_string": "Float", "value": 45.0
    }


def test_wavenumber_value_object():
    vo = value_object("kw", "Wavenumber", [4, 4])
    assert vo["type"] == 17 and vo["type_string"] == "Wavenumber"
    assert vo["value"] == [4, 4]


def test_value_range_adds_is_active():
    vo = value_object("post_remap", "Value range", [0.0, 1.0])
    assert vo["is_active"] is True and vo["value"] == [0.0, 1.0]


def test_dict_value_passthrough_for_advanced_type():
    full = {"label": "gradient", "type": 3, "type_string": "Color gradient", "value": []}
    assert value_object("gradient", "Color gradient", full) == full


def test_unknown_scalar_type_requires_full_object():
    try:
        value_object("g", "Color gradient", 5)
        assert False, "expected ParamError"
    except ParamError as e:
        assert "full value object" in str(e).lower()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_params.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.params'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/params.py
from hsd.catalog import TYPE_MAP


class ParamError(Exception):
    pass


def value_object(name, type_string, value):
    """Build a Hesiod param value object for an overridden parameter.

    If `value` is a dict, it is treated as a fully-formed value object and passed
    through (escape hatch for advanced types like Color gradient/Cloud/Path/
    Enumeration). Otherwise the param type must be in TYPE_MAP.
    """
    if isinstance(value, dict):
        return value

    if type_string not in TYPE_MAP:
        raise ParamError(
            f"param '{name}' has type '{type_string}' which needs a full value "
            "object; pass a dict value instead of a scalar"
        )

    code, ts = TYPE_MAP[type_string]
    vo = {"label": name, "type": code, "type_string": ts, "value": value}
    if type_string == "Value range":
        vo["is_active"] = True
    return vo
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_params.py -v`
Expected: PASS (5 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/params.py tests/hsd/test_params.py
git commit -m "feat(hsd): param value-object builder with dict passthrough"
```

---

## Task 4: Layout (deterministic node positions)

**Files:**
- Create: `scripts/hsd/layout.py`
- Test: `tests/hsd/test_layout.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_layout.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.layout import layout_positions


def test_layered_left_to_right():
    # a -> b -> c ; a and standalone d share column 0
    node_ids = ["a", "b", "c", "d"]
    links = [("a", "b"), ("b", "c")]
    pos = layout_positions(node_ids, links)
    # downstream nodes are further right
    assert pos["a"][0] < pos["b"][0] < pos["c"][0]
    # roots (a, d) share the leftmost column
    assert pos["a"][0] == pos["d"][0]


def test_deterministic():
    node_ids = ["a", "b", "c"]
    links = [("a", "b"), ("b", "c")]
    assert layout_positions(node_ids, links) == layout_positions(node_ids, links)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_layout.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.layout'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/layout.py
COL_SPACING = 320.0
ROW_SPACING = 200.0


def layout_positions(node_ids, links):
    """Assign (x, y) scene positions by longest-path depth from roots.

    `links` is a list of (from_id, to_id). Returns {node_id: (x, y)}.
    Deterministic: depth sets the column; insertion order sets the row.
    """
    succ = {n: [] for n in node_ids}
    indeg = {n: 0 for n in node_ids}
    for a, b in links:
        if a in succ and b in indeg:
            succ[a].append(b)
            indeg[b] += 1

    # longest-path depth via repeated relaxation (graphs are small/acyclic)
    depth = {n: 0 for n in node_ids}
    for _ in range(len(node_ids)):
        changed = False
        for a, b in links:
            if a in depth and b in depth and depth[b] < depth[a] + 1:
                depth[b] = depth[a] + 1
                changed = True
        if not changed:
            break

    rows = {}
    pos = {}
    for n in node_ids:                      # stable: spec/insertion order
        col = depth[n]
        row = rows.get(col, 0)
        rows[col] = row + 1
        pos[n] = (col * COL_SPACING, row * ROW_SPACING)
    return pos
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_layout.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/layout.py tests/hsd/test_layout.py
git commit -m "feat(hsd): deterministic layered node layout"
```

---

## Task 5: Builder — model + UI mirror + .hsd assembly

**Files:**
- Create: `scripts/hsd/builder.py`
- Test: `tests/hsd/test_builder.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_builder.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.builder import Graph


def _consistent(hsd):
    """model nodes/links must match UI nodes/links (ids + types)."""
    g = hsd["graph_manager"]["graph_nodes"]["graph"]
    ui = hsd["graph_tabs_widget"]["graph_node_widgets"]["graph"]
    model_nodes = {n["id"]: n["label"] for n in g["nodes"]}
    ui_nodes = {n["id"]: n["caption"] for n in ui["nodes"]}
    assert model_nodes == ui_nodes
    model_links = {(l["node_id_from"], l["port_id_from"],
                    l["node_id_to"], l["port_id_to"]) for l in g["links"]}
    ui_links = {(l["node_out_id"], l["port_out_id"],
                 l["node_in_id"], l["port_in_id"]) for l in ui["links"]}
    assert model_links == ui_links


def test_build_minimal_graph():
    g = Graph(config={"shape": [512, 512], "tiling": [1, 1], "overlap": 0.0})
    g.add_node("noise", "NoiseFbm", {"kw": {"label": "kw", "type": 17,
               "type_string": "Wavenumber", "value": [4, 4]}})
    g.add_node("exp", "ExportHeightmap", {})
    g.link("noise", "output", "exp", "input")
    g.set_export("noise", "output", "out.png")
    hsd = g.to_hsd()

    gm = hsd["graph_manager"]["graph_nodes"]["graph"]
    assert len(gm["nodes"]) == 2
    assert {n["label"] for n in gm["nodes"]} == {"NoiseFbm", "ExportHeightmap"}
    assert gm["model_config"]["shape.x"] == 512
    # export wired to the model id of the 'noise' node
    noise_id = next(n["id"] for n in gm["nodes"] if n["label"] == "NoiseFbm")
    ep = hsd["graph_manager"]["export_param"]
    assert ep["ids"] == [["graph", noise_id, "output"]]
    assert ep["export_path"] == "out.png"
    _consistent(hsd)


def test_id_count_is_next_free():
    g = Graph()
    g.add_node("a", "Abs", {})
    g.add_node("b", "Abs", {})
    hsd = g.to_hsd()
    assert hsd["graph_manager"]["graph_nodes"]["graph"]["id_count"] == 3
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_builder.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.builder'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/builder.py
from hsd.layout import layout_positions

HESIOD_VERSION = "v0.4.0"
SAVED_AT = "1970-01-01_00:00:00"   # deterministic; real saves stamp this in the GUI
LINK_TYPE = 2


class BuilderError(Exception):
    pass


class Graph:
    def __init__(self, config=None):
        self.config = config or {"shape": [1024, 1024], "tiling": [1, 1], "overlap": 0.0}
        self._nodes = []          # list of (name, type, value_objects_dict)
        self._name_to_id = {}
        self._links = []          # list of (from_name, from_port, to_name, to_port)
        self._exports = []        # list of (name, port, path)
        self._next_id = 1

    def add_node(self, name, node_type, value_objects):
        if name in self._name_to_id:
            raise BuilderError(f"duplicate node name: {name}")
        self._name_to_id[name] = str(self._next_id)
        self._next_id += 1
        self._nodes.append((name, node_type, dict(value_objects)))

    def link(self, from_name, from_port, to_name, to_port):
        self._links.append((from_name, from_port, to_name, to_port))

    def set_export(self, name, port, path):
        self._exports.append((name, port, path))

    # --- assembly -----------------------------------------------------------
    def _model_config(self):
        sx, sy = self.config["shape"]
        tx, ty = self.config.get("tiling", [1, 1])
        return {
            "hmap_transform_mode_cpu": 0,
            "hmap_transform_mode_gpu": 2,
            "overlap": self.config.get("overlap", 0.0),
            "shape.x": sx, "shape.y": sy,
            "tiling.x": tx, "tiling.y": ty,
        }

    def _model(self):
        nodes = []
        for name, ntype, vobjs in self._nodes:
            node = {"id": self._name_to_id[name], "label": ntype}
            node.update(vobjs)
            nodes.append(node)
        links = [{
            "node_id_from": self._name_to_id[a], "port_id_from": ap,
            "node_id_to": self._name_to_id[b], "port_id_to": bp,
        } for a, ap, b, bp in self._links]
        return {
            "id": "graph",
            "id_count": self._next_id,
            "links": links,
            "model_config": self._model_config(),
            "nodes": nodes,
        }

    def _ui(self):
        ids = [self._name_to_id[n] for n, _, _ in self._nodes]
        link_pairs = [(self._name_to_id[a], self._name_to_id[b])
                      for a, _, b, _ in self._links]
        pos = layout_positions(ids, link_pairs)
        ui_nodes = []
        for name, ntype, _ in self._nodes:
            nid = self._name_to_id[name]
            x, y = pos[nid]
            ui_nodes.append({
                "caption": ntype, "id": nid, "is_widget_visible": True,
                "scene_position.x": x, "scene_position.y": y,
            })
        ui_links = [{
            "link_type": LINK_TYPE,
            "node_out_id": self._name_to_id[a], "port_out_id": ap,
            "node_in_id": self._name_to_id[b], "port_in_id": bp,
        } for a, ap, b, bp in self._links]
        return {
            "comments": [], "current_link_type": LINK_TYPE, "groups": [],
            "id": "graph", "links": ui_links, "nodes": ui_nodes,
        }

    def _export_param(self):
        sx, sy = self.config["shape"]
        tx, ty = self.config.get("tiling", [1, 1])
        ids = [["graph", self._name_to_id[n], p] for n, p, _ in self._exports]
        path = self._exports[0][2] if self._exports else ""
        return {
            "export_path": path, "ids": ids, "overlap": self.config.get("overlap", 0.0),
            "shape.x": sx, "shape.y": sy, "tiling.x": tx, "tiling.y": ty,
        }

    def to_hsd(self):
        return {
            "Hesiod version": HESIOD_VERSION,
            "graph_manager": {
                "export_param": self._export_param(),
                "graph_nodes": {"graph": self._model()},
            },
            "graph_manager_widget": {"frames": {"graph": {"current_bg_tag": "NONE"}}},
            "graph_tabs_widget": {"graph_node_widgets": {"graph": self._ui()}},
            "saved_at": SAVED_AT,
        }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_builder.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/builder.py tests/hsd/test_builder.py
git commit -m "feat(hsd): graph builder emitting consistent model + UI mirror"
```

---

## Task 6: Compile spec → Graph

**Files:**
- Create: `scripts/hsd/compile.py`
- Test: `tests/hsd/test_compile.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_compile.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_compile_fixture_to_hsd():
    spec = Spec.from_file(FIX)
    cat = Catalog.load()
    hsd = compile_spec(spec, cat).to_hsd()

    g = hsd["graph_manager"]["graph_nodes"]["graph"]
    labels = {n["label"] for n in g["nodes"]}
    assert labels == {"NoiseFbm", "HydraulicParticle", "ColorizeGradient"}

    # overridden params became value objects on the NoiseFbm node
    noise = next(n for n in g["nodes"] if n["label"] == "NoiseFbm")
    assert noise["kw"]["type_string"] == "Wavenumber" and noise["kw"]["value"] == [4, 4]
    assert noise["seed"]["type_string"] == "Random seed number" and noise["seed"]["value"] == 1
    # un-overridden params are omitted (tolerant loader fills them)
    assert "lacunarity" not in noise

    # export wired
    ero_id = next(n["id"] for n in g["nodes"] if n["label"] == "HydraulicParticle")
    assert hsd["graph_manager"]["export_param"]["ids"] == [["graph", ero_id, "output"]]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_compile.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.compile'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/compile.py
from hsd.builder import Graph
from hsd.params import value_object


def compile_spec(spec, catalog):
    """Compile a Spec into a Graph using the catalog for param typing.

    Assumes the spec has already passed validation (see hsd.validate). Missing
    nodes/params here raise KeyError-style errors; call validate first for
    friendly messages.
    """
    g = Graph(config=spec.config or {"shape": [1024, 1024], "tiling": [1, 1],
                                      "overlap": 0.0})
    for node in spec.nodes:
        params_meta = catalog.params(node.type)
        vobjs = {}
        for pname, pvalue in node.params.items():
            type_string = params_meta.get(pname, {}).get("type", "")
            vobjs[pname] = value_object(pname, type_string, pvalue)
        g.add_node(node.id, node.type, vobjs)

    for a, ap, b, bp in spec.links:
        g.link(a, ap, b, bp)
    for n, p, path in spec.exports:
        g.set_export(n, p, path)
    return g
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_compile.py -v`
Expected: PASS (1 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/compile.py tests/hsd/test_compile.py
git commit -m "feat(hsd): compile spec into a graph using catalog param types"
```

---

## Task 7: Validator (L1/L2 + structured errors)

**Files:**
- Create: `scripts/hsd/validate.py`
- Test: `tests/hsd/test_validate.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_validate.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.validate import validate_spec

CAT = Catalog.load()
FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_good_spec_has_no_errors():
    assert validate_spec(Spec.from_file(FIX), CAT) == []


def test_unknown_node_type():
    spec = Spec.from_dict({"nodes": [{"id": "x", "type": "Nope"}]})
    errs = validate_spec(spec, CAT)
    assert any(e["level"] == "L1" and "unknown node type" in e["problem"].lower()
               for e in errs)


def test_unknown_param():
    spec = Spec.from_dict({"nodes": [{"id": "n", "type": "NoiseFbm",
                                      "params": {"not_a_param": 1}}]})
    errs = validate_spec(spec, CAT)
    assert any("not_a_param" in e["problem"] for e in errs)


def test_incompatible_link_datatype():
    # ColorizeGradient.output is VirtualTexture; ExportHeightmap.input is VirtualArray
    spec = Spec.from_dict({
        "nodes": [{"id": "c", "type": "ColorizeGradient"},
                  {"id": "e", "type": "ExportHeightmap"}],
        "links": [["c.output", "e.input"]],
    })
    errs = validate_spec(spec, CAT)
    assert any(e["level"] == "L2" and "data type" in e["problem"].lower()
               for e in errs)


def test_dangling_export():
    spec = Spec.from_dict({"nodes": [{"id": "n", "type": "NoiseFbm"}],
                           "export": [{"node": "missing", "port": "output",
                                       "path": "o.png"}]})
    errs = validate_spec(spec, CAT)
    assert any("export" in e["problem"].lower() for e in errs)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_validate.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.validate'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/validate.py
def _err(level, problem, suggestion="", node_id=None, link=None):
    return {"level": level, "node_id": node_id, "link": link,
            "problem": problem, "suggestion": suggestion}


def validate_spec(spec, catalog):
    """Return a list of structured error dicts; empty list means valid."""
    errors = []
    ids = {}

    # L1: node types and params
    for node in spec.nodes:
        if node.id in ids:
            errors.append(_err("L1", f"duplicate node id '{node.id}'", node_id=node.id))
        ids[node.id] = node.type
        if not catalog.has_node(node.type):
            errors.append(_err("L1", f"unknown node type '{node.type}'",
                               "run `hsd nodes --search <term>` to find a valid type",
                               node_id=node.id))
            continue
        params_meta = catalog.params(node.type)
        for pname in node.params:
            if pname not in params_meta:
                errors.append(_err(
                    "L1", f"unknown param '{pname}' on node type '{node.type}'",
                    f"run `hsd nodes --show {node.type}` to list its params",
                    node_id=node.id))

    # L2: links resolve + datatype compatibility
    for a, ap, b, bp in spec.links:
        link = (a, ap, b, bp)
        if a not in ids or b not in ids:
            errors.append(_err("L2", f"link references unknown node: {a} or {b}",
                               link=link))
            continue
        out = catalog.port(ids[a], ap) if catalog.has_node(ids[a]) else None
        inp = catalog.port(ids[b], bp) if catalog.has_node(ids[b]) else None
        if out is None or out.get("type") != "output":
            errors.append(_err("L2", f"'{a}.{ap}' is not an output port", link=link))
            continue
        if inp is None or inp.get("type") != "input":
            errors.append(_err("L2", f"'{b}.{bp}' is not an input port", link=link))
            continue
        if out["data_type"] != inp["data_type"]:
            errors.append(_err(
                "L2",
                f"incompatible data type: {a}.{ap} is {out['data_type']} but "
                f"{b}.{bp} is {inp['data_type']}",
                "Colorize* converts VirtualArray->VirtualTexture; insert one if "
                "bridging heightmap to colour",
                link=link))

    # L2: export targets resolve to a real output port
    for n, p, _path in spec.exports:
        if n not in ids:
            errors.append(_err("L2", f"export references unknown node '{n}'"))
            continue
        port = catalog.port(ids[n], p) if catalog.has_node(ids[n]) else None
        if port is None or port.get("type") != "output":
            errors.append(_err("L2", f"export target '{n}.{p}' is not an output port"))

    return errors
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_validate.py -v`
Expected: PASS (5 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/validate.py tests/hsd/test_validate.py
git commit -m "feat(hsd): layered spec validation with structured errors"
```

---

## Task 8: Lint existing .hsd (model↔UI consistency)

**Files:**
- Modify: `scripts/hsd/validate.py` (add `lint_file`)
- Test: `tests/hsd/test_lint.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_lint.py
import os, sys, json, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.validate import lint_file, consistency_errors


def test_real_example_is_consistent():
    # every shipped example must be model<->UI consistent
    f = sorted(glob.glob(os.path.join(
        os.path.dirname(__file__), "..", "..", "Hesiod", "data", "examples", "*.hsd")))[0]
    assert consistency_errors(json.load(open(f))) == []


def test_detects_inconsistent_mirror(tmp_path):
    hsd = {
        "graph_manager": {"graph_nodes": {"graph": {
            "links": [], "nodes": [{"id": "1", "label": "Abs"}]}}},
        "graph_tabs_widget": {"graph_node_widgets": {"graph": {
            "links": [], "nodes": [{"id": "1", "caption": "Bump"}]}}},
    }
    assert any("caption" in e or "label" in e or "mismatch" in e.lower()
               for e in consistency_errors(hsd))


def test_lint_file_reads_path(tmp_path):
    f = sorted(glob.glob(os.path.join(
        os.path.dirname(__file__), "..", "..", "Hesiod", "data", "examples", "*.hsd")))[0]
    # lint returns a dict with 'consistency' and 'validation' keys
    result = lint_file(f)
    assert "consistency" in result and "validation" in result
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_lint.py -v`
Expected: FAIL — `ImportError: cannot import name 'lint_file'`.

- [ ] **Step 3: Add implementation to `scripts/hsd/validate.py`**

```python
# append to scripts/hsd/validate.py
import json


def _model_view(hsd):
    g = hsd["graph_manager"]["graph_nodes"]["graph"]
    nodes = {n["id"]: n["label"] for n in g["nodes"]}
    links = sorted((l["node_id_from"], l["port_id_from"],
                    l["node_id_to"], l["port_id_to"]) for l in g["links"])
    return nodes, links


def _ui_view(hsd):
    g = hsd["graph_tabs_widget"]["graph_node_widgets"]["graph"]
    nodes = {n["id"]: n["caption"] for n in g["nodes"]}
    links = sorted((l["node_out_id"], l["port_out_id"],
                    l["node_in_id"], l["port_in_id"]) for l in g["links"])
    return nodes, links


def consistency_errors(hsd):
    """Pure-stdlib model<->UI consistency check (replaces deepdiff)."""
    errors = []
    mn, ml = _model_view(hsd)
    un, ul = _ui_view(hsd)
    if mn != un:
        errors.append(f"node mismatch between model and UI: model={mn} ui={un}")
    if ml != ul:
        errors.append(f"link mismatch between model and UI: model={ml} ui={ul}")
    return errors


def lint_file(path):
    with open(path) as f:
        hsd = json.load(f)
    return {"consistency": consistency_errors(hsd), "validation": []}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_lint.py -v`
Expected: PASS (3 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/validate.py tests/hsd/test_lint.py
git commit -m "feat(hsd): lint existing .hsd via stdlib model/UI consistency check"
```

---

## Task 9: Runner (locate binary + run --batch)

**Files:**
- Create: `scripts/hsd/run.py`
- Test: `tests/hsd/test_run.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_run.py
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.run import find_binary, build_batch_command


def test_find_binary_prefers_env(monkeypatch, tmp_path):
    fake = tmp_path / "hesiod"
    fake.write_text("")
    fake.chmod(0o755)
    monkeypatch.setenv("HESIOD_BIN", str(fake))
    assert find_binary() == str(fake)


def test_build_batch_command_includes_overrides():
    cmd = build_batch_command("/bin/hesiod", "g.hsd", shape=[512, 512],
                              tiling=[4, 4], overlap=0.25)
    assert cmd[0] == "/bin/hesiod"
    assert "--batch=g.hsd" in cmd
    assert "--shape=512,512" in cmd
    assert "--tiling=4,4" in cmd
    assert "--overlap=0.25" in cmd


def test_build_batch_command_minimal():
    cmd = build_batch_command("/bin/hesiod", "g.hsd")
    assert cmd == ["/bin/hesiod", "--batch=g.hsd"]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_run.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.run'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/run.py
import os
import subprocess
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parents[2]
_DEFAULT_BIN = _REPO_ROOT / "build" / "bin" / "hesiod"


class RunError(Exception):
    pass


def find_binary():
    env = os.environ.get("HESIOD_BIN")
    if env and Path(env).exists():
        return env
    if _DEFAULT_BIN.exists():
        return str(_DEFAULT_BIN)
    raise RunError(
        "hesiod binary not found; set HESIOD_BIN or build to build/bin/hesiod")


def build_batch_command(binary, hsd_path, shape=None, tiling=None, overlap=None):
    cmd = [binary, f"--batch={hsd_path}"]
    if shape:
        cmd.append(f"--shape={shape[0]},{shape[1]}")
    if tiling:
        cmd.append(f"--tiling={tiling[0]},{tiling[1]}")
    if overlap is not None:
        cmd.append(f"--overlap={overlap}")
    return cmd


def expected_outputs(hsd_path):
    """Given the export_path in a file, return (raw_png, preview_png) paths."""
    import json
    with open(hsd_path) as f:
        ep = json.load(f)["graph_manager"]["export_param"]
    path = ep.get("export_path", "")
    if not path:
        return (None, None)
    p = Path(path)
    if not p.suffix:
        p = p.with_suffix(".png")
    preview = p.with_name(p.stem + "_preview" + p.suffix)
    return (str(p), str(preview))


def run_batch(hsd_path, shape=None, tiling=None, overlap=None, cwd=None):
    binary = find_binary()
    cmd = build_batch_command(binary, hsd_path, shape, tiling, overlap)
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    raw, preview = expected_outputs(hsd_path)
    return {
        "returncode": proc.returncode,
        "stdout": proc.stdout,
        "stderr": proc.stderr,
        "raw_png": raw,
        "preview_png": preview,
    }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_run.py -v`
Expected: PASS (3 passed).

- [ ] **Step 5: Commit**

```bash
git add scripts/hsd/run.py tests/hsd/test_run.py
git commit -m "feat(hsd): batch runner (locate binary, build command, expected outputs)"
```

---

## Task 10: CLI

**Files:**
- Create: `scripts/hsd/cli.py`
- Create: `scripts/hsd/__main__.py`
- Test: `tests/hsd/test_cli.py`

- [ ] **Step 1: Write the failing test**

```python
# tests/hsd/test_cli.py
import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.cli import main

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_build_writes_hsd(tmp_path, capsys):
    out = tmp_path / "g.hsd"
    rc = main(["build", FIX, "-o", str(out)])
    assert rc == 0
    hsd = json.load(open(out))
    assert {n["label"] for n in hsd["graph_manager"]["graph_nodes"]["graph"]["nodes"]} \
        == {"NoiseFbm", "HydraulicParticle", "ColorizeGradient"}


def test_build_reports_validation_errors(tmp_path):
    bad = tmp_path / "bad.json"
    bad.write_text(json.dumps({"nodes": [{"id": "x", "type": "Nope"}]}))
    rc = main(["build", str(bad), "-o", str(tmp_path / "x.hsd")])
    assert rc != 0


def test_nodes_show(capsys):
    rc = main(["nodes", "--show", "NoiseFbm"])
    out = capsys.readouterr().out
    assert rc == 0
    assert "output" in out and "VirtualArray" in out
    assert "kw" in out
```

- [ ] **Step 2: Run test to verify it fails**

Run: `.venv-docs/bin/pytest tests/hsd/test_cli.py -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'hsd.cli'`.

- [ ] **Step 3: Write minimal implementation**

```python
# scripts/hsd/cli.py
import argparse
import json
import sys

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.validate import validate_spec, lint_file
from hsd.run import run_batch


def _print_errors(errors):
    for e in errors:
        loc = f" [{e['node_id']}]" if e.get("node_id") else ""
        sys.stderr.write(f"{e['level']}{loc}: {e['problem']}\n")
        if e.get("suggestion"):
            sys.stderr.write(f"      -> {e['suggestion']}\n")


def _cmd_build(args, cat):
    spec = Spec.from_file(args.spec)
    errors = validate_spec(spec, cat)
    if errors:
        _print_errors(errors)
        return 1
    hsd = compile_spec(spec, cat).to_hsd()
    with open(args.output, "w") as f:
        json.dump(hsd, f, indent=4)
    print(f"wrote {args.output}")
    return 0


def _cmd_validate(args, cat):
    errors = validate_spec(Spec.from_file(args.spec), cat)
    if errors:
        _print_errors(errors)
        return 1
    print("ok")
    return 0


def _cmd_lint(args, cat):
    result = lint_file(args.file)
    problems = result["consistency"] + result["validation"]
    if problems:
        for p in problems:
            sys.stderr.write(f"{p}\n")
        return 1
    print("ok")
    return 0


def _cmd_run(args, cat):
    res = run_batch(args.file, shape=args.shape, tiling=args.tiling,
                    overlap=args.overlap)
    sys.stdout.write(res["stdout"])
    sys.stderr.write(res["stderr"])
    if res["returncode"] != 0:
        return res["returncode"]
    if res["raw_png"]:
        print(f"raw: {res['raw_png']}")
        print(f"preview: {res['preview_png']}")
    return 0


def _cmd_make(args, cat):
    rc = _cmd_build(args, cat)
    if rc != 0 or not args.run:
        return rc
    args.file = args.output
    return _cmd_run(args, cat)


def _cmd_nodes(args, cat):
    if args.show:
        if not cat.has_node(args.show):
            sys.stderr.write(f"unknown node type: {args.show}\n")
            return 1
        print(f"{args.show}  [{cat.category(args.show)}]")
        print(f"  {cat.description(args.show)}")
        print("  ports:")
        for pid, p in cat.ports(args.show).items():
            print(f"    {p['type']:6} {pid}: {p['data_type']}")
        print("  params:")
        for pid, p in cat.params(args.show).items():
            print(f"    {pid}: {p['type']}")
        return 0
    for t in cat.node_types():
        if args.category and not cat.category(t).startswith(args.category):
            continue
        if args.search and args.search.lower() not in t.lower():
            continue
        print(f"{t}  [{cat.category(t)}]")
    return 0


def _pair(s):
    a, b = s.split(",")
    return [int(a), int(b)]


def main(argv=None):
    parser = argparse.ArgumentParser(prog="hsd")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("build"); p.add_argument("spec"); p.add_argument("-o", "--output", required=True)
    p = sub.add_parser("validate"); p.add_argument("spec")
    p = sub.add_parser("lint"); p.add_argument("file")
    p = sub.add_parser("run"); p.add_argument("file")
    p.add_argument("--shape", type=_pair); p.add_argument("--tiling", type=_pair)
    p.add_argument("--overlap", type=float)
    p = sub.add_parser("make"); p.add_argument("spec"); p.add_argument("-o", "--output", required=True)
    p.add_argument("--run", action="store_true")
    p.add_argument("--shape", type=_pair); p.add_argument("--tiling", type=_pair)
    p.add_argument("--overlap", type=float)
    p = sub.add_parser("nodes"); p.add_argument("--search"); p.add_argument("--category")
    p.add_argument("--show")

    args = parser.parse_args(argv)
    cat = Catalog.load()
    return {
        "build": _cmd_build, "validate": _cmd_validate, "lint": _cmd_lint,
        "run": _cmd_run, "make": _cmd_make, "nodes": _cmd_nodes,
    }[args.cmd](args, cat)
```

```python
# scripts/hsd/__main__.py
import sys
from hsd.cli import main

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run test to verify it passes**

Run: `.venv-docs/bin/pytest tests/hsd/test_cli.py -v`
Expected: PASS (3 passed).

- [ ] **Step 5: Verify the CLI runs end-to-end (no binary needed)**

Run: `PYTHONPATH=scripts python3 -m hsd build tests/hsd/fixtures/first_terrain.json -o /tmp/first.hsd && PYTHONPATH=scripts python3 -m hsd lint /tmp/first.hsd`
Expected: `wrote /tmp/first.hsd` then `ok`.

- [ ] **Step 6: Commit**

```bash
git add scripts/hsd/cli.py scripts/hsd/__main__.py tests/hsd/test_cli.py
git commit -m "feat(hsd): hsd CLI (nodes/build/validate/lint/run/make)"
```

---

## Task 11: Round-trip integration test (binary-gated)

**Files:**
- Test: `tests/hsd/test_integration.py`

This task **empirically confirms** the core assumption: a graph that omits un-overridden
params loads in Hesiod's tolerant deserializer and exports PNGs. It is skipped when no
binary is present so CI without a GPU still passes.

- [ ] **Step 1: Write the test**

```python
# tests/hsd/test_integration.py
import os, sys, json
import pytest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.run import run_batch, find_binary, RunError


def _have_binary():
    try:
        find_binary()
        return True
    except RunError:
        return False


@pytest.mark.skipif(not _have_binary(), reason="hesiod binary not available")
def test_roundtrip_noise_to_export(tmp_path):
    spec = Spec.from_dict({
        "config": {"shape": [64, 64], "tiling": [1, 1], "overlap": 0.0},
        "nodes": [
            {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
            {"id": "exp", "type": "ExportHeightmap"},
        ],
        "links": [["noise.output", "exp.input"]],
        "export": [{"node": "noise", "port": "output",
                    "path": str(tmp_path / "out.png")}],
    })
    cat = Catalog.load()
    hsd_path = tmp_path / "g.hsd"
    hsd_path.write_text(json.dumps(compile_spec(spec, cat).to_hsd(), indent=4))

    res = run_batch(str(hsd_path), shape=[64, 64], tiling=[1, 1], cwd=str(tmp_path))
    assert res["returncode"] == 0, res["stderr"]
    assert os.path.exists(res["raw_png"]), res["stdout"] + res["stderr"]
    assert os.path.exists(res["preview_png"])
```

- [ ] **Step 2: Run the test**

Run: `HESIOD_BIN=$PWD/build/bin/hesiod .venv-docs/bin/pytest tests/hsd/test_integration.py -v`

Expected (binary present): PASS, files created.
If it **fails on load** (tolerant-loader rejects omitted params), apply the fallback
below before continuing.

> **Fallback (only if round-trip fails on missing params):** the builder must emit full
> default param objects. Add `scripts/hsd/catalog_build.py` that runs
> `build/bin/hesiod --inventory` (from `build/bin/`), reads the produced
> `node_documentation_stub.json`, and writes `scripts/hsd/node_catalog.json` mapping each
> node type → its full default value objects. Then in `compile_spec`, start each node's
> `vobjs` from a deep copy of those defaults and overlay overrides. Re-run this test.
> Note: `--inventory` needs the Qt/OpenGL environment (see the "Running the dev build"
> project memory); the user runs it and commits the resulting `node_catalog.json`.

- [ ] **Step 3: Commit**

```bash
git add tests/hsd/test_integration.py
git commit -m "test(hsd): binary-gated round-trip noise->export integration"
```

---

## Task 12: Full suite + docstring entrypoint note

**Files:**
- Modify: `scripts/hsd/__init__.py` (usage note)

- [ ] **Step 1: Run the whole hsd suite**

Run: `.venv-docs/bin/pytest tests/hsd/ -v`
Expected: all pass (integration test skipped if no binary).

- [ ] **Step 2: Confirm no regression in existing tests**

Run: `.venv-docs/bin/pytest tests/ -v`
Expected: all pass.

- [ ] **Step 3: Add a usage note to the package docstring**

```python
# scripts/hsd/__init__.py
"""hsd toolkit: compile/validate/run Hesiod .hsd graphs.

CLI:  PYTHONPATH=scripts python3 -m hsd <build|validate|lint|run|make|nodes> ...
Lib:  from hsd.spec import Spec; from hsd.compile import compile_spec
"""
__version__ = "0.1.0"
```

- [ ] **Step 4: Commit**

```bash
git add scripts/hsd/__init__.py
git commit -m "docs(hsd): package usage note; foundation complete"
```

---

## Self-Review (completed)

- **Spec coverage:** catalog (Task 1), spec format (Task 2), param value objects/TYPE_MAP (Task 3), layout (Task 4), builder model+UI mirror (Task 5), compile (Task 6), L1/L2 validation (Task 7), lint/model↔UI consistency (Task 8), runner (Task 9), CLI subcommands incl. `nodes` (Task 10), round-trip/binary-gated execution + the `--inventory` fallback for defaults (Task 11), suite green (Task 12). All spec sections map to a task.
- **Deviation from spec (intentional, consistent with spec intent):** v1 uses `node_documentation.json` directly as the catalog and the **omit-defaults** builder strategy (justified by Otto's tolerant-deserializer note), so no separate committed `node_catalog.json` / `build-catalog` step is built in v1. Task 11 includes the `--inventory`-based fallback if the round-trip disproves the assumption.
- **Type consistency:** `Catalog.port/params/ports/has_node/category/description`, `Spec.from_file/from_dict` with `NodeSpec.id/type/params`, `value_object(name, type_string, value)`, `Graph.add_node/link/set_export/to_hsd`, `compile_spec(spec, catalog)`, `validate_spec(spec, catalog)`, `consistency_errors(hsd)`, `lint_file(path)`, `run_batch(...)`/`find_binary`/`build_batch_command` are used consistently across tasks.
- **No placeholders:** every code step contains complete code; every run step states the exact command and expected result.
