import os, sys, json
import pytest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.run import run_batch, find_binary, RunError
from hsd.png import read_png


def _have_binary():
    try:
        find_binary()
        return True
    except RunError:
        return False


@pytest.mark.skipif(not _have_binary(), reason="hesiod binary not available")
def test_roundtrip_noise_to_export(tmp_path):
    output = tmp_path / "out.png"
    spec = Spec.from_dict({
        "config": {"shape": [64, 64], "tiling": [1, 1], "overlap": 0.0},
        "nodes": [
            {"id": "noise", "type": "CoherentNoise", "params": {"kw": [4, 4], "seed": 1}},
            {"id": "exp", "type": "ExportHeightmap", "params": {
                "fname": {"value": str(output)}, "auto_export": True, "format": "png (16 bit)"}},
        ],
        "links": [["noise.output", "exp.input"]],
    })
    cat = Catalog.load()
    hsd_path = tmp_path / "g.hsd"
    hsd_path.write_text(json.dumps(compile_spec(spec, cat).to_hsd(), indent=4))

    res = run_batch(str(hsd_path), shape=[64, 64], tiling=[1, 1], cwd=str(tmp_path))
    assert res["returncode"] == 0, res["stderr"]
    assert output.exists(), res["stdout"] + res["stderr"]
    with output.open("rb") as stream:
        image = read_png(stream)
    assert (image["width"], image["height"], image["bitdepth"]) == (64, 64, 16)


@pytest.mark.skipif(not _have_binary(), reason="hesiod binary not available")
def test_roundtrip_blend_string_enum(tmp_path):
    """Two CoherentNoise -> Blend(blending_method='maximum') -> ExportHeightmap round-trips."""
    # Blend input port ids are 'input 1' and 'input 2'
    output = tmp_path / "blend_out.png"
    spec = Spec.from_dict({
        "config": {"shape": [64, 64], "tiling": [1, 1], "overlap": 0.0},
        "nodes": [
            {"id": "a", "type": "CoherentNoise"},
            {"id": "b", "type": "CoherentNoise"},
            {"id": "bl", "type": "Blend", "params": {"blending_method": "maximum"}},
            {"id": "ex", "type": "ExportHeightmap", "params": {
                "fname": {"value": str(output)}, "auto_export": True, "format": "png (16 bit)"}},
        ],
        "links": [
            ["a.output", "bl.input 1"],
            ["b.output", "bl.input 2"],
            ["bl.output", "ex.input"],
        ],
    })
    cat = Catalog.load()
    hsd_path = tmp_path / "blend_enum.hsd"
    hsd_path.write_text(json.dumps(compile_spec(spec, cat).to_hsd(), indent=4))

    res = run_batch(str(hsd_path), shape=[64, 64], tiling=[1, 1], cwd=str(tmp_path))
    assert res["returncode"] == 0, res["stderr"]
    assert output.exists(), res["stdout"] + res["stderr"]
    with output.open("rb") as stream:
        image = read_png(stream)
    assert (image["width"], image["height"], image["bitdepth"]) == (64, 64, 16)
