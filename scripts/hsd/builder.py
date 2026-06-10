from hsd.layout import layout_positions

HESIOD_VERSION = "v0.5.2"
SAVED_AT = "1970-01-01_00:00:00"   # deterministic; real saves stamp this in the GUI
LINK_TYPE = 2
# tuple values: immutable, so instances can never corrupt the shared defaults
DEFAULT_CONFIG = {"shape": (1024, 1024), "tiling": (1, 1), "overlap": 0.0}


class BuilderError(Exception):
    pass


class Graph:
    def __init__(self, config=None, graph_id="graph",
                 origin=(0.0, 0.0), size=(1.0, 1.0)):
        # merge per-key over defaults so a partial config (e.g. only "overlap")
        # doesn't KeyError on shape/tiling later
        self.config = {**DEFAULT_CONFIG, **(config or {})}
        self.graph_id = graph_id
        self.origin = list(origin)
        self.size = list(size)
        self._nodes = []          # list of (name, type, value_objects_dict)
        self._name_to_id = {}
        self._links = []          # list of (from_name, from_port, to_name, to_port)
        self._next_id = 1

    def add_node(self, name, node_type, value_objects):
        if name in self._name_to_id:
            raise BuilderError(
                f"duplicate node name '{name}' in graph '{self.graph_id}'")
        nid = str(self._next_id)
        self._name_to_id[name] = nid
        self._next_id += 1
        self._nodes.append((name, node_type, dict(value_objects)))
        return nid

    def set_raw(self, name, key, value):
        """Set a raw sibling key on a node dict (e.g. Broadcast's broadcast_tag,
        which the engine reads beside the attribute value-objects)."""
        for n, _t, vobjs in self._nodes:
            if n == name:
                vobjs[key] = value
                return
        raise BuilderError(
            f"unknown node name '{name}' in graph '{self.graph_id}'")

    def has_node(self, name):
        return name in self._name_to_id

    def node_id(self, name):
        if name not in self._name_to_id:
            raise BuilderError(
                f"unknown node name '{name}' in graph '{self.graph_id}'")
        return self._name_to_id[name]

    def link(self, from_name, from_port, to_name, to_port):
        self._links.append((from_name, from_port, to_name, to_port))

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
            "node_id_from": self.node_id(a), "port_id_from": ap,
            "node_id_to": self.node_id(b), "port_id_to": bp,
        } for a, ap, b, bp in self._links]
        return {
            "id": self.graph_id,
            "id_count": self._next_id,
            "links": links,
            "model_config": self._model_config(),
            "nodes": nodes,
            "origin": self.origin,
            "rotation_angle": 0.0,
            "size": self.size,
        }

    def _ui(self):
        ids = [self._name_to_id[n] for n, _, _ in self._nodes]
        link_pairs = [(self.node_id(a), self.node_id(b))
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
            "node_out_id": self.node_id(a), "port_out_id": ap,
            "node_in_id": self.node_id(b), "port_in_id": bp,
        } for a, ap, b, bp in self._links]
        return {
            "comments": [], "current_link_type": LINK_TYPE, "groups": [],
            "id": self.graph_id, "links": ui_links, "nodes": ui_nodes,
        }


class Project:
    """Multi-graph .hsd assembly: graphs, broadcast wiring, flatten export."""

    def __init__(self, config=None):
        self.config = {**DEFAULT_CONFIG, **(config or {})}
        self._graphs = {}             # graph_id -> Graph (insertion-ordered)
        self._deps = []               # (src_graph_id, dst_graph_id)
        self._flatten = None          # FlattenSpec or None
        self._order_override = None

    def add_graph(self, graph):
        if graph.graph_id in self._graphs:
            raise BuilderError(f"duplicate graph id: {graph.graph_id}")
        self._graphs[graph.graph_id] = graph

    def graph(self, graph_id):
        if graph_id not in self._graphs:
            raise BuilderError(f"unknown graph id: {graph_id}")
        return self._graphs[graph_id]

    def add_broadcast(self, src_graph, src_node, src_port, dst_graph, recv_name):
        src = self.graph(src_graph)
        dst = self.graph(dst_graph)
        # validate everything before mutating either graph, so a failed
        # broadcast never leaves an orphan Broadcast node behind
        if dst.has_node(recv_name):
            raise BuilderError(
                f"receive name '{recv_name}' already exists in graph '{dst_graph}'")
        bc_name = f"__bc_{dst_graph}_{recv_name}"
        if src.has_node(bc_name):
            raise BuilderError(
                f"duplicate broadcast '{src_graph}' -> '{dst_graph}.{recv_name}'")
        bc_id = src.add_node(bc_name, "Broadcast", {})
        tag = f"{src_graph}/Broadcast/{bc_id}"
        # engine reads broadcast_tag as a raw sibling key (BroadcastNode::json_from);
        # the String attribute mirrors it for GUI display
        src.set_raw(bc_name, "broadcast_tag", tag)
        src.set_raw(bc_name, "tag", {"label": "tag", "type": 13,
                                     "type_string": "String", "value": tag,
                                     "read_only": True})
        src.link(src_node, src_port, bc_name, "input")
        dst.add_node(recv_name, "Receive", {
            "tag": {"label": "tag", "type": 1, "type_string": "Choice",
                    "value": tag, "choice_list": ["NO TAG", tag]}})
        self._deps.append((src_graph, dst_graph))

    def set_flatten(self, flatten_spec):
        self._flatten = flatten_spec

    def set_graph_order(self, order):
        self._order_override = list(order)

    def _graph_order(self):
        if self._order_override is not None:
            missing = sorted(set(self._graphs) - set(self._order_override))
            unknown = sorted(set(self._order_override) - set(self._graphs))
            if missing or unknown or len(self._order_override) != len(self._graphs):
                raise BuilderError(
                    "graph_order override must list every graph exactly once; "
                    f"missing={missing} unknown={unknown}")
            return list(self._order_override)
        order = list(self._graphs)
        needs = {g: set() for g in order}      # g must come after needs[g]
        for src, dst in self._deps:
            if src != dst:
                needs[dst].add(src)
        out, placed = [], set()
        while len(out) < len(order):
            progressed = False
            for g in order:                    # declaration order breaks ties
                if g not in placed and needs[g] <= placed:
                    out.append(g)
                    placed.add(g)
                    progressed = True
            if not progressed:
                stuck = [g for g in order if g not in placed]
                raise BuilderError(
                    f"broadcast cycle among (or blocked by): {stuck}")
        return out

    def _export_param(self):
        f = self._flatten
        shape = (f.shape if f and f.shape else self.config["shape"])
        tiling = (f.tiling if f and f.tiling else self.config.get("tiling", [1, 1]))
        overlap = (f.overlap if f and f.overlap is not None
                   else self.config.get("overlap", 0.0))
        ids = []
        if f:
            ids = [[gid, self.graph(gid).node_id(name), port]
                   for gid, name, port in f.ids]
        return {
            "export_path": f.path if f else "",
            "ids": ids,
            "overlap": overlap,
            "shape.x": shape[0], "shape.y": shape[1],
            "tiling.x": tiling[0], "tiling.y": tiling[1],
        }

    def to_hsd(self):
        order = self._graph_order()
        return {
            "Hesiod version": HESIOD_VERSION,
            "graph_manager": {
                "export_param": self._export_param(),
                "graph_nodes": {gid: self._graphs[gid]._model() for gid in order},
                "graph_order": order,
                "id_count": 0,
            },
            "graph_manager_widget": {
                "frames": {gid: {"current_bg_tag": "NONE"} for gid in order}},
            "graph_tabs_widget": {
                "graph_node_widgets": {gid: self._graphs[gid]._ui() for gid in order}},
            "saved_at": SAVED_AT,
        }
