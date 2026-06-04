import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))

from generate_node_reference import build_nav_summary


def test_build_nav_summary_groups_by_primary_and_secondary():
    data = {
        "Abs": {"category": "Math/Base", "label": "Abs"},
        "Border": {"category": "Boundaries", "label": "Border"},
        "Erosion": {"category": "Erosion/Hydraulic", "label": "Erosion"},
    }
    out = build_nav_summary(data)

    # categories overview link is first
    assert out.splitlines()[0] == "* [Categories](categories.md)"
    # a primary-only category lists nodes one level deep
    assert "* Boundaries" in out
    assert "    * [Border](nodes/Border.md)" in out
    # a primary/secondary category nests two levels deep
    assert "* Math" in out
    assert "    * Base" in out
    assert "        * [Abs](nodes/Abs.md)" in out
    # primaries are alphabetically ordered
    assert out.index("* Boundaries") < out.index("* Erosion") < out.index("* Math")
