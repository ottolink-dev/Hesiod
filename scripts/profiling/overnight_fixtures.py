"""Deterministic, typed workload fixtures for the non-deprecated node inventory.

This exercises one explicit input/default-group combination, not every branch.
Never load arbitrary example paths or write exports outside the run directory.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from hsd.builder import Graph, Project
from inventory import inventory

POINTS = [[0.1, 0.2, 0.1], [0.3, 0.7, 0.4], [0.6, 0.25, 0.8], [0.85, 0.8, 0.5]]
POINT_OBJECTS = [dict(zip(('x', 'y', 'z'), point)) for point in POINTS]
OVERRIDES = {
    'Abs': {'vshift': 0.5}, 'Clamp': {'clamp': [0.3, 0.7]},
    'Remap': {'remap': [0.2, 0.8]}, 'Rescale': {'scaling': 0.8},
    'ShiftElevation': {'shift': 0.2}, 'TextureTransform': {'flip_x': True},
    'DistanceTransform': {'threshold': 0.5, 'transform_type': 1},
    'Cloud': {'cloud': POINT_OBJECTS}, 'Path': {'path': POINT_OBJECTS, 'closed': False},
    'Brush': {'hmap': {'shape': {'x': 8, 'y': 8},
                       'vector': [(x + y) / 14 for y in range(8) for x in range(8)]}},
}
# No external receiver is started or contacted by the suite.
BLOCKED = {'BlenderBridge': 'Requires an isolated Blender receiver/streaming fixture; not represented by a disconnected no-op.'}
OPTIONAL = {'dx', 'dy', 'control', 'envelope', 'background'}


def group_for(catalog, node):
    groups = catalog[node].get('parameter_groups') or []
    return groups[0]['name'] if groups else 'main'


def attributes(catalog, node, values=None):
    values = dict(values or {})
    params = catalog[node].get('parameters') or {}
    if 'seed' in params:
        values.setdefault('seed', 42)
    group = group_for(catalog, node)
    return {'current': group, 'containers': {group: {k: {'value': v} for k, v in values.items()}}}


def build_fixture(catalog, node):
    graph = Graph(config={'shape': (512, 512), 'tiling': (1, 1), 'overlap': 0.0})
    sources = {}

    def add(name, kind, values=None):
        graph.add_node(name, kind, attributes(catalog, kind, values))
        return name

    def source(dtype, variant=0, port=''):
        key = (dtype, variant)
        if key in sources:
            return sources[key]
        name = f'source{len(sources)}'
        sources[key] = (name, '')  # reserve a unique name before recursive dependencies
        if dtype == 'VirtualArray':
            add(name, 'GaussianPulse', {'radius': 0.22, 'center': [0.4 + 0.15 * variant, 0.5]})
            output = 'output'
        elif dtype == 'VirtualTexture':
            src, pin = source('VirtualArray', variant)
            add(name, 'ColorizeGradient')
            graph.link(src, pin, name, 'level')
            output = 'texture'
        elif dtype == 'Cloud':
            add(name, 'CloudLattice', {'npoints': 16, 'seed': 42 + variant})
            output = 'cloud'
        elif dtype == 'Path':
            add(name, 'Path', {'path': POINT_OBJECTS, 'closed': False})
            output = 'path'
        elif dtype == 'vector<float>':
            src, pin = source('Cloud', variant)
            add(name, 'CloudToVectors')
            graph.link(src, pin, name, 'cloud')
            output = 'v'
        elif dtype == 'Array':
            src, pin = source('VirtualArray', variant)
            add(name, 'HeightmapToKernel', {'radius': 0.02})
            graph.link(src, pin, name, 'heightmap')
            output = 'kernel'
        else:
            raise ValueError(f'unsupported input type: {dtype}')
        sources[key] = (name, output)
        return name, output

    feeds = []
    for port, spec in (catalog[node].get('ports') or {}).items():
        if spec['type'] != 'input' or port.lower() in OPTIONAL:
            continue
        # Keep coordinate/data vectors aligned to one cloud.
        variant = 0 if spec['data_type'] in ('Cloud', 'Path', 'vector<float>') else len(feeds) % 2
        src, output = source(spec['data_type'], variant, port)
        if spec['data_type'] == 'vector<float>' and port in ('x', 'y'):
            output = port
        feeds.append((src, output, port))
    values = dict(OVERRIDES.get(node, {}))
    params = catalog[node].get('parameters') or {}
    if node.startswith('Export'):
        values.update(auto_export=True, fname='exports/result')
        if 'pattern' in params:
            values['pattern'] = '{FILENAME}.{EXT}'
    if node == 'ImportHeightmap':
        values['fname'] = 'inputs/heightmap.pgm'
    if node == 'ImportTexture':
        values['fname'] = 'inputs/texture.ppm'
    if node in ('CloudFromCsv', 'PathFromCsv'):
        values['fname'] = 'inputs/points.csv'
    # Reject unknown file-reading nodes instead of invoking persisted/default paths.
    known_io = node.startswith('Export') or node in ('ImportHeightmap', 'ImportTexture', 'CloudFromCsv', 'PathFromCsv')
    if any(p.get('type') == 'Filename' for p in params.values()) and not known_io:
        raise ValueError('file parameter requires a reviewed fixture')
    add('target', node, values)
    for src, output, port in feeds:
        graph.link(src, output, 'target', port)
    project = Project()
    project.add_graph(graph)
    if node == 'Broadcast':
        graph.set_raw('target', 'broadcast_tag', f'graph/Broadcast/{graph.node_id("target")}')
    if node == 'Receive':
        # Replace the stand-alone receiver with a producer/receiver pair.
        producer = Graph(graph_id='producer')
        producer.add_node('input', 'GaussianPulse', attributes(catalog, 'GaussianPulse'))
        graph = Graph(graph_id='consumer')
        project = Project()
        project.add_graph(producer)
        project.add_graph(graph)
        project.add_broadcast('producer', 'input', 'output', 'consumer', 'target')
    return project.to_hsd(), graph.node_id('target')


def make_manifest(catalog):
    cases = []
    fixtures = {}
    for row in inventory():
        if row['queue'] == 'legacy':
            continue
        node = row['node']
        case = {'name': node, 'target': node, 'queue': row['queue'],
                'category': row['category'], 'source': row['source'],
                'group': None,
                'scope': 'backend_dispatch_only' if node == 'Preview' else 'one_synthetic_fixture',
                'status': 'ready'}
        try:
            if node not in catalog:
                raise ValueError('Node documentation missing; requires a reviewed fixture')
            case['group'] = group_for(catalog, node)
            if node in BLOCKED:
                raise ValueError(BLOCKED[node])
            graph, node_id = build_fixture(catalog, node)
            fixtures[node] = graph
            case['node_id'] = node_id
            case['expected_invocations'] = 2 if node == 'Receive' else 1
        except (ValueError, KeyError) as error:
            case.update(status='needs_fixture', reason=str(error))
        cases.append(case)
    return cases, fixtures


def write_inputs(folder):
    """Fixed-size inputs; image output resolution varies, geometry cardinality does not."""
    directory = folder / 'inputs'
    directory.mkdir()
    size = 64
    pixels = bytes((x + y) * 255 // (2 * size - 2) for y in range(size) for x in range(size))
    (directory / 'heightmap.pgm').write_bytes(f'P5\n{size} {size}\n255\n'.encode() + pixels)
    (directory / 'texture.ppm').write_bytes(f'P6\n{size} {size}\n255\n'.encode() +
                                         bytes(c for v in pixels for c in (v, 255 - v, 128)))
    (directory / 'points.csv').write_text(''.join(','.join(map(str, p)) + '\n' for p in POINTS))
    (folder / 'exports').mkdir()
