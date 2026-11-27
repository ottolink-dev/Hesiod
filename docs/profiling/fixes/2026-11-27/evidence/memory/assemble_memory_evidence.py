"""Archive completed fixed-memory evidence, excluding large runtime artifacts."""
import csv,gzip,hashlib,json,shutil,tarfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'output/fixes'
DEST=OUT/'memory-evidence'

def pack(destination,members):
    with destination.open('wb') as raw:
        with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0,compresslevel=6) as zipped:
            with tarfile.open(fileobj=zipped,mode='w') as archive:
                for path,name in sorted(members,key=lambda x:x[1]):
                    info=archive.gettarinfo(str(path),arcname=name)
                    info.mtime=0;info.uid=info.gid=0;info.uname=info.gname=''
                    with path.open('rb') as stream:archive.addfile(info,stream)

DEST.mkdir(exist_ok=True)
for name in ['memory-fixed-quilting','memory-fixed-export','memory-baseline-calibration']:
    source=OUT/name;target=DEST/name;target.mkdir(exist_ok=True)
    status=json.loads((source/'status.json').read_text())
    assert status['state']=='finished' and status['completed_runs']==status['planned_runs']
    results=json.loads((source/'results.json').read_text());assert all(r['status']=='ok' for r in results)
    members=[]
    for filename in ['config.json','provenance.json','status.json','report.json','report.csv','results.json']:
        shutil.copy2(source/filename,target/filename);members.append((source/filename,filename))
    audit=ROOT/'output/validation'/(name+'-policy-audit.json')
    assert json.loads(audit.read_text())['passed']
    shutil.copy2(audit,target/'policy-audit.json');members.append((audit,'policy-audit.json'))
    for run in sorted((source/'runs').iterdir()):
        if not run.is_dir():continue
        for filename in ['fixture.hsd','app.log','batch.log','result.json','measurement.json','measure-request.json','supervisor.log','child.pid','config/hesiod/hesiod.json']:
            path=run/filename
            if path.is_file():members.append((path,str(path.relative_to(source))))
    for filename in ['source.diff','CMakeCache.txt']:
        if (source/filename).is_file():members.append((source/filename,filename))
    for path in (source/'scripts').rglob('*'):
        if path.is_file() and path.suffix in {'.py','.json'}:members.append((path,str(path.relative_to(source))))
    if name=='memory-baseline-calibration':
        comparison=json.loads((OUT/'memory-baseline-calibration-comparison.json').read_text())
        association={k:comparison[k] for k in ['measured_binary_sha256','measured_binary_source_git_head','measured_binary_source_submodules','original_binary_provenance_path','current_orchestrator_git_head','current_run_provenance_path']}
        association['git_head_field_semantics']='The runner provenance git_head describes the current orchestration worktree. The measured executable belongs to the original assessment source and binary hash identified here.'
        path=target/'binary-source-association.json';path.write_text(json.dumps(association,indent=2)+'\n');members.append((path,path.name))
    archive=target/'run-evidence.tar.gz';pack(archive,members)
    print(name,'runs',len(results),'archive bytes',archive.stat().st_size)

for filename in ['memory-fix-comparison.json','memory-baseline-calibration-comparison.json','compare_memory_fixes.py','calibrate_frozen_baseline.py','run_fixed_memory.py','assemble_memory_evidence.py','memory-resources.jsonl','memory-fixed-run.log','memory-comparison-validation.log','memory-calibration-validation.log','memory-fixed-quilting.log','memory-fixed-export.log','memory-baseline-calibration.log']:
    shutil.copy2(OUT/filename,DEST/filename)
comparison=json.loads((OUT/'memory-fix-comparison.json').read_text())
with (DEST/'memory-fix-comparison.csv').open('w',newline='') as stream:
    writer=csv.DictWriter(stream,fieldnames=list(comparison['cells'][0]));writer.writeheader();writer.writerows(comparison['cells'])
manifest={str(p.relative_to(DEST)):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(DEST.rglob('*')) if p.is_file() and p.name!='manifest.json'}
(DEST/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('evidence files',len(manifest),'total bytes',sum(r['bytes'] for r in manifest.values()))
