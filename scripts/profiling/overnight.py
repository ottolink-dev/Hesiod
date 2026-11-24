#!/usr/bin/env python3
"""Prepare, manually launch, resume and report a bounded overnight Tracy suite."""
import argparse
import collections
import csv
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import time
from statistics import median

from inventory import ROOT, inventory
from overnight_fixtures import make_manifest
from run_pilot import git, run_one, sha256, write_json

RUNTIME_ENV = ('PATH', 'LD_LIBRARY_PATH', 'QT_PLUGIN_PATH', 'QML2_IMPORT_PATH',
               'XDG_DATA_DIRS', 'OCL_ICD_VENDORS', 'OPENCL_VENDOR_PATH',
               'FONTCONFIG_FILE', 'FONTCONFIG_PATH', 'LIBGL_DRIVERS_PATH',
               '__EGL_VENDOR_LIBRARY_DIRS', 'XDG_RUNTIME_DIR', 'DBUS_SESSION_BUS_ADDRESS')


def tool(name):
    found = shutil.which(name)
    if found:
        return Path(found).resolve()
    package = 'time' if name == 'time' else 'tracy'
    paths = sorted(Path('/nix/store').glob(f'*-{package}-*/bin/{name}'))
    if not paths:
        raise ValueError(f'{name} not found; supply its path explicitly')
    return paths[-1]


def prepare(args):
    root = args.output.resolve()
    binary = args.binary.resolve()
    if not binary.is_file() or not (binary.parent / 'data').is_dir():
        raise ValueError('binary and its adjacent data directory are required')
    if (binary.parent / 'portable.flag').exists() or (binary.parent / 'hesiod.json').exists():
        raise ValueError('portable settings would bypass isolated workload settings')
    tools = {name: (getattr(args, name) or tool(executable)).resolve()
             for name, executable in [('capture', 'tracy-capture'), ('exporter', 'tracy-csvexport'), ('time_tool', 'time')]}
    if any(not p.is_file() for p in tools.values()):
        raise ValueError('a requested tool does not exist')
    if args.repeats < 1 or not args.sizes or any(s < 64 or s > 4096 for s in args.sizes):
        raise ValueError('use positive repeats and sizes between 64 and 4096')
    if args.max_hours <= 0 or args.timeout < 1 or not 1 <= args.port <= 65535 or args.max_rss_mib < 32:
        raise ValueError('invalid time, memory or port limit')
    if args.max_rss_mib >= 8192:
        raise ValueError('RSS stop threshold must be below the service 8 GiB hard limit')
    catalog = json.loads((ROOT / 'Hesiod/data/node_documentation.json').read_text())
    cases, fixtures = make_manifest(catalog)
    if args.cases:
        missing = set(args.cases) - {c['name'] for c in cases}
        if missing:
            raise ValueError(f'unknown/non-active cases: {sorted(missing)}')
        cases = [c for c in cases if c['name'] in args.cases]
        fixtures = {k: v for k, v in fixtures.items() if k in args.cases}
    root.mkdir(parents=True, exist_ok=False)
    (root / 'bin').mkdir()
    shutil.copy2(binary, root / 'bin/hesiod')
    shutil.copytree(binary.parent / 'data', root / 'bin/data')
    for name, path in tools.items():
        shutil.copy2(path, root / 'bin' / name)
    for package in ('profiling', 'hsd'):
        shutil.copytree(ROOT / 'scripts' / package, root / 'scripts' / package,
                        ignore=shutil.ignore_patterns('__pycache__'))
    (root / 'fixtures').mkdir()
    for name, graph in fixtures.items():
        write_json(root / 'fixtures' / f'{name}.hsd', graph)
    write_json(root / 'manifest.json', {'cases': cases})
    cache = binary.parent.parent / 'CMakeCache.txt'
    if cache.exists():
        shutil.copy2(cache, root / 'CMakeCache.txt')
    # Content locks include binaries, scripts, fixture graphs and copied application data.
    hashes = {str(p.relative_to(root)): sha256(p) for p in sorted(root.rglob('*')) if p.is_file()}
    config = {'sizes': sorted(set(args.sizes)), 'repeats': args.repeats,
              'max_hours': args.max_hours, 'timeout': args.timeout,
              'max_rss_mib': args.max_rss_mib, 'port': args.port,
              'max_disk_gib': 20, 'python': sys.executable,
              'environment': {k: os.environ[k] for k in RUNTIME_ENV if k in os.environ},
              'hashes': hashes, 'git_head': git('rev-parse', 'HEAD'),
              'git_status': git('status', '--short'),
              'submodules': git('submodule', 'status', '--recursive'),
              'source_binary': str(binary), 'tools': {k: str(v) for k, v in tools.items()},
              'scope': 'One explicit synthetic fixture/default parameter group per non-deprecated type. Not algorithm-variant coverage or an output-correctness test. GPU device memory is not measured.',
              'created_at': time.strftime('%Y-%m-%dT%H:%M:%S%z')}
    write_json(root / 'config.json', config)
    (root / 'config.sha256').write_text(sha256(root / 'config.json') + '\n')
    # The launcher contains no shell-expanded configuration or credentials.
    launcher = root / 'launch'
    launcher.write_text('#!' + sys.executable + '\n'
                        'import os, pathlib, sys\n'
                        'root = pathlib.Path(__file__).resolve().parent\n'
                        'os.execv(sys.executable, [sys.executable, str(root / "scripts/profiling/overnight.py"), "launch", str(root), *sys.argv[1:]])\n')
    launcher.chmod(0o755)
    report(root)
    counts = collections.Counter(c['status'] for c in cases)
    print(f'Prepared {len(cases)} entries: {dict(counts)}\nManual launch: {launcher}')


def verify(root, config):
    config_digest = root / 'config.sha256'
    if config_digest.exists() and sha256(root / 'config.json') != config_digest.read_text().strip():
        raise ValueError('suite configuration changed; prepare a new suite')
    for relative, digest in config['hashes'].items():
        path = root / relative
        if not path.is_file() or sha256(path) != digest:
            raise ValueError(f'snapshot changed: {relative}; prepare a new suite')


def result_map(root):
    found = {}
    for path in (root / 'runs').glob('*/result.json'):
        if '.interrupted.' in path.parent.name:
            continue
        data = json.loads(path.read_text())
        found[(data['case'], data['size'], data['repeat'])] = data
    return found


def report(root):
    config = json.loads((root / 'config.json').read_text())
    cases = json.loads((root / 'manifest.json').read_text())['cases']
    results = result_map(root)
    rows = []
    expected = len(config['sizes']) * config['repeats']
    for case in cases:
        runs = [r for r in results.values() if r['case'] == case['name']]
        good = [r for r in runs if r['status'] == 'ok']
        bad = [r for r in runs if r['status'] != 'ok']
        status = ('needs_fixture' if case['status'] != 'ready' else
                  'failed' if bad else 'measured_fixture' if len(good) == expected else
                  'partial' if good else 'pending')
        row = {'node': case['name'], 'group': case['group'], 'scope': case['scope'],
               'status': status, 'successful_runs': len(good), 'planned_runs': expected,
               'failed_runs': len(bad), 'reason': bad[-1].get('error', '') if bad else case.get('reason', '')}
        for size in config['sizes']:
            items = [r for r in good if r['size'] == size]
            values = [r['target_ms'] for r in items]
            row[f'{size}_ms_median'] = median(values) if values else None
            row[f'{size}_ms_min'] = min(values) if values else None
            row[f'{size}_ms_max'] = max(values) if values else None
            row[f'{size}_process_peak_mib_median'] = median(r['process_peak_rss_kib'] / 1024 for r in items) if items else None
            row[f'{size}_rss_undersampled_runs'] = sum(r['rss_samples_in_target'] < 2 for r in items)
        rows.append(row)
    temporary = root / 'report.csv.tmp'
    with temporary.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(root / 'report.csv')
    write_json(root / 'report.json', {'counts': dict(collections.Counter(r['status'] for r in rows)),
                                     'nodes': rows})
    write_json(root / 'results.json', list(results.values()))
    return rows


def schedule(cases, config):
    # Breadth first: every ready node at the smallest size before repetitions/growth.
    order = {'full_array': 0, 'configurable_tiles': 1, 'review_other': 2}
    ready = sorted((c for c in cases if c['status'] == 'ready'),
                   key=lambda c: (order.get(c['queue'], 3), c['name']))
    for size in config['sizes']:
        for repeat in range(1, config['repeats'] + 1):
            for case in ready:
                yield case, size, repeat


def disk_usage(root):
    return sum(p.stat().st_size for p in root.rglob('*') if p.is_file() and not p.is_symlink())


def run(args):
    root = args.directory.resolve()
    with (root / 'run.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError('this suite is already running')
        return run_locked(args, root)


def run_locked(args, root):
    config = json.loads((root / 'config.json').read_text())
    verify(root, config)
    previous = json.loads((root / 'status.json').read_text()) if (root / 'status.json').exists() else {}
    if previous and not args.resume:
        raise ValueError('suite already started; use --resume or prepare a new suite')
    spent = previous.get('elapsed_seconds', 0)
    budget = config['max_hours'] * 3600 - spent
    if budget <= 0:
        raise ValueError('suite time budget exhausted; prepare a new suite for more work')
    # Avoid capturing another profiled program on the selected port.
    with socket.socket() as probe:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        probe.bind(('127.0.0.1', config['port']))
    os.environ.update(config['environment'])
    start = time.monotonic()
    cases = json.loads((root / 'manifest.json').read_text())['cases']
    completed = result_map(root)
    blocked = {key[0] for key, value in completed.items() if value['status'] != 'ok'}
    settings = argparse.Namespace(output=root / 'runs', binary=root / 'bin/hesiod', baseline=root / 'bin/hesiod',
                                  capture=root / 'bin/capture', exporter=root / 'bin/exporter',
                                  time_tool=root / 'bin/time_tool', port=config['port'],
                                  timeout=config['timeout'], capture_seconds=config['timeout'] + 10,
                                  overnight=True, deadline=start + budget, max_rss_mib=config['max_rss_mib'])
    settings.output.mkdir(exist_ok=True)
    state = {'state': 'running', 'elapsed_seconds': spent, 'pid': os.getpid(),
             'started_at': time.strftime('%Y-%m-%dT%H:%M:%S%z')}
    write_json(root / 'status.json', state)
    stop_requested = False

    def request_stop(signum, frame):
        nonlocal stop_requested
        stop_requested = True
        settings.deadline = time.monotonic()

    old_term = signal.signal(signal.SIGTERM, request_stop)
    old_int = signal.signal(signal.SIGINT, request_stop)
    try:
        for case, size, repeat in schedule(cases, config):
            if stop_requested or time.monotonic() >= settings.deadline:
                state['state'] = 'stopped' if stop_requested else 'budget_exhausted'
                break
            key = (case['name'], size, repeat)
            if key in completed or case['name'] in blocked:
                continue
            if shutil.disk_usage(root).free < 2 * 1024**3 or disk_usage(root) > config['max_disk_gib'] * 1024**3:
                state['state'] = 'disk_budget_exhausted'
                break
            folder = settings.output / f"{case['name']}-{size}-r{repeat}-tracy"
            if folder.exists():
                folder.rename(folder.with_name(folder.name + f'.interrupted.{time.time_ns()}'))
            state['current'] = f"{case['name']} {size} r{repeat}"
            write_json(root / 'status.json', state)
            result = run_one(settings, case, root / 'fixtures' / f"{case['name']}.hsd", size, repeat, 'tracy')
            # A deliberate stop is resumable, not evidence of a broken node.
            if stop_requested or time.monotonic() >= settings.deadline:
                if result['status'] != 'ok':
                    folder.rename(folder.with_name(folder.name + f'.interrupted.{time.time_ns()}'))
            else:
                completed[key] = result
                if result['status'] != 'ok':
                    blocked.add(case['name'])
            state['elapsed_seconds'] = spent + time.monotonic() - start
            write_json(root / 'status.json', state)
            report(root)
        else:
            state['state'] = 'finished'
    finally:
        state['elapsed_seconds'] = spent + time.monotonic() - start
        state.pop('current', None)
        write_json(root / 'status.json', state)
        rows = report(root)
        print(json.dumps({'state': state['state'], 'coverage': dict(collections.Counter(r['status'] for r in rows))}), flush=True)
        signal.signal(signal.SIGTERM, old_term)
        signal.signal(signal.SIGINT, old_int)
    # 'finished' means the queue was processed, not that every fixture succeeded.
    return 0 if state['state'] == 'finished' and all(r['status'] == 'measured_fixture' for r in rows) else 2


def launch(args):
    root = args.directory.resolve()
    config = json.loads((root / 'config.json').read_text())
    unit = 'hesiod-profile-' + re.sub('[^a-zA-Z0-9_-]', '-', root.name)
    command = ['systemd-run', '--user', '--unit=' + unit, '--collect',
               '--property=Type=exec', '--property=MemoryMax=8G', '--property=MemorySwapMax=0',
               '--property=OOMPolicy=continue', '--property=TasksMax=256', '--property=Nice=10',
               '--property=CPUWeight=20', '--property=IOWeight=20',
               '--property=KillMode=control-group', '--property=TimeoutStopSec=15',
               '--property=RuntimeMaxSec=' + str(int(config['max_hours'] * 3600) + 30),
               '--property=StandardOutput=append:' + str(root / 'service.log'),
               '--property=StandardError=append:' + str(root / 'service.log'),
               '--working-directory=' + str(root), config['python'], '-u',
               str(root / 'scripts/profiling/overnight.py'), 'run', str(root)]
    if args.resume:
        command.append('--resume')
    subprocess.run(command, check=True)
    print(f'Launched {unit}.service\nProgress: {root / "status.json"}\nReport: {root / "report.csv"}\nStop: systemctl --user stop {unit}.service')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prep = commands.add_parser('prepare')
    prep.add_argument('--output', type=Path, required=True)
    prep.add_argument('--binary', type=Path, default=ROOT / 'build/tracy/bin/hesiod')
    for name in ('capture', 'exporter', 'time-tool'):
        prep.add_argument('--' + name, type=Path)
    prep.add_argument('--sizes', type=int, nargs='+', default=[512, 1024, 2048])
    prep.add_argument('--repeats', type=int, default=3)
    prep.add_argument('--timeout', type=int, default=120)
    prep.add_argument('--max-hours', type=float, default=8)
    prep.add_argument('--max-rss-mib', type=int, default=6144)
    prep.add_argument('--port', type=int, default=8097)
    prep.add_argument('--cases', nargs='+', help='Restrict a validation suite; omit for the overnight inventory')
    for verb in ('run', 'launch', 'report'):
        cmd = commands.add_parser(verb)
        cmd.add_argument('directory', type=Path)
        if verb != 'report':
            cmd.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    try:
        if args.command == 'prepare':
            prepare(args)
        elif args.command == 'run':
            return run(args)
        elif args.command == 'launch':
            launch(args)
        else:
            report(args.directory.resolve())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'{error}\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
