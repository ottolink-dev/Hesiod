from pathlib import Path
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
folder = Path(__file__).resolve().parent
build = root / 'build/assessment'
commands = Path('/tmp/783-profile-commands.txt').read_text().splitlines()
compile = shlex.split(next(c for c in commands if ' -c ' in c and 'highmap_workload.cpp.o' in c))
compile = compile[:compile.index('-MD')]
object = folder / 'repro-baseline.o'
compile += ['-std=gnu++20', '-fno-fast-math', '-fno-finite-math-only', '-c', str(folder / 'repro.cpp'), '-o', str(object)]
link = shlex.split(next(c for c in commands if ' -o bin/profile_highmap ' in c))[2:-2]
link = [item for item in link if not item.startswith('-Wl,--dependency-file=')]
index = link.index('-o')
link = link[:index - 1] + [str(object), '-o', str(folder / 'repro-baseline')] + link[index + 2:]
link = [str(root / 'output/fixes/baseline/libhighmap.a') if item == 'external/HighMap/HighMap/libhighmap.a' else item for item in link]
(folder / 'commands.json').write_text(json.dumps([compile, link], indent=2) + '\n')
subprocess.run(compile, cwd=build, check=True)
subprocess.run(link, cwd=build, check=True)
