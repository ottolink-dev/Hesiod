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
