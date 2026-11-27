#!/usr/bin/env python3
"""Exercise positive catalogue validation and two intentionally broken copies."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
binary = args.binary.resolve()
root = args.output.resolve()
root.mkdir(parents=True, exist_ok=False)
results = []
for case in ('valid', 'missing-node', 'missing-port'):
    folder = root / case
    (folder / 'data').mkdir(parents=True)
    for path in (binary.parent / 'data').iterdir():
        target = folder / 'data' / path.name
        if path.name != 'node_documentation.json':
            target.symlink_to(path, target_is_directory=path.is_dir())
    catalog = json.loads((binary.parent / 'data/node_documentation.json').read_text())
    if case == 'missing-node':
        catalog.pop('Abs')
    elif case == 'missing-port':
        catalog['Preview']['ports'].pop('forest')
    (folder / 'data/node_documentation.json').write_text(json.dumps(catalog))
    env = dict(os.environ, QT_QPA_PLATFORM='offscreen', OMP_NUM_THREADS='1',
               XDG_CONFIG_HOME=str(folder / 'config'), XDG_CACHE_HOME=str(folder / 'cache'))
    command = [str(binary), '--check-port-links']
    process = subprocess.run(command, cwd=folder, env=env, capture_output=True, text=True, timeout=60)
    log = process.stdout + process.stderr
    (folder / 'app.log').write_text(log)
    if case == 'valid':
        assert process.returncode == 0 and 'check-port-links: 0 failure(s)' in log
    elif case == 'missing-node':
        assert process.returncode == 1 and "missing port documentation for 'Abs'" in log
        assert 'check-port-links: 1 failure(s)' in log
    else:
        assert process.returncode == 1
        assert 'Preview [Forest, want IN]: catalog offered=false but live node selected=true' in log
    results.append({'case': case, 'command': command, 'returncode': process.returncode,
                    'diagnostic_behavior_passed': True})
(root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('All3catalogue diagnostic checks passed (valid, missing node, missing port)')
