import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.layout import layout_positions


def test_layered_left_to_right():
    # a -> b -> c ; a and standalone d share column 0
    node_ids = ["a", "b", "c", "d"]
    links = [("a", "b"), ("b", "c")]
    pos = layout_positions(node_ids, links)
    # downstream nodes are further right
    assert pos["a"][0] < pos["b"][0] < pos["c"][0]
    # roots (a, d) share the leftmost column
    assert pos["a"][0] == pos["d"][0]


def test_deterministic():
    node_ids = ["a", "b", "c"]
    links = [("a", "b"), ("b", "c")]
    assert layout_positions(node_ids, links) == layout_positions(node_ids, links)
