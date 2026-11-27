#!/usr/bin/env python3
"""Record isolated native outputs, then compare a baseline and candidate build.

Uses fixed-seed 128-square graphs with 1/4 tile grids. Export flip regressions
compare export flags against independently reflected asymmetric input samples.
No numerical tolerance or source-control subtraction is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import sys

from memory_study import make_fixture, prepare_run, measure_process, validate_run
from overnight_fixtures import attributes, build_fixture
from run_pilot import sha256, write_json
from hsd.builder import Graph, Project
from hsd.png import read_png

SIZE = 128


def restore_graph(document):
    old = document['graph_manager']['graph_nodes']['graph']
    graph = Graph()
    for node in old['nodes']:
        graph.add_node(node['id'], node['label'],
                       {key: value for key, value in node.items() if key not in ('id', 'label')})
    for link in old['links']:
        graph.link(link['node_id_from'], link['port_id_from'], link['node_id_to'], link['port_id_to'])
    return graph


def document(graph):
    project = Project()
    project.add_graph(graph)
    return project.to_hsd()


def export_detail_fixtures(catalog):
    # All five HighMap normal blending modes, each with and without a mask.
    # Vary flips as well as grid, using a genuine asymmetric detail normal map.
    for tiles in (1, 4):
        for method, axis in enumerate(('none', 'x', 'y', 'xy', 'xy')):
            for masked in (False, True):
                fixture, _ = make_fixture(catalog, 'ExportAsset', 'full')
                model = fixture['graph_manager']['graph_nodes']['graph']
                target = next(n for n in model['nodes'] if n['label'] == 'ExportAsset')
                model['links'] = [link for link in model['links']
                                  if not (link['node_id_to'] == target['id'] and
                                          (link['port_id_to'] == 'normal map details' or
                                           (link['port_id_to'] == 'mask' and not masked)))]
                target['containers'][target['current']].update({
                    key: {'value': value} for key, value in {
                        'blending_method': method, 'detail_scaling': .65,
                        'flip_x': 'x' in axis, 'flip_y': 'y' in axis}.items()})
                graph = restore_graph(fixture)
                graph.add_node('detail-height', 'GaussianPulse', attributes(catalog, 'GaussianPulse', {
                    'center': [.63, .27], 'radius': .18}))
                graph.add_node('detail-normal', 'HeightmapToNormalMap',
                               attributes(catalog, 'HeightmapToNormalMap'))
                graph.link('detail-height', 'output', 'detail-normal', 'input')
                graph.link('detail-normal', 'normal map', target['id'], 'normal map details')
                yield (f'ExportAsset-detail-m{method}-mask{int(masked)}-{axis}-t{tiles}',
                       document(graph), tiles, {'kind': 'unchanged'})


def fixtures(catalog):
    for tiles in (1, 4):
        for node, variants in [
                ('ExportAsset', ('minimal', 'full')),
                ('HeightmapToNormalMap', ('full',)),
                ('CloudRandomDistance', ('density', 'no-density')),
                ('QuiltingShuffle', ('full',)), ('QuiltingExpand', ('full',)),
                ('TextureQuiltingShuffle', ('full',)), ('TextureQuiltingExpand', ('full',))]:
            for variant in variants:
                if node == 'ExportAsset':
                    fixture, _ = make_fixture(catalog, node, variant)
                else:
                    fixture, target = build_fixture(catalog, node)
                    if variant == 'no-density':
                        model = fixture['graph_manager']['graph_nodes']['graph']
                        model['nodes'] = [n for n in model['nodes'] if n['id'] == target]
                        model['links'] = []
                    graph = restore_graph(fixture)
                    for port, spec in catalog[node]['ports'].items():
                        if spec['type'] != 'output':
                            continue
                        kind, input_port, extension = {
                            'VirtualArray': ('ExportHeightmap', 'input', 'png'),
                            'VirtualTexture': ('ExportTexture', 'texture', 'png'),
                            'Cloud': ('ExportCloud', 'input', 'csv'),
                        }[spec['data_type']]
                        name = 'export-' + port.replace(' ', '_')
                        graph.add_node(name, kind, attributes(catalog, kind, {
                            'auto_export': True, 'fname': f'exports/{name}.{extension}', '16 bit': True}))
                        graph.link(target, port, name, input_port)
                    fixture = document(graph)
                yield f'{node}-{variant}-t{tiles}', fixture, tiles, {'kind': 'unchanged'}

        for node in ('ExportNormalMap', 'ExportTiled'):
            for depth in (8, 16):
                for axis in ('none', 'x', 'y', 'xy'):
                    for method in (('flags',) if axis == 'none' else ('flags', 'source')):
                        values = []
                        for y in range(SIZE):
                            for x in range(SIZE):
                                xx = SIZE - 1 - x if method == 'source' and 'x' in axis else x
                                yy = SIZE - 1 - y if method == 'source' and 'y' in axis else y
                                u, v = xx / (SIZE - 1), yy / (SIZE - 1)
                                values.append(.6 * u * u + .3 * v + .1 * u * v)
                        graph = Graph()
                        graph.add_node('source', 'Brush', attributes(catalog, 'Brush', {
                            'hmap': {'shape': {'x': SIZE, 'y': SIZE}, 'vector': values}}))
                        graph.add_node('target', node, attributes(catalog, node, {
                            'auto_export': True, 'fname': 'exports/result.png',
                            'flip_x': method == 'flags' and 'x' in axis,
                            'flip_y': method == 'flags' and 'y' in axis,
                            '16bit': depth == 16, 'bit_depth': f'{depth} bit',
                            'tiling.x': 2, 'tiling.y': 2}))
                        graph.link('source', 'output', 'target', 'input')
                        prefix = f'{node}-{depth}-t{tiles}'
                        yield f'{prefix}-{axis}-{method}', document(graph), tiles, {
                            'kind': 'flip' if axis != 'none' else 'unchanged',
                            'prefix': prefix, 'axis': axis, 'method': method}


def fingerprint(path):
    result = {'bytes': path.stat().st_size, 'sha256': sha256(path)}
    if path.suffix == '.png':
        image = read_png(str(path))
        result.update(dimensions=[image['width'], image['height']], bitdepth=image['bitdepth'],
                      pixels_sha256=hashlib.sha256(json.dumps(image, sort_keys=True).encode()).hexdigest())
    elif path.suffix == '.glb':
        data = path.read_bytes()
        magic, version, length = struct.unpack_from('<4sII', data)
        if magic != b'glTF' or version != 2 or length != len(data):
            raise ValueError('invalid GLB header')
        chunks, offset = [], 12
        while offset < len(data):
            size, kind = struct.unpack_from('<II', data, offset)
            content = data[offset + 8:offset + 8 + size]
            if len(content) != size:
                raise ValueError('truncated GLB chunk')
            if kind == 0x4E4F534A:
                content = json.dumps(json.loads(content), sort_keys=True, separators=(',', ':')).encode()
            chunks.append([kind, hashlib.sha256(content).hexdigest()])
            offset += 8 + size
        result['normalized_chunks'] = chunks
    return result


def expected_artifacts(name):
    if name.startswith('ExportTiled-'):
        return 4
    if name.startswith('TextureQuilting'):
        return 6  # guide texture/heightmap plus all four secondary textures
    if name.startswith('ExportAsset-'):
        return 2 if name.startswith('ExportAsset-minimal-') else 3
    return 1


def record(args):
    source_binary = args.binary.resolve()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    (root / 'bin').mkdir()
    binary = root / 'bin/hesiod'
    shutil.copy2(source_binary, binary)
    shutil.copytree(source_binary.parent / 'data', root / 'bin/data')
    catalog = json.loads((binary.parent / 'data/node_documentation.json').read_text())
    results = {}
    write_json(root / 'provenance.json', {'binary': str(source_binary), 'binary_sha256': sha256(binary),
                                         'script_sha256': sha256(Path(__file__)),
                                         'size': SIZE, 'suite': args.suite})
    cases = export_detail_fixtures(catalog) if args.suite == 'export-detail' else fixtures(catalog)
    for name, fixture, tiles, metadata in cases:
        folder = root / name
        command, env = prepare_run(folder, binary, fixture, SIZE, tiles, 0, 'ram-sequential', 1)
        expected = [{'graph': 'graph', 'id': n['id'], 'type': n['label']}
                    for n in fixture['graph_manager']['graph_nodes']['graph']['nodes']]
        result = dict(metadata, command=command, fixture_sha256=sha256(folder / 'fixture.hsd'))
        try:
            result.update(measure_process(command, folder, env, 90, 2048))
            if result['app_exit'] != 0 or result['failure_kind']:
                raise ValueError('native process failed')
            result.update(validate_run(folder, expected, SIZE, tiles, 0,
                                       export=name.startswith('ExportAsset-')))
            artifacts = {str(p.relative_to(folder / 'exports')): fingerprint(p)
                         for p in sorted((folder / 'exports').iterdir()) if p.is_file()}
            if len(artifacts) != expected_artifacts(name):
                raise ValueError(f'expected {expected_artifacts(name)} exports; got {len(artifacts)}')
            if name.startswith(('ExportNormalMap-', 'ExportTiled-')):
                depth = int(name.split('-')[1])
                width = SIZE // 2 if name.startswith('ExportTiled-') else SIZE
                if any(a.get('bitdepth') != depth or a.get('dimensions') != [width, width]
                       for a in artifacts.values()):
                    raise ValueError('incorrect export depth or dimensions')
            result.update(status='ok', artifacts=artifacts)
        except (OSError, ValueError) as error:
            result.update(status='failed', error=str(error))
        results[name] = result
        write_json(folder / 'result.json', result)
        write_json(root / 'results.json', results)
        print(name, result['status'], result.get('error', ''), flush=True)
    return 0 if all(r['status'] == 'ok' for r in results.values()) else 1


def signature(record):
    return {name: value.get('normalized_chunks', value['sha256'])
            for name, value in record['artifacts'].items()}


def compare(args):
    baseline = json.loads((args.baseline / 'results.json').read_text())
    candidate = json.loads((args.candidate / 'results.json').read_text())
    checks = []
    if set(baseline) != set(candidate):
        raise ValueError('case identities differ')
    for name, result in candidate.items():
        old = baseline[name]
        passed = (result['status'] == old['status'] == 'ok'
                  and result['fixture_sha256'] == old['fixture_sha256']
                  and len(result['artifacts']) == len(old['artifacts']) == expected_artifacts(name))
        if passed and result['kind'] == 'flip' and result['method'] == 'flags':
            reference = baseline[f"{result['prefix']}-{result['axis']}-source"]
            passed = (reference['status'] == 'ok' and signature(result) == signature(reference)
                      and signature(old) != signature(reference))
            relation = 'fixed export flags equal independently pre-reflected baseline input; old flags differ'
        else:
            passed = passed and signature(result) == signature(old)
            relation = 'exact PNG/CSV bytes or normalized GLB chunks match baseline'
        checks.append({'case': name, 'passed': passed, 'relation': relation})
    report = {'passed': all(c['passed'] for c in checks), 'cases': len(checks), 'checks': checks,
              'baseline': json.loads((args.baseline / 'provenance.json').read_text()),
              'candidate': json.loads((args.candidate / 'provenance.json').read_text()),
              'scope': '128-square deterministic CPU graphs, sequential tiles; no tolerance; no performance claim'}
    write_json(args.output, report)
    print(json.dumps({'passed': report['passed'], 'cases': report['cases'],
                      'failed': [c['case'] for c in checks if not c['passed']]}))
    return 0 if report['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    recorder = commands.add_parser('record')
    recorder.add_argument('--binary', type=Path, required=True)
    recorder.add_argument('--output', type=Path, required=True)
    recorder.add_argument('--suite', choices=('standard', 'export-detail'), default='standard')
    comparison = commands.add_parser('compare')
    comparison.add_argument('--baseline', type=Path, required=True)
    comparison.add_argument('--candidate', type=Path, required=True)
    comparison.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    return record(args) if args.command == 'record' else compare(args)


if __name__ == '__main__':
    sys.exit(main())
