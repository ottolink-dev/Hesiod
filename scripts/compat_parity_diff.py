#!/usr/bin/env python3
"""Diff two --parity-dump outputs (legacy reference vs facade build).

Usage: compat_parity_diff.py ref.json new.json
Exit 0 = parity (allowed conversions only), 1 = differences found.
"""
import json, sys

def norm(v):
    if isinstance(v, float):
        return round(v, 5)
    if isinstance(v, list):
        return [norm(x) for x in v]
    if isinstance(v, dict):
        return {k: norm(x) for k, x in v.items()}
    return v

def main(ref_path, new_path):
    ref = json.load(open(ref_path))
    new = json.load(open(new_path))
    fails = []
    for node, rrec in ref.items():
        if node not in new:
            fails.append(f"{node}: missing from new dump"); continue
        nrec = new[node]
        for key, r in rrec.items():
            if key == "__order":
                if norm(r) != norm(nrec.get("__order")):
                    fails.append(f"{node}: order {r} != {nrec.get('__order')}")
                continue
            n = nrec.get(key)
            if n is None:
                fails.append(f"{node}.{key}: missing"); continue
            for field in ("type", "label", "value", "bounds", "category"):
                if norm(r.get(field)) != norm(n.get(field)):
                    fails.append(f"{node}.{key}.{field}: {r.get(field)!r} != {n.get(field)!r}")
    for node in new:
        if node not in ref:
            fails.append(f"{node}: extra node in new dump")
    print(f"{len(fails)} difference(s)")
    for f in fails[:200]:
        print(" ", f)
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
