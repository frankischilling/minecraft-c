"""Portable audit of original Furnace evidence; Java/assets are not required."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
EXECUTION = ('FurnaceObservation.java', 'FurnaceJdi.java', 'declare.py', 'cases.json', 'capture.py')
PUBLICATION = ('projection.py', 'audit.py', 'publish.py', 'test_packet.py', 'README.md', 'scan_callers.py')
OUTPUTS = ('finite.json', 'source-pins.json', 'traces.json', 'caller-inventory.json', 'native-observations.h')


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
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def int32(value):
    return type(value) is int and -2147483648 <= value <= 2147483647


def stack(value):
    require(type(value) is dict and set(value) == {'id', 'count', 'damage', 'animation'} and
            all(int32(v) for v in value.values()), 'closed signed raw Stack fields')


def state(value):
    require(type(value) is dict and set(value) == {'fields', 'xyz', 'world_null', 'slots',
            'player', 'player_changed', 'stacks'}, 'closed actual state snapshot')
    for key in ('fields', 'xyz'):
        require(value[key] is None or type(value[key]) is list and len(value[key]) == 3 and
                all(int32(v) for v in value[key]), 'actual signed tile fields/coordinates')
    require(value['world_null'] in (None, True) and
            (value['player_changed'] is None or type(value['player_changed']) is bool),
            'explicit unbound original owner state')
    require(type(value['stacks']) is dict and len(value['stacks']) <= 8, 'bounded observed Stack identities')
    for name, item in value['stacks'].items():
        require(re.fullmatch(r'[AB]|N[1-9][0-9]*', name) is not None, 'actual per-case identity label')
        stack(item)
    for key, count in (('slots', 3), ('player', 36)):
        require(value[key] is None or type(value[key]) is list and len(value[key]) == count and
                all(label is None or label in value['stacks'] for label in value[key]),
                'bounded slots refer to actual recorded Stack objects')


def tag(kind, value, depth=0):
    require(type(kind) is int and 0 <= kind <= 10 and depth <= 16, 'bounded exact original NBT tag')
    if kind in (1, 2, 3, 4):
        bits = {1: 8, 2: 16, 3: 32, 4: 64}[kind]
        require(type(value) is int and -(1 << (bits - 1)) <= value < 1 << (bits - 1), 'raw signed NBT width')
    elif kind == 8:
        require(type(value) is str and len(value) <= 64, 'bounded original NBT text')
    elif kind == 9:
        require(type(value) is list and len(value) == 2 and type(value[0]) is int and
                0 <= value[0] <= 10 and type(value[1]) is list and len(value[1]) <= 8,
                'bounded original typed NBT list')
        for child in value[1]:
            tag(value[0], child, depth + 1)
    elif kind == 10:
        require(type(value) is dict and len(value) <= 12, 'bounded original NBT Compound')
        for key, child in value.items():
            require(type(key) is str and type(child) is list and len(child) == 2, 'original named NBT tag')
            tag(*child, depth + 1)
    else:
        require(False, 'undeclared NBT payload type')


def validate(cases, observations):
    require(type(cases) is list and 1 <= len(cases) <= 256 and
            len({c['profile'] for c in cases}) == len(cases), 'bounded unique predeclared cases')
    require(set(observations) == {'rows', 'recipes_before', 'recipes_after'}, 'closed original cohort')
    rows = observations['rows']
    require(type(rows) is list and [r['profile'] for r in rows] == [c['profile'] for c in cases],
            'complete ordered original outcomes')
    for key in ('recipes_before', 'recipes_after'):
        require(type(observations[key]) is dict and len(observations[key]) == 10 and all(
            re.fullmatch(r'[1-9][0-9]*', name) and type(v) is list and len(v) == 4 and all(int32(x) for x in v)
            for name, v in observations[key].items()), 'actual full catalog fields')
    for index, (case, row) in enumerate(zip(cases, rows)):
        require(set(row) == {'index', 'profile', 'operation', 'returned', 'result', 'exception',
            'exception_message', 'before', 'after', 'tile_same', 'array_same', 'layout', 'bm_caches',
            'nbt', 'input_nbt'}, 'closed original result row')
        require(row['index'] == index and row['operation'] == case['operation'] and
                all(type(row[k]) is bool for k in ('returned', 'tile_same', 'array_same')),
                'literal operation and typed return/identity flags')
        require(row['result'] is None or type(row['result']) in (str, bool) or int32(row['result']),
                'raw original result')
        require((row['returned'] and row['exception'] is None and row['exception_message'] is None) or
                (not row['returned'] and type(row['exception']) is str and
                 row['exception'].startswith('java.lang.') and row['result'] is None and
                 (row['exception_message'] is None or type(row['exception_message']) is str)),
                'literal original return or exception, without normalization')
        state(row['before'])
        state(row['after'])
        before = row['before']
        require(before['stacks'] == case['stacks'] and
                before['fields'] == (None if case['tile_null'] else case['fields']) and
                before['xyz'] == (None if case['tile_null'] else case['xyz']) and
                before['slots'] == (None if case['tile_null'] or case['array_null'] else
                    [case['tile'].get(str(i)) for i in range(3)]) and
                before['player'] == (None if case['player_null'] else
                    [case['player'].get(str(i)) for i in range(36)]) and
                before['player_changed'] == (None if case['player_null'] else False),
                'actual before-state equals all declared inputs')
        for key in ('nbt', 'input_nbt'):
            value = row[key]
            if value is None:
                continue
            require(set(value) == {'bytes', 'sha256', 'tags'} and type(value['bytes']) is int and
                    0 < value['bytes'] < 4096 and re.fullmatch('[0-9a-f]{64}', value['sha256']),
                    'exact original NBT wire extent/hash and typed readback')
            tag(10, value['tags'])
        if row['layout'] is not None:
            require(len(row['layout']) == 39 and [s[0] for s in row['layout']] == list(range(39)),
                    'complete original BM layout')
            for slot in row['layout']:
                require(type(slot) is list and len(slot) == 6 and slot[1] in ('SK', 'IX') and
                        slot[5] in ('gp', 'vq') and all(int32(slot[i]) for i in (0, 2, 3, 4)),
                        'actual typed Slot graph')
        require(row['bm_caches'] is None or type(row['bm_caches']) is list and
                len(row['bm_caches']) == 3 and all(int32(v) for v in row['bm_caches']), 'actual BM counters')


def validate_traces(cases, traces, codes, pins):
    archive = {owner + '.' + method['name'] + method['descriptor']: method
               for owner, value in pins['classes'].items() for method in value['methods']}
    require([row['index'] for row in traces] == list(range(len(cases))), 'all actual source trace boundaries')
    seen = set()

    def value(item):
        if item is None:
            return
        require(type(item) is dict and type(item.get('type')) is str, 'actual typed argument/receiver')
        if 'value' in item:
            require(set(item) == {'type', 'value'} and (type(item['value']) in (str, bool) or int32(item['value'])),
                    'actual primitive/text value')
        else:
            require(set(item) <= {'type', 'identity', 'fields', 'length'} and
                    type(item.get('identity')) is str, 'actual object identity value')
            if 'fields' in item:
                require(item['type'] == 'iz' and len(item['fields']) == 4 and all(int32(v) for v in item['fields']),
                        'actual Stack fields at source method entry')
            if 'length' in item:
                require(type(item['length']) is int and 0 <= item['length'] <= 32000, 'actual array extent')
    for row in traces:
        require(set(row) == {'index', 'entries'} and type(row['entries']) is list and len(row['entries']) <= 512,
                'bounded ordered source-entry sequence')
        for entry in row['entries']:
            require(set(entry) == {'method', 'receiver', 'arguments', 'source_parent'} and entry['method'] in codes,
                    'actual entered method has a live Code identity')
            seen.add(entry['method'])
            value(entry['receiver'])
            require(type(entry['arguments']) is list and len(entry['arguments']) <= 8, 'actual source arguments')
            for item in entry['arguments']:
                value(item)
            require(type(entry['source_parent']) is list and len(entry['source_parent']) <= 1, 'nearest selected actual caller')
            for parent in entry['source_parent']:
                require(set(parent) == {'method', 'pc'} and parent['method'] in archive and
                        type(parent['pc']) is int and 0 <= parent['pc'] < archive[parent['method']]['code_bytes'],
                        'actual source call-site index')
    require(seen == set(codes), 'each live Code pin was entered')
    for name, code in codes.items():
        require(name in archive and code == {key: archive[name][key] for key in ('code_bytes', 'code_sha256')},
                'entered live method Code equals original archive')


def audit(directory=HERE):
    metadata = load(directory / 'provenance.json')
    require(metadata['client_jar_sha256'] == JAR, 'original archive identity')
    for group, names in (('execution_helpers', EXECUTION), ('publication_helpers', PUBLICATION), ('outputs', OUTPUTS)):
        require(set(metadata[group]) == set(names), 'complete public fingerprint inventory')
        for name in names:
            require(digest(directory / name) == metadata[group][name], 'public packet fingerprint: ' + name)
    cases = load(directory / 'cases.json')['cases']
    finite = load(directory / 'finite.json')
    require(finite['client_jar_sha256'] == JAR, 'original observation archive identity')
    validate(cases, finite['observations'])
    pins = load(directory / 'source-pins.json')
    traces = load(directory / 'traces.json')
    validate_traces(cases, traces['rows'], traces['live_Code'], pins)
    projection = module('furnace_public_projection', directory / 'projection.py')
    require(projection.tokens((directory / 'native-observations.h').read_text()) ==
            projection.tokens(projection.header(cases, finite['observations']['rows'])), 'native table equals actual observations')
    return {'original_cases': len(cases), 'BM_cases': sum(c['operation'].startswith('bm-') for c in cases),
            'source_entries': sum(len(r['entries']) for r in traces['rows']), 'live_Code_pins': len(traces['live_Code']),
            'scope': 'Original component evidence; World/TC/tick completion, concrete Player, GUI and native replay remain separate.'}


if __name__ == '__main__':
    print(audit())
