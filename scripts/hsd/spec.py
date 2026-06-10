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
        config = d.get("config", {}) or {}
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
            for key in ("dims", "extent"):
                if (not isinstance(grid.get(key), (list, tuple))
                        or len(grid[key]) != 2):
                    raise SpecError(f"grid must have 2-element '{key}'")

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
            flatten = FlattenSpec(export["path"], ids,
                                  shape=export.get("shape"),
                                  tiling=export.get("tiling"),
                                  overlap=export.get("overlap"))

        return cls(config, graphs, broadcasts, flatten,
                   grid=grid, graph_order=d.get("graph_order"))

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
        return GraphSpec(raw["id"], nodes, links,
                         origin=raw.get("origin"), size=raw.get("size"),
                         cell=raw.get("cell"), config=raw.get("config", {}) or {})


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
