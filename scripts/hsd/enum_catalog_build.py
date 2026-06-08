"""
enum_catalog_build.py  —  generate scripts/hsd/data/enum_catalog.json

Sources:
  1. scripts/hsd/data/highmap_enum_values.json   — int values per enumerator
  2. Hesiod/include/hesiod/app/enum_mappings.hpp  — 24 choice→symbol maps
  3. Hesiod/src/model/nodes/**/*.cpp              — node param→map associations
  4. Hesiod/data/node_documentation.json          — canonical node-type names

Output shape:
  {
    "maps":       { map_name: { choice: int, ... }, ... },
    "node_param": { NodeType: { param_key: map_name, ... }, ... }
  }
"""

import json
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Repo root (this file lives at scripts/hsd/enum_catalog_build.py)
# ---------------------------------------------------------------------------
REPO = Path(__file__).resolve().parent.parent.parent
DATA_DIR = REPO / "scripts" / "hsd" / "data"
HIGHMAP_JSON = DATA_DIR / "highmap_enum_values.json"
ENUM_MAPPINGS_HPP = REPO / "Hesiod" / "include" / "hesiod" / "app" / "enum_mappings.hpp"
NODES_DIR = REPO / "Hesiod" / "src" / "model" / "nodes"
NODE_DOC_JSON = REPO / "Hesiod" / "data" / "node_documentation.json"
OUT_JSON = DATA_DIR / "enum_catalog.json"


# ---------------------------------------------------------------------------
# Step 1: build int-value table  {EnumTypeName: {ENUMERATOR: int}}
# ---------------------------------------------------------------------------
def build_int_table() -> dict:
    raw = json.loads(HIGHMAP_JSON.read_text())
    table: dict = {}
    for key, val in raw.items():
        if key.startswith("_"):
            if key == "_HesiodLocal":
                # val is { EnumType: {ENUMERATOR: int}, "_note": "..." }
                for subkey, subval in val.items():
                    if subkey.startswith("_"):
                        continue
                    assert isinstance(subval, dict)
                    table[subkey] = subval
        else:
            assert isinstance(val, dict)
            table[key] = val
    return table


# ---------------------------------------------------------------------------
# Step 2: parse enum_mappings.hpp → maps  {map_name: {choice: int}}
# ---------------------------------------------------------------------------
def parse_enum_mappings(int_table: dict) -> dict:
    text = ENUM_MAPPINGS_HPP.read_text()

    # Find each  <name>_map[...] = { ... };  block.
    # The outer regex captures names that contain "_map" (covers noise_type_map_fbm too).
    map_blocks = re.findall(
        r'(\w*_map\w*)\s*=\s*\{(.*?)\};',
        text,
        re.DOTALL,
    )

    maps: dict = {}
    unresolved: list = []

    # Entry regex: {"choice string", Symbol::BOL}
    # Allow digits in the symbol so F1_SQUARED, SIMPLEX2, etc. resolve.
    entry_re = re.compile(
        r'\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*([A-Za-z0-9_:]+)\s*\}'
    )

    for map_name, block in map_blocks:
        choices: dict = {}
        for m in entry_re.finditer(block):
            choice = m.group(1)
            symbol = m.group(2)
            # Split on ::; take last two parts as (EnumType, ENUMERATOR)
            parts = symbol.split("::")
            if len(parts) < 2:
                unresolved.append((map_name, choice, symbol))
                continue
            enum_type = parts[-2]
            enumerator = parts[-1]
            if enum_type not in int_table:
                unresolved.append((map_name, choice, symbol))
                continue
            if enumerator not in int_table[enum_type]:
                unresolved.append((map_name, choice, symbol))
                continue
            choices[choice] = int_table[enum_type][enumerator]
        if choices:
            maps[map_name] = choices

    if unresolved:
        print("ERROR: unresolved symbols:", file=sys.stderr)
        for item in unresolved:
            print(f"  {item}", file=sys.stderr)
        sys.exit(1)

    return maps


# ---------------------------------------------------------------------------
# Step 3: parse node cpp files → node_param associations
# ---------------------------------------------------------------------------
def camel_to_snake(name: str) -> str:
    """CamelCase → snake_case (no digit boundaries, keeping digits attached)."""
    s = re.sub(r'([a-z])([A-Z])', r'\1_\2', name)
    s = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', s)
    return s.lower()


def build_node_param(maps: dict) -> dict:
    # Build snake → CamelCase lookup from canonical node-type names
    node_types = list(json.loads(NODE_DOC_JSON.read_text()).keys())
    snake_to_camel: dict = {camel_to_snake(t): t for t in node_types}

    map_names = set(maps.keys())

    node_param: dict = {}

    cpp_files = list(NODES_DIR.rglob("*.cpp"))

    for fpath in cpp_files:
        text = fpath.read_text(errors="replace")

        # Only process files that reference enum_mappings
        if "enum_mappings." not in text:
            continue

        # Collect constexpr const char* constants in this file
        const_map: dict = {}
        for m in re.finditer(
            r'constexpr\s+const\s+char\s*\*\s*(\w+)\s*=\s*"((?:[^"\\]|\\.)*)"',
            text,
        ):
            const_map[m.group(1)] = m.group(2)

        # Find which node type(s) this file defines via setup_<snake>_node
        node_types_in_file: list = []
        for m in re.finditer(r'void\s+setup_(\w+)_node\s*\(', text):
            snake = m.group(1)
            if snake in snake_to_camel:
                node_types_in_file.append(snake_to_camel[snake])

        if not node_types_in_file:
            continue

        # Find every add_attr<EnumAttribute>(..., enum_mappings.<map_name>...) call.
        # The call may span multiple lines, so search the whole file text.
        # We capture: the KEY argument (first arg, after the opening paren)
        # and the map_name (after "enum_mappings.").
        # Pattern: add_attr<EnumAttribute>( KEY , ... , enum_mappings.MAPNAME ...
        # KEY is either "literal" or an identifier (A_XXX constant).
        attr_re = re.compile(
            r'add_attr\s*<\s*EnumAttribute\s*>\s*\('
            r'\s*'
            r'(?:"((?:[^"\\]|\\.)*)"|(\w+))'   # groups 1,2: literal or identifier
            r'\s*,'                              # comma after KEY
            r'(?:[^;]*?)'                        # label + anything before map
            r'enum_mappings\s*\.\s*(\w+_map)',  # group 3: map name
            re.DOTALL,
        )

        for m in attr_re.finditer(text):
            lit_key = m.group(1)
            id_key = m.group(2)
            map_name = m.group(3)

            if map_name not in map_names:
                continue  # skip maps not in our catalog

            # Resolve the param key
            if lit_key is not None:
                param_key = lit_key
            elif id_key is not None:
                if id_key in const_map:
                    param_key = const_map[id_key]
                else:
                    # Could not resolve — skip
                    print(
                        f"  NOTE: unresolved key identifier {id_key!r} in {fpath.name}",
                        file=sys.stderr,
                    )
                    continue
            else:
                continue

            for node_type in node_types_in_file:
                if node_type not in node_param:
                    node_param[node_type] = {}
                node_param[node_type][param_key] = map_name

    return node_param


# ---------------------------------------------------------------------------
# Write output
# ---------------------------------------------------------------------------
def write_catalog(maps: dict, node_param: dict) -> None:
    catalog = {
        "maps": {k: dict(sorted(v.items())) for k, v in sorted(maps.items())},
        "node_param": {
            k: dict(sorted(v.items())) for k, v in sorted(node_param.items())
        },
    }
    OUT_JSON.write_text(json.dumps(catalog, indent=2, sort_keys=True) + "\n")
    print(f"Written: {OUT_JSON}")


# ---------------------------------------------------------------------------
# Acceptance validation (run after writing the catalog)
# ---------------------------------------------------------------------------
def validate(catalog: dict) -> None:
    import glob as _glob

    maps = catalog["maps"]
    node_param = catalog["node_param"]

    hsd_pattern_list = (
        list(
            Path(REPO / "Hesiod" / "data" / "examples").glob("*.hsd")
        )
        + list(
            Path(REPO / "Hesiod" / "data" / "bootstraps").glob("*.hsd")
        )
        + [REPO / "Hesiod" / "data" / "default.hsd"]
    )

    covered = 0
    mismatch = 0
    skipped = 0
    skipped_pairs: set = set()
    mismatch_list: list = []

    def iter_nodes(fpath: Path):
        """Yield node dicts from an hsd file."""
        try:
            d = json.loads(fpath.read_text())
        except Exception:
            return
        gm = d.get("graph_manager", {})
        for graph_name, graph in gm.get("graph_nodes", {}).items():
            for node_data in graph.get("nodes", []):
                if isinstance(node_data, dict):
                    yield node_data

    for hsd_file in hsd_pattern_list:
        if not hsd_file.exists():
            continue
        for node_data in iter_nodes(hsd_file):
            node_label = node_data.get("label")
            if not node_label:
                continue
            for param_key, attr_val in node_data.items():
                if not isinstance(attr_val, dict):
                    continue
                if attr_val.get("type_string") != "Enumeration":
                    continue
                choice = attr_val.get("choice")
                value = attr_val.get("value")
                if choice is None or value is None:
                    continue
                # Look up in catalog
                np_entry = node_param.get(node_label, {})
                map_name = np_entry.get(param_key)
                if map_name is None:
                    skipped += 1
                    skipped_pairs.add((node_label, param_key))
                    continue
                # Verify
                map_dict = maps.get(map_name, {})
                resolved = map_dict.get(choice)
                if resolved is None:
                    # choice not in map (shouldn't happen if catalog is correct)
                    skipped += 1
                    skipped_pairs.add((node_label, param_key))
                    continue
                if resolved == value:
                    covered += 1
                else:
                    mismatch += 1
                    mismatch_list.append(
                        {
                            "file": str(hsd_file.name),
                            "node": node_label,
                            "param": param_key,
                            "map": map_name,
                            "choice": choice,
                            "expected_value": value,
                            "got_value": resolved,
                        }
                    )

    # ---- Sanity asserts ----
    assert maps["blending_method_map"]["maximum"] == 3, (
        f"blending_method_map.maximum should be 3, got {maps['blending_method_map'].get('maximum')}"
    )
    assert maps["stamping_blend_method_map"]["maximum"] == 1, (
        f"stamping_blend_method_map.maximum should be 1, got {maps['stamping_blend_method_map'].get('maximum')}"
    )
    assert maps["noise_type_map"]["OpenSimplex2"] == 4, (
        f"noise_type_map.OpenSimplex2 should be 4, got {maps['noise_type_map'].get('OpenSimplex2')}"
    )
    assert node_param.get("Blend", {}).get("blending_method") == "blending_method_map", (
        f"Blend.blending_method should map to blending_method_map, got {node_param.get('Blend', {}).get('blending_method')}"
    )
    assert node_param.get("Stamping", {}).get("blend_method") == "stamping_blend_method_map", (
        f"Stamping.blend_method should map to stamping_blend_method_map, got {node_param.get('Stamping', {}).get('blend_method')}"
    )

    total_choices = sum(len(v) for v in maps.values())

    print("\n===== ACCEPTANCE VALIDATION =====")
    print(f"Maps count:          {len(maps)}  (expect 23; hpp has 23 _map entries)")
    print(f"Total choice→int:    {total_choices}")
    print(f"Covered triples:     {covered}")
    print(f"Skipped triples:     {skipped}")
    print(f"MISMATCH count:      {mismatch}  (require 0)")
    print()
    if skipped_pairs:
        print("Skipped (node, param) pairs:")
        for pair in sorted(skipped_pairs):
            print(f"  {pair[0]!r} / {pair[1]!r}")
    if mismatch_list:
        print("\nMISMATCHES:")
        for item in mismatch_list:
            print(f"  {item}")
    print()
    print("Sanity asserts: PASS")

    if mismatch > 0:
        print(f"\nFAIL: {mismatch} mismatches — do NOT commit", file=sys.stderr)
        sys.exit(1)

    print("RESULT: OK — 0 mismatches")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    print("Step 1: building int table...")
    int_table = build_int_table()
    print(f"  {len(int_table)} enum types loaded")

    print("Step 2: parsing enum_mappings.hpp...")
    maps = parse_enum_mappings(int_table)
    print(f"  {len(maps)} maps parsed")

    print("Step 3: parsing node cpp files...")
    node_param = build_node_param(maps)
    print(f"  {len(node_param)} node types with enum params")

    print("Step 4: writing catalog...")
    write_catalog(maps, node_param)

    print("Step 5: running acceptance validation...")
    catalog = json.loads(OUT_JSON.read_text())
    validate(catalog)
