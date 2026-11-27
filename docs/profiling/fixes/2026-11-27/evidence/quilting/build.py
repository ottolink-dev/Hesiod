from pathlib import Path
import shlex,subprocess,sys
root=Path('/home/barrulus/dev/Hesiod-783');folder=root/'output/fixes/quilting';build=root/'build/assessment'
commands=Path('/tmp/783-profile-commands.txt').read_text().splitlines()
c=shlex.split(next(x for x in commands if ' -c ' in x and 'highmap_workload.cpp.o' in x))
c=c[:c.index('-MD')]+['-std=gnu++20']
objects=[]
for src in [folder/'compare.cpp']+([root/'external/HighMap/HighMap/src/synthesis/quilting.cpp',root/'external/HighMap/HighMap/src/operator/stitching_helpers.cpp'] if sys.argv[1]=='new' else []):
 cc=shlex.split(next(x for x in commands if ' -c ' in x and str(src) in x)) if src.name!='compare.cpp' else c
 cc=cc[:cc.index('-MD')] if '-MD' in cc else cc
 obj=folder/(src.stem+'-'+sys.argv[1]+'.o');subprocess.run(cc+['-c',str(src),'-o',str(obj)],cwd=build,check=True);objects.append(str(obj))
l=shlex.split(next(x for x in commands if ' -o bin/profile_highmap ' in x));l=l[2:-2]
l=[x for x in l if not x.startswith('-Wl,--dependency-file=')]
i=l.index('-o');l=l[:i-1]+objects+['-o',str(folder/sys.argv[1])]+l[i+2:]
l=[str(root/'output/fixes/baseline/libhighmap.a') if x=='external/HighMap/HighMap/libhighmap.a' else x for x in l]
subprocess.run(l,cwd=build,check=True)
