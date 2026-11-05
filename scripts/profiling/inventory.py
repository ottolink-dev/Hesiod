#!/usr/bin/env python3
"""Generate a source-based profiling queue; signals are triage, not measurements."""
import argparse
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def uncomment(text):
    # Preserve strings and line numbers so evidence points to the original source.
    pattern = r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: m[0] if m[0].startswith('"') else
                  re.sub(r'[^\n]', ' ', m[0]), text)


SIGNALS = {
    'forced_single_array': r'\bcm_single_array\b|\bVA_SINGLE_ARRAY\b',
    'to_array': r'\bto_array\s*\(',
    'from_array': r'\bfrom_array\s*\(',
    'tile_dispatch': r'\bfor_each_tile\s*\(',
    'cpu_mode': r'\bcm_cpu\b',
    'gpu_mode': r'\bcm_gpu\b',
    'gpu_call': r'\bhmap::gpu::',
    'post_process': r'\bpost_process_heightmap\s*\(',
}


def inventory(root=ROOT):
    factory = uncomment((root / 'Hesiod/src/model/nodes/node_factory.cpp').read_text())
    categories = dict(re.findall(r'\{"(\w+)",\s*"([^"]+)"\}', factory))
    registrations = dict(re.findall(r'\bSETUP_NODE\((\w+),\s*(\w+)\);', factory))
    registrations.update(Broadcast='broadcast', Receive='receive')
    sources = {}
    for path in (root / 'Hesiod/src/model/nodes/nodes_function').glob('*.cpp'):
        source = uncomment(path.read_text())
        for slug in re.findall(r'void\s+compute_(\w+)_node\s*\(', source):
            sources[slug] = (path, source)
    for name, slug in sorted(registrations.items()):
        path, source = sources.get(slug, (
            root / f'Hesiod/src/model/nodes/nodes_function/{slug}.cpp', ''))
        matches = {k: list(re.finditer(v, source)) for k, v in SIGNALS.items()}
        evidence = '; '.join(f'{k}:{source.count(chr(10), 0, m.start()) + 1}'
                             for k, found in matches.items() for m in found)
        category = categories.get(name, 'UNLISTED')
        deprecated = 'DEPRECATED' in category
        constants = dict(re.findall(r'constexpr\s+const\s+char\s*\*\s*(\w+)\s*=\s*"([^"]+)"', source))
        groups = [constants.get(value, value.strip('"')) for value in
                  re.findall(r'set_current_group\(("[^"]+"|\w+)\)', source)]
        risk = ('full_array' if any(matches[k] for k in
                                  ('forced_single_array', 'to_array', 'from_array'))
                else 'configurable_tiles' if matches['tile_dispatch'] else 'review_other')
        yield {
            'node': name, 'category': category,
            'queue': 'legacy' if deprecated else risk,
            'status': 'unmeasured',
            'source': str(path.relative_to(root)),
            'source_missing': not bool(source),
            'example_exists': (root / f'Hesiod/data/examples/{name}.hsd').exists(),
            'groups': ';'.join(sorted(set(groups))),
            **{k: len(v) for k, v in matches.items()},
            'evidence_lines': evidence,
        }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    rows = list(inventory())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    for queue in sorted({r['queue'] for r in rows}):
        print(queue, sum(r['queue'] == queue for r in rows))
    print('total', len(rows))


if __name__ == '__main__':
    main()
