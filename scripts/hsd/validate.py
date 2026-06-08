import json


def _err(level, problem, suggestion="", node_id=None, link=None):
    return {"level": level, "node_id": node_id, "link": link,
            "problem": problem, "suggestion": suggestion}


def validate_spec(spec, catalog):
    """Return a list of structured error dicts; empty list means valid."""
    errors = []
    ids = {}

    # Load enum catalog once; if absent, skip enum-choice validation (no false positives)
    try:
        from hsd.enums import EnumCatalog
        enum_catalog = EnumCatalog.load()
    except FileNotFoundError:
        enum_catalog = None

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
        for pname, pvalue in node.params.items():
            if pname not in params_meta:
                errors.append(_err(
                    "L1", f"unknown param '{pname}' on node type '{node.type}'",
                    f"run `hsd nodes --show {node.type}` to list its params",
                    node_id=node.id))
                continue
            # Enum-choice validation: only for string values on Enumeration params
            param_type = params_meta[pname].get("type", "")
            if (param_type == "Enumeration"
                    and isinstance(pvalue, str)
                    and enum_catalog is not None):
                if enum_catalog.map_for(node.type, pname) is not None:
                    valid = enum_catalog.choices(node.type, pname)
                    if pvalue not in valid:
                        errors.append(_err(
                            "L1",
                            f"invalid enum choice '{pvalue}' for param "
                            f"'{pname}' on node type '{node.type}'",
                            f"valid choices: {', '.join(valid)}",
                            node_id=node.id))

    # L2: links resolve + datatype compatibility
    known = {nid for nid, ntype in ids.items() if catalog.has_node(ntype)}
    for a, ap, b, bp in spec.links:
        link = (a, ap, b, bp)
        if a not in ids or b not in ids:
            errors.append(_err("L2", f"link references unknown node: {a} or {b}",
                               link=link))
            continue
        # an unknown node type already produced an L1 error; don't pile on a
        # misleading secondary "not an output/input port" for the same cause
        if a not in known or b not in known:
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
        if n not in known:        # unknown node type already reported at L1
            continue
        port = catalog.port(ids[n], p) if catalog.has_node(ids[n]) else None
        if port is None or port.get("type") != "output":
            errors.append(_err("L2", f"export target '{n}.{p}' is not an output port"))

    return errors


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
