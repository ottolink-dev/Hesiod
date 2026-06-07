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
            "origin": [0.0, 0.0],
            "rotation_angle": 0.0,
            "size": [1.0, 1.0],
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
                "graph_order": ["graph"],
                "id_count": 0,
            },
            "graph_manager_widget": {"frames": {"graph": {"current_bg_tag": "NONE"}}},
            "graph_tabs_widget": {"graph_node_widgets": {"graph": self._ui()}},
            "saved_at": SAVED_AT,
        }
