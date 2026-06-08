import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")
CAT = Catalog.load()


def test_compile_fixture_to_hsd():
    spec = Spec.from_file(FIX)
    hsd = compile_spec(spec, CAT).to_hsd()

    g = hsd["graph_manager"]["graph_nodes"]["graph"]
    labels = {n["label"] for n in g["nodes"]}
    assert labels == {"NoiseFbm", "HydraulicParticle", "ColorizeGradient"}

    # overridden params became value objects on the NoiseFbm node
    noise = next(n for n in g["nodes"] if n["label"] == "NoiseFbm")
    assert noise["kw"]["type_string"] == "Wavenumber" and noise["kw"]["value"] == [4, 4]
    assert noise["seed"]["type_string"] == "Random seed number" and noise["seed"]["value"] == 1
    # un-overridden params are omitted (tolerant loader fills them)
    assert "lacunarity" not in noise

    # export wired
    ero_id = next(n["id"] for n in g["nodes"] if n["label"] == "HydraulicParticle")
    assert hsd["graph_manager"]["export_param"]["ids"] == [["graph", ero_id, "output"]]


def test_compile_enum_string_param():
    """Blend.blending_method as plain string compiles to the correct value object."""
    spec = Spec.from_dict({
        "nodes": [
            {"id": "a", "type": "NoiseFbm"},
            {"id": "b", "type": "NoiseFbm"},
            {"id": "bl", "type": "Blend", "params": {"blending_method": "maximum"}},
        ],
        "links": [
            ["a.output", "bl.input 1"],
            ["b.output", "bl.input 2"],
        ],
    })
    hsd = compile_spec(spec, CAT).to_hsd()
    g = hsd["graph_manager"]["graph_nodes"]["graph"]
    blend_node = next(n for n in g["nodes"] if n["label"] == "Blend")
    vo = blend_node["blending_method"]
    assert vo["value"] == 3
    assert vo["choice"] == "maximum"
    assert vo["type"] == 4
    assert vo["type_string"] == "Enumeration"
