#!/usr/bin/env python3
"""Measure focused node memory scaling in fresh Linux processes, without Tracy.

Peak RSS is obtained from wait4 for each directly executed child. It includes
startup, sources, target, exports and teardown; it is not allocation ownership,
GPU memory, or a per-node peak. The /proc sampler is only a stop guard.
"""
import argparse
import collections
import copy
import csv
import itertools
import json
import math
import os
from pathlib import Path
import platform
import shutil
import signal
import struct
import subprocess
import sys
import time
from statistics import median

from overnight_fixtures import build_fixture
from run_pilot import ROOT, application_errors, git, sha256, write_json

CANDIDATES = ('ExportAsset', 'TextureQuiltingShuffle', 'TextureQuiltingExpand')
CASES = (*CANDIDATES, 'Abs', 'GaussianPulse', 'ColorizeGradient')
MINIMAL_INPUTS = {
    'ExportAsset': {'elevation'},
    'TextureQuiltingShuffle': {'texture (guide)', 'heightmap (guide)'},
    'TextureQuiltingExpand': {'texture (guide)', 'heightmap (guide)'},
}
POLICIES = {
    'ram-sequential': {'flag': '--force-sequential', 'storage': 'VA_RAM'},
    'disk-sequential': {'flag': '--min-memory', 'storage': 'VA_DISK_LRU_STREAM'},
}
ENVIRONMENT_KEYS = ('PATH', 'LD_LIBRARY_PATH', 'QT_PLUGIN_PATH', 'QML2_IMPORT_PATH',
                    'XDG_DATA_DIRS', 'OCL_ICD_VENDORS', 'OPENCL_VENDOR_PATH',
                    'FONTCONFIG_FILE', 'FONTCONFIG_PATH', 'LIBGL_DRIVERS_PATH',
                    '__EGL_VENDOR_LIBRARY_DIRS', 'XDG_RUNTIME_DIR', 'LD_PRELOAD',
                    'GLIBC_TUNABLES', 'MALLOC_ARENA_MAX', 'MALLOC_TRIM_THRESHOLD_',
                    'MALLOC_MMAP_THRESHOLD_', 'OMP_DYNAMIC', 'OMP_THREAD_LIMIT',
                    'MKL_NUM_THREADS')


def make_fixture(catalog, case, variant='full', inputs_only=False):
    """Return a deterministic graph and all nodes whose completion is required.

    Full fixtures connect every input, as in the October survey. Minimal quilts
    retain both guides and omit A-D; the missing-heightmap fallback is not tested.
    Input controls have exactly the corresponding source subgraph, no target.
    """
    if case not in CASES or variant not in ('minimal', 'full'):
        raise ValueError('unknown case or input variant')
    selected = copy.deepcopy(catalog)
    required = MINIMAL_INPUTS.get(case, {'Abs': {'input'}, 'ColorizeGradient': {'level'}}.get(case, set()))
    ports = (selected.get(case) or {}).get('ports') or {}
    if any(name not in ports or ports[name].get('type') != 'input' for name in required):
        raise ValueError(f'{case} documentation is missing required fixture input ports')
    if variant == 'minimal' and case in MINIMAL_INPUTS:
        selected[case]['ports'] = {
            name: spec for name, spec in selected[case]['ports'].items()
            if spec['type'] != 'input' or name in MINIMAL_INPUTS[case]}
    graph, target_id = build_fixture(selected, case)
    graph_node = graph['graph_manager']['graph_nodes']['graph']
    if inputs_only:
        graph_node['nodes'] = [n for n in graph_node['nodes'] if n['id'] != target_id]
        graph_node['links'] = [link for link in graph_node['links']
                               if link['node_id_to'] != target_id]
        if not graph_node['nodes']:
            raise ValueError(f'{case} has no source graph for an input-only control')
    expected = [{'graph': 'graph', 'id': n['id'], 'type': n['label']}
                for n in graph_node['nodes']]
    return graph, expected


def fresh_fixture_nodes(graph):
    nodes = [node for item in graph['graph_manager']['graph_nodes'].values()
             for node in item['nodes']]
    if any('runtime_info' in node for node in nodes):
        raise ValueError('persisted node runtime_info would invalidate completion evidence')
    return nodes


def prepare_run(folder, binary, graph, size, tiles, overlap, policy, threads=1):
    """Prepare an isolated cwd; return direct Hesiod argv and environment.

    This also supports external allocation profilers: wrap the returned command,
    keeping cwd=folder and env=environment. Such measurements belong in a separate
    series because profiler allocation overhead changes RSS and elapsed time.
    """
    fresh_fixture_nodes(graph)
    folder, binary = folder.resolve(), binary.resolve()
    folder.mkdir(parents=True, exist_ok=False)
    (folder / 'data').symlink_to(binary.parent / 'data', target_is_directory=True)
    (folder / 'exports').mkdir()
    (folder / 'tmp').mkdir(mode=0o700)
    config = folder / 'config' / 'hesiod'
    config.mkdir(parents=True)
    write_json(config / 'hesiod.json', {
        'app_settings': {'global.omp_num_threads': threads}, 'style_settings': {}})
    write_json(folder / 'fixture.hsd', graph)
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen',
               XDG_CONFIG_HOME=str(folder / 'config'),
               XDG_CACHE_HOME=str(folder / 'cache'), TMPDIR=str(folder / 'tmp'),
               OMP_NUM_THREADS=str(threads), OPENBLAS_NUM_THREADS=str(threads),
               HESIOD_TRACY_WAIT='0', TRACY_NO_SYS_TRACE='1')
    command = [str(binary), f'--batch={folder / "fixture.hsd"}',
               f'--shape={size},{size}', f'--tiling={tiles},{tiles}',
               f'--overlap={overlap:g}', POLICIES[policy]['flag']]
    return command, env


def measure_process(command, folder, env, timeout, max_rss_mib, supervisor_script=None):
    """Measure through a fresh small supervisor, never the controller's child RSS.

    wait4 inside the supervisor accounts for its direct Hesiod child. The fresh
    interpreter prevents catalog/results memory in this controller from setting
    the inherited pre-exec RSS floor. The remaining supervisor floor is recorded.
    """
    folder = folder.resolve()
    request = folder / 'measure-request.json'
    write_json(request, dict(command=command, timeout=timeout, max_rss_mib=max_rss_mib))
    supervisor_script = (supervisor_script or Path(__file__).with_name('memory_child.py')).resolve()
    completed = False
    with (folder / 'supervisor.log').open('w') as log:
        supervisor = subprocess.Popen([sys.executable, str(supervisor_script), str(request)],
                                      cwd=folder, env=env, stdout=log, stderr=subprocess.STDOUT,
                                      start_new_session=True)
        try:
            code = supervisor.wait(timeout=timeout + 30)
            if code != 0:
                raise ValueError(f'memory supervisor exited {code}; inspect supervisor.log')
            completed = True
        except BaseException:
            # Let the supervisor reap and account for its direct workload first.
            if supervisor.poll() is None:
                supervisor.terminate()
                try:
                    supervisor.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
            raise
        finally:
            # Backstop if the supervisor was killed or could not finish cleanup.
            if not completed:
                child_pid = folder / 'child.pid'
                if child_pid.exists():
                    try:
                        os.killpg(int(child_pid.read_text()), signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                if supervisor.poll() is None:
                    try:
                        os.killpg(supervisor.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    supervisor.wait()
    return json.loads((folder / 'measurement.json').read_text())


def validate_run(folder, expected, size, tiles, overlap, export=False):
    fresh_fixture_nodes(json.loads((folder / 'fixture.hsd').read_text()))
    logs = (folder / 'app.log').read_text(errors='replace')
    benign = 'smooth_overlap_buffers requires at least 2 tiles in memory, skipping'
    # Known single-tile no-op only. Never hide this warning for tiled workloads.
    warnings = logs.count(benign) if tiles == 1 else 0
    if tiles == 1:
        logs = '\n'.join(line for line in logs.splitlines() if benign not in line)
    errors, defaults = application_errors(logs)
    if errors:
        raise ValueError('application logged an error; inspect app.log')
    for marker in (f'compute shape: {{{size}, {size}}}',
                   f'compute tiling: {{{tiles}, {tiles}}}'):
        if marker not in logs:
            raise ValueError(f'missing effective configuration: {marker}')
    overlap_values = [line.split('compute overlap: ', 1)[1].strip()
                      for line in logs.splitlines() if 'compute overlap: ' in line]
    if not overlap_values or not math.isclose(float(overlap_values[-1]), overlap,
                                              rel_tol=1e-6, abs_tol=1e-7):
        raise ValueError('missing or mismatched effective overlap')
    batch = folder / 'batch.log'
    if not batch.is_file():
        raise ValueError('missing completed batch.log')
    timings = {}
    for line in batch.read_text().splitlines():
        if not line.strip():
            continue
        parts = line.split('|')
        if len(parts) != 3 or (parts[0], parts[1]) in timings:
            raise ValueError('malformed or duplicate completed runtime row')
        timings[(parts[0], parts[1])] = float(parts[2])
    expected_keys = {(str(n['id']), n['type']) for n in expected}
    if set(timings) != expected_keys or len(expected_keys) != len(expected):
        raise ValueError('completed runtime rows do not exactly match expected nodes')
    starts = []
    for node in expected:
        # BaseNode assigns update_time only at NRS_UPDATE_END; fresh generated
        # fixtures carry no persisted runtime_info. batch.log is written after
        # graph worker finish. Exceptions also reach END, hence error rejection.
        # Concurrent native loggers can lose/fragment trace lines: count them
        # only as diagnostics, not as reliable completion evidence.
        marker = f'computing node [{node["type"]}]/[{node["id"]}]'
        starts.append(dict(node, occurrences=logs.count(marker)))
        timing = timings[(str(node['id']), node['type'])]
        if not math.isfinite(timing) or timing <= 0:
            raise ValueError(f'missing/invalid completed runtime for {node["type"]}/{node["id"]}')
    artifacts = []
    if export:
        meshes = list((folder / 'exports').glob('*.glb'))
        if len(meshes) != 1:
            raise ValueError('expected one exported GLB mesh')
        path = meshes[0]
        with path.open('rb') as stream:
            header = stream.read(20)
            if len(header) != 20:
                raise ValueError('truncated GLB mesh')
            magic, version, length, json_length, chunk_type = struct.unpack('<4sIIII', header)
            if magic != b'glTF' or version != 2 or length != path.stat().st_size or chunk_type != 0x4E4F534A:
                raise ValueError('invalid GLB mesh header')
            if json_length > length - 20:
                raise ValueError('invalid GLB JSON chunk length')
            mesh_json = json.loads(stream.read(json_length))
            if not mesh_json.get('meshes'):
                raise ValueError('GLB contains no mesh')
        textures = list((folder / 'exports').glob('*.png'))
        if not textures:
            raise ValueError('missing exported normal map')
        for texture in textures:
            with texture.open('rb') as stream:
                header = stream.read(24)
            if (len(header) != 24 or header[:8] != b'\x89PNG\r\n\x1a\n' or
                    header[12:16] != b'IHDR' or
                    struct.unpack('>II', header[16:24]) != (size, size)):
                raise ValueError('invalid exported PNG dimensions/header')
        artifacts = [{'path': str(p.relative_to(folder)), 'bytes': p.stat().st_size,
                      'sha256': sha256(p)} for p in [*meshes, *textures]]
    return {'validation': 'fresh fixture and positive completed batch runtime for exactly the expected node set, '
                          'error-free logs, effective configuration; '
                          + ('GLB structure and PNG dimensions' if export else
                             'numerical output correctness is not assessed'),
            'compute_start_log_occurrences': starts,
            'compiled_setting_defaults_used': defaults,
            'known_single_tile_noop_warnings': warnings, 'artifacts': artifacts}


def job_key(job):
    return (job['case'], job['variant'], job['inputs_only'], job['policy'],
            job['tiles'], job['overlap'])


def jobs_for(config):
    for size, repeat, case, policy, tiles, overlap in itertools.product(
            config['sizes'], range(1, config['repeats'] + 1), config['cases'],
            config['policies'], config['tiles'], config['overlaps']):
        # A single tile has no halo neighbour: nonzero overlaps are redundant.
        if tiles == 1 and overlap != 0:
            continue
        variants = config['variants'] if case in CANDIDATES else ['full']
        for variant in variants:
            modes = [False, True] if config['include_inputs'] and case != 'GaussianPulse' else [False]
            for inputs_only in modes:
                yield dict(case=case, variant=variant, inputs_only=inputs_only,
                           size=size, repeat=repeat, policy=policy, tiles=tiles, overlap=overlap)


def run_name(job):
    return (f'{job["case"]}-{job["variant"]}' + ('-inputs' if job['inputs_only'] else '') +
            f'-{job["policy"]}-{job["size"]}-t{job["tiles"]}-o{job["overlap"]:g}-r{job["repeat"]}')


def summarize(config, results):
    """Report incomplete cells and compare only complete adjacent-size medians.

    Guard: peak_large <= peak_small * pixel_ratio * slack + absolute_slack.
    It deliberately does not subtract process peaks or claim node allocations.
    """
    indexed = {run_name(row): row for row in results}
    if len(indexed) != len(results):
        raise ValueError('duplicate measurement identities')
    planned = list(jobs_for(config))
    if set(indexed) - {run_name(job) for job in planned}:
        raise ValueError('measurements outside the planned matrix')
    groups = collections.defaultdict(list)
    for job in planned:
        groups[(*job_key(job), job['size'])].append(indexed.get(run_name(job)))
    cells = []
    for key, runs in sorted(groups.items()):
        good = [r for r in runs if r and r['status'] == 'ok' and
                isinstance(r.get('process_peak_rss_kib'), (int, float)) and
                math.isfinite(r['process_peak_rss_kib']) and r['process_peak_rss_kib'] > 0 and
                isinstance(r.get('wall_seconds'), (int, float)) and
                math.isfinite(r['wall_seconds']) and r['wall_seconds'] >= 0]
        peaks = [r['process_peak_rss_kib'] / 1024 for r in good]
        case, variant, inputs_only, policy, tiles, overlap, size = key
        cells.append(dict(case=case, variant=variant, inputs_only=inputs_only, policy=policy,
                          tiles=tiles, overlap=overlap, size=size,
                          planned=len(runs), successful=len(good),
                          status='complete' if len(good) == len(runs) else 'incomplete',
                          process_peak_mib_median=median(peaks) if peaks else None,
                          process_peak_mib_min=min(peaks) if peaks else None,
                          process_peak_mib_max=max(peaks) if peaks else None,
                          wall_seconds_median=median(r['wall_seconds'] for r in good) if good else None))
    comparisons = []
    series = collections.defaultdict(list)
    for cell in cells:
        if cell['size'] >= config['check_min_size']:
            series[job_key(cell)].append(cell)
    for cells_by_size in series.values():
        for small, large in zip(cells_by_size, cells_by_size[1:]):
            row = {key: large[key] for key in ('case', 'variant', 'inputs_only', 'policy', 'tiles', 'overlap')}
            row.update(small_size=small['size'], large_size=large['size'], status='incomplete')
            if small['status'] == large['status'] == 'complete':
                pixels = (large['size'] / small['size']) ** 2
                low, high = small['process_peak_mib_median'], large['process_peak_mib_median']
                limit = low * pixels * config['scaling_slack'] + config['absolute_slack_mib']
                row.update(status='pass' if high <= limit else 'fail',
                           process_peak_ratio=high / low, pixel_ratio=pixels,
                           pixel_normalized_peak_ratio=high / low / pixels,
                           limit_mib=limit, observed_mib=high)
            comparisons.append(row)
    complete = bool(cells) and all(c['status'] == 'complete' for c in cells)
    check_status = ('pass' if complete and comparisons and all(c['status'] == 'pass' for c in comparisons)
                    else 'fail' if any(c['status'] == 'fail' for c in comparisons)
                    else 'incomplete')
    return {'complete': complete, 'memory_scaling_check': check_status,
            'cells': cells, 'comparisons': comparisons,
            'limitations': 'Whole-process kernel peak RSS including startup and source nodes; '
                           'not per-node allocation totals or GPU VRAM. Scalar RSS guards '
                           'can miss localized growth hidden by fixed process overhead. '
                           'Node completion does not establish numerical output correctness.'}


def report(root):
    config = json.loads((root / 'config.json').read_text())
    results = [json.loads(p.read_text()) for p in sorted((root / 'runs').glob('*/result.json'))]
    summary = summarize(config, results)
    write_json(root / 'results.json', results)
    write_json(root / 'report.json', summary)
    if summary['cells']:
        with (root / 'report.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(summary['cells'][0]))
            writer.writeheader()
            writer.writerows(summary['cells'])
    return summary


def validate_args(args):
    if not sys.platform.startswith('linux') or not hasattr(os, 'wait4'):
        raise ValueError('Linux wait4 and /proc are required')
    if args.repeats < 1 or args.threads < 1:
        raise ValueError('repeats and threads must be positive')
    if any(s < 64 or s > 8192 for s in args.sizes):
        raise ValueError('sizes must be between 64 and 8192')
    if any(t < 1 or any(s % t or s // t < 16 for s in args.sizes) for t in args.tiles):
        raise ValueError('tile counts must divide every size and leave tiles at least 16 pixels wide')
    if any(not math.isfinite(o) or not 0 <= o < 1 for o in args.overlaps):
        raise ValueError('overlaps must be finite and in [0, 1)')
    if 1 in args.tiles and 0 not in args.overlaps:
        raise ValueError('single-tile comparisons require overlap 0')
    for name in ('timeout', 'max_hours', 'max_rss_mib', 'max_disk_gib', 'scaling_slack'):
        value = getattr(args, name)
        if not math.isfinite(value) or value <= 0:
            raise ValueError(f'{name} must be finite and positive')
    if args.scaling_slack < 1 or args.check_min_size < 64:
        raise ValueError('scaling slack must be >=1 and check minimum size >=64')
    if not math.isfinite(args.absolute_slack_mib) or args.absolute_slack_mib < 0:
        raise ValueError('absolute slack must be finite and nonnegative')
    binary = args.binary.resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK) or not (binary.parent / 'data').is_dir():
        raise ValueError('an executable and adjacent data directory are required')
    if (binary.parent / 'portable.flag').exists() or (binary.parent / 'hesiod.json').exists():
        raise ValueError('portable settings would bypass isolated workload settings')


def run(args):
    validate_args(args)
    root, binary = args.output.resolve(), args.binary.resolve()
    config = {key: value for key, value in vars(args).items()
              if key not in ('command', 'output', 'binary')}
    for key in ('sizes', 'tiles', 'overlaps'):
        config[key] = sorted(set(config[key]))
    for key in ('cases', 'variants', 'policies'):
        config[key] = list(dict.fromkeys(config[key]))
    root.mkdir(parents=True, exist_ok=False)
    (root / 'runs').mkdir()
    # Snapshot the executable/data so an ongoing build cannot change a series.
    (root / 'bin').mkdir()
    shutil.copy2(binary, root / 'bin' / 'hesiod')
    shutil.copytree(binary.parent / 'data', root / 'bin' / 'data')
    for package in ('profiling', 'hsd'):
        shutil.copytree(ROOT / 'scripts' / package, root / 'scripts' / package,
                        ignore=shutil.ignore_patterns('__pycache__'))
    measured_binary = root / 'bin' / 'hesiod'
    catalog = json.loads((measured_binary.parent / 'data/node_documentation.json').read_text())
    write_json(root / 'config.json', config)
    (root / 'source.diff').write_text(git('diff', 'HEAD', '--', '.', ':!external'))
    provenance = {'created_at': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
                  'git_head': git('rev-parse', 'HEAD'), 'git_status': git('status', '--short'),
                  'submodules': git('submodule', 'status', '--recursive'),
                  'platform': platform.platform(), 'python': sys.version,
                  'cpuinfo': Path('/proc/cpuinfo').read_text(),
                  'loadavg_at_start': Path('/proc/loadavg').read_text().strip(),
                  'binary': str(binary), 'binary_sha256': sha256(measured_binary),
                  'runner_sha256': sha256(Path(__file__)),
                  'source_diff_sha256': sha256(root / 'source.diff'),
                  'environment': {k: os.environ[k] for k in ENVIRONMENT_KEYS if k in os.environ},
                  'runtime_policy': {p: dict(POLICIES[p], cpu_dispatch='VA_SEQUENTIAL',
                                            gpu_dispatch='VA_SEQUENTIAL',
                                            single_array_dispatch='VA_SINGLE_ARRAY',
                                            trim_storage=False) for p in config['policies']},
                  'environment_overrides': {'QT_QPA_PLATFORM': 'offscreen',
                                            'OMP_NUM_THREADS': str(args.threads),
                                            'OPENBLAS_NUM_THREADS': str(args.threads),
                                            'HESIOD_TRACY_WAIT': '0', 'TRACY_NO_SYS_TRACE': '1'},
                  'backend': 'CPU algorithms for the selected fixtures; no GPU memory measurement',
                  'graph_scheduling': 'Default concurrent graph worker: sequential tile dispatch and '
                                      'OMP_NUM_THREADS do not serialize independent source nodes.',
                  'measurement': 'Fresh stdlib Python supervisor directly spawns Hesiod and records Linux '
                                 'wait4 ru_maxrss (KiB) for that child, not the supervisor. Includes the '
                                 'whole lifetime and inherited supervisor RSS floor (recorded per run); '
                                 'kernel accounting may include waited descendants. No shell/time/profiler '
                                 'wrapper around Hesiod. /proc sampling only enforces a soft stop guard.',
                  'script_sha256': {str(p.relative_to(ROOT)): sha256(p)
                                    for base in ('scripts/profiling', 'scripts/hsd')
                                    for p in sorted((ROOT / base).glob('*.py'))},
                  'data_sha256': {str(p.relative_to(measured_binary.parent)): sha256(p)
                                  for p in sorted((measured_binary.parent / 'data').rglob('*')) if p.is_file()}}
    dependencies = {}
    for source in sorted((binary.parent.parent / '_deps').glob('*-src')):
        if (source / '.git').exists():
            dependencies[source.name] = {
                'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip(),
                'status': subprocess.check_output(['git', 'status', '--short'], cwd=source, text=True).strip()}
    provenance['fetchcontent_dependencies'] = dependencies
    write_json(root / 'provenance.json', provenance)
    cache = binary.parent.parent / 'CMakeCache.txt'
    if cache.is_file():
        shutil.copy2(cache, root / 'CMakeCache.txt')
    deadline = time.monotonic() + args.max_hours * 3600
    blocked = set()
    started = time.monotonic()
    state = {'state': 'running', 'planned_runs': len(list(jobs_for(config))), 'completed_runs': 0}
    write_json(root / 'status.json', state)
    try:
        for job in jobs_for(config):
            if time.monotonic() >= deadline:
                state['state'] = 'time_budget_exhausted'
                break
            if job_key(job) in blocked:
                continue  # keep other policies/fixtures moving, never escalate failed series
            used = sum(p.stat().st_size for p in (root / 'runs').rglob('*')
                       if p.is_file() and not p.is_symlink())
            if used > args.max_disk_gib * 1024**3 or shutil.disk_usage(root).free < 1024**3:
                state['state'] = 'disk_budget_exhausted'
                break
            folder = root / 'runs' / run_name(job)
            graph, expected = make_fixture(catalog, job['case'], job['variant'], job['inputs_only'])
            command, env = prepare_run(folder, measured_binary, graph, job['size'], job['tiles'],
                                       job['overlap'], job['policy'], args.threads)
            result = dict(job, status='failed', command=command,
                          started_at=time.strftime('%Y-%m-%dT%H:%M:%S%z'),
                          graph_sha256=sha256(folder / 'fixture.hsd'))
            state['current'] = run_name(job)
            write_json(root / 'status.json', state)
            try:
                result.update(measure_process(command, folder, env,
                                              min(args.timeout, deadline - time.monotonic()), args.max_rss_mib,
                                              supervisor_script=root / 'scripts/profiling/memory_child.py'))
                if result['failure_kind'] or result['app_exit'] != 0:
                    raise ValueError(result['failure_kind'] or f'application exit {result["app_exit"]}')
                result.update(validate_run(folder, expected, job['size'], job['tiles'], job['overlap'],
                                           export=job['case'] == 'ExportAsset' and not job['inputs_only']))
                result['status'] = 'ok'
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                result['error'] = str(error)
                blocked.add(job_key(job))
            except KeyboardInterrupt:
                result.update(status='interrupted', error='interrupted by signal')
                raise
            finally:
                write_json(folder / 'result.json', result)
                state['completed_runs'] += 1
            print(f'{run_name(job)}: {result["status"]} '
                  f'{result.get("process_peak_rss_kib", 0) / 1024:.1f} MiB '
                  f'{result.get("error", "")}', flush=True)
            report(root)
        else:
            state['state'] = 'finished'
    except KeyboardInterrupt:
        state['state'] = 'interrupted'
    except Exception as error:
        state.update(state='failed', error=str(error))
        raise
    finally:
        state.pop('current', None)
        state['elapsed_seconds'] = time.monotonic() - started
        write_json(root / 'status.json', state)
        summary = report(root)
    print(json.dumps({'state': state['state'], 'complete': summary['complete'],
                      'memory_scaling_check': summary['memory_scaling_check']}), flush=True)
    return 0 if summary['complete'] and (not args.check or summary['memory_scaling_check'] == 'pass') else 2


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    measure = commands.add_parser('run', help='run a new, bounded serial matrix')
    measure.add_argument('--binary', type=Path, required=True)
    measure.add_argument('--output', type=Path, required=True)
    measure.add_argument('--cases', choices=CASES, nargs='+', default=[*CANDIDATES, 'Abs'])
    measure.add_argument('--variants', choices=('minimal', 'full'), nargs='+', default=['minimal', 'full'])
    measure.add_argument('--include-inputs', action='store_true', help='also measure matched source-only subgraphs')
    measure.add_argument('--sizes', type=int, nargs='+', default=[512, 1024, 2048])
    measure.add_argument('--tiles', type=int, nargs='+', default=[1, 4])
    measure.add_argument('--overlaps', type=float, nargs='+', default=[0, 0.1])
    measure.add_argument('--policies', choices=tuple(POLICIES), nargs='+', default=['ram-sequential'])
    measure.add_argument('--threads', type=int, default=1)
    measure.add_argument('--repeats', type=int, default=3)
    measure.add_argument('--timeout', type=float, default=600)
    measure.add_argument('--max-hours', type=float, default=8)
    measure.add_argument('--max-rss-mib', type=float, default=6144)
    measure.add_argument('--max-disk-gib', type=float, default=20)
    measure.add_argument('--scaling-slack', type=float, default=1.25)
    measure.add_argument('--absolute-slack-mib', type=float, default=32)
    measure.add_argument('--check-min-size', type=int, default=512)
    measure.add_argument('--check', action='store_true', help='require complete memory-scaling comparisons to pass')
    summary = commands.add_parser('report', help='regenerate report without executing workloads')
    summary.add_argument('directory', type=Path)
    summary.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.command == 'run':
            return run(args)
        result = report(args.directory.resolve())
        print(json.dumps({k: result[k] for k in ('complete', 'memory_scaling_check')}))
        return 0 if result['complete'] and (not args.check or result['memory_scaling_check'] == 'pass') else 2
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    # Termination must leave a report and reap the current workload too.
    def request_stop(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, request_stop)
    sys.exit(main())
