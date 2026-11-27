"""Regenerate disjoint global-peak ownership groups from captured stack exports."""
import collections
import csv
import json
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else 'output/allocation-study')

def category(stack):
    if 'hesiod::compute_export_asset_node' in stack:
        if 'Assimp::JoinVerticesProcess' in stack:
            return 'export_assimp_join_vertices'
        if 'Assimp::SceneCombiner::Copy' in stack:
            return 'export_assimp_scene_copy'
        if 'Assimp::Exporter::Export' in stack:
            return 'export_assimp_other'
        if 'hmap::helper_build_mesh' in stack:
            return 'export_original_mesh'
        if 'hmap::normal_map(' in stack:
            return 'export_normal_map_generation'
        if 'hmap::VirtualTexture::from_arrays' in stack:
            return 'export_normal_map_virtual_channels'
        if 'hmap::VirtualArray::to_array' in stack:
            return 'export_elevation_or_mask_gather'
        if 'hmap::Array::Array(hmap::Array const&)' in stack:
            return 'export_direct_channel_copies'
        if 'hmap::Array::Array(glm::vec<>, float)' in stack:
            return 'export_alpha_channel'
        return 'export_other'
    if 'hesiod::compute_texture_quilting_' in stack:
        if 'hmap::quilting(' in stack:
            if 'hmap::Array::operator=(hmap::Array const&)' in stack:
                return 'quilting_secondary_output_copy_assignment'
            return 'quilting_core_outputs_strips_patches'
        if 'hmap::VirtualArray::to_array' in stack:
            return 'quilting_gathered_full_arrays'
        if 'hmap::VirtualArray::from_array' in stack:
            return 'quilting_scattered_outputs'
        if 'hmap::quilting_expand(' in stack or 'hmap::quilting_shuffle(' in stack:
            return 'quilting_wrapper_resample_or_crop'
        return 'quilting_other'
    if 'hmap::gpu::init_opencl' in stack:
        return 'opencl_initialization'
    return 'other_process'

rows=[]
for folder in sorted((root/'runs').iterdir()):
    result=json.loads((folder/'result.json').read_text())
    if result['status'] != 'ok':
        raise ValueError(f'failed capture: {folder}')
    groups=collections.Counter()
    stacks=[]
    for line in (folder/'analysis/peak-stacks.txt').read_text().splitlines():
        stack, separator, count = line.rpartition(' ')
        if not separator or not count.isdigit():
            raise ValueError(f'invalid peak stack: {folder}')
        stacks.append({'stack':stack, 'bytes':int(count)})
    for item in stacks:
        groups[category(item['stack'])]+=item['bytes']
    assert sum(groups.values()) == result['intercepted_heap_at_global_peak_bytes']
    row={k:result[k] for k in ('case','variant','size','tiles','overlap','policy','binary_sha256','git_head')}
    row.update(capture=folder.name,heap_peak_bytes=sum(groups.values()),
               heap_peak_mib=sum(groups.values())/1024**2,
               exclusive_groups_bytes=dict(sorted(groups.items())),
               raw_capture_sha256=result['capture']['sha256'])
    rows.append(row)
summary={'accounting':'Each allocation stack is assigned to exactly one group. Values are live intercepted heap bytes at the process global heap peak, not independent per-stack maxima, RSS, VRAM, or savings predictions.',
         'classification':'Source-frame/function substring rules are retained in summarize.py. Includes all zero-cost stacks; group totals verified against capture peak.',
         'captures':rows}
(root/'allocation-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
columns=['capture','case','variant','size','tiles','heap_peak_bytes','heap_peak_mib']
group_names=sorted({g for row in rows for g in row['exclusive_groups_bytes']})
with (root/'allocation-summary.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=columns+group_names)
    writer.writeheader()
    for row in rows:
        writer.writerow({**{k:row[k] for k in columns},**{g:row['exclusive_groups_bytes'].get(g,0) for g in group_names}})
for row in rows:
    print(row['capture'],f"{row['heap_peak_mib']:.3f} MiB",row['exclusive_groups_bytes'])
