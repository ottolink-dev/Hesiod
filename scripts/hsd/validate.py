import json


def _err(level, problem, suggestion="", node_id=None, link=None, graph_id=None):
    return {"level": level, "node_id": node_id, "link": link,
            "graph_id": graph_id, "problem": problem, "suggestion": suggestion}


def blocking(errors):
    """True if any error should fail the build ('W' entries are warnings)."""
    return any(e["level"] != "W" for e in errors)


def validate_spec(spec, catalog):
    """Return a list of structured error dicts; empty list means valid.

    Levels: L1 node/param, L2 link/port (per graph), L3 project-level
    (graphs/placement/broadcasts/flatten), W non-fatal warning.
    """
    errors = []

    try:
        from hsd.enums import EnumCatalog
        enum_catalog = EnumCatalog.load()
    except FileNotFoundError:
        enum_catalog = None

    # --- L3: graph ids and placement -------------------------------------
    by_id = {}
    for gs in spec.graphs:
        if gs.id in by_id:
            errors.append(_err("L3", f"duplicate graph id '{gs.id}'", graph_id=gs.id))
        by_id.setdefault(gs.id, gs)
        if gs.cell is not None and (gs.origin is not None or gs.size is not None):
            errors.append(_err(
                "L3", f"graph '{gs.id}' sets both 'cell' and 'origin'/'size'",
                "use cell (grid placement) OR origin/size (explicit), not both",
                graph_id=gs.id))
        if gs.cell is not None:
            if spec.grid is None:
                errors.append(_err(
                    "L3", f"graph '{gs.id}' uses 'cell' but no top-level 'grid' is defined",
                    'add "grid": {"dims": [C, R], "extent": [W, H]}', graph_id=gs.id))
            else:
                dims = spec.grid["dims"]
                c, r = gs.cell
                if not (0 <= c < dims[0] and 0 <= r < dims[1]):
                    errors.append(_err(
                        "L3", f"graph '{gs.id}' cell {gs.cell} is outside grid dims {dims}",
                        graph_id=gs.id))

    # --- receive nodes injected by broadcasts, per destination graph -----
    recv_by_graph = {}
    for bc in spec.broadcasts:
        recv_by_graph.setdefault(bc.dst_graph, set())
        if bc.recv_name in recv_by_graph[bc.dst_graph]:
            errors.append(_err(
                "L3", f"duplicate receive name '{bc.recv_name}' in graph '{bc.dst_graph}'",
                graph_id=bc.dst_graph))
        recv_by_graph[bc.dst_graph].add(bc.recv_name)

    # --- per-graph L1/L2 ---------------------------------------------------
    for gs in spec.graphs:
        extra = {name: "Receive" for name in recv_by_graph.get(gs.id, set())}
        errors.extend(_validate_graph(gs, catalog, enum_catalog, extra))

    # --- L3: broadcasts ------------------------------------------------------
    for bc in spec.broadcasts:
        if bc.src_graph not in by_id:
            errors.append(_err("L3", f"broadcast source graph '{bc.src_graph}' does not exist"))
            # still check dst side below
        if bc.dst_graph not in by_id:
            errors.append(_err("L3", f"broadcast destination graph '{bc.dst_graph}' does not exist"))
            continue
        if bc.src_graph == bc.dst_graph:
            errors.append(_err(
                "L3", f"broadcast source and destination are the same graph '{bc.src_graph}'",
                "use a direct link instead; same-graph broadcasts have no defined update order",
                graph_id=bc.src_graph))
        if bc.recv_name in {n.id for n in by_id[bc.dst_graph].nodes}:
            errors.append(_err(
                "L3", f"receive name '{bc.recv_name}' collides with a declared node "
                      f"in graph '{bc.dst_graph}'",
                graph_id=bc.dst_graph))
        if bc.src_graph not in by_id:
            continue
        src_types = {n.id: n.type for n in by_id[bc.src_graph].nodes}
        if bc.src_node not in src_types:
            errors.append(_err(
                "L3", f"broadcast source node '{bc.src_graph}.{bc.src_node}' does not exist",
                graph_id=bc.src_graph))
            continue
        ntype = src_types[bc.src_node]
        if not catalog.has_node(ntype):
            continue   # already reported at L1
        port = catalog.port(ntype, bc.src_port)
        if port is None or port.get("type") != "output":
            errors.append(_err(
                "L3", f"broadcast source '{bc.src_node}.{bc.src_port}' is not an output port",
                graph_id=bc.src_graph))
        elif port["data_type"] != "VirtualArray":
            errors.append(_err(
                "L3", f"broadcast source '{bc.src_node}.{bc.src_port}' is "
                      f"{port['data_type']}; broadcasts carry VirtualArray only",
                graph_id=bc.src_graph))

    # --- L3: broadcast cycles -------------------------------------------------
    needs = {gs.id: set() for gs in spec.graphs}
    for bc in spec.broadcasts:
        if bc.src_graph in needs and bc.dst_graph in needs and bc.src_graph != bc.dst_graph:
            needs[bc.dst_graph].add(bc.src_graph)
    placed = set()
    while True:
        ready = [g for g in needs if g not in placed and needs[g] <= placed]
        if not ready:
            break
        placed.update(ready)
    stuck = sorted(set(needs) - placed)
    if stuck:
        errors.append(_err(
            "L3", f"broadcast cycle between graphs: {stuck}",
            "the engine evaluates graphs in order; a cycle cannot be satisfied"))

    # --- W: spatial sanity (Phase 0 findings) ---------------------------------
    # Receive resamples in WORLD coordinates: a broadcast into a graph whose
    # frame doesn't overlap the source frame receives only fill zeros. And the
    # flatten canvas is the union bbox of ALL graphs, stretched into the export
    # shape with no aspect correction.
    from hsd.compile import placement

    frames = {}
    for gs in spec.graphs:
        try:
            frames[gs.id] = placement(gs, spec.grid)
        except (KeyError, TypeError, ValueError, ZeroDivisionError, IndexError):
            pass   # malformed grid/cell already reported above

    def _overlaps(fa, fb):
        (ax, ay), (aw, ah) = fa
        (bx, by), (bw, bh) = fb
        return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah

    for bc in spec.broadcasts:
        fa, fb = frames.get(bc.src_graph), frames.get(bc.dst_graph)
        if fa and fb and not _overlaps(fa, fb):
            errors.append(_err(
                "W", f"broadcast '{bc.src_graph}' -> '{bc.dst_graph}': graph frames "
                     "do not overlap in world space; the Receive node will produce "
                     "only zeros (Receive resamples in world coordinates)",
                "make the source graph's origin/size cover the destination's"))

    if spec.flatten and spec.flatten.shape and frames:
        xs = [o[0] for o, s in frames.values()] + [o[0] + s[0] for o, s in frames.values()]
        ys = [o[1] for o, s in frames.values()] + [o[1] + s[1] for o, s in frames.values()]
        world_w, world_h = max(xs) - min(xs), max(ys) - min(ys)
        sx, sy = spec.flatten.shape
        if world_w > 0 and world_h > 0 and sy > 0 and abs(sx / sy - world_w / world_h) > 1e-6:
            errors.append(_err(
                "W", f"export shape {sx}x{sy} does not match the world bbox aspect "
                     f"({world_w:g}x{world_h:g}); the engine stretches with no "
                     "aspect correction (anisotropic pixels)",
                f"use a shape with aspect {world_w:g}:{world_h:g}"))

    # --- L3: flatten export -----------------------------------------------------
    if spec.flatten:
        if len(set(spec.flatten.legacy_paths)) > 1:
            errors.append(_err(
                "W", "export entries carry differing paths; the flatten export "
                     f"has a single path and uses '{spec.flatten.path}'",
                "use one path, or per-node Export* nodes for separate files"))
        for gid, name, port_id in spec.flatten.ids:
            if gid not in by_id:
                errors.append(_err("L3", f"export id references unknown graph '{gid}'"))
                continue
            types = {n.id: n.type for n in by_id[gid].nodes}
            for rname in recv_by_graph.get(gid, set()):
                types.setdefault(rname, "Receive")
            if name not in types:
                errors.append(_err(
                    "L3", f"export id references unknown node '{gid}.{name}'", graph_id=gid))
                continue
            if not catalog.has_node(types[name]):
                continue   # already reported at L1
            port = catalog.port(types[name], port_id)
            if port is None or port.get("type") != "output":
                errors.append(_err(
                    "L3", f"export target '{gid}.{name}.{port_id}' is not an output port",
                    graph_id=gid))

    return errors


def _validate_graph(gs, catalog, enum_catalog, extra_nodes):
    """L1/L2 checks for one graph. extra_nodes maps injected node names
    (broadcast receives) to their node type."""
    errors = []
    ids = {}

    # L1: node types and params
    for node in gs.nodes:
        if node.id in ids:
            errors.append(_err("L1", f"duplicate node id '{node.id}'",
                               node_id=node.id, graph_id=gs.id))
        ids[node.id] = node.type
        if not catalog.has_node(node.type):
            errors.append(_err("L1", f"unknown node type '{node.type}'",
                               "run `hsd nodes --search <term>` to find a valid type",
                               node_id=node.id, graph_id=gs.id))
            continue
        params_meta = catalog.params(node.type)
        for pname, pvalue in node.params.items():
            if pname not in params_meta:
                errors.append(_err(
                    "L1", f"unknown param '{pname}' on node type '{node.type}'",
                    f"run `hsd nodes --show {node.type}` to list its params",
                    node_id=node.id, graph_id=gs.id))
                continue
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
                            node_id=node.id, graph_id=gs.id))
                else:
                    errors.append(_err(
                        "L1",
                        f"enum param '{pname}' on node type '{node.type}' is not "
                        f"auto-resolvable from the string '{pvalue}'",
                        f"pass a full value-object dict for '{pname}' instead "
                        f"(run `hsd nodes --show {node.type}` to inspect it)",
                        node_id=node.id, graph_id=gs.id))

    for k, v in extra_nodes.items():
        ids.setdefault(k, v)

    # L2: links resolve + datatype compatibility
    known = {nid for nid, ntype in ids.items() if catalog.has_node(ntype)}
    for a, ap, b, bp in gs.links:
        link = (a, ap, b, bp)
        if a not in ids or b not in ids:
            errors.append(_err("L2", f"link references unknown node: {a} or {b}",
                               link=link, graph_id=gs.id))
            continue
        if a not in known or b not in known:
            continue   # unknown node type already produced an L1 error
        out = catalog.port(ids[a], ap)
        inp = catalog.port(ids[b], bp)
        if out is None or out.get("type") != "output":
            errors.append(_err("L2", f"'{a}.{ap}' is not an output port",
                               link=link, graph_id=gs.id))
            continue
        if inp is None or inp.get("type") != "input":
            errors.append(_err("L2", f"'{b}.{bp}' is not an input port",
                               link=link, graph_id=gs.id))
            continue
        if out["data_type"] != inp["data_type"]:
            errors.append(_err(
                "L2",
                f"incompatible data type: {a}.{ap} is {out['data_type']} but "
                f"{b}.{bp} is {inp['data_type']}",
                "Colorize* converts VirtualArray->VirtualTexture; insert one if "
                "bridging heightmap to colour",
                link=link, graph_id=gs.id))

    return errors


def _model_view(hsd, gid):
    g = hsd["graph_manager"]["graph_nodes"][gid]
    nodes = {n["id"]: n["label"] for n in g["nodes"]}
    links = sorted((l["node_id_from"], l["port_id_from"],
                    l["node_id_to"], l["port_id_to"]) for l in g["links"])
    return nodes, links


def _ui_view(hsd, gid):
    g = hsd["graph_tabs_widget"]["graph_node_widgets"][gid]
    nodes = {n["id"]: n["caption"] for n in g["nodes"]}
    links = sorted((l["node_out_id"], l["port_out_id"],
                    l["node_in_id"], l["port_in_id"]) for l in g["links"])
    return nodes, links


def consistency_errors(hsd):
    """Pure-stdlib model<->UI consistency check, all graphs."""
    errors = []
    model_ids = set(hsd["graph_manager"]["graph_nodes"])
    ui_ids = set(hsd["graph_tabs_widget"]["graph_node_widgets"])
    if model_ids != ui_ids:
        errors.append(f"graph id mismatch between model and UI: "
                      f"model={sorted(model_ids)} ui={sorted(ui_ids)}")
    for gid in sorted(model_ids & ui_ids):
        mn, ml = _model_view(hsd, gid)
        un, ul = _ui_view(hsd, gid)
        if mn != un:
            errors.append(f"[{gid}] node mismatch between model and UI: "
                          f"model={mn} ui={un}")
        if ml != ul:
            errors.append(f"[{gid}] link mismatch between model and UI: "
                          f"model={ml} ui={ul}")
    return errors


def lint_file(path):
    with open(path) as f:
        hsd = json.load(f)
    return {"consistency": consistency_errors(hsd)}
