#!/usr/bin/env python3
"""Generate node-reference example graphs (.hsd) for Hesiod nodes.

For every non-WIP node that does not have a hand-authored ("curated") example,
this emits a small, valid example graph into ``Hesiod/data/examples/<Node>.hsd``
using the hsd toolkit, so the node-reference "Example" sections can be kept up to
date automatically (render with ``hesiod --snapshot``; wire with
``scripts/generate_node_reference.py`` — both driven by ``scripts/update_doc.sh``).

Templates
  - primitives ............... the node alone (shows its generated output)
  - transform nodes .......... GaborWaveFbm -> node   (input + result side by side)
  - binary operators ......... two distinct GaborWaveFbm sources -> node
  - texture nodes ............ GaborWaveFbm -> ColorizeGradient -> node
  - cloud nodes .............. CloudLattice (+ value source) -> node
  - path nodes ............... CloudLattice -> CloudToPath -> node
  - export/debug/sink nodes .. fed the matching-typed source(s)

Curated examples (CURATED) and a few cross-graph routing nodes are never touched.

Usage:
  python3 scripts/gen_node_examples.py            # (re)generate example .hsd
  python3 scripts/gen_node_examples.py --check     # report-only; exit 1 on coverage gap
"""
import sys
import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "scripts"))
from hsd.catalog import Catalog          # noqa: E402
from hsd.spec import Spec                # noqa: E402
from hsd.compile import compile_spec     # noqa: E402
from hsd.validate import validate_spec, blocking  # noqa: E402

EX_DIR = REPO / "Hesiod" / "data" / "examples"
CFG = {"shape": [512, 512], "tiling": [1, 1], "overlap": 0.0}

# Hand-authored examples — never overwrite these.
CURATED = frozenset({
    "Badlands", "BumpLorentzian", "ClampOblique", "CloudRandom",
    "CloudSetValuesFromBorderDistance", "CloudSetValuesFromHeightmap",
    "CloudSetValuesFromMinDistance", "CloudShuffle", "CloudToPath",
    "CoastalErosionDiffusion", "CoastalErosionProfile", "ExportCloudToPly",
    "ExportPointsToPly", "FindCutPath", "FloodingFromBoundaries",
    "FloodingFromPoint", "FloodingLakeSystem", "FloodingUniformLevel",
    "FlowSimulationViscous", "HeightmapToNormalMap", "HemisphereFieldFbm",
    "Island", "IslandLandMask", "MergeWaterDepths", "Mixer",
    "MorphologicalGradient", "MountainCone", "MountainInselberg",
    "NormalMapToHeightmap", "PathFractalize", "PathShuffle",
    "ReverseAboveTheshold", "Rifts", "Rifts_autosave", "RotateDisplacement",
    "SelectSoilFlow", "SelectSoilRocks", "SelectSoilWeathered", "Skeleton",
    "SnowMeltingMap", "SnowSimulation", "SpectralEqualizer", "Strata",
    "StrataCells", "Thru", "Transfer", "ValleyFill", "Vorolines",
    "WaterDepthDryOut", "WaterDepthFromMask", "WaterMask", "WatershedRidge",
    "WaveletNoise", "ZeroedEdges",
})

# Nodes deliberately left without a generated example (need a multi-graph setup).
SKIP = frozenset({"Receive", "Broadcast"})

# Params tuned so the node's effect is actually visible (defaults would be no-ops).
PARAMS_OVERRIDE = {
    "Clamp": {"clamp": [0.3, 0.7]},
    "Remap": {"remap": [0.5, 1.0]},
    "Rescale": {"scaling": 0.5},
    "ShiftElevation": {"shift": 0.3},
}

# --- typed source fragments: stype -> (nodes, links, output_ref) -------------
SOURCES = {
    "VA":     ([{"id": "va", "type": "GaborWaveFbm", "params": {"kw": [2, 2]}}], [], "va.output"),
    "VA2":    ([{"id": "va2", "type": "GaborWaveFbm", "params": {"kw": [4, 4], "seed": 2}}], [], "va2.output"),
    "TEX":    ([{"id": "tva", "type": "GaborWaveFbm", "params": {"kw": [2, 2]}},
                {"id": "tcol", "type": "ColorizeGradient", "params": {}}],
               [["tva.output", "tcol.level"]], "tcol.texture"),
    "TEX2":   ([{"id": "tva2", "type": "GaborWaveFbm", "params": {"kw": [4, 4], "seed": 2}},
                {"id": "tcol2", "type": "ColorizeGradient", "params": {}}],
               [["tva2.output", "tcol2.level"]], "tcol2.texture"),
    "CLOUD":  ([{"id": "cl", "type": "CloudLattice", "params": {}}], [], "cl.cloud"),
    "CLOUD2": ([{"id": "cl2", "type": "CloudRandomWeibull", "params": {}}], [], "cl2.cloud"),
    "PATH":   ([{"id": "pcl", "type": "CloudLattice", "params": {}},
                {"id": "c2p", "type": "CloudToPath", "params": {}}],
               [["pcl.cloud", "c2p.cloud"]], "c2p.path"),
}

# Explicit per-node wiring for nodes whose primary input(s) can't be inferred
# from a single VirtualArray "input" (texture/cloud/path/multi-typed sinks).
BESPOKE_PLAN = {
    # Geometry/Cloud
    "Cloud": [], "CloudFromCsv": [], "CloudLattice": [],
    "CloudRandomPowerLaw": [], "CloudRandomWeibull": [],
    "CloudMerge": [("cloud1", "CLOUD"), ("cloud2", "CLOUD2")],
    "CloudRandomDensity": [("density", "VA")],
    "CloudRandomDistance": [("density", "VA")],
    "CloudRemapValues": [("input", "CLOUD")],
    "CloudSDF": [("cloud", "CLOUD")],
    "CloudToArrayInterp": [("cloud", "CLOUD")],
    "CloudToVectors": [("cloud", "CLOUD")],
    # Geometry/Path
    "Path": [], "PathFromCsv": [],
    "PathDig": [("input", "VA"), ("path", "PATH")],
    "PathFind": [("heightmap", "VA"), ("waypoints", "PATH")],
    "PathDecimate": [("input", "PATH")],
    "PathInflate": [("input", "PATH")],
    "PathMeanderize": [("input", "PATH")],
    "PathResample": [("input", "PATH")],
    "PathSDF": [("path", "PATH")],
    "PathSmooth": [("input", "PATH")],
    "PathToCloud": [("path", "PATH")],
    "PathToHeightmap": [("path", "PATH")],
    # Texture
    "TextureUvChecker": [],
    "ColorizeGradient": [("level", "VA")],
    "ColorizeSolid": [("alpha", "VA")],
    "ColorAdjust": [("texture_in", "TEX")],
    "SetAlpha": [("texture in", "TEX"), ("alpha", "VA2")],
    "TextureToHeightmap": [("texture", "TEX")],
    "TextureSelectColor": [("texture", "TEX")],
    "TextureSplitChannels": [("texture", "TEX")],
    "MixTexture": [("texture1", "TEX"), ("texture2", "TEX2")],
    "MixNormalMap": [("normal map base", "TEX"), ("normal map detail", "TEX2")],
    "TextureAdvectionParticle": [("input", "TEX"), ("elevation", "VA2")],
    "TextureQuiltingExpand": [("texture A", "TEX"), ("heightmap (guide)", "VA2")],
    "TextureQuiltingShuffle": [("texture A", "TEX"), ("heightmap (guide)", "VA2")],
    # Export
    "ExportHeightmap": [("input", "VA")],
    "ExportNormalMap": [("input", "VA")],
    "ExportTiled": [("input", "VA")],
    "ExportTexture": [("texture", "TEX")],
    "ExportCloud": [("input", "CLOUD")],
    "ExportPath": [("input", "PATH")],
    "ExportAsset": [("elevation", "VA"), ("texture", "TEX")],
    "ImportHeightmap": [], "ImportTexture": [],
    # Debug
    "Compare": [("a", "VA"), ("b", "VA2")],
    "Debug": [("input", "VA")],
    "Preview": [("elevation", "VA")],
    # Routing / Bridges
    "Toggle": [("input A", "VA"), ("input B", "VA2")],
    "BlenderBridge": [("elevation", "VA"), ("texture", "TEX")],
}

# Category-based fallback (auto-covers new nodes the bespoke plan doesn't list).
VA_INPUT_CATS = ("Filter", "Erosion", "Operator", "Math", "Boundaries",
                 "Hydrology", "Converter", "Terrain Features")
BINARY_PAIRS = [("input 1", "input 2"), ("a", "b"),
                ("feature 1", "feature 2"), ("A", "B")]
MODIFIER = {"mask", "dx", "dy", "control", "envelope", "noise", "alpha",
            "gradient", "thru", "ref", "reference", "base", "background",
            "blend", "threshold", "angle", "bedrock", "zmax", "dr",
            "seed_mask", "water_depth", "angle_shift", "depth_map"}


def _from_plan(node, feeds):
    nodes, links = [], []
    for stype in dict.fromkeys(s for _, s in feeds):
        sn, sl, _ = SOURCES[stype]
        nodes += sn
        links += sl
    nodes.append({"id": "n", "type": node, "params": PARAMS_OVERRIDE.get(node, {})})
    for port, stype in feeds:
        links.append([SOURCES[stype][2], f"n.{port}"])
    return {"config": CFG, "nodes": nodes, "links": links}


def _va_inputs(cat, node):
    return [k for k, v in cat.ports(node).items()
            if v.get("type") == "input" and v.get("data_type") == "VirtualArray"]


def make_spec(cat, node):
    """Return (spec_dict, label) or (None, reason)."""
    if node in BESPOKE_PLAN:
        return _from_plan(node, BESPOKE_PLAN[node]), "bespoke"

    category = cat.category(node)
    if category.startswith("Primitive"):
        return ({"config": CFG, "nodes": [{"id": "n", "type": node,
                 "params": PARAMS_OVERRIDE.get(node, {})}], "links": []}, "primitive")

    if not category.startswith(VA_INPUT_CATS):
        return None, f"no template for category '{category}'"

    vas = set(_va_inputs(cat, node))
    for a, b in BINARY_PAIRS:
        if a in vas and b in vas:
            return _from_plan(node, [(a, "VA"), (b, "VA2")]), "binary"
    if "input" in vas:
        pin = "input"
    else:
        non_mod = [k for k in _va_inputs(cat, node) if k.lower() not in MODIFIER]
        pin = non_mod[0] if non_mod else (next(iter(vas), None))
    if pin is None:
        return None, "no VirtualArray input"
    return _from_plan(node, [(pin, "VA")]), "single"


def main():
    check = "--check" in sys.argv
    cat = Catalog.load()
    written, skipped, failed = [], [], []

    for node in cat.node_types():
        if node in CURATED or node in SKIP:
            continue
        if cat.category(node).startswith("WIP"):
            continue

        spec_dict, label = make_spec(cat, node)
        if spec_dict is None:
            skipped.append((node, label))
            continue
        try:
            spec = Spec.from_dict(spec_dict)
            errs = validate_spec(spec, cat)
            if blocking(errs):
                failed.append((node, "; ".join(e.get("problem", str(e)) for e in errs)[:120]))
                continue
            proj = compile_spec(spec, cat)
            if not check:
                (EX_DIR / f"{node}.hsd").write_text(json.dumps(proj.to_hsd(), indent=4))
            written.append(node)
        except Exception as e:  # noqa: BLE001
            failed.append((node, f"{type(e).__name__}: {e}"[:120]))

    verb = "would generate" if check else "generated"
    print(f"{verb}: {len(written)}   curated (kept): {len(CURATED)}   "
          f"skipped: {len(skipped)}   failed: {len(failed)}")
    for node, why in skipped:
        print(f"  skip  {node:28} {why}")
    for node, why in failed:
        print(f"  FAIL  {node:28} {why}")

    if check and failed:
        print("\nCoverage gap: some nodes have no valid generated example.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
