import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.spec import Spec, SpecError

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_parse_fixture():
    spec = Spec.from_file(FIX)
    assert spec.config["shape"] == [1024, 1024]
    # a flat (single-graph) spec parses into one graph with id "graph"
    assert [g.id for g in spec.graphs] == ["graph"]
    g = spec.graphs[0]
    assert [n.id for n in g.nodes] == ["noise", "ero", "col"]
    assert g.nodes[0].type == "NoiseFbm"
    assert g.nodes[0].params == {"kw": [4, 4], "seed": 1}
    assert g.links[0] == ("noise", "output", "ero", "input")
    # the flat export list folds into a FlattenSpec keyed by ("graph", node, port)
    assert spec.flatten.ids[0] == ("graph", "ero", "output")
    assert spec.flatten.legacy_paths[0] == "terrain.png"


def test_malformed_link_raises():
    bad = {"nodes": [{"id": "a", "type": "X"}], "links": [["a", "b.in"]]}
    try:
        Spec.from_dict(bad)
        assert False, "expected SpecError"
    except SpecError as e:
        assert "endpoint" in str(e).lower()


def test_missing_node_id_raises():
    try:
        Spec.from_dict({"nodes": [{"type": "X"}]})
        assert False, "expected SpecError"
    except SpecError as e:
        assert "id" in str(e)


def test_link_not_a_pair_raises():
    try:
        Spec.from_dict({"nodes": [{"id": "a", "type": "X"}],
                        "links": [["a.out"]]})
        assert False, "expected SpecError"
    except SpecError as e:
        assert "pair" in str(e).lower()


def test_export_missing_key_raises():
    try:
        Spec.from_dict({"nodes": [{"id": "a", "type": "X"}],
                        "export": [{"node": "a", "port": "output"}]})
        assert False, "expected SpecError"
    except SpecError as e:
        assert "export" in str(e).lower()
