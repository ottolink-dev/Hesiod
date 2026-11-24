#!/usr/bin/env python3
"""Run bounded, sequential profiling cases and retain failures as evidence."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import resource
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from hsd.builder import Graph, Project
from inventory import inventory


def write_json(path, value):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def fixture(manifest, case):
    graph = Graph(config={'shape': (512, 512), 'tiling': (1, 1), 'overlap': 0.0})
    def attributes(values):
        return {'current': 'main', 'containers': {'main': {
            k: {'value': v} for k, v in values.items()}}}
    graph.add_node('source', manifest['source']['type'], attributes(manifest['source']['values']))
    if case['name'] != 'source':
        graph.add_node('target', case['target'], attributes(case.get('values', {})))
        graph.link('source', 'output', 'target', 'input')
    project = Project()
    project.add_graph(graph)
    return project.to_hsd()


def analyze_csv(path, target, node_id, size, expected_invocations=1):
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    text = f'{target} id={node_id} shape={size}x{size} tiling=1x1'
    zones = [r for r in rows if r['name'] == 'Node algorithm' and r['value'] == text]
    if len(zones) != expected_invocations:
        raise ValueError(f'expected {expected_invocations} completed target zones, found {len(zones)}: {text}')
    intervals = [(int(z['ns_since_start']), int(z['exec_time_ns'])) for z in zones]
    duration = sum(length for _, length in intervals)
    if any(length <= 0 for _, length in intervals):
        raise ValueError('target zone has no duration')
    rss = [r for r in rows if r['name'] == 'Process RSS']
    peaks = [float(r['value']) for r in rows if r['name'] == 'Process peak RSS']
    if not rss or not peaks:
        raise ValueError('capture is missing process memory plots')
    during = [float(r['value']) for r in rss
              if any(start <= int(r['ns_since_start']) <= start + length
                     for start, length in intervals)]
    return {
        'target_ms': duration / 1e6,
        'target_invocations': len(zones),
        'rss_samples_in_target': len(during),
        'sampled_process_rss_max_during_target_bytes': max(during) if len(during) >= 2 else None,
        'rss_assessment': 'sampled_process_memory' if len(during) >= 2 else 'insufficient_samples',
        'sampled_process_rss_max_bytes': max(float(r['value']) for r in rss),
        'kernel_process_peak_rss_max_bytes': max(peaks),
    }


def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def application_errors(logs):
    # The isolated settings intentionally leave UI fields at compiled defaults.
    # Hesiod logs these omissions as errors. Only tolerate them during settings
    # loading; the same message during graph loading is a genuine failure.
    in_settings = False
    errors = []
    defaults = 0
    for line in logs.splitlines():
        if 'AppSettings::json_from' in line:
            in_settings = True
        if 'StyleSettings::json_from' in line:
            in_settings = False
        if in_settings and re.search(r'Required json key "[^"]+" not found\.$', line):
            defaults += 1
        elif re.search(r'\[(?:error|critical)\]|\[---[EC]---\]|JSON parsing error|abandoning this graph', line):
            errors.append(line)
    return errors, defaults


def process_group_rss_kib(group):
    total = 0
    for entry in Path('/proc').glob('[0-9]*'):
        try:
            fields = (entry / 'stat').read_text().rsplit(')', 1)[1].split()
            if int(fields[2]) == group:
                for line in (entry / 'status').read_text().splitlines():
                    if line.startswith('VmRSS:'):
                        total += int(line.split()[1])
        except (OSError, ValueError, IndexError):
            pass  # exited while being sampled
    return total


def child_limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (512 * 1024**2, 512 * 1024**2))


def run_one(args, case, graph_path, size, repeat, mode):
    name = f"{case['name']}-{size}-r{repeat}-{mode}"
    folder = args.output / name
    folder.mkdir()
    if getattr(args, 'overnight', False):
        from overnight_fixtures import write_inputs
        write_inputs(folder)
    # Separate writable working/config directories; share only read-only application data.
    binary = args.binary if mode == 'tracy' else args.baseline
    (folder / 'data').symlink_to(binary.parent / 'data', target_is_directory=True)
    config = folder / 'config' / 'hesiod'
    config.mkdir(parents=True)
    write_json(config / 'hesiod.json', {
        'app_settings': {'global.omp_num_threads': 1}, 'style_settings': {}})
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen',
               XDG_CONFIG_HOME=str(folder / 'config'),
               TRACY_PORT=str(args.port), TRACY_NO_SYS_TRACE='1',
               HESIOD_TRACY_WAIT='1' if mode == 'tracy' else '0')
    command = [str(binary), f'--batch={graph_path}', f'--shape={size},{size}',
               '--tiling=1,1', '--overlap=0']
    timed = [str(args.time_tool), '-f', '%M', '-o', str(folder / 'peak-rss-kib.txt'), *command]
    result = {'case': case['name'], 'target': case['target'], 'size': size,
              'repeat': repeat, 'mode': mode, 'command': command,
              'graph_sha256': sha256(graph_path), 'status': 'failed'}
    capture = app = None
    started = time.monotonic()
    try:
        with (folder / 'app.log').open('w') as app_log, (folder / 'capture.log').open('w') as cap_log:
            if mode == 'tracy':
                capture = subprocess.Popen([
                    str(args.capture), '-a', '127.0.0.1', '-p', str(args.port),
                    '-s', str(args.capture_seconds), '-o', str(folder / 'capture.tracy')],
                    stdout=cap_log, stderr=subprocess.STDOUT)
            # A process group permits timeout cleanup of GNU time and its application child.
            app = subprocess.Popen(timed, cwd=folder, env=env, stdout=app_log,
                                   stderr=subprocess.STDOUT, start_new_session=True,
                                   preexec_fn=child_limits if getattr(args, 'overnight', False) else None)
            try:
                if getattr(args, 'overnight', False):
                    while app.poll() is None:
                        if time.monotonic() >= args.deadline:
                            raise TimeoutError('suite time budget reached')
                        if time.monotonic() - started > args.timeout:
                            raise TimeoutError('per-run timeout')
                        if process_group_rss_kib(app.pid) > args.max_rss_mib * 1024:
                            raise MemoryError('per-run RSS limit')
                        time.sleep(0.1)
                    result['app_exit'] = app.returncode
                else:
                    result['app_exit'] = app.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                os.killpg(app.pid, 15)
                try:
                    app.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(app.pid, 9)
                    app.wait()
                raise
            if capture is not None:
                result['capture_exit'] = capture.wait(timeout=20)
            if result['app_exit'] != 0 or result.get('capture_exit', 0) != 0:
                raise ValueError('application or capture exited unsuccessfully')
        logs = (folder / 'app.log').read_text(errors='replace')
        if getattr(args, 'overnight', False):
            # All nightly fixtures use 1x1 tiles and zero overlap: there are no
            # neighbouring tile buffers to smooth. Preserve the known log separately.
            benign = 'smooth_overlap_buffers requires at least 2 tiles in memory, skipping'
            result['known_single_tile_noop_warnings'] = logs.count(benign)
            logs = '\n'.join(line for line in logs.splitlines() if benign not in line)
        errors, result['compiled_setting_defaults_used'] = application_errors(logs)
        if errors:
            raise ValueError('application logged an error; inspect app.log')
        if f'computing node [{case["target"]}]/' not in logs:
            raise ValueError('target did not execute')
        result['process_peak_rss_kib'] = int((folder / 'peak-rss-kib.txt').read_text().strip())
        if mode == 'tracy':
            with (folder / 'zones.csv').open('w') as stream:
                subprocess.run([str(args.exporter), '-u', '-p', str(folder / 'capture.tracy')],
                               stdout=stream, stderr=subprocess.PIPE, check=True, timeout=30)
            result.update(analyze_csv(folder / 'zones.csv', case['target'],
                                      case.get('node_id', '1' if case['name'] == 'source' else '2'), size,
                                      case.get('expected_invocations', 1)))
        if getattr(args, 'overnight', False) and case['target'].startswith('Export'):
            if not any(p.is_file() and p.stat().st_size for p in (folder / 'exports').rglob('*')):
                raise ValueError('export target wrote no nonempty output')
        result['status'] = 'ok'
    except (ValueError, OSError, MemoryError, subprocess.SubprocessError) as error:
        result['error'] = str(error)
        result['failure_kind'] = type(error).__name__
    finally:
        if app is not None and app.poll() is None:
            os.killpg(app.pid, 15)
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(app.pid, 9)
                app.wait()
        stop(capture)
    result['wall_seconds_including_profiler_shutdown'] = time.monotonic() - started
    write_json(folder / 'result.json', result)
    print(f"{name}: {result['status']} {result.get('error', '')}", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=Path(__file__).with_name('pilot.json'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--time-tool', type=Path, required=True, help='GNU time executable')
    parser.add_argument('--sizes', type=int, nargs='+', default=[512, 1024])
    parser.add_argument('--repeats', type=int, default=1)
    parser.add_argument('--cases', nargs='+')
    parser.add_argument('--port', type=int, default=8097)
    parser.add_argument('--capture-seconds', type=int, default=10)
    parser.add_argument('--timeout', type=int, default=60)
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('this runner currently measures Linux process RSS')
    if any(size < 64 or size > 4096 for size in args.sizes) or args.repeats < 1:
        parser.error('sizes must be 64..4096 and repeats must be positive')
    if not 1 <= args.port <= 65535 or args.capture_seconds < 1 or args.timeout < 1:
        parser.error('invalid port or timeout')
    for field in ('binary', 'baseline', 'capture', 'exporter', 'time_tool', 'manifest', 'output'):
        setattr(args, field, getattr(args, field).resolve())
    for binary in (args.binary, args.baseline):
        if not binary.is_file() or not (binary.parent / 'data').is_dir():
            parser.error(f'missing executable or adjacent data directory: {binary}')
        if (binary.parent / 'hesiod.json').exists() or (binary.parent / 'portable.flag').exists():
            parser.error('portable settings would bypass the isolated pilot configuration')
    manifest = json.loads(args.manifest.read_text())
    cases = [c for c in manifest['cases'] if not args.cases or c['name'] in args.cases]
    if not cases or (args.cases and set(args.cases) - {c['name'] for c in cases}):
        parser.error('unknown/empty case selection')
    args.output.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(args.manifest, args.output / 'manifest.json')
    metadata = {
        'git_head': git('rev-parse', 'HEAD'), 'git_status': git('status', '--short'),
        'submodules': git('submodule', 'status', '--recursive'),
        'platform': platform.platform(), 'cpu': platform.processor(),
        'cpuinfo': Path('/proc/cpuinfo').read_text(),
        'arguments': {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
        'sha256': {k: sha256(getattr(args, k)) for k in
                   ('binary', 'baseline', 'capture', 'exporter', 'time_tool', 'manifest')},
        'runner_sha256': sha256(Path(__file__)),
        'environment_overrides': {'QT_QPA_PLATFORM': 'offscreen', 'TRACY_NO_SYS_TRACE': '1',
                                  'global.omp_num_threads': 1},
        'limitations': 'Fresh processes, OS/driver caches not flushed. Whole-process RSS, not per-node allocations. Source defaults are revision-dependent. One CPU input and default target variants only. Compare repeats before drawing conclusions.',
    }
    write_json(args.output / 'provenance.json', metadata)
    for label, binary in (('tracy', args.binary), ('baseline', args.baseline)):
        cache = binary.parent.parent / 'CMakeCache.txt'
        if cache.exists():
            shutil.copyfile(cache, args.output / f'{label}-CMakeCache.txt')
    rows = list(inventory())
    with (args.output / 'inventory.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    results = []
    for case in cases:
        path = args.output / f"{case['name']}.hsd"
        write_json(path, fixture(manifest, case))
        for size in args.sizes:
            for repeat in range(1, args.repeats + 1):
                for mode in ('baseline', 'tracy'):
                    result = run_one(args, case, path, size, repeat, mode)
                    results.append(result)
                    write_json(args.output / 'results.json', results)
                    # Never silently proceed to larger workloads after a failed run.
                    if result['status'] != 'ok':
                        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
