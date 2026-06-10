from hsd.builder import Graph, Project, DEFAULT_CONFIG
from hsd.params import value_object


def placement(graph_spec, grid):
    """Resolve (origin, size) from explicit fields or a grid cell.

    Grid math: cell [col, row] -> origin = [col*cw, row*ch], size = [cw, ch]
    with cw, ch = extent/dims. Orientation confirmed in Phase 0 (see design
    doc findings).
    """
    if graph_spec.cell is not None:
        dims, extent = grid["dims"], grid["extent"]
        cw, ch = extent[0] / dims[0], extent[1] / dims[1]
        col, row = graph_spec.cell
        return [col * cw, row * ch], [cw, ch]
    origin = graph_spec.origin if graph_spec.origin is not None else [0.0, 0.0]
    size = graph_spec.size if graph_spec.size is not None else [1.0, 1.0]
    return origin, size


def compile_spec(spec, catalog):
    """Compile a Spec into a Project using the catalog for param typing.

    Assumes the spec has already passed validation (see hsd.validate). Missing
    nodes/params here raise KeyError-style errors; call validate first for
    friendly messages.
    """
    # Load enum catalog once; guard FileNotFoundError so graphs without string
    # enum params still compile even if the catalog is absent.
    try:
        from hsd.enums import EnumCatalog
        enum_catalog = EnumCatalog.load()
    except FileNotFoundError:
        enum_catalog = None

    base_config = {**DEFAULT_CONFIG, **(spec.config or {})}
    project = Project(config=base_config)

    for gs in spec.graphs:
        origin, size = placement(gs, spec.grid)
        g = Graph(config={**base_config, **(gs.config or {})},
                  graph_id=gs.id, origin=origin, size=size)
        for node in gs.nodes:
            params_meta = catalog.params(node.type)
            vobjs = {}
            for pname, pvalue in node.params.items():
                type_string = params_meta.get(pname, {}).get("type", "")
                vobjs[pname] = value_object(pname, type_string, pvalue,
                                            node_type=node.type,
                                            enum_catalog=enum_catalog)
            g.add_node(node.id, node.type, vobjs)
        for a, ap, b, bp in gs.links:
            g.link(a, ap, b, bp)
        project.add_graph(g)

    for bc in spec.broadcasts:
        project.add_broadcast(bc.src_graph, bc.src_node, bc.src_port,
                              bc.dst_graph, bc.recv_name)

    if spec.flatten:
        project.set_flatten(spec.flatten)
    if spec.graph_order is not None:
        project.set_graph_order(spec.graph_order)
    return project
