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


# --- enum choice validation tests ---

def test_invalid_enum_choice_yields_l1_error():
    spec = Spec.from_dict({
        "nodes": [{"id": "bl", "type": "Blend",
                   "params": {"blending_method": "not_a_method"}}],
    })
    errs = validate_spec(spec, CAT)
    l1 = [e for e in errs if e["level"] == "L1"]
    assert any("not_a_method" in e["problem"] for e in l1), (
        f"expected L1 error naming the bad choice; got {errs}"
    )
    # suggestion should list valid choices
    matching = next(e for e in l1 if "not_a_method" in e["problem"])
    assert "maximum" in matching.get("suggestion", "")


def test_valid_enum_choice_no_error():
    spec = Spec.from_dict({
        "nodes": [{"id": "bl", "type": "Blend",
                   "params": {"blending_method": "maximum"}}],
    })
    errs = validate_spec(spec, CAT)
    assert not any("blending_method" in e.get("problem", "") for e in errs), (
        f"unexpected errors for valid enum string: {errs}"
    )


def test_uncatalogued_enum_string_yields_l1_error():
    # CloudRandom.method is an Enumeration in the doc but has NO catalog mapping,
    # so a bare string can't be resolved to an int -> validate must flag it (L1)
    # rather than letting build/make die with a ParamError traceback.
    spec = Spec.from_dict({
        "nodes": [{"id": "c", "type": "CloudRandom",
                   "params": {"method": "some_choice"}}],
    })
    errs = validate_spec(spec, CAT)
    l1 = [e for e in errs if e["level"] == "L1" and "method" in e["problem"]]
    assert l1, f"expected an L1 error for the uncatalogued enum string; got {errs}"
    err = l1[0]
    assert "auto-resolvable" in err["problem"]
    assert "value-object dict" in err.get("suggestion", "")


def test_dict_enum_value_not_validated():
    """A dict value-object passthrough must not trigger enum validation."""
    full = {"label": "blending_method", "type": 4, "type_string": "Enumeration",
            "choice": "maximum", "value": 3}
    spec = Spec.from_dict({
        "nodes": [{"id": "bl", "type": "Blend",
                   "params": {"blending_method": full}}],
    })
    errs = validate_spec(spec, CAT)
    assert not any("blending_method" in e.get("problem", "") for e in errs)
