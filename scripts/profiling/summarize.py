#!/usr/bin/env python3
"""Summarize completed repetitions without turning process RSS into node allocations."""
import argparse
import csv
import json
from pathlib import Path
from statistics import median


def summarize(results):
    rows = []
    for case, size in sorted({(r['case'], r['size']) for r in results}):
        runs = [r for r in results if (r['case'], r['size']) == (case, size)]
        traced = [r for r in runs if r['mode'] == 'tracy' and r['status'] == 'ok']
        baseline = [r for r in runs if r['mode'] == 'baseline' and r['status'] == 'ok']
        times = [r['target_ms'] for r in traced]
        baseline_peaks = [r['process_peak_rss_kib'] / 1024 for r in baseline]
        traced_peaks = [r['process_peak_rss_kib'] / 1024 for r in traced]
        rows.append({
            'case': case, 'node': runs[0]['target'], 'size': size,
            'traced_ok': len(traced), 'baseline_ok': len(baseline),
            'failed': sum(r['status'] != 'ok' for r in runs),
            'target_ms_median': median(times) if times else None,
            'target_ms_min': min(times) if times else None,
            'target_ms_max': max(times) if times else None,
            'baseline_process_peak_mib_median': median(baseline_peaks) if baseline_peaks else None,
            'baseline_process_peak_mib_min': min(baseline_peaks) if baseline_peaks else None,
            'baseline_process_peak_mib_max': max(baseline_peaks) if baseline_peaks else None,
            'traced_process_peak_mib_median': median(traced_peaks) if traced_peaks else None,
            'traced_process_peak_mib_min': min(traced_peaks) if traced_peaks else None,
            'traced_process_peak_mib_max': max(traced_peaks) if traced_peaks else None,
            'traced_runs_with_at_least_two_rss_samples_in_target': sum(r['rss_samples_in_target'] >= 2 for r in traced),
        })
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    results = json.loads((args.directory / 'results.json').read_text())
    rows = summarize(results)
    if not rows:
        parser.error('no results')
    with (args.directory / 'summary.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    with (args.directory / 'inventory.csv').open() as stream:
        coverage = list(csv.DictReader(stream))
    for node in coverage:
        done = [r for r in rows if r['node'] == node['node'] and r['traced_ok']]
        failed = [r for r in rows if r['node'] == node['node'] and r['failed']]
        node['status'] = ('pilot_with_failures' if failed else 'pilot_variant_measured') if done else ('pilot_failed' if failed else 'unmeasured')
        node['pilot_sizes'] = ';'.join(str(r['size']) for r in done)
        node['scope'] = 'Only the manifest input/variant; other groups and parameter branches remain unmeasured' if done else ''
    with (args.directory / 'coverage.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(coverage[0]))
        writer.writeheader()
        writer.writerows(coverage)
    print(f'{len(rows)} case/resolution summaries; {sum(r["status"] == "ok" for r in results)}/{len(results)} successful runs')


if __name__ == '__main__':
    main()
