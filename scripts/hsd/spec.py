import json
from dataclasses import dataclass, field


class SpecError(Exception):
    pass


def _vec2(name, value, *, integer=False, positive=False):
    """Validate a 2-element numeric list; returns it as a plain list."""
    num_types = (int,) if integer else (int, float)
    if (not isinstance(value, (list, tuple)) or len(value) != 2
            or not all(isinstance(v, num_types) and not isinstance(v, bool)
                       for v in value)):
        kind = "2 integers" if integer else "2 numbers"
        raise SpecError(f"'{name}' must be a list of {kind}, got: {value!r}")
    if positive and not all(v > 0 for v in value):
        raise SpecError(f"'{name}' components must be > 0, got: {value!r}")
    return list(value)


def _num(name, value):
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        raise SpecError(f"'{name}' must be a number, got: {value!r}")
    return value


def _config_dict(name, value):
    """Validate a compute-config dict (shape/tiling/overlap typed correctly)."""
    if value is None:
        return {}
    if not isinstance(value, dict):
        raise SpecError(f"'{name}' must be an object, got: {value!r}")
    out = dict(value)
    if "shape" in out:
        out["shape"] = _vec2(f"{name}.shape", out["shape"], integer=True, positive=True)
    if "tiling" in out:
        out["tiling"] = _vec2(f"{name}.tiling", out["tiling"], integer=True, positive=True)
    if "overlap" in out:
        _num(f"{name}.overlap", out["overlap"])
    return out


@dataclass
class NodeSpec:
    id: str
    type: str
    params: dict = field(default_factory=dict)


@dataclass
class GraphSpec:
    id: str
    nodes: list                        # list[NodeSpec]
    links: list                        # list[(from_node, from_port, to_node, to_port)]
    origin: list | None = None         # [x, y] explicit placement
    size: list | None = None           # [w, h] explicit placement
    cell: list | None = None           # [col, row] grid placement
    config: dict = field(default_factory=dict)   # per-graph compute overrides


@dataclass
class BroadcastSpec:
    src_graph: str
    src_node: str
    src_port: str
    dst_graph: str
    recv_name: str


@dataclass
class FlattenSpec:
    path: str
    ids: list                          # list[(graph_id, node_name, port)]
    shape: list | None = None          # None -> compile-time default from config
    tiling: list | None = None
    overlap: float | None = None
    legacy_paths: list = field(default_factory=list)  # all paths from list form


@dataclass
class Spec:
    config: dict
    graphs: list                       # list[GraphSpec]
    broadcasts: list                   # list[BroadcastSpec]
    flatten: FlattenSpec | None = None   # None -> no flatten export
    grid: dict | None = None           # {"dims": [C, R], "extent": [W, H]}
    graph_order: list | None = None    # explicit override

    @classmethod
    def from_file(cls, path):
        with open(path) as f:
            return cls.from_dict(json.load(f))

    @classmethod
    def from_dict(cls, d):
        config = _config_dict("config", d.get("config"))
        if "graphs" in d:
            return cls._from_multi(d, config)
        return cls._from_flat(d, config)

    # --- multi-graph form ----------------------------------------------------
    @classmethod
    def _from_multi(cls, d, config):
        for key in ("nodes", "links"):
            if key in d:
                raise SpecError(
                    f"top-level '{key}' is not allowed alongside 'graphs'; "
                    "move it inside a graph entry")

        grid = d.get("grid")
        if grid is not None:
            if not isinstance(grid, dict):
                raise SpecError(f"grid must be an object, got: {grid!r}")
            dims = _vec2("grid.dims", grid.get("dims"), integer=True, positive=True)
            extent = _vec2("grid.extent", grid.get("extent"), positive=True)
            grid = {"dims": dims, "extent": extent}

        graphs = [cls._graph_spec(raw) for raw in d.get("graphs", []) or []]
        if not graphs:
            raise SpecError("'graphs' must contain at least one graph")

        broadcasts = []
        for entry in d.get("broadcasts", []) or []:
            if not isinstance(entry, (list, tuple)) or len(entry) != 2:
                raise SpecError(f"broadcast must be a [src, dst] pair, got: {entry!r}")
            src, dst = entry
            broadcasts.append(BroadcastSpec(*_endpoint3(src), *_endpoint2(dst)))

        flatten = None
        export = d.get("export")
        if export is not None:
            if not isinstance(export, dict):
                raise SpecError(
                    "multi-graph specs use the object export form: "
                    '{"path": ..., "ids": [[graph, node, port], ...]}')
            if "path" not in export or not export.get("ids"):
                raise SpecError("export object needs 'path' and a non-empty 'ids'")
            ids = []
            for triple in export["ids"]:
                if not isinstance(triple, (list, tuple)) or len(triple) != 3:
                    raise SpecError(
                        f"export id must be [graph, node, port], got: {triple!r}")
                ids.append(tuple(triple))
            raw_shape = export.get("shape")
            shape = _vec2("export.shape", raw_shape, integer=True, positive=True) if raw_shape is not None else None
            raw_tiling = export.get("tiling")
            tiling = _vec2("export.tiling", raw_tiling, integer=True, positive=True) if raw_tiling is not None else None
            raw_overlap = export.get("overlap")
            if raw_overlap is not None:
                _num("export.overlap", raw_overlap)
            flatten = FlattenSpec(export["path"], ids,
                                  shape=shape,
                                  tiling=tiling,
                                  overlap=raw_overlap)

        graph_order = d.get("graph_order")
        if graph_order is not None:
            if (not isinstance(graph_order, list)
                    or not all(isinstance(g, str) for g in graph_order)):
                raise SpecError(
                    f"'graph_order' must be a list of graph ids, got: {graph_order!r}")

        return cls(config, graphs, broadcasts, flatten,
                   grid=grid, graph_order=graph_order)

    # --- flat (single-graph, backward-compatible) form -----------------------
    @classmethod
    def _from_flat(cls, d, config):
        raw = {"id": "graph", "nodes": d.get("nodes", []),
               "links": d.get("links", [])}
        graph = cls._graph_spec(raw)

        flatten = None
        exports = d.get("export", []) or []
        if isinstance(exports, dict):
            raise SpecError(
                "the object export form requires top-level 'graphs'; flat specs "
                "use a list of {node, port, path} entries")
        if exports:
            ids, paths = [], []
            for e in exports:
                missing = [k for k in ("node", "port", "path") if k not in e]
                if missing:
                    raise SpecError(f"export entry missing {missing}: {e!r}")
                ids.append(("graph", e["node"], e["port"]))
                paths.append(e["path"])
            flatten = FlattenSpec(paths[0], ids, legacy_paths=paths)

        return cls(config, [graph], [], flatten)

    @classmethod
    def _graph_spec(cls, raw):
        if not isinstance(raw, dict):
            raise SpecError(f"graph entry must be an object, got: {raw!r}")
        if "id" not in raw:
            raise SpecError(f"graph is missing 'id': {sorted(raw)}")
        nodes = []
        for n in raw.get("nodes", []) or []:
            if "id" not in n:
                raise SpecError(f"node is missing 'id': {n}")
            if "type" not in n:
                raise SpecError(f"node '{n.get('id')}' is missing 'type'")
            nodes.append(NodeSpec(n["id"], n["type"], n.get("params", {}) or {}))
        links = []
        for entry in raw.get("links", []) or []:
            if not isinstance(entry, (list, tuple)) or len(entry) != 2:
                raise SpecError(f"link must be a [from, to] pair, got: {entry!r}")
            a, b = entry
            links.append((*_endpoint(a), *_endpoint(b)))
        gid = raw["id"]
        pfx = f"graph '{gid}': "

        raw_origin = raw.get("origin")
        origin = _vec2(f"{pfx}origin", raw_origin) if raw_origin is not None else None

        raw_size = raw.get("size")
        size = _vec2(f"{pfx}size", raw_size, positive=True) if raw_size is not None else None

        raw_cell = raw.get("cell")
        if raw_cell is not None:
            cell = _vec2(f"{pfx}cell", raw_cell, integer=True)
            if any(v < 0 for v in cell):
                raise SpecError(f"{pfx}'cell' components must be >= 0, got: {raw_cell!r}")
        else:
            cell = None

        return GraphSpec(gid, nodes, links,
                         origin=origin, size=size,
                         cell=cell,
                         config=_config_dict(f"{pfx}config", raw.get("config")))


def _endpoint(s):
    if not isinstance(s, str) or s.count(".") != 1:
        raise SpecError(f"link endpoint must be 'node.port', got: {s!r}")
    node, port = s.split(".")
    if not node or not port:
        raise SpecError(f"link endpoint must be 'node.port', got: {s!r}")
    return node, port


def _endpoint3(s):
    if not isinstance(s, str) or s.count(".") != 2:
        raise SpecError(
            f"broadcast source must be 'graph.node.port', got: {s!r}")
    parts = s.split(".")
    if not all(parts):
        raise SpecError(f"broadcast source must be 'graph.node.port', got: {s!r}")
    return parts[0], parts[1], parts[2]


def _endpoint2(s):
    if not isinstance(s, str) or s.count(".") != 1:
        raise SpecError(
            f"broadcast destination must be 'graph.receiveName', got: {s!r}")
    graph, name = s.split(".")
    if not graph or not name:
        raise SpecError(f"broadcast destination must be 'graph.receiveName', got: {s!r}")
    return graph, name
