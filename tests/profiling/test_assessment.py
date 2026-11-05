import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/profiling'))
from inventory import inventory, uncomment
from run_pilot import analyze_csv, application_errors, fixture
from summarize import summarize


class AssessmentTests(unittest.TestCase):
    def test_failed_measurements_are_retained_but_excluded_from_statistics(self):
        common = {'case': 'abs', 'size': 512, 'target': 'Abs', 'mode': 'tracy',
                  'process_peak_rss_kib': 1024, 'rss_samples_in_target': 0}
        rows = summarize([
            dict(common, status='ok', target_ms=2),
            dict(common, status='ok', target_ms=4),
            dict(common, status='failed', target_ms=1000),
        ])
        self.assertEqual(rows[0]['target_ms_median'], 3)
        self.assertEqual(rows[0]['failed'], 1)
        self.assertEqual(rows[0]['traced_ok'], 2)
        self.assertEqual(rows[0]['baseline_ok'], 0)
        self.assertIsNone(rows[0]['baseline_process_peak_mib_median'])

    def test_only_settings_default_messages_are_tolerated(self):
        logs = '\n'.join([
            'AppSettings::json_from',
            '[---E---] Required json key "colors.bg_deep" not found.',
            'StyleSettings::json_from',
            '[---E---] Required json key "shape.x" not found.',
            '[---E---] BaseNode::compute: failed',
        ])
        errors, defaults = application_errors(logs)
        self.assertEqual(defaults, 1)
        self.assertEqual(len(errors), 2)

    def test_comments_do_not_create_fake_signals(self):
        source = '// to_array()\n"https://example"; /* cm_single_array\n */\nfrom_array();'
        result = uncomment(source)
        self.assertNotIn('to_array()', result)
        self.assertNotIn('cm_single_array', result)
        self.assertIn('"https://example"', result)
        self.assertEqual(source.count('\n'), result.count('\n'))

    def test_registry_resolves_nonmatching_filenames_and_group_constants(self):
        nodes = {r['node']: r for r in inventory()}
        self.assertFalse(any(r['source_missing'] for r in nodes.values()))
        self.assertTrue(nodes['Median3x3']['source'].endswith('median_3x3.cpp'))
        self.assertIn('FBM', nodes['CoherentNoise']['groups'])
        self.assertEqual(nodes['DistanceTransform']['queue'], 'full_array')
        self.assertEqual(nodes['Abs']['queue'], 'configurable_tiles')
        for node in nodes.values():
            if 'DEPRECATED' in node['category']:
                self.assertEqual(node['queue'], 'legacy')

    def test_fixtures_have_only_source_and_target_and_no_exports(self):
        manifest = json.loads((ROOT / 'scripts/profiling/pilot.json').read_text())
        for case in manifest['cases']:
            data = fixture(manifest, case)['graph_manager']
            graph = data['graph_nodes']['graph']
            self.assertEqual(len(graph['nodes']), 1 if case['name'] == 'source' else 2)
            self.assertEqual(graph['nodes'][-1]['label'], case['target'])
            self.assertEqual(data['export_param']['export_path'], '')
            self.assertEqual(len(graph['links']), len(graph['nodes']) - 1)

    def csv_result(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'zones.csv'
            with path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=['name', 'ns_since_start', 'exec_time_ns', 'value'])
                writer.writeheader()
                writer.writerows(rows)
            return analyze_csv(path, 'Abs', '2', 512)

    def test_short_zones_are_not_reported_as_measured_memory_peaks(self):
        result = self.csv_result([
            {'name': 'Node algorithm', 'ns_since_start': 100, 'exec_time_ns': 100,
             'value': 'Abs id=2 shape=512x512 tiling=1x1'},
            {'name': 'Process RSS', 'ns_since_start': 150, 'value': 1234},
            {'name': 'Process peak RSS', 'ns_since_start': 150, 'value': 2345},
        ])
        self.assertEqual(result['rss_assessment'], 'insufficient_samples')
        self.assertIsNone(result['sampled_process_rss_max_during_target_bytes'])

    def test_missing_or_truncated_target_is_rejected(self):
        with self.assertRaises(ValueError):
            self.csv_result([{'name': 'Node algorithm', 'ns_since_start': 100,
                              'exec_time_ns': 0, 'value': 'wrong node'}])


if __name__ == '__main__':
    unittest.main()
