import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.spec import Spec, SpecError

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_parse_fixture():
    spec = Spec.from_file(FIX)
    assert spec.config["shape"] == [1024, 1024]
    assert [n.id for n in spec.nodes] == ["noise", "ero", "col"]
    assert spec.nodes[0].type == "NoiseFbm"
    assert spec.nodes[0].params == {"kw": [4, 4], "seed": 1}
    assert spec.links[0] == ("noise", "output", "ero", "input")
    assert spec.exports[0] == ("ero", "output", "terrain.png")


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
    except SpecError:
        pass
