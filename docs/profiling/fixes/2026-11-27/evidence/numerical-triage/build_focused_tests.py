from pathlib import Path
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
folder = Path(__file__).resolve().parent
build = root / 'build/assessment'
commands = Path('/tmp/783-profile-commands.txt').read_text().splitlines()
record = []

def run(argv):
    record.append(argv)
    (folder / 'focused-commands.json').write_text(json.dumps(record, indent=2) + '\n')
    subprocess.run(argv, cwd=build, check=True)

base = shlex.split(next(c for c in commands if ' -c ' in c and 'highmap_workload.cpp.o' in c))
base = base[:base.index('-MD')]
objects = []
for name in ['tests/src/test_math_histogram.cpp', 'tests/src/test_remove_endpoints.cpp', 'HighMap/src/internal/assert.cpp', 'HighMap/src/filters/filters.cpp']:
    source = root / 'external/HighMap' / name
    command = base if name.startswith('tests/') else shlex.split(next(c for c in commands if ' -c ' in c and str(source) in c))
    command = command[:command.index('-MD')] if '-MD' in command else command
    target = folder / (source.stem + '-focused.o')
    # Production filters retain the project's original compile flags. Tests and
    # the assertion helper explicitly retain IEEE checks.
    extra = ['-std=gnu++20', '-I' + str(root / 'output/validation/googletest/googletest/include')]
    if source.name != 'filters.cpp':
        extra += ['-fno-fast-math', '-fno-finite-math-only']
    run(command + extra + ['-c', str(source), '-o', str(target)])
    objects.append(str(target))

link = shlex.split(next(c for c in commands if ' -o bin/profile_highmap ' in c))[2:-2]
link = [item for item in link if not item.startswith('-Wl,--dependency-file=')]
index = link.index('-o')
link = link[:index - 1] + objects + ['-o', str(folder / 'focused-tests')] + link[index + 2:]
link = [str(root / 'output/fixes/baseline/libhighmap.a') if item == 'external/HighMap/HighMap/libhighmap.a' else item for item in link]
index = link.index(str(root / 'output/fixes/baseline/libhighmap.a'))
link[index:index] = ['lib/libgtest_main.a', 'lib/libgtest.a']
run(link)
