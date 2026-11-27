"""Compare fixed app runs with the exact matching, reused frozen baseline cells."""
import collections
import json
from pathlib import Path
from statistics import median
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/profiling'))
import memory_study as study


def load(folder):
    folder=ROOT/folder
    config=json.loads((folder/'config.json').read_text())
    provenance=json.loads((folder/'provenance.json').read_text())
    report=json.loads((folder/'report.json').read_text())
    results=json.loads((folder/'results.json').read_text())
    audit=json.loads((ROOT/'output/validation'/(folder.name+'-policy-audit.json')).read_text())
    assert report['complete'] and audit['passed']
    assert all(r['status']=='ok' for r in results)
    return dict(path=str(folder),config=config,provenance=provenance,results=results)


def main():
    baseline=[load('output/memory-ram'),load('output/memory-disk')]
    fixed=[load('output/fixes/memory-fixed-export'),load('output/fixes/memory-fixed-quilting')]
    old={study.run_name(row):(s,row) for s in baseline for row in s['results']}
    comparisons=[]
    grouped=collections.defaultdict(lambda: {'baseline':[],'fixed':[]})
    used_baseline=set()
    provenance_checks=[]
    for s in fixed:
        for row in s['results']:
            name=study.run_name(row)
            assert name in old and name not in used_baseline
            base,previous=old[name]
            used_baseline.add(name)
            assert previous['graph_sha256']==row['graph_sha256'],f'fixture changed: {name}'
            assert base['config']['threads']==s['config']['threads']==1
            policy=row['policy']
            assert base['provenance']['runtime_policy'][policy]==s['provenance']['runtime_policy'][policy]
            assert base['provenance']['environment_overrides']==s['provenance']['environment_overrides']
            for key in ['platform','backend','graph_scheduling','measurement','environment','fetchcontent_dependencies']:
                assert base['provenance'][key]==s['provenance'][key],key
            for key in ['scripts/profiling/memory_study.py','scripts/profiling/memory_child.py','scripts/profiling/overnight_fixtures.py']:
                assert base['provenance']['script_sha256'][key]==s['provenance']['script_sha256'][key],key
            assert base['provenance']['binary_sha256']!=s['provenance']['binary_sha256']
            key=(*study.job_key(row),row['size'])
            grouped[key]['baseline'].append(previous)
            grouped[key]['fixed'].append(row)
            artifacts=lambda r:{a['path']:(a['bytes'],a['sha256']) for a in r['artifacts']}
            if row['case']=='ExportAsset':
                assert artifacts(previous)==artifacts(row), f'ExportAsset artifact mismatch: {name}'
            comparisons.append({'run':name,'baseline_result':base['path']+'/runs/'+name+'/result.json','fixed_result':s['path']+'/runs/'+name+'/result.json','identical_fixture_bytes':True,'export_artifacts_checked':row['case']=='ExportAsset','identical_export_artifacts':artifacts(previous)==artifacts(row) if row['case']=='ExportAsset' else None})
        baseline_data=baseline[0]['provenance']['data_sha256']
        fixed_data=s['provenance']['data_sha256']
        data_changes={key:{'baseline':baseline_data.get(key),'fixed':fixed_data.get(key)} for key in sorted(set(baseline_data)|set(fixed_data)) if baseline_data.get(key)!=fixed_data.get(key)}
        allowed_data_changes={'data/node_documentation.json','data/bootstraps/stepping_shore.hsd','data/git_version.txt'}
        assert set(data_changes)<=allowed_data_changes, f'unreviewed data changes: {set(data_changes)-allowed_data_changes}'
        version_stamp_review={'file':'data/git_version.txt','baseline_text':(Path(baseline[0]['path'])/'bin/data/git_version.txt').read_text(),'fixed_text':(Path(s['path'])/'bin/data/git_version.txt').read_text(),'reason':'Generated version identity used by About/splash UI; expected to differ between the original and fixed executable builds, not a numerical source asset.'}
        provenance_checks.append({'reviewed_version_stamp':version_stamp_review,'data_changes':data_changes,'fixed_path':s['path'],'binary_sha256':s['provenance']['binary_sha256'],'git_head':s['provenance']['git_head'],'submodules':s['provenance']['submodules']})
    cells=[]
    for key,versions in sorted(grouped.items()):
        a,b=versions['baseline'],versions['fixed']
        assert len(a)==len(b)==3
        case,variant,inputs_only,policy,tiles,overlap,size=key
        medians={k:median(r['process_peak_rss_kib']/1024 for r in rows) for k,rows in versions.items()}
        cell=dict(case=case,variant=variant,inputs_only=inputs_only,policy=policy,tiles=tiles,overlap=overlap,size=size,repeats_per_version=3,baseline_peak_mib=medians['baseline'],fixed_peak_mib=medians['fixed'],fixed_to_baseline_peak_ratio=medians['fixed']/medians['baseline'],peak_reduction_percent=100*(1-medians['fixed']/medians['baseline']))
        for version,rows in versions.items():
            peaks=[r['process_peak_rss_kib']/1024 for r in rows]
            cell[version+'_peak_mib_range']=[min(peaks),max(peaks)]
            cell[version+'_wall_seconds_median']=median(r['wall_seconds'] for r in rows)
        cells.append(cell)
    assert len(comparisons)==96 and len(cells)==32
    result={'reused_baseline_runs':96,'new_fixed_runs':96,'cells':cells,'run_pairs':comparisons,'fixed_provenance':provenance_checks,'baseline_provenance':[{'path':s['path'],'binary_sha256':s['provenance']['binary_sha256'],'git_head':s['provenance']['git_head']} for s in baseline],'verification':'Exact same fixture bytes, run identities, threads, runtime policy, full recorded environment, environment overrides, fetched dependency revisions/statuses, measurement method, relevant runner source hashes and successful per-series policy audits. Binary changes are intentional and recorded. Existing baseline observations are reused, never counted as additional executions.','limitations':'Whole-process kernel peak RSS includes sources and startup, not allocation ownership or total host RAM. Disk page cache is excluded. Shared-host timings are descriptive. Quilting node completion is not numerical-output validation; separate native differential tests and application output comparisons provide that evidence.'}
    output=ROOT/'output/fixes/memory-fix-comparison.json'
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(output)
    print('cells',len(cells),'new fixed runs',len(comparisons),'identical exports',sum(r['identical_export_artifacts'] is True for r in comparisons),'export runs',sum(r['export_artifacts_checked'] for r in comparisons))

if __name__=='__main__': main()
