from pathlib import Path
import shlex,subprocess,sys,json
root=Path(__file__).resolve().parents[3]
build=root/'build/assessment'
out=root/'output/fixes/single-array'
name=sys.argv[1]
commands=(out/'baseline-build-commands.txt').read_text().splitlines()
args=shlex.split(commands[0]); clean=[]; i=0
while i<len(args):
 a=args[i]
 if a in ['-o','-MT','-MF']:
  i+=2;continue
 if a in ['-MD']:
  i+=1;continue
 if a.endswith('/test_virtual_array.cpp'):a=str(Path(sys.argv[2]).resolve()) if len(sys.argv)>2 else str(root/'external/HighMap/tests/src/test_single_array_compute.cpp')
 clean.append(a);i+=1
obj=out/(name+'.o');binary=out/name
clean+=['-o',str(obj)]
if len(sys.argv)>3: clean[1:1]=['-I'+str(Path(sys.argv[3]).resolve())]
subprocess.run(clean,cwd=build,check=True)
link=shlex.split(commands[1].removeprefix(': && ').removesuffix(' && :'))
result=[];i=0
while i<len(link):
 a=link[i]
 if a=='-o': i+=2;continue
 if a.endswith('.o') or a.startswith('-Wl,--dependency-file='):i+=1;continue
 if a=='external/HighMap/HighMap/libhighmap.a':a=str(root/'output/fixes/baseline/libhighmap.a')
 result.append(a);i+=1
result[1:1]=[str(obj)]
result+=['-o',str(binary)]
(out/(name+'-commands.json')).write_text(json.dumps([clean,result],indent=2)+'\n')
subprocess.run(result,cwd=build,check=True)
