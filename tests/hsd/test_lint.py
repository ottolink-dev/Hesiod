import os, sys, json, glob
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.validate import lint_file, consistency_errors


def test_real_example_is_consistent():
    # every shipped example must be model<->UI consistent
    f = sorted(glob.glob(os.path.join(
        os.path.dirname(__file__), "..", "..", "Hesiod", "data", "examples", "*.hsd")))[0]
    assert consistency_errors(json.load(open(f))) == []


def test_detects_inconsistent_mirror(tmp_path):
    hsd = {
        "graph_manager": {"graph_nodes": {"graph": {
            "links": [], "nodes": [{"id": "1", "label": "Abs"}]}}},
        "graph_tabs_widget": {"graph_node_widgets": {"graph": {
            "links": [], "nodes": [{"id": "1", "caption": "Bump"}]}}},
    }
    assert any("caption" in e or "label" in e or "mismatch" in e.lower()
               for e in consistency_errors(hsd))


def test_lint_file_reads_path(tmp_path):
    f = sorted(glob.glob(os.path.join(
        os.path.dirname(__file__), "..", "..", "Hesiod", "data", "examples", "*.hsd")))[0]
    # lint returns a dict with a 'consistency' key (list of model/UI mismatches)
    result = lint_file(f)
    assert "consistency" in result and isinstance(result["consistency"], list)
