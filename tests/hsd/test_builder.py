import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.builder import Graph, Project
from hsd.spec import FlattenSpec


def _consistent(hsd):
    """model nodes/links must match UI nodes/links (ids + types)."""
    g = hsd["graph_manager"]["graph_nodes"]["graph"]
    ui = hsd["graph_tabs_widget"]["graph_node_widgets"]["graph"]
    model_nodes = {n["id"]: n["label"] for n in g["nodes"]}
    ui_nodes = {n["id"]: n["caption"] for n in ui["nodes"]}
    assert model_nodes == ui_nodes
    model_links = {(l["node_id_from"], l["port_id_from"],
                    l["node_id_to"], l["port_id_to"]) for l in g["links"]}
    ui_links = {(l["node_out_id"], l["port_out_id"],
                 l["node_in_id"], l["port_in_id"]) for l in ui["links"]}
    assert model_links == ui_links


def _project(graph, config=None):
    """Wrap a single graph in a Project (export is a project-level concern now)."""
    p = Project(config=config if config is not None else graph.config)
    p.add_graph(graph)
    return p


def test_build_minimal_graph():
    cfg = {"shape": [512, 512], "tiling": [1, 1], "overlap": 0.0}
    g = Graph(config=cfg)
    g.add_node("noise", "NoiseFbm", {"kw": {"label": "kw", "type": 17,
               "type_string": "Wavenumber", "value": [4, 4]}})
    g.add_node("exp", "ExportHeightmap", {})
    g.link("noise", "output", "exp", "input")
    p = _project(g, config=cfg)
    p.set_flatten(FlattenSpec("out.png", [("graph", "noise", "output")]))
    hsd = p.to_hsd()

    gm = hsd["graph_manager"]["graph_nodes"]["graph"]
    assert len(gm["nodes"]) == 2
    assert {n["label"] for n in gm["nodes"]} == {"NoiseFbm", "ExportHeightmap"}
    assert gm["model_config"]["shape.x"] == 512
    # export wired to the model id of the 'noise' node
    noise_id = next(n["id"] for n in gm["nodes"] if n["label"] == "NoiseFbm")
    ep = hsd["graph_manager"]["export_param"]
    assert ep["ids"] == [["graph", noise_id, "output"]]
    assert ep["export_path"] == "out.png"
    _consistent(hsd)


def test_id_count_is_next_free():
    g = Graph()
    g.add_node("a", "Abs", {})
    g.add_node("b", "Abs", {})
    hsd = _project(g).to_hsd()
    assert hsd["graph_manager"]["graph_nodes"]["graph"]["id_count"] == 3


def test_partial_config_fills_defaults():
    # a config missing shape/tiling must not KeyError; defaults fill the gaps
    g = Graph(config={"overlap": 0.5})
    g.add_node("n", "Abs", {})
    hsd = _project(g).to_hsd()
    mc = hsd["graph_manager"]["graph_nodes"]["graph"]["model_config"]
    assert mc["shape.x"] == 1024 and mc["shape.y"] == 1024
    assert mc["tiling.x"] == 1 and mc["tiling.y"] == 1
    assert mc["overlap"] == 0.5
