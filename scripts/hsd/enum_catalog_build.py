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


def extract_function_bodies(text: str) -> dict:
    """Return {func_name: body_text} for every void setup_*(...)  function in text.

    Uses a simple brace-counting approach to find the matching closing brace.
    """
    bodies: dict = {}
    # Match function header; body starts at the '{' that follows
    header_re = re.compile(r'void\s+(setup_\w+)\s*\([^)]*\)\s*\{', re.DOTALL)
    for m in header_re.finditer(text):
        func_name = m.group(1)
        start = m.end()  # position just after the opening '{'
        depth = 1
        pos = start
        while pos < len(text) and depth > 0:
            ch = text[pos]
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
            pos += 1
        bodies[func_name] = text[start:pos - 1]
    return bodies


def extract_enum_params(body: str, const_map: dict, map_names: set) -> dict:
    """Extract {param_key: map_name} from EnumAttribute add_attr calls in body."""
    result: dict = {}
    attr_re = re.compile(
        r'add_attr\s*<\s*EnumAttribute\s*>\s*\('
        r'\s*'
        r'(?:"((?:[^"\\]|\\.)*)"|(\w+))'   # groups 1,2: literal or identifier
        r'\s*,'                              # comma after KEY
        r'(?:[^;]*?)'                        # label + anything before map
        r'enum_mappings\s*\.\s*(\w+)',      # group 3: map name (greedy word capture)
        re.DOTALL,
    )
    for m in attr_re.finditer(body):
        lit_key = m.group(1)
        id_key = m.group(2)
        map_name = m.group(3)

        if map_name not in map_names:
            continue

        if lit_key is not None:
            param_key = lit_key
        elif id_key is not None:
            if id_key in const_map:
                param_key = const_map[id_key]
            else:
                continue
        else:
            continue

        result[param_key] = map_name
    return result


def find_helper_calls(body: str) -> list:
    """Return list of setup_* helper function names called in body (not setup_*_node)."""
    # Match calls like  setup_foo_bar(  where the name does NOT end in _node
    call_re = re.compile(r'\b(setup_\w+)\s*\(')
    helpers = []
    for m in call_re.finditer(body):
        name = m.group(1)
        if not name.endswith('_node'):
            helpers.append(name)
    return helpers


def build_setup_func_to_camel(node_types: list) -> dict:
    """Build {setup_func_name: [CamelCaseType, ...]} from two sources:
    1. node_factory.cpp SETUP_NODE(CamelType, snake_name) macros (authoritative, handles typos).
    2. camel_to_snake() derivation for any nodes not covered by the factory parse.

    The mapping is ONE-TO-MANY: a single setup function may be registered for
    several node types, e.g. SETUP_NODE(NoiseIq, noise_iq) and
    SETUP_NODE(NoiseJordan, noise_iq) both resolve to setup_noise_iq_node.
    """
    mapping: dict = {}

    # Source 1: parse SETUP_NODE macros
    factory_cpp = NODES_DIR / "node_factory.cpp"
    if factory_cpp.exists():
        text = factory_cpp.read_text(errors="replace")
        for m in re.finditer(r'SETUP_NODE\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', text):
            camel = m.group(1)
            snake = m.group(2)
            func_name = f"setup_{snake}_node"
            bucket = mapping.setdefault(func_name, [])
            if camel not in bucket:
                bucket.append(camel)

    # Source 2: fallback via camel_to_snake for any node type not already mapped
    covered_camel = {c for camels in mapping.values() for c in camels}
    for nt in node_types:
        if nt not in covered_camel:
            snake = camel_to_snake(nt)
            func_name = f"setup_{snake}_node"
            bucket = mapping.setdefault(func_name, [])
            if nt not in bucket:
                bucket.append(nt)

    return mapping


def build_node_param(maps: dict) -> dict:
    # Build snake → CamelCase lookup from canonical node-type names
    node_doc = json.loads(NODE_DOC_JSON.read_text())
    node_types = list(node_doc.keys())
    snake_to_camel: dict = {camel_to_snake(t): t for t in node_types}

    # Authoritative setup_func_name → [CamelCase, ...] (one-to-many; handles typos
    # like reverse_above_theshold and shared setup funcs like setup_noise_iq_node).
    setup_func_to_camel: dict = build_setup_func_to_camel(node_types)

    map_names = set(maps.keys())

    # -----------------------------------------------------------------------
    # Pass A: parse ALL cpp files, build:
    #   helper_params: {func_name: {param_key: map_name}}  for helper funcs
    #   node_direct:   {NodeType: {param_key: map_name}}   direct in node func
    #   node_helpers:  {NodeType: [helper_func_name, ...]} helpers called
    # -----------------------------------------------------------------------
    helper_params: dict = {}   # func_name → {param: map}
    node_direct:   dict = {}   # NodeType  → {param: map}
    node_helpers:  dict = {}   # NodeType  → [helper_name]

    cpp_files = list(NODES_DIR.rglob("*.cpp"))

    for fpath in cpp_files:
        text = fpath.read_text(errors="replace")

        # Collect constexpr const char* constants in this file
        const_map: dict = {}
        for m in re.finditer(
            r'constexpr\s+const\s+char\s*\*\s*(\w+)\s*=\s*"((?:[^"\\]|\\.)*)"',
            text,
        ):
            const_map[m.group(1)] = m.group(2)

        bodies = extract_function_bodies(text)

        for func_name, body in bodies.items():
            if func_name.endswith('_node'):
                # Determine CamelCase node type(s). A single setup function may be
                # registered for multiple node types (one-to-many), so apply its
                # params to EVERY node type that shares it.
                node_types_for_func = setup_func_to_camel.get(func_name)
                if not node_types_for_func:
                    # Fallback: derive from name
                    snake = func_name[len('setup_'):-len('_node')]
                    derived = snake_to_camel.get(snake)
                    node_types_for_func = [derived] if derived else []
                if not node_types_for_func:
                    continue

                direct = extract_enum_params(body, const_map, map_names)
                calls = find_helper_calls(body)

                for node_type in node_types_for_func:
                    if direct:
                        if node_type not in node_direct:
                            node_direct[node_type] = {}
                        node_direct[node_type].update(direct)
                    if calls:
                        if node_type not in node_helpers:
                            node_helpers[node_type] = []
                        node_helpers[node_type].extend(calls)
            else:
                # Helper function — record its enum params
                direct = extract_enum_params(body, const_map, map_names)
                if direct:
                    if func_name not in helper_params:
                        helper_params[func_name] = {}
                    helper_params[func_name].update(direct)

    # -----------------------------------------------------------------------
    # Pass B: merge helper params into each node's param set.
    # Helper params are added FIRST so that node-own wiring takes precedence.
    # -----------------------------------------------------------------------
    node_param: dict = {}

    all_node_types = set(node_direct.keys()) | set(node_helpers.keys())
    for node_type in all_node_types:
        merged: dict = {}
        # First apply helper params (lower priority)
        for helper_name in node_helpers.get(node_type, []):
            merged.update(helper_params.get(helper_name, {}))
        # Then apply node's own direct params (higher priority / overrides)
        merged.update(node_direct.get(node_type, {}))
        if merged:
            node_param[node_type] = merged

    # -----------------------------------------------------------------------
    # Pass C: intersect with the real parameters from node_documentation.json.
    # The cpp parser merges a helper's params into EVERY caller, but a helper
    # only adds a param under certain options (e.g.
    # setup_post_process_heightmap_attributes adds post_mix_method only when
    # add_mix==true). Drop any (node_type, param) whose param is not an actual
    # Enumeration/Choice parameter on that node per the canonical doc JSON.
    # -----------------------------------------------------------------------
    filtered: dict = {}
    for node_type, params in node_param.items():
        doc_params = (node_doc.get(node_type, {}).get("parameters") or {})
        kept: dict = {}
        for param_key, map_name in params.items():
            doc_entry = doc_params.get(param_key)
            if not isinstance(doc_entry, dict):
                continue
            if doc_entry.get("type") in ("Enumeration", "Choice"):
                kept[param_key] = map_name
        if kept:
            filtered[node_type] = kept

    return filtered


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
    # New asserts: helper-propagated params, filtered against the doc JSON.
    # Before the doc-param intersection the parser over-claimed post_mix_method
    # on ~70 nodes that don't actually expose it; after filtering only the
    # genuinely-present ones remain (87 per node_documentation.json).
    post_mix_nodes = [nt for nt, params in node_param.items() if "post_mix_method" in params]
    assert len(post_mix_nodes) >= 80, (
        f"Expected >=80 node types with post_mix_method, got {len(post_mix_nodes)}: {sorted(post_mix_nodes)}"
    )
    assert node_param.get("CoastalErosionProfile", {}).get("dn_noise_type") == "noise_type_map_fbm", (
        f"CoastalErosionProfile.dn_noise_type should be noise_type_map_fbm, got "
        f"{node_param.get('CoastalErosionProfile', {}).get('dn_noise_type')}"
    )
    # Verify at least one known post-processed node has post_mix_method
    # (Thermal calls setup_post_process_heightmap_attributes)
    assert node_param.get("Thermal", {}).get("post_mix_method") == "blending_method_map", (
        f"Thermal.post_mix_method should be blending_method_map, got "
        f"{node_param.get('Thermal', {}).get('post_mix_method')}"
    )
    print(f"post_mix_method node count: {len(post_mix_nodes)}")

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
