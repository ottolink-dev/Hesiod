#!/usr/bin/env python3
"""Capture focused Hesiod allocations with heaptrack, separately from native RSS.

The peak flamegraph reports live intercepted heap bytes at the global heap peak.
It is neither RSS nor the sum of independent per-stack high-water marks.
"""
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import signal
import sys

from memory_study import (CANDIDATES, POLICIES, ENVIRONMENT_KEYS, make_fixture, measure_process,
                          prepare_run, validate_run)
from run_pilot import ROOT, git, sha256, write_json


def executable(value):
    path = Path(shutil.which(str(value)) or value).resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise ValueError(f'executable not found: {value}')
    return path


def read_peak_stacks(path):
    """Parse heaptrack's collapsed peak stack export, retaining exclusive bytes."""
    stacks = []
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        stack, separator, count = line.rpartition(' ')
        if not separator or not stack or not count.isdigit():
            raise ValueError('invalid peak flamegraph line')
        stacks.append({'stack': stack, 'bytes': int(count)})
    if not stacks or sum(row['bytes'] for row in stacks) <= 0:
        raise ValueError('peak flamegraph is empty or has no live allocations')
    return sorted(stacks, key=lambda row: row['bytes'], reverse=True)


def analyze_capture(capture, printer, folder, timeout, max_rss_mib, supervisor_script=None):
    folder.mkdir()
    flamegraph = folder / 'peak-stacks.txt'
    command = [str(printer), '--file', str(capture), '--merge-backtraces=0',
               '--print-peaks=1', '--print-allocators=1', '--print-temporary=1',
               '--peak-limit=30', '--flamegraph-cost-type=peak',
               '--print-flamegraph', str(flamegraph)]
    process = measure_process(command, folder, dict(os.environ), timeout, max_rss_mib,
                              supervisor_script=supervisor_script)
    (folder / 'app.log').rename(folder / 'allocations.txt')
    write_json(folder / 'process.json', {'command': command, **process})
    if process['failure_kind'] or process['app_exit'] != 0:
        raise ValueError(f'heaptrack_print failed: {process}')
    stacks = read_peak_stacks(flamegraph)
    write_json(folder / 'peak-stacks.json', stacks)
    return {'intercepted_heap_at_global_peak_bytes': sum(s['bytes'] for s in stacks),
            'peak_stack_count': len(stacks), 'analysis_command': command}


def run(args):
    for name in ('timeout', 'max_rss_mib'):
        if not math.isfinite(getattr(args, name)) or getattr(args, name) <= 0:
            raise ValueError(f'{name} must be finite and positive')
    if not 64 <= args.size <= 8192 or args.tiles < 1 or args.size % args.tiles or args.size // args.tiles < 16:
        raise ValueError('invalid size/tiles (64..8192; tile width at least 16)')
    if not math.isfinite(args.overlap) or not 0 <= args.overlap < 1 or (args.tiles == 1 and args.overlap != 0):
        raise ValueError('invalid overlap (single-tile requires zero)')
    binary, heaptrack, printer = map(executable, (args.binary, args.heaptrack, args.printer))
    if not (binary.parent / 'data').is_dir():
        raise ValueError('binary requires adjacent data directory')
    if (binary.parent / 'portable.flag').exists() or (binary.parent / 'hesiod.json').exists():
        raise ValueError('portable settings would bypass isolated settings')
    folder = args.output.resolve()
    catalog = json.loads((binary.parent / 'data/node_documentation.json').read_text())
    graph, expected = make_fixture(catalog, args.case, args.variant)
    command, env = prepare_run(folder, binary, graph, args.size, args.tiles,
                               args.overlap, args.policy, 1)
    for package in ('profiling', 'hsd'):
        shutil.copytree(ROOT / 'scripts' / package, folder / 'scripts' / package,
                        ignore=shutil.ignore_patterns('__pycache__'))
    # Reuse a native suite's frozen binary and provenance when supplied.
    suite_provenance = binary.parent.parent / 'provenance.json'
    if suite_provenance.is_file():
        shutil.copy2(suite_provenance, folder / 'native-suite-provenance.json')
    cache = binary.parent.parent / 'CMakeCache.txt'
    if cache.is_file():
        shutil.copy2(cache, folder / 'CMakeCache.txt')
    capture_command = [str(heaptrack), '--record-only', '--output',
                       str(folder / 'heaptrack'), *command]
    result = {'status': 'failed', 'case': args.case, 'variant': args.variant,
              'size': args.size, 'tiles': args.tiles, 'overlap': args.overlap,
              'policy': args.policy, 'command': capture_command,
              'git_head': git('rev-parse', 'HEAD'), 'git_status': git('status', '--short'),
              'submodules': git('submodule', 'status', '--recursive'),
              'binary': str(binary), 'binary_sha256': sha256(binary),
              'environment': {k: env[k] for k in (*ENVIRONMENT_KEYS, 'OMP_NUM_THREADS',
                              'OPENBLAS_NUM_THREADS', 'QT_QPA_PLATFORM', 'XDG_CONFIG_HOME',
                              'XDG_CACHE_HOME') if k in env},
              'script_sha256': {str(p.relative_to(folder)): sha256(p)
                               for p in sorted((folder / 'scripts').rglob('*.py'))},
              'data_sha256': {str(p.relative_to(binary.parent)): sha256(p)
                             for p in sorted((binary.parent / 'data').rglob('*')) if p.is_file()}, 'fixture_sha256': sha256(folder / 'fixture.hsd'),
              'tools': {str(p): sha256(p) for p in (heaptrack, printer)},
              'scope': 'intercepted CPU heap allocations; no GPU VRAM, mappings or allocator overhead; '
                       'instrumented RSS/time must not be mixed with native measurements'}
    write_json(folder / 'result.json', result)
    try:
        supervisor_script = folder / 'scripts/profiling/memory_child.py'
        process = measure_process(capture_command, folder, env, args.timeout, args.max_rss_mib,
                                  supervisor_script=supervisor_script)
        result['profiler_process'] = process
        if process['failure_kind'] or process['app_exit'] != 0:
            raise ValueError(f'heaptrack/application failed: {process}')
        result.update(validate_run(folder, expected, args.size, args.tiles, args.overlap,
                                   export=args.case == 'ExportAsset'))
        captures = [p for p in folder.glob('heaptrack*') if p.is_file() and
                    p.suffix in ('.gz', '.zst')]
        if len(captures) != 1 or captures[0].stat().st_size == 0:
            raise ValueError('expected exactly one nonempty heaptrack capture')
        capture = captures[0]
        result['capture'] = {'path': capture.name, 'sha256': sha256(capture),
                             'bytes': capture.stat().st_size}
        result.update(analyze_capture(capture, printer, folder / 'analysis',
                                      args.timeout, args.max_rss_mib, supervisor_script=supervisor_script))
        if sha256(binary) != result['binary_sha256']:
            raise ValueError('binary changed during capture; use a frozen native-suite binary')
        result['status'] = 'ok'
    except KeyboardInterrupt:
        result.update(status='interrupted', error='interrupted by signal')
    except (ValueError, OSError) as error:
        result['error'] = str(error)
    finally:
        write_json(folder / 'result.json', result)
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'ok' else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--heaptrack', default='heaptrack')
    parser.add_argument('--printer', default='heaptrack_print')
    parser.add_argument('--case', choices=CANDIDATES, required=True)
    parser.add_argument('--variant', choices=('minimal', 'full'), default='full')
    parser.add_argument('--size', type=int, default=512)
    parser.add_argument('--tiles', type=int, default=1)
    parser.add_argument('--overlap', type=float, default=0)
    parser.add_argument('--policy', choices=POLICIES, default='ram-sequential')
    parser.add_argument('--timeout', type=float, default=900)
    parser.add_argument('--max-rss-mib', type=float, default=8192)
    args = parser.parse_args(argv)
    try:
        return run(args)
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    def request_stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, request_stop)
    sys.exit(main())
