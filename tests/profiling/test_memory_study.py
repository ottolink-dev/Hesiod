import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/profiling'))
from memory_study import (jobs_for, main, make_fixture, measure_process, prepare_run,
                          report, run_name, summarize, validate_args, validate_run)
from memory_child import measure_direct
from run_pilot import write_json


def configuration(**updates):
    config = dict(cases=['Abs'], variants=['minimal', 'full'], include_inputs=False,
                  sizes=[512, 1024], repeats=2, tiles=[1], overlaps=[0],
                  policies=['ram-sequential'], scaling_slack=1.25,
                  absolute_slack_mib=32, check_min_size=512)
    config.update(updates)
    return config


def successful(config):
    return [dict(job, status='ok', process_peak_rss_kib=100 * (job['size'] / 512) ** 2 * 1024,
                 wall_seconds=0.1) for job in jobs_for(config)]


class MemoryStudyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads((ROOT / 'Hesiod/data/node_documentation.json').read_text())

    def test_minimal_and_full_inputs_are_explicit_and_controls_match(self):
        for case, minimal_ports in [
                ('ExportAsset', {'elevation'}),
                ('TextureQuiltingShuffle', {'texture (guide)', 'heightmap (guide)'}),
                ('TextureQuiltingExpand', {'texture (guide)', 'heightmap (guide)'})]:
            for variant in ('minimal', 'full'):
                graph, expected = make_fixture(self.catalog, case, variant)
                nodes = graph['graph_manager']['graph_nodes']['graph']
                target = next(n for n in nodes['nodes'] if n['label'] == case)
                ports = {link['port_id_to'] for link in nodes['links'] if link['node_id_to'] == target['id']}
                if variant == 'minimal':
                    self.assertEqual(ports, minimal_ports)
                else:
                    self.assertGreater(len(ports), len(minimal_ports))
                control, control_expected = make_fixture(self.catalog, case, variant, inputs_only=True)
                control_nodes = control['graph_manager']['graph_nodes']['graph']
                self.assertEqual(control_nodes['nodes'], [n for n in nodes['nodes'] if n != target])
                self.assertEqual(control_expected, [n for n in expected if n['type'] != case])
                self.assertTrue(control_expected)
        with self.assertRaises(ValueError):
            make_fixture(self.catalog, 'GaussianPulse', inputs_only=True)
        missing = copy.deepcopy(self.catalog)
        missing['ExportAsset']['ports'].pop('elevation')
        with self.assertRaisesRegex(ValueError, 'missing required'):
            make_fixture(missing, 'ExportAsset', 'minimal')

    def test_schedule_is_breadth_first_and_single_tile_halo_not_duplicated(self):
        config = configuration(cases=['ExportAsset', 'Abs'], tiles=[1, 4], overlaps=[0, .1], include_inputs=True)
        jobs = list(jobs_for(config))
        self.assertEqual(len({run_name(job) for job in jobs}), len(jobs))
        self.assertEqual(len(jobs), 72)
        self.assertTrue(all(j['overlap'] == 0 for j in jobs if j['tiles'] == 1))
        self.assertTrue(all(j['variant'] == 'full' for j in jobs if j['case'] == 'Abs'))
        sizes = [j['size'] for j in jobs]
        self.assertEqual(sizes, sorted(sizes))

    def test_peak_growth_guard_uses_complete_medians_and_pixel_growth(self):
        config = configuration()
        results = successful(config)
        summary = summarize(config, results)
        self.assertEqual(summary['memory_scaling_check'], 'pass')
        self.assertEqual(summary['comparisons'][0]['pixel_normalized_peak_ratio'], 1)
        self.assertEqual(summary['comparisons'][0]['limit_mib'], 532)
        for row in results:
            if row['size'] == 1024:
                row['process_peak_rss_kib'] = 600 * 1024
        self.assertEqual(summarize(config, results)['memory_scaling_check'], 'fail')

    def test_failed_invalid_and_missing_results_never_pass(self):
        config = configuration()
        results = successful(config)
        for bad in [dict(status='failed'), dict(status='interrupted'),
                    dict(process_peak_rss_kib=float('nan')), dict(process_peak_rss_kib=0),
                    dict(process_peak_rss_kib='large'), dict(wall_seconds=-1)]:
            with self.subTest(bad=bad):
                altered = copy.deepcopy(results)
                altered[-1].update(bad)
                summary = summarize(config, altered)
                self.assertFalse(summary['complete'])
                self.assertEqual(summary['memory_scaling_check'], 'incomplete')
        self.assertEqual(summarize(config, [])['memory_scaling_check'], 'incomplete')
        self.assertEqual(summarize(config, results[:-1])['memory_scaling_check'], 'incomplete')
        with self.assertRaises(ValueError):
            summarize(config, results + [results[0]])
        altered = copy.deepcopy(results)
        altered[0]['size'] = 2048
        with self.assertRaises(ValueError):
            summarize(config, altered)

    def test_guard_cannot_pass_without_two_sizes_at_minimum(self):
        config = configuration(check_min_size=1024)
        summary = summarize(config, successful(config))
        self.assertTrue(summary['complete'])
        self.assertEqual(summary['memory_scaling_check'], 'incomplete')
        self.assertEqual(summary['comparisons'], [])

    def test_partial_report_cli_returns_nonzero(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'runs').mkdir()
            write_json(root / 'config.json', configuration())
            self.assertEqual(main(['report', str(root), '--check']), 2)
            self.assertTrue((root / 'report.csv').exists())
            self.assertFalse(json.loads((root / 'report.json').read_text())['complete'])

    def test_isolated_settings_and_explicit_execution_policy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'bin/data').mkdir(parents=True)
            binary = root / 'bin/hesiod'
            graph, _ = make_fixture(self.catalog, 'Abs')
            for policy, flag in [('ram-sequential', '--force-sequential'), ('disk-sequential', '--min-memory')]:
                folder = root / policy
                command, env = prepare_run(folder, binary, graph, 512, 4, .1, policy, threads=2)
                self.assertEqual(command[0], str(binary))
                self.assertIn('--tiling=4,4', command)
                self.assertIn(flag, command)
                self.assertNotIn('--ipc', command)
                self.assertEqual(env['XDG_CONFIG_HOME'], str(folder / 'config'))
                self.assertEqual(env['TMPDIR'], str(folder / 'tmp'))
                self.assertTrue((folder / 'tmp').is_dir())
                self.assertEqual(env['HESIOD_TRACY_WAIT'], '0')
                self.assertEqual(env['OMP_NUM_THREADS'], '2')
                self.assertEqual(json.loads((folder / 'config/hesiod/hesiod.json').read_text())[
                    'app_settings']['global.omp_num_threads'], 2)

    def test_invalid_resource_and_matrix_options_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'data').mkdir()
            binary = root / 'hesiod'
            binary.write_text('')
            binary.chmod(0o755)
            base = dict(configuration(), binary=binary, threads=1, timeout=1,
                        max_hours=1, max_rss_mib=1024, max_disk_gib=1)
            validate_args(argparse.Namespace(**base))
            for update in [dict(sizes=[0]), dict(tiles=[3]), dict(tiles=[0]),
                           dict(overlaps=[float('nan')]), dict(overlaps=[1]),
                           dict(overlaps=[.1]), dict(repeats=0), dict(threads=0),
                           dict(timeout=float('inf')), dict(max_hours=-1),
                           dict(scaling_slack=.5), dict(absolute_slack_mib=-1)]:
                with self.subTest(update=update), self.assertRaises(ValueError):
                    validate_args(argparse.Namespace(**dict(base, **update)))
            (root / 'portable.flag').touch()
            with self.assertRaises(ValueError):
                validate_args(argparse.Namespace(**base))

    def test_end_to_end_runner_snapshots_sources_and_reports_a_complete_matrix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'bin/data').mkdir(parents=True)
            shutil.copyfile(ROOT / 'Hesiod/data/node_documentation.json',
                            root / 'bin/data/node_documentation.json')
            binary = root / 'bin/hesiod'
            binary.write_text(f"#!{sys.executable}\n" + '''
import json, pathlib, sys
args = dict(arg[2:].split('=', 1) for arg in sys.argv[1:] if '=' in arg)
size = int(args['shape'].split(',')[0])
tiles = int(args['tiling'].split(',')[0])
print(f'compute shape: {{{size}, {size}}}')
print(f'compute tiling: {{{tiles}, {tiles}}}')
print('compute overlap: ' + args['overlap'])
fixture = json.loads(pathlib.Path(args['batch']).read_text())
nodes = fixture['graph_manager']['graph_nodes']['graph']['nodes']
rows = []
for node in nodes:
    print('computing node [' + node['label'] + ']/[' + node['id'] + ']')
    rows.append(node['id'] + '|' + node['label'] + '|0.1')
pathlib.Path('batch.log').write_text('\\n'.join(rows))
''')
            binary.chmod(0o755)
            output = root / 'study'
            self.assertEqual(main(['run', '--binary', str(binary), '--output', str(output),
                                   '--cases', 'Abs', '--include-inputs', '--sizes', '64', '128',
                                   '--tiles', '1', '--overlaps', '0', '--repeats', '1',
                                   '--check-min-size', '64', '--check']), 0)
            self.assertTrue((output / 'scripts/profiling/memory_study.py').is_file())
            self.assertTrue((output / 'scripts/hsd/builder.py').is_file())
            self.assertEqual((output / 'scripts/profiling/memory_study.py').read_bytes(),
                             (ROOT / 'scripts/profiling/memory_study.py').read_bytes())
            self.assertTrue(report(output)['complete'])
            self.assertEqual(main(['report', str(output), '--check']), 0)

    def write_validation_files(self, folder, export=False):
        (folder / 'app.log').write_text('\n'.join([
            'compute shape: {64, 64}', 'compute tiling: {1, 1}', 'compute overlap: 0',
            'computing node [ExportAsset]/[2]']))
        write_json(folder / 'fixture.hsd', {'graph_manager': {'graph_nodes': {'graph': {
            'nodes': [{'id': '2', 'label': 'ExportAsset'}]}}}})
        (folder / 'batch.log').write_text('2|ExportAsset|0.1\n')
        if export:
            (folder / 'exports').mkdir()
            data = json.dumps({'meshes': [{'primitives': []}]}).encode()
            (folder / 'exports/result.glb').write_bytes(struct.pack('<4sIIII', b'glTF', 2, len(data) + 20,
                                                                         len(data), 0x4E4F534A) + data)
            (folder / 'exports/result_nmap.png').write_bytes(
                b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 64, 64))
        return [{'graph': 'graph', 'id': '2', 'type': 'ExportAsset'}]

    def test_completion_and_export_validation_reject_partial_or_wrong_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            expected = self.write_validation_files(folder, export=True)
            self.assertEqual(len(validate_run(folder, expected, 64, 1, 0, export=True)['artifacts']), 2)
            logs = (folder / 'app.log').read_text()
            for suffix in ['\n[error] computation failed', '\n[critical] computation failed']:
                (folder / 'app.log').write_text(logs + suffix)
                with self.assertRaises(ValueError):
                    validate_run(folder, expected, 64, 1, 0, export=True)
            (folder / 'app.log').write_text(logs.replace('computing node', 'not computing'))
            self.assertEqual(validate_run(folder, expected, 64, 1, 0)[
                'compute_start_log_occurrences'][0]['occurrences'], 0)
            (folder / 'app.log').write_text(logs)
            for batch in ('', '2|ExportAsset|0', '2|ExportAsset|nan',
                          '2|ExportAsset|0.1\n2|ExportAsset|0.2',
                          '2|ExportAsset|0.1\n3|Abs|0.2'):
                (folder / 'batch.log').write_text(batch)
                with self.assertRaises(ValueError):
                    validate_run(folder, expected, 64, 1, 0)
            (folder / 'batch.log').write_text('2|ExportAsset|0.1')
            with self.assertRaises(ValueError):
                validate_run(folder, expected, 64, 4, 0)
            with self.assertRaises(ValueError):
                validate_run(folder, expected, 64, 1, .1)
            fixture = json.loads((folder / 'fixture.hsd').read_text())
            fixture['graph_manager']['graph_nodes']['graph']['nodes'][0]['runtime_info'] = {'update_time': 10}
            write_json(folder / 'fixture.hsd', fixture)
            with self.assertRaisesRegex(ValueError, 'persisted'):
                validate_run(folder, expected, 64, 1, 0)
            with self.assertRaisesRegex(ValueError, 'persisted'):
                prepare_run(folder / 'stale', folder / 'binary', fixture, 64, 1, 0, 'ram-sequential')
            fixture['graph_manager']['graph_nodes']['graph']['nodes'][0].pop('runtime_info')
            write_json(folder / 'fixture.hsd', fixture)
            (folder / 'exports/result.glb').write_bytes(b'glTF')
            with self.assertRaises(ValueError):
                validate_run(folder, expected, 64, 1, 0, export=True)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'wait4 RSS measurement is Linux-only')
    def test_real_child_peak_is_not_cumulative_and_failure_is_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # This large live controller allocation must not become either
            # direct workload's kernel peak after fork/exec.
            controller_memory = bytearray(100 * 1024 * 1024)
            peaks = []
            for label, megabytes in [('large', 64), ('small', 1)]:
                folder = root / label
                folder.mkdir()
                result = measure_process([sys.executable, '-c',
                    f'x=bytearray({megabytes}*1024*1024); print(len(x))'],
                    folder, dict(os.environ), timeout=10, max_rss_mib=1024)
                self.assertEqual(result['app_exit'], 0)
                self.assertIsNone(result['failure_kind'])
                peaks.append(result['process_peak_rss_kib'])
            self.assertGreater(peaks[0] - peaks[1], 30 * 1024)
            self.assertLess(peaks[1], 40 * 1024)
            self.assertEqual(len(controller_memory), 100 * 1024 * 1024)
            self.assertGreater(result['supervisor_rss_before_spawn_kib'], 0)
            folder = root / 'failed'
            folder.mkdir()
            result = measure_process([sys.executable, '-c', 'raise SystemExit(3)'],
                                     folder, dict(os.environ), timeout=10, max_rss_mib=1024)
            self.assertEqual(result['app_exit'], 3)
            self.assertGreater(result['process_peak_rss_kib'], 0)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'wait4 RSS measurement is Linux-only')
    def test_timeout_and_memory_guard_reap_real_children(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for label, timeout, limit in [('timeout', .15, 1024), ('rss_limit', 5, 1)]:
                folder = root / label
                folder.mkdir()
                result = measure_process([sys.executable, '-c', 'import time; time.sleep(60)'],
                                         folder, dict(os.environ), timeout, limit)
                self.assertEqual(result['failure_kind'], label)
                self.assertEqual(result['app_exit'], -9)
                self.assertLess(result['wall_seconds'], 3)
                self.assertGreater(result['process_peak_rss_kib'], 0)


    @unittest.skipUnless(sys.platform.startswith('linux'), 'process group cleanup is Linux-only')
    def test_termination_reaps_workload_and_kills_its_descendants(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'bin/data').mkdir(parents=True)
            shutil.copyfile(ROOT / 'Hesiod/data/node_documentation.json',
                            root / 'bin/data/node_documentation.json')
            binary = root / 'bin/hesiod'
            binary.write_text(f'#!{sys.executable}\n' + '''
import json, os, pathlib, subprocess, sys, time
child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])
pathlib.Path('descendants.json').write_text(json.dumps([os.getpid(), child.pid]))
time.sleep(60)
''')
            binary.chmod(0o755)
            output = root / 'study'
            with (root / 'controller.log').open('w') as log:
                controller = subprocess.Popen([sys.executable, str(ROOT / 'scripts/profiling/memory_study.py'),
                    'run', '--binary', str(binary), '--output', str(output), '--cases', 'Abs',
                    '--sizes', '64', '--tiles', '1', '--overlaps', '0', '--repeats', '1'],
                    stdout=log, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 15
                    pid_files = []
                    while time.monotonic() < deadline:
                        pid_files = list((output / 'runs').glob('*/descendants.json'))
                        if pid_files:
                            break
                        if controller.poll() is not None:
                            self.fail((root / 'controller.log').read_text())
                        time.sleep(.05)
                    self.assertTrue(pid_files, 'workload failed to start')
                    pids = json.loads(pid_files[0].read_text())
                    controller.terminate()
                    self.assertEqual(controller.wait(timeout=10), 2)
                finally:
                    if controller.poll() is None:
                        controller.kill()
                        controller.wait()
                for pid in pids:
                    state = Path(f'/proc/{pid}/stat')
                    if state.exists():
                        self.assertEqual(state.read_text().rsplit(')', 1)[1].split()[0], 'Z')
                self.assertEqual(json.loads((output / 'status.json').read_text())['state'], 'interrupted')
                self.assertFalse(json.loads((output / 'report.json').read_text())['complete'])

    @unittest.skipUnless(sys.platform.startswith('linux'), 'wait4 RSS measurement is Linux-only')
    def test_exit_during_kill_does_not_lose_accounting(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            original_kill = os.killpg

            def disappear(pid, signum):
                original_kill(pid, signum)
                raise ProcessLookupError('simulated exit before killpg')

            with patch('memory_child.os.killpg', side_effect=disappear):
                result = measure_direct([sys.executable, '-c', 'import time; time.sleep(60)'],
                                         folder, dict(os.environ), .1, 1024)
            self.assertEqual(result['failure_kind'], 'timeout')
            self.assertGreater(result['process_peak_rss_kib'], 0)


if __name__ == '__main__':
    unittest.main()
