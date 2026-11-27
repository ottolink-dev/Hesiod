"""Three fresh frozen-baseline checks, separate from the 96 fixed comparisons."""
import datetime,json,shutil,subprocess,sys
from pathlib import Path
from statistics import median
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/profiling'))
import memory_study as study
OUT=ROOT/'output/fixes';name='memory-baseline-calibration';folder=OUT/name
original=ROOT/'output/memory-ram'
source=json.loads((original/'provenance.json').read_text())
mem={line.split(':')[0]:int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines()}
resource={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'phase':name,'available_kib':mem['MemAvailable'],'disk_free_bytes':shutil.disk_usage(ROOT).free}
with (OUT/'memory-resources.jsonl').open('a') as log:log.write(json.dumps(resource)+'\n')
assert resource['available_kib']>30*1024**2 and resource['disk_free_bytes']>40*1024**3
command=[sys.executable,'scripts/profiling/memory_study.py','run','--binary','output/memory-ram/bin/hesiod','--output','output/fixes/'+name,'--cases','ExportAsset','TextureQuiltingShuffle','TextureQuiltingExpand','--variants','full','--sizes','1024','--tiles','1','--overlaps','0','--policies','ram-sequential','--repeats','1','--max-disk-gib','32']
with (OUT/(name+'.log')).open('w') as log:subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
subprocess.run([sys.executable,'output/validation/audit_memory_policy.py','output/fixes/'+name],cwd=ROOT,check=True)
rows=json.loads((folder/'results.json').read_text());fresh=json.loads((folder/'provenance.json').read_text());old=json.loads((original/'results.json').read_text())
assert fresh['binary_sha256']==source['binary_sha256']
assert fresh['data_sha256']==source['data_sha256']
assert fresh['runtime_policy']==source['runtime_policy']
assert fresh['environment_overrides']==source['environment_overrides']
for key in ['scripts/profiling/memory_study.py','scripts/profiling/memory_child.py','scripts/profiling/overnight_fixtures.py']:
 assert fresh['script_sha256'][key]==source['script_sha256'][key]
checks=[]
for row in rows:
 previous=[r for r in old if study.job_key(r)==study.job_key(row) and r['size']==row['size']]
 assert len(previous)==3 and all(r['status']=='ok' for r in [*previous,row])
 assert all(r['graph_sha256']==row['graph_sha256'] for r in previous)
 artifacts=lambda r:{a['path']:(a['bytes'],a['sha256']) for a in r['artifacts']}
 if row['case']=='ExportAsset':assert all(artifacts(r)==artifacts(row) for r in previous)
 peaks=[r['process_peak_rss_kib']/1024 for r in previous];peak=row['process_peak_rss_kib']/1024
 checks.append({'case':row['case'],'baseline_repeat_count':3,'baseline_peak_mib_range':[min(peaks),max(peaks)],'baseline_peak_mib_median':median(peaks),'fresh_peak_mib':peak,'fresh_peak_within_existing_range':min(peaks)<=peak<=max(peaks),'fresh_to_existing_median_ratio':peak/median(peaks),'identical_fixture_bytes':True,'identical_export_artifacts':True if row['case']=='ExportAsset' else None,'baseline_result_paths':[str(original/'runs'/study.run_name(r)/'result.json') for r in previous],'fresh_result_path':str(folder/'runs'/study.run_name(row)/'result.json')})
assert len(checks)==3
report={'kind':'fresh frozen-baseline reproducibility calibration','command':command,'new_calibration_runs':3,'measured_binary_sha256':source['binary_sha256'],'measured_binary_source_git_head':source['git_head'],'measured_binary_source_submodules':source['submodules'],'original_binary_provenance_path':str(original/'provenance.json'),'current_orchestrator_git_head':fresh['git_head'],'current_run_provenance_path':str(folder/'provenance.json'),'checks':checks,'limitations':'One new run per case, not added baseline repeats or new uncertainty estimates. The current worktree supplied unchanged orchestration; the measured binary and assets are from the original 1a0a0de7 assessment snapshot. Whole-process RSS and shared-host observational timings retain the main study limitations. No memory-growth check can be inferred from this single resolution.'}
(OUT/'memory-baseline-calibration-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(checks,indent=2))
