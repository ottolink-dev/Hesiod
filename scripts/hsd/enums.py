import json
from pathlib import Path

_DEFAULT = Path(__file__).resolve().parent / "data" / "enum_catalog.json"


class EnumError(Exception):
    pass


class EnumCatalog:
    def __init__(self, maps, node_param):
        self._maps = maps
        self._node_param = node_param

    @classmethod
    def load(cls, path=None):
        path = Path(path) if path else _DEFAULT
        if not path.exists():
            raise FileNotFoundError(
                f"enum catalog not found at {path}; run enum_catalog_build.py")
        with open(path) as f:
            d = json.load(f)
        return cls(d["maps"], d["node_param"])

    def map_for(self, node_type, param):
        """Return the map name for (node_type, param), or None if not catalogued."""
        return self._node_param.get(node_type, {}).get(param)

    def choices(self, node_type, param):
        """Sorted list of valid choice strings for (node_type, param), or None."""
        m = self.map_for(node_type, param)
        if m is None:
            return None
        return sorted(self._maps.get(m, {}))

    def resolve(self, node_type, param, choice):
        """Resolve a choice STRING to its integer value.

        Returns None if (node_type, param) is not catalogued (caller falls back to
        requiring a full value-object dict). Raises EnumError if the param IS
        catalogued but `choice` is not a valid option (message lists valid choices).
        """
        m = self.map_for(node_type, param)
        if m is None:
            return None
        table = self._maps.get(m, {})
        if choice not in table:
            valid = ", ".join(sorted(table))
            raise EnumError(
                f"invalid choice {choice!r} for {node_type}.{param}; "
                f"valid options: {valid}")
        return table[choice]
