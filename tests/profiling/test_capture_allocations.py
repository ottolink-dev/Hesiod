import json
import os
import shutil
import signal
import subprocess
from pathlib import Path
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/profiling'))
from capture_allocations import analyze_capture, read_peak_stacks


class AllocationCaptureTests(unittest.TestCase):
    def tool(self, folder, code):
        path = folder / 'printer'
        path.write_text(f'#!{sys.executable}\n' + code)
        path.chmod(0o755)
        return path

    def test_peak_export_counts_exclusive_stacks_once(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            printer = self.tool(folder, '''import pathlib, sys
assert '--merge-backtraces=0' in sys.argv
assert '--flamegraph-cost-type=peak' in sys.argv
pathlib.Path(sys.argv[sys.argv.index('--print-flamegraph')+1]).write_text(
    'main;common;first 1024\\nmain;common;second 2048\\n')
print('unmerged allocation report')
''')
            result = analyze_capture(folder / 'input.gz', printer, folder / 'analysis', 5, 512)
            self.assertEqual(result['intercepted_heap_at_global_peak_bytes'], 3072)
            self.assertEqual(result['peak_stack_count'], 2)
            stacks = json.loads((folder / 'analysis/peak-stacks.json').read_text())
            self.assertEqual(stacks[0]['bytes'], 2048)
            self.assertIn('unmerged allocation report',
                          (folder / 'analysis/allocations.txt').read_text())

    def test_printer_failure_retains_report_and_is_not_success(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            printer = self.tool(folder, 'import sys\nprint("corrupt capture")\nsys.exit(7)\n')
            with self.assertRaisesRegex(ValueError, 'heaptrack_print failed'):
                analyze_capture(folder / 'input.gz', printer, folder / 'analysis', 5, 512)
            saved = json.loads((folder / 'analysis/process.json').read_text())
            self.assertEqual(saved['app_exit'], 7)
            self.assertIn('corrupt capture', (folder / 'analysis/allocations.txt').read_text())
            self.assertFalse((folder / 'analysis/peak-stacks.json').exists())

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux process groups required')
    def test_printer_timeout_kills_descendants(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            printer = self.tool(folder, '''import pathlib, subprocess, sys, time
child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])
pathlib.Path('descendant.pid').write_text(str(child.pid))
time.sleep(60)
''')
            with self.assertRaisesRegex(ValueError, 'heaptrack_print failed'):
                analyze_capture(folder / 'input.gz', printer, folder / 'analysis', 0.5, 512)
            saved = json.loads((folder / 'analysis/process.json').read_text())
            self.assertEqual(saved['failure_kind'], 'timeout')
            pid = int((folder / 'analysis/descendant.pid').read_text())
            # Orphan reaping is asynchronous; a zombie is already terminated.
            status = Path(f'/proc/{pid}/stat')
            for _ in range(20):
                if not status.exists() or status.read_text().rsplit(')', 1)[1].split()[0] == 'Z':
                    break
                time.sleep(0.05)
            else:
                self.fail('printer descendant survived timeout')

    @unittest.skipUnless(sys.platform.startswith('linux'), 'Linux process groups required')
    def test_sigterm_retains_interrupted_result_and_kills_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'data').mkdir()
            shutil.copy2(ROOT / 'Hesiod/data/node_documentation.json', folder / 'data')
            binary = folder / 'hesiod'
            binary.write_text('#!/bin/sh\nexit 0\n')
            binary.chmod(0o755)
            heaptrack = self.tool(folder, """import pathlib, subprocess, sys, time
child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])
pathlib.Path('descendant.pid').write_text(str(child.pid))
time.sleep(60)
""")
            output = folder / 'capture'
            process = subprocess.Popen(
                [sys.executable, str(ROOT / 'scripts/profiling/capture_allocations.py'),
                 '--binary', str(binary), '--output', str(output), '--case', 'ExportAsset',
                 '--heaptrack', str(heaptrack), '--printer', str(heaptrack)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 15
                while not (output / 'descendant.pid').exists() and time.monotonic() < deadline:
                    if process.poll() is not None:
                        self.fail(str(process.communicate()))
                    time.sleep(0.05)
                self.assertTrue((output / 'descendant.pid').exists())
                process.send_signal(signal.SIGTERM)
                stdout, stderr = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 1, (stdout, stderr))
                result = json.loads((output / 'result.json').read_text())
                self.assertEqual(result['status'], 'interrupted')
                pid = int((output / 'descendant.pid').read_text())
                status = Path(f'/proc/{pid}/stat')
                if status.exists():
                    self.assertEqual(status.read_text().rsplit(')', 1)[1].split()[0], 'Z')
            finally:
                if process.poll() is None:
                    process.kill()
                process.communicate()

    def test_malformed_or_empty_peak_export_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'stacks'
            for invalid in ('', 'main;allocate -1\n', 'main;allocate nan\n', 'main 0\n', '123\n'):
                with self.subTest(invalid=invalid):
                    path.write_text(invalid)
                    with self.assertRaises(ValueError):
                        read_peak_stacks(path)


if __name__ == '__main__':
    unittest.main()
