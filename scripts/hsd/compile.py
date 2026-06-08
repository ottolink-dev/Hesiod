from hsd.builder import Graph
from hsd.params import value_object


def compile_spec(spec, catalog):
    """Compile a Spec into a Graph using the catalog for param typing.

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

    g = Graph(config=spec.config or {"shape": [1024, 1024], "tiling": [1, 1],
                                      "overlap": 0.0})
    for node in spec.nodes:
        params_meta = catalog.params(node.type)
        vobjs = {}
        for pname, pvalue in node.params.items():
            type_string = params_meta.get(pname, {}).get("type", "")
            vobjs[pname] = value_object(pname, type_string, pvalue,
                                        node_type=node.type,
                                        enum_catalog=enum_catalog)
        g.add_node(node.id, node.type, vobjs)

    for a, ap, b, bp in spec.links:
        g.link(a, ap, b, bp)
    for n, p, path in spec.exports:
        g.set_export(n, p, path)
    return g
