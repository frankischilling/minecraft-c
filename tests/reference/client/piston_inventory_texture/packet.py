"""Audit the full finite original JQ metadata-texture matrix and dependency prefix."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
FILES = ('PistonTextureObservation.java', 'inputs.json', 'capture_reference.py',
         'observations.tsv', 'guards.txt', 'initialization.json', 'source-pins.json')
CODE = '1bc359974ab6ba0b56df268c0ee8d4508b9f541d1949b4ebffe4bcff46ada221'


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, 'duplicate JSON key: ' + name)
        result[name] = value
    return result


def load(path):
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique)


def lines(path):
    data = path.read_bytes()
    require(data.endswith(b'\n') and b'\r' not in data, 'exact ASCII LF transport')
    return data.decode('ascii').splitlines()


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(provenance['client_jar_sha256'] == JAR and
            provenance['original_inputs_unchanged'] is True and
            provenance['original_helpers_unchanged'] is True, 'unchanged original authority')
    for key, value in (('format', 1), ('repeats', 2), ('rows', 34612), ('texture_rows', 34320)):
        require(type(provenance[key]) is int and provenance[key] == value, 'exact ' + key)
    require(set(provenance['files_sha256']) == set(FILES), 'complete captured file inventory')
    for name in FILES:
        require(digest(directory / name) == provenance['files_sha256'][name], 'changed captured file: ' + name)
    guards = ['JQ_GUARD source rows=34320 network=0 native=0 writes=0 execs=0',
              'JQ_GUARD controls network=2 native=1 writes=1 execs=1']
    require(lines(directory / 'guards.txt') == provenance['source_guards'] == guards,
            'zero source attempts and five real denial controls')
    initialization = load(directory / 'initialization.json')
    require(type(initialization) is list and len(initialization) == 2, 'two fresh initialization observations')
    for observation in initialization:
        require(set(observation) == {'wj_initializations', 'prefix_call', 'before_front_or_invalid_prefixes'} and
                type(observation['wj_initializations']) is int and observation['wj_initializations'] == 1 and
                type(observation['prefix_call']) is int and observation['prefix_call'] == 3 and
                observation['before_front_or_invalid_prefixes'] is False,
                'actual WJ initialization only inside the reached opposite-side call')
    require(set(provenance['compiled_helpers_sha256']) ==
            {'PistonTextureObservation.class', 'PistonTextureObservation$1.class'} and
            all(re.fullmatch('[0-9a-f]{64}', value)
                for value in provenance['compiled_helpers_sha256'].values()) and
            len(provenance['original_stdout_sha256']) == len(provenance['original_stderr_sha256']) == 2,
            'fresh process and authored helper identities')
    inputs = load(directory / 'inputs.json')
    require(inputs['block_ids'] == [29, 33] and
            inputs['metadata'] == [*range(32), -1, -2, -9, -17, 32, 40, -(1 << 31), (1 << 31) - 1] and
            inputs['sides'] == [-(1 << 31), -1, *range(8), (1 << 31) - 1] and
            inputs['texture_modes'] == ['constructor', -3, 197] and
            inputs['bounds_scenes'] == ['unit', 'partial', 'outside', 'signed-zero', 'nan-bs', 'nan-bt',
                                         'nan-bu', 'nan-bv', 'nan-bw', 'nan-bx', 'all-nan',
                                         'infinite-full', 'infinite-partial'] and
            inputs['opposite_mutations'] == [-(1 << 31), -1, 42, (1 << 31) - 1] and
            inputs['prefix_sides'] == [0, 1, 1, 0] and inputs['prefix_metadata'] == [6, 1, 9, 1] and
            type(inputs['inventory_metadata']) is int and inputs['inventory_metadata'] == 1,
            'complete declared source input axes')
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR and set(pins['classes']) == {'jq', 'uu', 'wj'},
            'actual JQ and dependent class identities')
    for name, class_hash, method_count in (
            ('jq', '3f7ab2c288ea884e9aab49119a85e630d4f7638d45d8a84f0db2d397a3ef3c61', 22),
            ('uu', 'caeaf354357d66fc168c7f03bf62bb6c6a1fe459e9be6b537d4d5d1efabd0845', 74),
            ('wj', '7291b1994c75f9e48461ff066ea92bf1c0b082ed7ec62ee777dbe860dd64bdec', 2)):
        detail = pins['classes'][name]
        require(detail['class_sha256'] == class_hash and len(detail['methods']) == method_count and
                len({(m['name'], m['descriptor']) for m in detail['methods']}) == method_count,
                'complete method/class inventory: ' + name)
    for owner, method, descriptor, length, code in (
            ('jq', 'a', '(II)I', 104, CODE),
            ('jq', 'd', '(I)I', 5, '86334e8a47f1d553ad03907c4d9fd8876e418569fec2f31d862ae58a6741f7a5'),
            ('jq', 'e', '(I)Z', 13, '3ae3a53d374463d8bac25a54d5a0b82c5ebdf7dfe132b806bd5d8bc9f206a26f'),
            ('jq', 'g', '()V', 11, '5dc1c82f3cdd3780dbb2f2a355899743f0564a78212ba4c18175a190d0d91973'),
            ('wj', '<clinit>', '()V', 125, '0128ab8d8b4eb217492dab5e3443442acf3c99805c0bafb9c2be6400820a042a')):
        methods = [m for m in pins['classes'][owner]['methods'] if
                   m['name'] == method and m['descriptor'] == descriptor]
        require(len(methods) == 1 and methods[0]['code_sha256'] == code and
                type(methods[0]['code_bytes']) is int and methods[0]['code_bytes'] == length,
                'exact reached source method Code identity')
    rows = lines(directory / 'observations.tsv')
    require(len(rows) == 34612, 'complete comparison row inventory')
    cursor = texture_index = 0

    def row(tag, prefix, fields):
        nonlocal cursor
        require(cursor < len(rows), 'truncated original matrix')
        cells = rows[cursor].split('\t')
        cursor += 1
        require(len(cells) == fields and cells[:len(prefix) + 1] == [tag, *map(str, prefix)],
                'original row schema/order: ' + tag)
        return cells

    for prefix in range(4):
        require(rows[cursor] == 'JQ_PREFIX_BEFORE ' + str(prefix), 'prefix caller entry')
        cursor += 1
        require(re.fullmatch('JQ_PREFIX_AFTER ' + str(prefix) + ' [0-9a-f]{8}', rows[cursor]),
                'prefix getter result')
        cursor += 1
    input_bounds = {}
    for block_id in inputs['block_ids']:
        for scene in range(13):
            for mode in range(3):
                prepared = row('JQ_INPUT', [block_id, scene, mode], 6)
                require(re.fullmatch('[0-9a-f]{8}', prepared[4]), 'actual live bm bits')
                bounds = prepared[5].split(',')
                require(len(bounds) == 6 and all(re.fullmatch('[0-9a-f]{16}', value) for value in bounds),
                        'six actual widened source bounds')
                if mode != 0:
                    require(prepared[4] == ('fffffffd' if mode == 1 else '000000c5'), 'late bm input')
                input_bounds[(block_id, scene, mode)] = bounds
                for metadata in inputs['metadata']:
                    for side in inputs['sides']:
                        call = row('JQ_TEX', [texture_index, block_id, scene, mode, side, metadata], 9)
                        require(re.fullmatch('[0-9a-f]{8}', call[7]) and call[8] == '1',
                                'actual returned texture and unchanged bounds')
                        texture_index += 1
        for direction in range(6):
            for extended in range(2):
                for changed in inputs['opposite_mutations']:
                    for probe in range(2):
                        side = changed if probe == 0 else ((changed + 1 + (1 << 31)) % (1 << 32)) - (1 << 31)
                        call = row('JQ_MUTATE', [block_id, direction, extended, changed, probe, side], 8)
                        require(re.fullmatch('[0-9a-f]{8}', call[7]), 'live WJ element result')
        prepared = row('JQ_INVENTORY_BOUNDS', [block_id], 3)
        require(prepared[2] == ','.join(['0' * 16] * 3 + ['3ff0000000000000'] * 3),
                'actual inventory unit-bound hook')
        for side in range(6):
            call = row('JQ_INVENTORY', [block_id, side], 4)
            require(re.fullmatch('[0-9a-f]{8}', call[3]), 'ordinary metadata1 inventory texture')
    require(cursor == len(rows) and texture_index == 34320, 'complete finite scope')
    for block_id in inputs['block_ids']:
        for scene in range(13):
            require(input_bounds[(block_id, scene, 0)] == input_bounds[(block_id, scene, 1)] ==
                    input_bounds[(block_id, scene, 2)], 'same actual bounds through bm changes')
    return {'rows': rows, 'inputs': inputs, 'provenance': provenance}
