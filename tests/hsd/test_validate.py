import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.validate import validate_spec

CAT = Catalog.load()
FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_good_spec_has_no_errors():
    assert validate_spec(Spec.from_file(FIX), CAT) == []


def test_unknown_node_type():
    spec = Spec.from_dict({"nodes": [{"id": "x", "type": "Nope"}]})
    errs = validate_spec(spec, CAT)
    assert any(e["level"] == "L1" and "unknown node type" in e["problem"].lower()
               for e in errs)


def test_unknown_param():
    spec = Spec.from_dict({"nodes": [{"id": "n", "type": "NoiseFbm",
                                      "params": {"not_a_param": 1}}]})
    errs = validate_spec(spec, CAT)
    assert any("not_a_param" in e["problem"] for e in errs)


def test_incompatible_link_datatype():
    # ColorizeGradient.texture is VirtualTexture; ExportHeightmap.input is VirtualArray
    spec = Spec.from_dict({
        "nodes": [{"id": "c", "type": "ColorizeGradient"},
                  {"id": "e", "type": "ExportHeightmap"}],
        "links": [["c.texture", "e.input"]],
    })
    errs = validate_spec(spec, CAT)
    assert any(e["level"] == "L2" and "data type" in e["problem"].lower()
               for e in errs)


def test_unknown_type_link_no_secondary_error():
    # a link off an unknown-type node should only report the L1 unknown-type,
    # not a misleading secondary L2 "not an output port"
    spec = Spec.from_dict({
        "nodes": [{"id": "x", "type": "Nope"},
                  {"id": "e", "type": "ExportHeightmap"}],
        "links": [["x.output", "e.input"]],
    })
    errs = validate_spec(spec, CAT)
    assert any(e["level"] == "L1" for e in errs)
    assert not any(e["level"] == "L2" and "output port" in e["problem"] for e in errs)


def test_dangling_export():
    spec = Spec.from_dict({"nodes": [{"id": "n", "type": "NoiseFbm"}],
                           "export": [{"node": "missing", "port": "output",
                                       "path": "o.png"}]})
    errs = validate_spec(spec, CAT)
    assert any("export" in e["problem"].lower() for e in errs)
