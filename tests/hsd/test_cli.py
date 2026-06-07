import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.cli import main

FIX = os.path.join(os.path.dirname(__file__), "fixtures", "first_terrain.json")


def test_build_writes_hsd(tmp_path, capsys):
    out = tmp_path / "g.hsd"
    rc = main(["build", FIX, "-o", str(out)])
    assert rc == 0
    hsd = json.load(open(out))
    assert {n["label"] for n in hsd["graph_manager"]["graph_nodes"]["graph"]["nodes"]} \
        == {"NoiseFbm", "HydraulicParticle", "ColorizeGradient"}


def test_build_reports_validation_errors(tmp_path):
    bad = tmp_path / "bad.json"
    bad.write_text(json.dumps({"nodes": [{"id": "x", "type": "Nope"}]}))
    rc = main(["build", str(bad), "-o", str(tmp_path / "x.hsd")])
    assert rc != 0


def test_nodes_show(capsys):
    rc = main(["nodes", "--show", "NoiseFbm"])
    out = capsys.readouterr().out
    assert rc == 0
    assert "output" in out and "VirtualArray" in out
    assert "kw" in out
