"""Run the approved fixed-binary96-case matrix after native correctness checks."""
import datetime,json,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output/fixes'

def run(name,cases,variants):
    mem={line.split(':')[0]:int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines()}
    resource={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'phase':name,'available_kib':mem['MemAvailable'],'swap_free_kib':mem['SwapFree'],'disk_free_bytes':shutil.disk_usage(ROOT).free}
    with (OUT/'memory-resources.jsonl').open('a') as log:log.write(json.dumps(resource)+'\n')
    assert resource['available_kib']>30*1024**2
    assert resource['disk_free_bytes']>40*1024**3
    command=[sys.executable,'scripts/profiling/memory_study.py','run','--binary','build/assessment/bin/hesiod','--output','output/fixes/'+name,'--cases',*cases,'--variants',*variants,'--sizes','1024','2048','--tiles','1','4','--overlaps','0','--policies','ram-sequential','disk-sequential','--repeats','3','--check','--max-disk-gib','32']
    print(json.dumps({'launch':command,'resources':resource}),flush=True)
    with (OUT/(name+'.log')).open('w') as log:
        subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    subprocess.run([sys.executable,'output/validation/audit_memory_policy.py','output/fixes/'+name],cwd=ROOT,check=True)
    print('COMPLETE',name,flush=True)

subprocess.run(['git','diff','--exit-code','HEAD','--','Hesiod','scripts','external'],cwd=ROOT,check=True)
run('memory-fixed-quilting',['TextureQuiltingShuffle','TextureQuiltingExpand'],['full'])
run('memory-fixed-export',['ExportAsset'],['minimal','full'])
subprocess.run([sys.executable,'output/fixes/compare_memory_fixes.py'],cwd=ROOT,check=True)
