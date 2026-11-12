"""Strict readers for the retained original Bed component observations."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SCOPE = ('85 prepared original VE neighbor/day/remote/absent-partner components. Genuine '
         'World/Map-provider/Chunk/DC/NH/UQ; explicitly prepared original App and Font widths. '
         'No night, sleep, wake, Nether explosion, startup, GUI or native result. '
         'Item Math/default-Random motion/rotation are outside selected outputs.')
COMPARE_SCOPE = ('85 World-only Bed component projections: complete Block/metadata planes, '
                 'selected Item/Stack/publication fields, World Random, actual CW result identity '
                 'and reached Player/message-key boundary. Original notice, Player constructor '
                 'snapshot and complete JDI journal remain retained and excluded from native '
                 'comparison; no native DC/UQ/Font/translation completion or loaded-game claim.')
SOURCES = ('BedInteractionObservation.java', 'BedInteractionJdi.java', 'capture_reference.py',
           'validate.py', 'profiles.json', 'profiles.tsv', 'source-pins.json',
           '../../client/controller/capture.py', '../../entity/player/classfile.py')
RECEIPT = ('exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete', 'reader_errors',
           'output_overflow', 'stdout_file', 'stderr_file', 'stdout_bytes', 'stderr_bytes',
           'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit')
COMPLETE = ('BED_INTERACTION_COMPLETE cases=85 network=0 native=0 writes=0 exec=0 '
            'prepared-app-font=1 genuine-DC-UQ=1 sleep=0 explosion=0 draw=0')
PROVIDER = 'PROVIDER_FRONTIER Overworld=true Nether=false next=VE95..256 source-explosion-uninvoked=1'
FACING = ((0, 1), (-1, 0), (0, -1), (1, 0))


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


def constant(value):
    raise ValueError('non-JSON numeric constant: ' + value)


def loads(text):
    return json.loads(text, object_pairs_hook=unique, parse_constant=constant)


def load(path):
    return loads(Path(path).read_text(encoding='utf-8'))


def integer(value, low=-(1 << 31), high=(1 << 31) - 1):
    return type(value) is int and low <= value <= high


def word(value, length):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', value) is not None


def utf16(value):
    return type(value) is str and len(value) % 4 == 0 and re.fullmatch('[0-9a-f]*', value) is not None


def keys(value, expected, message):
    require(type(value) is dict and set(value) == set(expected), message)


def compare(left, right, label='Bed'):
    comparisons = 0

    def visit(a, b, path):
        nonlocal comparisons
        require(type(a) is type(b), label + ' type differs: ' + path)
        if type(b) is dict:
            require(a.keys() == b.keys(), label + ' keys differ: ' + path)
            for key in b:
                visit(a[key], b[key], path + '/' + key)
        elif type(b) is list:
            require(len(a) == len(b), label + ' extent differs: ' + path)
            for index, (x, y) in enumerate(zip(a, b)):
                visit(x, y, path + '/' + str(index))
        else:
            comparisons += 1
            require(a == b, label + ' value differs: ' + path)

    visit(left, right, '')
    return comparisons


def profiles():
    value = load(HERE / 'profiles.json')
    text = (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines()
    expected = [[4 * meta + 2 * partner + remote, 0, meta, partner, remote]
                for meta in range(16) for partner in range(2) for remote in range(2)]
    expected += [[64 + meta, 1, meta, 1, 0] for meta in range(16)]
    expected += [[80 + meta, 1, meta, 0, 0] for meta in range(4)] + [[84, 1, 0, 0, 1]]
    require(type(value) is list and len(value) == len(text) == 85, 'Bed profile extent')
    for row, line, declared in zip(value, text, expected):
        require(type(row) is list and len(row) == 5 and all(integer(x) for x in row) and
                row == declared and line == ' '.join(map(str, row)), 'Bed exact authored profile')
    return value


def coordinates(profile):
    _, _, metadata, _, _ = profile
    dx, dz = FACING[metadata & 3]
    head = bool(metadata & 8)
    return [8 + (-dx if head else dx), 64, 8 + (-dz if head else dz)], [8, 64, 8] if head else [8 + dx, 64, 8 + dz]


def sparse(mapping):
    return [[x, y, z, block, metadata] for (x, y, z), (block, metadata) in
            sorted(mapping.items(), key=lambda item: (item[0][0], item[0][2], item[0][1]))
            if block != 0 or metadata != 0]


def input_cells(profile):
    _, kind, metadata, partner, _ = profile
    mate, _ = coordinates(profile)
    cells = {(8, 64, 8): (26, metadata)}
    if partner:
        mate_metadata = (metadata & 7 if metadata & 8 else metadata | 8) if kind else (
            metadata & 3 if metadata & 8 else (metadata & 3) | 8)
        cells[tuple(mate)] = (26, mate_metadata)
    return sparse(cells)


def planes(cells):
    require(type(cells) is list and len(cells) <= 32768, 'Bed sparse plane extent')
    blocks, metadata = bytearray(32768), bytearray(16384)
    prior = None
    for cell in cells:
        require(type(cell) is list and len(cell) == 5 and all(integer(x) for x in cell),
                'Bed sparse cell types')
        x, y, z, block, nibble = cell
        order = (x, z, y)
        require(0 <= x < 16 and 0 <= z < 16 and 0 <= y < 128 and 0 <= block <= 255 and
                0 <= nibble <= 15 and (block != 0 or nibble != 0) and
                (prior is None or prior < order), 'Bed sparse cell domain/order/uniqueness')
        prior = order
        index = (x * 16 + z) * 128 + y
        blocks[index] = block
        metadata[index >> 1] |= nibble << ((index & 1) * 4)
    return hashlib.sha256(blocks + metadata).hexdigest()


def output_cells(row, profile):
    cells = {tuple(cell[:3]): tuple(cell[3:]) for cell in input_cells(profile)}
    mate, resolved = coordinates(profile)
    if row['kind'] == 'neighbor':
        cells[(8, 64, 8)] = (row['id'], row['metadata_after'])
        old = cells.get(tuple(mate), (0, 0))
        cells[tuple(mate)] = (row['partner_id'], old[1])
    else:
        cells[(8, 64, 8)] = (row['origin_id'], row['metadata'])
        cells[tuple(resolved)] = (row['resolved_id'], row['resolved_metadata'])
    result = sparse(cells)
    require(planes(result) == row['output_planes'], 'Bed observed output fields reconstruct full original planes')
    return result


def random_state(value):
    require(type(value) is list and len(value) == 3 and word(value[0], 16) and
            int(value[0], 16) < (1 << 48) and type(value[1]) is bool and word(value[2], 16),
            'Bed Random state schema')


def items(value):
    require(type(value) is list and len(value) <= 1, 'Bed selected Item extent')
    for item in value:
        keys(item, ('class', 'stack', 'position', 'pickup_delay', 'world_member', 'chunk_members'),
             'Bed Item exact schema')
        require(item['class'] == 'hl' and type(item['stack']) is list and len(item['stack']) == 4 and
                all(integer(x) for x in item['stack']) and item['stack'] == [355, 1, 0, 0] and
                type(item['position']) is list and len(item['position']) == 3 and
                all(word(x, 16) for x in item['position']) and integer(item['pickup_delay']) and
                item['pickup_delay'] == 10 and item['world_member'] is True and
                integer(item['chunk_members']) and item['chunk_members'] == 1, 'Bed actual selected Item fields')


def player_before(value):
    require(type(value) is list and len(value) == 10 and integer(value[0]) and value[0] == 20 and
            value[1] is False and integer(value[2]) and value[2] == 0 and value[3:5] == [None, None],
            'Bed prepared Player prefix schema')
    for offset, extent, length in ((5, 3, 16), (6, 3, 16), (7, 3, 8), (8, 2, 8), (9, 6, 16)):
        require(type(value[offset]) is list and len(value[offset]) == extent and
                all(word(x, length) for x in value[offset]), 'Bed original-only Player words')


def rows(value):
    require(type(value) is list and len(value) == 85, 'Bed original row extent')
    for row, profile in zip(value, profiles()):
        number, kind, metadata, partner, remote = profile
        common = {'case', 'kind', 'metadata', 'partner', 'remote', 'input_planes', 'output_planes',
                  'random_before', 'random_after'}
        more = {'changed_id', 'partner_xyz', 'id', 'metadata_after', 'partner_id', 'items'} if not kind else {
            'resolved_xyz', 'returned', 'origin_id', 'resolved_id', 'resolved_metadata', 'daytime',
            'provider_allows', 'notice', 'player_unchanged', 'player_before'}
        keys(row, common | more, 'Bed original exact row schema')
        require(integer(row['case']) and row['case'] == number and row['kind'] == ('activate' if kind else 'neighbor') and
                integer(row['metadata']) and row['metadata'] == metadata and type(row['partner']) is bool and
                row['partner'] == bool(partner) and type(row['remote']) is bool and row['remote'] == bool(remote),
                'Bed original row profile/order/types')
        require(word(row['input_planes'], 64) and word(row['output_planes'], 64) and
                planes(input_cells(profile)) == row['input_planes'], 'Bed exact prepared input full planes')
        random_state(row['random_before']); random_state(row['random_after'])
        require(row['random_before'] == ['00000005deecefc9', False, '0000000000000000'], 'Bed actual seeded Random')
        mate, resolved = coordinates(profile)
        if not kind:
            require(integer(row['changed_id']) and row['changed_id'] == 81 and
                    type(row['partner_xyz']) is list and all(integer(x) for x in row['partner_xyz']) and
                    row['partner_xyz'] == mate and all(integer(row[k], 0, 255) for k in ('id', 'partner_id')) and
                    integer(row['metadata_after'], 0, 15), 'Bed neighbor input/output fields')
            items(row['items'])
            drop = not partner and not remote and not (metadata & 8)
            require(len(row['items']) == int(drop) and
                    (drop or row['random_after'] == row['random_before']), 'Bed selected drop/Random extent')
        else:
            require(type(row['resolved_xyz']) is list and all(integer(x) for x in row['resolved_xyz']) and
                    row['resolved_xyz'] == resolved and row['returned'] is True and row['daytime'] is True and
                    row['provider_allows'] is True and all(integer(row[k], 0, 255) for k in ('origin_id', 'resolved_id')) and
                    integer(row['resolved_metadata'], 0, 15) and type(row['notice']) is list and
                    row['random_after'] == row['random_before'], 'Bed activation output types/Random')
            day = 64 <= number < 80
            require((row['player_unchanged'] is True if day else row['player_unchanged'] is None) and
                    len(row['notice']) == int(day), 'Bed genuine reached Player/notice extent')
            if day:
                player_before(row['player_before'])
                require(type(row['notice'][0]) is list and len(row['notice'][0]) == 2 and
                        utf16(row['notice'][0][0]) and integer(row['notice'][0][1]) and
                        row['notice'][0][1] == 0, 'Bed retained original notice UTF16/age')
            else:
                require(row['player_before'] is None, 'Bed unreached Player snapshot')
        output_cells(row, profile)
    return value


CALLS = {
    ('ve', 'b', '(Lfd;IIII)V', 'fd', 'e', '(III)I'): {5},
    ('ve', 'b', '(Lfd;IIII)V', 'fd', 'a', '(III)I'): {48, 94},
    ('ve', 'b', '(Lfd;IIII)V', 'fd', 'f', '(IIII)Z'): {64, 110},
    ('ve', 'b', '(Lfd;IIII)V', 'uu', 'g', '(Lfd;IIII)V'): {129},
    ('uu', 'a', '(Lfd;IIIIF)V', 'java.util.Random', 'nextFloat', '()F'): {32},
    ('uu', 'a', '(Lfd;IIILiz;)V', 'java.util.Random', 'nextFloat', '()F'): {16, 39, 62},
    ('uu', 'a', '(Lfd;IIILiz;)V', 'fd', 'b', '(Lsn;)Z'): {119},
    ('ve', 'a', '(Lfd;IIILgs;)Z', 'fd', 'e', '(III)I'): {14, 80},
    ('ve', 'a', '(Lfd;IIILgs;)Z', 'fd', 'a', '(III)I'): {63},
    ('ve', 'a', '(Lfd;IIILgs;)Z', 'xa', 'f', '()Z'): {89},
    ('ve', 'a', '(Lfd;IIILgs;)Z', 'gs', 'b', '(III)Lcw;'): {388},
    ('gs', 'b', '(III)Lcw;', 'fd', 'f', '()Z'): {49},
    ('ve', 'a', '(Lfd;IIILgs;)Z', 'dc', 'b', '(Ljava/lang/String;)V'): {424},
    ('dc', 'b', '(Ljava/lang/String;)V', 'uq', 'c', '(Ljava/lang/String;)V'): {8},
    ('uq', 'c', '(Ljava/lang/String;)V', 'nh', 'a', '()Lnh;'): {0},
    ('uq', 'c', '(Ljava/lang/String;)V', 'nh', 'a', '(Ljava/lang/String;)Ljava/lang/String;'): {6},
    ('uq', 'c', '(Ljava/lang/String;)V', 'uq', 'a', '(Ljava/lang/String;)V'): {12},
    ('ve', 'a', '(Lfd;IIIZ)V', 'fd', 'e', '(III)I'): {4},
    ('ve', 'a', '(Lfd;IIIZ)V', 'fd', 'd', '(IIII)V'): {36},
}


def value_type(value):
    if value is None or type(value) is bool or integer(value):
        return
    if type(value) is str:
        require(value in ('fd', 'hl', 'nh', 'cw.c') or word(value, 8), 'Bed selected reference/float value')
        return
    keys(value, ('utf16',), 'Bed selected String schema')
    require(utf16(value['utf16']), 'Bed selected String UTF16')


def call_values(call):
    args, result, descriptor = call['arguments'], call['result'], call['descriptor']

    def string(value):
        return type(value) is dict and set(value) == {'utf16'} and utf16(value['utf16'])

    if descriptor in ('(III)I', '(IIII)Z', '(IIII)V', '(III)Lcw;'):
        extent = 4 if descriptor.startswith('(IIII)') else 3
        require(len(args) == extent and all(integer(x) for x in args), 'Bed exact int argument types')
    elif descriptor == '(Lfd;IIII)V':
        require(len(args) == 5 and args[0] == 'fd' and all(integer(x) for x in args[1:]),
                'Bed final-drop receiver/int arguments')
    elif descriptor == '(Lsn;)Z':
        require(args == ['hl'], 'Bed actual Item publication argument')
    elif descriptor.startswith('(Ljava/lang/String;)'):
        require(len(args) == 1 and string(args[0]), 'Bed actual String argument')
    else:
        require(args == [], 'Bed no-argument call descriptor')
    if descriptor.endswith(')V'):
        require(result is None, 'Bed void result')
    elif descriptor.endswith(')Z'):
        require(type(result) is bool, 'Bed boolean result type')
    elif descriptor.endswith(')I'):
        require(integer(result, 0, 15 if call['method'] == 'e' else 255), 'Bed integer result type')
    elif descriptor == '()F':
        require(word(result, 8), 'Bed Float raw result word')
    elif descriptor == '(III)Lcw;':
        require(result == 'cw.c', 'Bed actual current enum result identity')
    elif descriptor == '()Lnh;':
        require(result == 'nh', 'Bed actual translation singleton result')
    else:
        require(descriptor == '(Ljava/lang/String;)Ljava/lang/String;' and string(result),
                'Bed translated String result')


def journals(value, original):
    require(type(value) is list and len(value) == 85, 'Bed journal extent')
    for index, row in enumerate(value):
        keys(row, ('event', 'case', 'calls'), 'Bed journal exact schema')
        require(row['event'] == 'journal' and integer(row['case']) and row['case'] == index and
                type(row['calls']) is list and len(row['calls']) <= 256, 'Bed journal case/extent')
        times = []
        for call in row['calls']:
            keys(call, ('caller', 'caller_method', 'caller_descriptor', 'pc', 'class', 'method',
                        'descriptor', 'arguments', 'entered', 'returned', 'completed', 'result'),
                 'Bed call exact schema')
            names = ('caller', 'caller_method', 'caller_descriptor', 'class', 'method', 'descriptor')
            require(all(type(call[k]) is str for k in names), 'Bed selected call method names')
            key = tuple(call[k] for k in names)
            require(key in CALLS and integer(call['pc']) and call['pc'] in CALLS[key] and
                    type(call['arguments']) is list and len(call['arguments']) <= 5 and
                    integer(call['entered'], 0, 511) and integer(call['returned'], 0, 511) and
                    call['entered'] < call['returned'] and call['completed'] is True,
                    'Bed selected call site/entry/normal return')
            for arg in call['arguments']:
                value_type(arg)
            value_type(call['result'])
            call_values(call)
            times += [call['entered'], call['returned']]
        require(sorted(times) == list(range(len(row['calls']) * 2)) and
                [c['entered'] for c in row['calls']] == sorted(c['entered'] for c in row['calls']),
                'Bed complete ordered entry/return ticks')
        for a in row['calls']:
            for b in row['calls']:
                require(not (a['entered'] < b['entered'] < a['returned'] < b['returned']),
                        'Bed call intervals must nest or be disjoint')
        calls = row['calls']
        sleep = [c for c in calls if c['class'] == 'gs' and c['descriptor'] == '(III)Lcw;']
        message = [c for c in calls if c['class'] == 'dc' and c['descriptor'] == '(Ljava/lang/String;)V']
        if 64 <= index < 80:
            require(len(sleep) == len(message) == 1 and sleep[0]['result'] == 'cw.c' and
                    sleep[0]['returned'] < message[0]['entered'], 'Bed actual CW return precedes DC notice')
            setters = [c for c in calls if c['class'] == 'fd' and c['descriptor'] == '(IIII)V']
            require(all(c['returned'] < sleep[0]['entered'] for c in setters), 'Bed occupancy clear returned before sleep query')
            translate = [c for c in calls if c['class'] == 'nh' and c['descriptor'] == '(Ljava/lang/String;)Ljava/lang/String;']
            add = [c for c in calls if c['class'] == 'uq' and c['method'] == 'a' and c['descriptor'] == '(Ljava/lang/String;)V']
            require(len(translate) == len(add) == 1 and message[0]['arguments'] == translate[0]['arguments'] and
                    translate[0]['result'] == add[0]['arguments'][0] == {'utf16': original[index]['notice'][0][0]},
                    'Bed retained original translation/notice continuity')
        else:
            require(sleep == message == [], 'Bed unreached message/sleep children')
    require(sum(len(row['calls']) for row in value) == 408, 'Bed complete selected call count')
    return value


def codes(value):
    require(type(value) is list and len(value) == 20, 'Bed live Code extent')
    pins = load(HERE / 'source-pins.json')
    require(type(pins) is dict and len(pins) == 24, 'Bed original class pin inventory')
    seen = set()
    for row in value:
        keys(row, ('event', 'class', 'method', 'descriptor', 'bytes', 'sha256'), 'Bed live Code exact schema')
        require(row['event'] == 'code' and all(type(row[k]) is str for k in ('class', 'method', 'descriptor')) and
                integer(row['bytes'], 1, 65535) and word(row['sha256'], 64), 'Bed live Code types')
        key = (row['class'], row['method'], row['descriptor'])
        require(key not in seen, 'Bed duplicate live Code method'); seen.add(key)
        if row['class'] == 'java.util.Random':
            require(key == ('java.util.Random', 'nextFloat', '()F') and row['bytes'] == 11 and
                    row['sha256'] == '31d19315f8f329460d8d6158901124502b6d25e41a6bc61878e5b97d5245f17a',
                    'Bed actual JDK17 Random Code pin')
        else:
            require(row['class'] in pins, 'Bed live Code declaring original class')
            found = [m for m in pins[row['class']]['methods'] if (m['name'], m['descriptor']) == key[1:]]
            require(len(found) == 1 and row['bytes'] == found[0]['code_length'] and
                    row['sha256'] == found[0]['code_sha256'], 'Bed unchanged original live Code pin')
    required = {(c[3], c[4], c[5]) for c in CALLS}
    required |= {('ve', 'a', '(Lfd;IIILgs;)Z'), ('ve', 'a', '(Lfd;IIIZ)V'),
                 ('ve', 'a', '(Lfd;IIIIF)V'), ('ve', 'a', '(ILjava/util/Random;)I'), ('ve', 'b', '(Lfd;IIII)V')}
    require(seen == required, 'Bed exact selected live Code identity inventory')
    return value


def packet(value):
    keys(value, ('rows', 'journals', 'code'), 'Bed finite packet exact schema')
    rows(value['rows']); journals(value['journals'], value['rows']); codes(value['code'])
    return value


def original_text(text):
    lines = text.splitlines()
    require(lines and lines[-1] == COMPLETE and lines.count(PROVIDER) == 1, 'Bed guarded original completion/provider probe')
    require(all(line.startswith(('CLASS ', 'BED_INTERACTION ')) or line == PROVIDER or
                line in ('16 achievements', '151 recipes') for line in lines[:-1]), 'Bed original stdout inventory')
    result = rows([loads(line[len('BED_INTERACTION '):]) for line in lines if line.startswith('BED_INTERACTION ')])
    pins = load(HERE / 'source-pins.json')
    expected = ['CLASS ' + name + ' ' + str(v['class_bytes']) + ' ' + v['class_sha256'] for name, v in pins.items()]
    require([line for line in lines if line.startswith('CLASS ')] == expected, 'Bed actual original class origin/bytes')
    return result


def journal_text(text, original):
    value = [loads(line) for line in text.splitlines()]
    require(all(type(v) is dict and v.get('event') in ('journal', 'code') for v in value), 'Bed JDI record kinds')
    calls = journals([v for v in value if v['event'] == 'journal'], original)
    code = codes([v for v in value if v['event'] == 'code'])
    return calls, code


def projection(value):
    packet(value)
    result = []
    for row, journal, profile in zip(value['rows'], value['journals'], profiles()):
        native = {k: row[k] for k in ('case', 'kind', 'metadata', 'partner', 'remote', 'random_before', 'random_after')}
        native.update(input_cells=input_cells(profile), output_cells=output_cells(row, profile))
        if row['kind'] == 'neighbor':
            native.update({k: row[k] for k in ('id', 'metadata_after', 'partner_id', 'items')})
        else:
            native.update({k: row[k] for k in ('returned', 'origin_id', 'resolved_id', 'resolved_metadata',
                                             'daytime', 'provider_allows', 'player_unchanged')})
            sleep = [c for c in journal['calls'] if c['class'] == 'gs' and c['descriptor'] == '(III)Lcw;']
            message = [c for c in journal['calls'] if c['class'] == 'dc' and c['descriptor'] == '(Ljava/lang/String;)V']
            native['sleep_result'] = sleep[0]['result'] if sleep else None
            native['message_keys'] = [c['arguments'][0]['utf16'] for c in message]
        result.append(native)
    return result
