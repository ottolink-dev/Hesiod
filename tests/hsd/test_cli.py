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


def test_make_builds_without_run_flag(tmp_path):
    # make without --run should behave like build (no binary needed)
    out = tmp_path / "g.hsd"
    rc = main(["make", FIX, "-o", str(out)])
    assert rc == 0
    assert out.exists()


def test_nodes_show(capsys):
    rc = main(["nodes", "--show", "NoiseFbm"])
    out = capsys.readouterr().out
    assert rc == 0
    assert "output" in out and "VirtualArray" in out
    assert "kw" in out


def test_nodes_show_enum_choices(capsys):
    """--show should list valid choices inline for catalogued Enumeration params."""
    rc = main(["nodes", "--show", "Blend"])
    out = capsys.readouterr().out
    assert rc == 0
    # blending_method is Enumeration and catalogued — choices must appear
    assert "blending_method: Enumeration" in out
    assert "maximum" in out
    assert "replace" in out
    # choices are presented as a bracketed pipe-separated list
    assert "[" in out and "|" in out


def test_nodes_show_enum_choices_noisefbm(capsys):
    """--show lists noise_type choices for NoiseFbm."""
    rc = main(["nodes", "--show", "NoiseFbm"])
    out = capsys.readouterr().out
    assert rc == 0
    assert "noise_type: Enumeration" in out
    assert "Perlin" in out
