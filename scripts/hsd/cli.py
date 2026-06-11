import argparse
import json
import sys

from hsd.catalog import Catalog
from hsd.enums import EnumCatalog
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.params import ParamError
from hsd.builder import BuilderError
from hsd.enums import EnumError
from hsd.validate import validate_spec, lint_file, blocking
from hsd.run import run_batch
from hsd.png import read_png
from hsd.inspect import stats, edges, landfrac, profile


def _print_errors(errors):
    for e in errors:
        gid = f" ({e['graph_id']})" if e.get("graph_id") else ""
        loc = f" [{e['node_id']}]" if e.get("node_id") else ""
        sys.stderr.write(f"{e['level']}{gid}{loc}: {e['problem']}\n")
        if e.get("suggestion"):
            sys.stderr.write(f"      -> {e['suggestion']}\n")


def _cmd_build(args, cat):
    spec = Spec.from_file(args.spec)
    errors = validate_spec(spec, cat)
    if errors:
        _print_errors(errors)
        if blocking(errors):
            return 1
    try:
        hsd = compile_spec(spec, cat).to_hsd()
    except (ParamError, EnumError, BuilderError) as exc:
        sys.stderr.write(f"error: {exc}\n")
        return 1
    with open(args.output, "w") as f:
        json.dump(hsd, f, indent=4)
    print(f"wrote {args.output}")
    return 0


def _cmd_validate(args, cat):
    errors = validate_spec(Spec.from_file(args.spec), cat)
    if errors:
        _print_errors(errors)
        if blocking(errors):
            return 1
    print("ok")
    return 0


def _cmd_lint(args, cat):
    result = lint_file(args.file)
    problems = result["consistency"]
    if problems:
        for p in problems:
            sys.stderr.write(f"{p}\n")
        return 1
    print("ok")
    return 0


def _cmd_run(args, cat):
    res = run_batch(args.file, shape=args.shape, tiling=args.tiling,
                    overlap=args.overlap)
    sys.stdout.write(res["stdout"])
    sys.stderr.write(res["stderr"])
    if res["returncode"] != 0:
        return res["returncode"]
    if res["raw_png"]:
        print(f"raw: {res['raw_png']}")
        print(f"preview: {res['preview_png']}")
    return 0


def _cmd_make(args, cat):
    rc = _cmd_build(args, cat)
    if rc != 0 or not args.run:
        return rc
    args.file = args.output
    return _cmd_run(args, cat)


def _cmd_inspect(args, cat):
    try:
        img = read_png(args.file)
    except Exception as exc:
        sys.stderr.write(f"error reading PNG: {exc}\n")
        return 1

    s = stats(img)
    print(f"width:    {s['width']}")
    print(f"height:   {s['height']}")
    print(f"bitdepth: {s['bitdepth']}")
    print(f"channels: {s['channels']}")
    print(f"min:      {s['min']:.6f}")
    print(f"max:      {s['max']:.6f}")
    print(f"mean:     {s['mean']:.6f}")

    if args.edges:
        e = edges(img)
        print(f"top:      {e['top']:.6f}")
        print(f"bottom:   {e['bottom']:.6f}")
        print(f"left:     {e['left']:.6f}")
        print(f"right:    {e['right']:.6f}")
        print(f"lr_match: {e['lr_match']:.6f}")

    if args.landfrac is not None:
        threshold = args.landfrac
        frac = landfrac(img, threshold)
        print(f"landfrac ({threshold}): {frac:.6f}")

    if args.profile:
        p = profile(img, args.profile, args.profile_n)
        label = f"profile ({args.profile}, n={args.profile_n})"
        values = "  ".join(f"{v:.4f}" for v in p)
        print(f"{label}: [{values}]")

    return 0


def _cmd_nodes(args, cat):
    if args.show:
        if not cat.has_node(args.show):
            sys.stderr.write(f"unknown node type: {args.show}\n")
            return 1
        # Load enum catalog once; skip enrichment if missing.
        try:
            enum_catalog = EnumCatalog.load()
        except FileNotFoundError:
            enum_catalog = None
        print(f"{args.show}  [{cat.category(args.show)}]")
        print(f"  {cat.description(args.show)}")
        print("  ports:")
        for pid, p in cat.ports(args.show).items():
            print(f"    {p['type']:6} {pid}: {p['data_type']}")
        print("  params:")
        for pid, p in cat.params(args.show).items():
            ptype = p["type"]
            if ptype in ("Enumeration", "Choice") and enum_catalog is not None:
                choices = enum_catalog.choices(args.show, pid)
                if choices is not None:
                    print(f"    {pid}: {ptype}  [{' | '.join(choices)}]")
                else:
                    print(f"    {pid}: {ptype}  (pass a full value-object dict)")
            else:
                print(f"    {pid}: {ptype}")
        return 0
    for t in cat.node_types():
        if args.category and not cat.category(t).startswith(args.category):
            continue
        if args.search and args.search.lower() not in t.lower():
            continue
        print(f"{t}  [{cat.category(t)}]")
    return 0


def _pair(s):
    try:
        a, b = s.split(",")
        return [int(a), int(b)]
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"expected 'W,H' (e.g. '512,512'), got: {s!r}")


def main(argv=None):
    parser = argparse.ArgumentParser(prog="hsd")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("build"); p.add_argument("spec"); p.add_argument("-o", "--output", required=True)
    p = sub.add_parser("validate"); p.add_argument("spec")
    p = sub.add_parser("lint"); p.add_argument("file")
    p = sub.add_parser("run"); p.add_argument("file")
    p.add_argument("--shape", type=_pair); p.add_argument("--tiling", type=_pair)
    p.add_argument("--overlap", type=float)
    p = sub.add_parser("make"); p.add_argument("spec"); p.add_argument("-o", "--output", required=True)
    p.add_argument("--run", action="store_true")
    p.add_argument("--shape", type=_pair); p.add_argument("--tiling", type=_pair)
    p.add_argument("--overlap", type=float)
    p = sub.add_parser("nodes"); p.add_argument("--search"); p.add_argument("--category")
    p.add_argument("--show")

    p = sub.add_parser("inspect"); p.add_argument("file")
    p.add_argument("--edges", action="store_true",
                   help="print mean value of each border edge and lr_match seam metric")
    p.add_argument("--landfrac", nargs="?", type=float, const=0.5, default=None,
                   metavar="T",
                   help="print land fraction (pixels >= T, default T=0.5)")
    p.add_argument("--profile", choices=["row", "col"],
                   help="print evenly-spaced row or column means")
    p.add_argument("--profile-n", dest="profile_n", type=int, default=16,
                   metavar="N",
                   help="number of profile samples (default 16)")

    args = parser.parse_args(argv)
    cat = Catalog.load()
    return {
        "build": _cmd_build, "validate": _cmd_validate, "lint": _cmd_lint,
        "run": _cmd_run, "make": _cmd_make, "nodes": _cmd_nodes,
        "inspect": _cmd_inspect,
    }[args.cmd](args, cat)
