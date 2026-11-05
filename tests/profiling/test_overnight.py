import argparse
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/profiling'))
from inventory import inventory
from overnight_fixtures import make_manifest, write_inputs
from overnight import report, schedule, verify, run
from run_pilot import run_one, sha256, write_json


class OvernightTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads((ROOT / 'Hesiod/data/node_documentation.json').read_text())
        cls.cases, cls.fixtures = make_manifest(cls.catalog)

    def test_every_non_deprecated_type_has_an_explicit_entry(self):
        active = {r['node'] for r in inventory() if r['queue'] != 'legacy'}
        self.assertEqual({c['name'] for c in self.cases}, active)
        self.assertEqual(len(active), len(self.cases))
        self.assertTrue(all(c['status'] == 'ready' or c.get('reason') for c in self.cases))

    def test_missing_documentation_is_explicitly_unmeasured(self):
        catalog = dict(self.catalog)
        catalog.pop('Abs')
        cases, fixtures = make_manifest(catalog)
        case = next(c for c in cases if c['name'] == 'Abs')
        self.assertEqual(case['status'], 'needs_fixture')
        self.assertIn('documentation missing', case['reason'])
        self.assertNotIn('Abs', fixtures)

    def test_generated_links_have_matching_declared_types(self):
        for name, project in self.fixtures.items():
            self.assertFalse(project['graph_manager']['export_param']['export_path'])
            for graph in project['graph_manager']['graph_nodes'].values():
                nodes = {n['id']: n for n in graph['nodes']}
                self.assertEqual(len(nodes), len(graph['nodes']))
                for link in graph['links']:
                    src = self.catalog[nodes[link['node_id_from']]['label']]['ports'][link['port_id_from']]
                    dst = self.catalog[nodes[link['node_id_to']]['label']]['ports'][link['port_id_to']]
                    self.assertEqual(src['type'], 'output', name)
                    self.assertEqual(dst['type'], 'input', name)
                    self.assertEqual(src['data_type'], dst['data_type'], name)
                for node in nodes.values():
                    if 'containers' not in node:
                        continue
                    for container in node['containers'].values():
                        for key, attr in container.items():
                            if key == 'fname':
                                self.assertTrue(attr['value'].startswith(('inputs/', 'exports/')), name)
                                self.assertNotIn('..', attr['value'])

    def test_breadth_first_scheduling_and_blocked_fixture_exclusion(self):
        cases = [dict(name=n, queue='full_array', status=s) for n, s in
                 [('A', 'ready'), ('B', 'ready'), ('C', 'needs_fixture')]]
        jobs = list(schedule(cases, {'sizes': [64, 128], 'repeats': 2}))
        self.assertEqual([(c['name'], size, rep) for c, size, rep in jobs[:4]],
                         [('A', 64, 1), ('B', 64, 1), ('A', 64, 2), ('B', 64, 2)])
        self.assertEqual(len(jobs), 8)

    def test_snapshot_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            file = root / 'binary'
            file.write_text('original')
            config = {'hashes': {'binary': sha256(file)}}
            verify(root, config)
            file.write_text('changed')
            with self.assertRaises(ValueError):
                verify(root, config)

    def test_report_distinguishes_partial_failed_pending_and_needs_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cases = [dict(name=n, group='main', scope='test', status='ready') for n in 'ABCD']
            cases[-1].update(status='needs_fixture', reason='receiver required')
            write_json(root / 'config.json', {'sizes': [64, 128], 'repeats': 1})
            write_json(root / 'manifest.json', {'cases': cases})
            for name, state in [('A', 'ok'), ('B', 'failed')]:
                folder = root / 'runs' / name
                folder.mkdir(parents=True)
                write_json(folder / 'result.json', dict(case=name, status=state, size=64, repeat=1,
                            target_ms=1, process_peak_rss_kib=1024, rss_samples_in_target=0, error='timeout'))
            rows = {r['node']: r for r in report(root)}
            self.assertEqual([rows[n]['status'] for n in 'ABCD'], ['partial', 'failed', 'pending', 'needs_fixture'])
            self.assertIsNone(rows['B']['64_ms_median'])

    def test_memory_limit_stops_a_real_child_and_preserves_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'data').mkdir()
            (root / 'runs').mkdir()
            app = root / 'app'
            app.write_text(f'#!{sys.executable}\nimport time\ntime.sleep(60)\n')
            app.chmod(0o755)
            timer = root / 'timer'
            timer.write_text(f'#!{sys.executable}\nimport os,sys\na=sys.argv[sys.argv.index("-o")+2:]\nos.execv(a[0],a)\n')
            timer.chmod(0o755)
            graph = root / 'graph.hsd'
            graph.write_text('{}')
            import time
            args = argparse.Namespace(output=root / 'runs', binary=app, baseline=app,
                    time_tool=timer, overnight=True, max_rss_mib=1, timeout=5,
                    deadline=time.monotonic()+10, port=8097)
            result = run_one(args, {'name': 'test', 'target': 'test'}, graph, 64, 1, 'baseline')
            self.assertEqual(result['status'], 'failed')
            self.assertEqual(result['failure_kind'], 'MemoryError')
            self.assertIn('RSS limit', result['error'])
            self.assertTrue((root / 'runs/test-64-r1-baseline/result.json').exists())
            args.max_rss_mib = 1024
            args.timeout = 0.15
            result = run_one(args, {'name': 'timeout', 'target': 'test'}, graph, 64, 1, 'baseline')
            self.assertEqual(result['failure_kind'], 'TimeoutError')
            self.assertLess(result['wall_seconds_including_profiler_shutdown'], 3)

    def test_resume_skips_completed_jobs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = dict(sizes=[64], repeats=1, max_hours=1, hashes={}, environment={},
                          timeout=1, port=0, max_rss_mib=100, max_disk_gib=1)
            write_json(root / 'config.json', config)
            write_json(root / 'manifest.json', {'cases': [dict(name='A', target='A',
                       status='ready', queue='full_array', group='main', scope='test')]})
            write_json(root / 'status.json', {'state': 'stopped', 'elapsed_seconds': 2})
            folder = root / 'runs/A-64-r1-tracy'
            folder.mkdir(parents=True)
            write_json(folder / 'result.json', dict(case='A', size=64, repeat=1, status='ok',
                       target_ms=1, process_peak_rss_kib=1024, rss_samples_in_target=0))
            # No live workload needs a port in this completed-run resume test.
            with patch('overnight.socket.socket'):
                self.assertEqual(run(argparse.Namespace(directory=root, resume=True)), 0)
            self.assertEqual(json.loads((root / 'status.json').read_text())['state'], 'finished')


if __name__ == '__main__':
    unittest.main()
