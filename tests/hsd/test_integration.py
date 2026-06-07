import os, sys, json
import pytest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.run import run_batch, find_binary, RunError


def _have_binary():
    try:
        find_binary()
        return True
    except RunError:
        return False


@pytest.mark.skipif(not _have_binary(), reason="hesiod binary not available")
def test_roundtrip_noise_to_export(tmp_path):
    spec = Spec.from_dict({
        "config": {"shape": [64, 64], "tiling": [1, 1], "overlap": 0.0},
        "nodes": [
            {"id": "noise", "type": "NoiseFbm", "params": {"kw": [4, 4], "seed": 1}},
            {"id": "exp", "type": "ExportHeightmap"},
        ],
        "links": [["noise.output", "exp.input"]],
        "export": [{"node": "noise", "port": "output",
                    "path": str(tmp_path / "out.png")}],
    })
    cat = Catalog.load()
    hsd_path = tmp_path / "g.hsd"
    hsd_path.write_text(json.dumps(compile_spec(spec, cat).to_hsd(), indent=4))

    res = run_batch(str(hsd_path), shape=[64, 64], tiling=[1, 1], cwd=str(tmp_path))
    assert res["returncode"] == 0, res["stderr"]
    assert os.path.exists(res["raw_png"]), res["stdout"] + res["stderr"]
    assert os.path.exists(res["preview_png"])
