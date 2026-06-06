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
