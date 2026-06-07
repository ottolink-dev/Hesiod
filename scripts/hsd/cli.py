import argparse
import json
import sys

from hsd.catalog import Catalog
from hsd.spec import Spec
from hsd.compile import compile_spec
from hsd.validate import validate_spec, lint_file
from hsd.run import run_batch


def _print_errors(errors):
    for e in errors:
        loc = f" [{e['node_id']}]" if e.get("node_id") else ""
        sys.stderr.write(f"{e['level']}{loc}: {e['problem']}\n")
        if e.get("suggestion"):
            sys.stderr.write(f"      -> {e['suggestion']}\n")


def _cmd_build(args, cat):
    spec = Spec.from_file(args.spec)
    errors = validate_spec(spec, cat)
    if errors:
        _print_errors(errors)
        return 1
    hsd = compile_spec(spec, cat).to_hsd()
    with open(args.output, "w") as f:
        json.dump(hsd, f, indent=4)
    print(f"wrote {args.output}")
    return 0


def _cmd_validate(args, cat):
    errors = validate_spec(Spec.from_file(args.spec), cat)
    if errors:
        _print_errors(errors)
        return 1
    print("ok")
    return 0


def _cmd_lint(args, cat):
    result = lint_file(args.file)
    problems = result["consistency"] + result["validation"]
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


def _cmd_nodes(args, cat):
    if args.show:
        if not cat.has_node(args.show):
            sys.stderr.write(f"unknown node type: {args.show}\n")
            return 1
        print(f"{args.show}  [{cat.category(args.show)}]")
        print(f"  {cat.description(args.show)}")
        print("  ports:")
        for pid, p in cat.ports(args.show).items():
            print(f"    {p['type']:6} {pid}: {p['data_type']}")
        print("  params:")
        for pid, p in cat.params(args.show).items():
            print(f"    {pid}: {p['type']}")
        return 0
    for t in cat.node_types():
        if args.category and not cat.category(t).startswith(args.category):
            continue
        if args.search and args.search.lower() not in t.lower():
            continue
        print(f"{t}  [{cat.category(t)}]")
    return 0


def _pair(s):
    a, b = s.split(",")
    return [int(a), int(b)]


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

    args = parser.parse_args(argv)
    cat = Catalog.load()
    return {
        "build": _cmd_build, "validate": _cmd_validate, "lint": _cmd_lint,
        "run": _cmd_run, "make": _cmd_make, "nodes": _cmd_nodes,
    }[args.cmd](args, cat)
