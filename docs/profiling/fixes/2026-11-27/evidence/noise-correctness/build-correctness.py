from pathlib import Path
import shlex,subprocess,sys
root=Path('/home/barrulus/dev/Hesiod-783');folder=root/'output/fixes/quilting';build=root/'build/assessment';hm=root/'external/HighMap'
kind=sys.argv[1]
sets={
 'parity-tests':[hm/'tests/src/test_gpu_cpu.cpp',hm/'tests/src/main.cpp',hm/'HighMap/src/primitives/coherent_noise/noise.cpp',hm/'HighMap/src/internal/assert.cpp'],
 'clamp-tests':[hm/'tests/src/test_range.cpp',hm/'HighMap/src/range/range.cpp'],
 'clamp-fixed':[folder/'clamp-repro.cpp',hm/'HighMap/src/range/range.cpp'],
 'noise-fixed':[folder/'noise-repro.cpp',hm/'HighMap/src/primitives/coherent_noise/noise.cpp'],
}
commands=Path('/tmp/783-profile-commands.txt').read_text().splitlines();objects=[]
for src in sets[kind]:
 if '/HighMap/src/' in str(src):c=shlex.split(next(x for x in commands if ' -c ' in x and str(src) in x))
 else:c=shlex.split(next(x for x in commands if ' -c ' in x and 'highmap_workload.cpp.o' in x))
 c=c[:c.index('-MD')]+['-std=gnu++20','-I'+str(root/'output/validation/googletest/googletest/include')]
 obj=folder/(src.stem+'-'+kind+'.o');subprocess.run(c+['-c',str(src),'-o',str(obj)],cwd=build,check=True);objects.append(str(obj))
l=shlex.split(next(x for x in commands if ' -o bin/profile_highmap ' in x))[2:-2];l=[x for x in l if not x.startswith('-Wl,--dependency-file=')];i=l.index('-o');l=l[:i-1]+objects+['-o',str(folder/kind)]+l[i+2:]
l=[str(root/'output/fixes/baseline/libhighmap.a') if x=='external/HighMap/HighMap/libhighmap.a' else x for x in l]
if 'tests' in kind:
 i=l.index(str(root/'output/fixes/baseline/libhighmap.a'));l[i:i]=['lib/libgtest_main.a','lib/libgtest.a']
subprocess.run(l,cwd=build,check=True)
