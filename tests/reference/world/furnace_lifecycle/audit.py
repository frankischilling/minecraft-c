"""Portable validation of the original saved-World Furnace lifecycle packet."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
MODES = ('interpreter', 'default', 'trace-interpreter', 'trace-default')
EXECUTION = ('FurnaceLifecycleObservation.java', 'FurnaceLifecycleJdi.java', 'cases.json', 'fixture.py', 'capture.py')
PUBLICATION = ('audit.py', 'publish.py', 'projection.py', 'test_packet.py', 'README.md')
OUTPUTS = ('finite.json', 'traces.json', 'random.json', 'setup.json', 'source-pins.json', 'native-observations.h')


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = dict(pairs)
    require(len(result) == len(pairs), 'unique JSON fields')
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf8'), object_pairs_hook=unique,
                      parse_constant=lambda value: require(False, 'finite JSON values'))


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def ints(values, count):
    return type(values) is list and len(values) == count and all(type(v) is int and -2147483648 <= v <= 2147483647 for v in values)


def identities(values, known):
    require(type(values) is list and len(values) <= 8 and all(v in known for v in values), 'bounded actual Tile aliases')


def state(value, compact=False):
    common = {'block', 'metadata', 'iterating', 'suppression', 'active', 'pending', 'tiles'}
    require(type(value) is dict and set(value) == common | ({'map'} if compact else
        {'maps', 'stacks', 'array_sha256', 'light_queues', 'entities'}), 'closed actual World state')
    require(type(value['block']) is int and 0 <= value['block'] <= 255 and
            type(value['metadata']) is int and 0 <= value['metadata'] <= 15 and
            type(value['iterating']) is bool and type(value['suppression']) is bool, 'actual typed Block/World state')
    require(type(value['tiles']) is dict and len(value['tiles']) <= 8, 'bounded observed Tile objects')
    for name, tile in value['tiles'].items():
        require(re.fullmatch('T[0-7]', name) is not None and set(tile) ==
                ({'fields', 'invalid', 'world_same'} if compact else {'fields', 'invalid', 'world_same', 'xyz', 'slots'}),
                'closed observed SK identity')
        require(ints(tile['fields'], 3) and type(tile['invalid']) is bool and type(tile['world_same']) is bool,
                'actual signed SK fields and validity')
        if not compact:
            require(ints(tile['xyz'], 3) and type(tile['slots']) is list and len(tile['slots']) == 3 and
                    all(v is None or v in value['stacks'] for v in tile['slots']), 'actual three-cell SK aliases')
    identities(value['active'], value['tiles'])
    identities(value['pending'], value['tiles'])
    if compact:
        identities(value['map'], value['tiles'])
        return
    require(type(value['maps']) is list and len(value['maps']) == 4 and
            [v['chunk'] for v in value['maps']] == [[0, 0], [0, 1], [1, 0], [1, 1]], 'actual four canonical Chunk Maps')
    for chunk in value['maps']:
        require(set(chunk) == {'chunk', 'values'}, 'closed actual Chunk Map projection')
        identities(chunk['values'], value['tiles'])
    require(type(value['stacks']) is dict and len(value['stacks']) <= 8, 'bounded actual Stack owners')
    for name, fields in value['stacks'].items():
        require(re.fullmatch('S[0-7]', name) is not None and ints(fields, 4), 'actual signed Stack fields')
    require(type(value['array_sha256']) is list and len(value['array_sha256']) == 20 and all(
        type(v) is str and re.fullmatch('[0-9a-f]{64}', v) for v in value['array_sha256']), 'all actual Chunk array hashes')
    require(type(value['light_queues']) is list and len(value['light_queues']) <= 1024 and all(ints(v, 7) for v in value['light_queues']),
            'actual bounded light queue records')
    require(type(value['entities']) is int and 0 <= value['entities'] <= 8, 'actual Entity List extent')


def validate(declaration, rows):
    steps = declaration['steps']
    require(type(rows) is list and len(rows) == len(steps), 'all predeclared World steps')
    previous = None
    for index, (step, row) in enumerate(zip(steps, rows)):
        require(set(row) == {'index', 'name', 'operation', 'returned', 'exception', 'exception_message', 'before', 'after', 'rng_unchanged'} and
                type(row['index']) is int and row['index'] == index and row['name'] == step['name'] and row['operation'] == step['operation'] and
                type(row['returned']) is bool and type(row['rng_unchanged']) is bool, 'literal typed original step')
        require((row['returned'] and row['exception'] is None and row['exception_message'] is None) or
                (not row['returned'] and type(row['exception']) is str and
                 (row['exception_message'] is None or type(row['exception_message']) is str)), 'literal original return/failure')
        state(row['before'])
        state(row['after'])
        if previous is not None:
            expected = copy.deepcopy(previous)
            author = step['author']
            for key, position in (('burn', 0), ('cook', 2)):
                if key in author:
                    expected['tiles']['T0']['fields'][position] = author[key]
            if author.get('fuel_null', False):
                expected['tiles']['T0']['slots'][1] = None
            require(row['before'] == expected, 'state continuity changes only explicit between-step inputs')
        previous = row['after']


def validate_random(declaration, streams):
    require(set(streams) == set(MODES), 'all four raw World Random streams')
    for mode, rows in streams.items():
        require(len(rows) == len(declaration['steps']), 'all actual Random boundaries')
        previous = None
        for index, (step, row) in enumerate(zip(declaration['steps'], rows)):
            require(set(row) == {'index', 'name', 'before', 'after'} and row['index'] == index and row['name'] == step['name'],
                    'literal Random boundary identity')
            for key in ('before', 'after'):
                value = row[key]
                require(type(value) is list and len(value) == 3 and re.fullmatch('[0-9a-f]{12}', value[0]) and
                        type(value[1]) is bool and re.fullmatch('[0-9a-f]{16}', value[2]), 'raw seed/Gaussian fields')
            require(previous is None or row['before'] == previous, 'unreset actual World Random continuity')
            previous = row['after']


def validate_traces(declaration, traces, codes, pins):
    methods = {owner + '.' + m['name'] + m['descriptor']: m for owner, value in pins['classes'].items() for m in value['methods']}
    require([row['index'] for row in traces] == list(range(len(declaration['steps']))), 'all actual phase boundaries')
    seen = set()
    for row in traces:
        require(set(row) == {'index', 'phases'} and len(row['phases']) <= 1024, 'bounded original phases')
        for phase in row['phases']:
            require(set(phase) == {'event', 'method', 'receiver', 'arguments', 'source_parent', 'state'} and
                    phase['event'] in ('entry', 'exit') and phase['method'] in codes and type(phase['arguments']) is list,
                    'closed original entry/exit phase')
            seen.add(phase['method'])
            state(phase['state'], True)
            require(len(phase['source_parent']) <= 1, 'nearest actual selected caller')
            for parent in phase['source_parent']:
                require(set(parent) == {'method', 'pc'} and parent['method'] in methods and type(parent['pc']) is int and
                        0 <= parent['pc'] < methods[parent['method']]['code_bytes'], 'actual original caller PC')
    require(seen == set(codes), 'all live pins were observed')
    for name, value in codes.items():
        require(name in methods and value == {k: methods[name][k] for k in ('code_bytes', 'code_sha256')}, 'actual live Code matches archive')


def audit(directory=HERE):
    metadata = load(directory / 'provenance.json')
    require(metadata['client_jar_sha256'] == JAR, 'original archive')
    for group, names in (('execution_helpers', EXECUTION), ('publication_helpers', PUBLICATION), ('outputs', OUTPUTS)):
        require(set(metadata[group]) == set(names), 'complete public inventory')
        for name in names:
            require(digest(directory / name) == metadata[group][name], 'public packet pin: ' + name)
    declaration = load(directory / 'cases.json')
    rows = load(directory / 'finite.json')['rows']
    validate(declaration, rows)
    validate_random(declaration, load(directory / 'random.json')['streams'])
    traces = load(directory / 'traces.json')
    validate_traces(declaration, traces['rows'], traces['live_Code'], load(directory / 'source-pins.json'))
    setup = load(directory / 'setup.json')['world']
    projection = module('furnace_lifecycle_public_projection', directory / 'projection.py')
    require(projection.tokens((directory / 'native-observations.h').read_text()) ==
            projection.tokens(projection.header(declaration, rows, setup)), 'native post-step projection equals original observations')
    return {'steps': len(rows), 'source_phases': sum(len(r['phases']) for r in traces['rows']),
            'live_Code_pins': len(traces['live_Code']), 'scope': 'Original World.g/TC lifecycle; native post-step replay and full gameplay remain separate.'}


if __name__ == '__main__':
    print(audit())
