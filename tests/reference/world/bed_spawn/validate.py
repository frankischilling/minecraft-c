"""Strict readers for the authored inputs and retained original Bed observations."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate key: ' + key)
        result[key] = value
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def profiles():
    declared = load(HERE / 'profiles.json')
    text = (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines()
    require(type(declared) is list and len(declared) == len(text) == 30, 'Bed profile extent')
    result = []
    names = set()
    for index, (entry, line) in enumerate(zip(declared, text)):
        require(type(entry) is dict and set(entry) == {'name', 'values'} and
                type(entry['name']) is str and entry['name'] not in names, 'Bed profile declaration')
        names.add(entry['name'])
        row = [int(word) for word in line.split(' ')]
        require(all(type(word) is int and -(1 << 31) <= word < (1 << 31) for word in row) and
                row == entry['values'] and len(row) >= 8 and row[0] == index and
                row[1] in (0, 1) and len(row) == 8 + 5 * row[7] and row[7] >= 0 and
                0 <= row[3] < 128, 'Bed exact profile transport')
        result.append(row)
    return result


def planes(row):
    chunks = {(x, z): (bytearray(32768), bytearray(16384))
              for x in range(-2, 2) for z in range(-2, 2)}
    def cell(x, y, z, block, metadata):
        require((x >> 4, z >> 4) in chunks and 0 <= y < 128 and 0 <= block <= 255,
                'Bed declared cell domain')
        blocks, values = chunks[x >> 4, z >> 4]
        index = ((x & 15) * 16 + (z & 15)) * 128 + y
        blocks[index] = block
        shift = (index & 1) * 4
        values[index >> 1] = ((values[index >> 1] & ~(15 << shift)) |
                              ((metadata & 15) << shift))
    for offset in range(8, len(row), 5):
        cell(*row[offset:offset + 5])
    x, y, z, metadata = row[2:6]
    blocks, values = chunks[x >> 4, z >> 4]
    block = blocks[((x & 15) * 16 + (z & 15)) * 128 + y]
    cell(x, y, z, block, metadata)
    value = hashlib.sha256()
    for (x, z), (blocks, values) in chunks.items():
        value.update(struct.pack('>ii', x, z)); value.update(blocks); value.update(values)
    return value.hexdigest()


def rows(text):
    parsed = [json.loads(line[len('BED_SPAWN '):], object_pairs_hook=unique)
              for line in text.splitlines() if line.startswith('BED_SPAWN ')]
    inputs = profiles()
    require(len(parsed) == 30, 'Bed complete original rows')
    keys = {'schema', 'case', 'input', 'result', 'exception', 'planes_before', 'planes_after',
            'random_before', 'random_after', 'provider_same', 'input_same', 'fresh_result'}
    for row, profile in zip(parsed, inputs):
        require(type(row) is dict and set(row) == keys and row['schema'] == 'BED_SPAWN_V1' and
                type(row['case']) is int and row['case'] == profile[0] and
                row['input'] == profile and all(type(x) is int for x in row['input']) and
                row['exception'] == 'NONE', 'Bed original row schema/order')
        require(row['result'] is None or (type(row['result']) is list and len(row['result']) == 3 and
                all(type(x) is int and -(1 << 31) <= x < (1 << 31) for x in row['result'])), 'Bed result')
        require(row['planes_before'] == row['planes_after'] == planes(profile), 'Bed actual input planes/preservation')
        require(row['random_before'] == row['random_after'] == ['00000005deecefc9', False, '0000000000000000'] and
                type(row['random_before'][1]) is bool and type(row['random_after'][1]) is bool,
                'Bed actual seeded Random preservation')
        require(row['provider_same'] is True and row['input_same'] is True and
                type(row['fresh_result']) is bool and row['fresh_result'] == (row['result'] is not None),
                'Bed identity preservation')
    return parsed


def queries(values, profile):
    require(type(values) is list and 1 <= len(values) <= 64, 'Bed query extent')
    keys = {'kind', 'pc', 'x', 'y', 'z', 'completed', 'result'}
    sites = {'CAPTURED_ACQUIRE': {22, 45, 68, 91}, 'BLOCK': {110},
             'METADATA': {4}, 'SUPPORT': {101}, 'AIR': {113, 127}}
    for row in values:
        require(type(row) is dict and set(row) == keys and row['kind'] in sites and
                all(type(row[key]) is int and -(1 << 31) <= row[key] < (1 << 31)
                    for key in ('pc', 'x', 'y', 'z', 'result')) and row['pc'] in sites[row['kind']] and
                row['completed'] is True, 'Bed selected query schema/site')
        if row['kind'] in ('SUPPORT', 'AIR'):
            require(row['result'] in (0, 1), 'Bed boolean query result')
        if row['kind'] == 'CAPTURED_ACQUIRE':
            require(row['y'] == 0 and row['result'] == 0, 'Bed unused Chunk result')
    if profile[1] == 0:
        require(values[0]['kind'] == 'METADATA' and not any(
                row['kind'] in ('CAPTURED_ACQUIRE', 'BLOCK') for row in values), 'Bed direct query family')
    else:
        require(len(values) >= 5 and [row['kind'] for row in values[:5]] ==
                ['CAPTURED_ACQUIRE'] * 4 + ['BLOCK'] and
                [row['pc'] for row in values[:5]] == [22, 45, 68, 91, 110] and
                not any(row['kind'] in ('CAPTURED_ACQUIRE', 'BLOCK') for row in values[5:]),
                'Bed wrapper captured-provider query order')
    return values


def journals(text):
    records = [json.loads(line, object_pairs_hook=unique) for line in text.splitlines() if line]
    result = []
    codes = []
    for row in records:
        require(type(row) is dict and row.get('event') in ('code', 'journal'), 'Bed JDI record kind')
        if row['event'] == 'code':
            require(set(row) == {'event', 'class', 'method', 'descriptor', 'bytes', 'sha256'} and
                    type(row['bytes']) is int and row['bytes'] > 0 and
                    type(row['sha256']) is str and re.fullmatch('[0-9a-f]{64}', row['sha256']), 'Bed JDI Code record')
            codes.append(row)
        else:
            require(set(row) == {'event', 'case', 'queries'} and type(row['case']) is int and
                    row['case'] == len(result) and len(result) < 30, 'Bed JDI case order')
            queries(row['queries'], profiles()[row['case']])
            result.append({'case': row['case'], 'journal': row['queries']})
    require(len(result) == 30 and len(codes) == 7, 'Bed complete JDI journals/Code methods')
    expected = {('gs', 'a', '(Lfd;Lbr;)Lbr;'), ('ve', 'f', '(Lfd;IIII)Lbr;'),
                ('fd', 'a', '(III)I'), ('fd', 'e', '(III)I'), ('fd', 'h', '(III)Z'),
                ('fd', 'd', '(III)Z'), ('ok', 'c', '(II)Llm;')}
    require({(row['class'], row['method'], row['descriptor']) for row in codes} == expected,
            'Bed actual reached Code identity inventory')
    pins = load(HERE / 'source-pins.json')['classes']
    for row in codes:
        matches = [method for method in pins[row['class']]['methods']
                   if method['name'] == row['method'] and method['descriptor'] == row['descriptor']]
        require(len(matches) == 1 and row['bytes'] == matches[0]['code_length'] and
                row['sha256'] == matches[0]['code_sha256'], 'Bed actual VM Code pin')
    return result, codes
