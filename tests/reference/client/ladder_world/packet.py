"""Validate original Ladder operands, query order, Code pins and packed vertices."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
FROZEN = {'LadderObservation.java': 'e199fd13df8cb0a81b7684d2671c65ba956bda543f94513dbb623724f18e2b85', 'LadderJdi.java': 'd04c5064ad346d6634044a0a115999966341d088934f75a797fb52d8aa2e1901', 'profiles.txt': 'd84338c975314418e9a0cdae40b72d1a8f6941d5963aeeb030829d934ac2f019', 'declared-inputs.json': '2d42b42912570a9ee0dd9a4e6e8dfc89acb25b7cd541af6d51cbdf1bd066c302', 'source-pins.json': 'fffa890a7bce95dd676085c8efd430215ea79ea2f39aa575324807c92e605cb3', 'capture_reference.py': 'c58943c071f009d1b2ba162ced9faf5b9e972610eb598994642aea51d0c165bf'}
CAPTURED = {'selected.txt': '27a60db511b9793204ecccf425bcfcbdae9208ff49582022c9fd7d774efca415', 'events.jsonl': '46a1bedd74b4d85ff3197f79392b4d112b709577643e882889ea92291c150c9c', 'live-code.tsv': 'de8b7852816f73a046e5835f3537f1335d3544260b68175e0ea6b0cf3ec573bc'}
FILES = tuple(FROZEN) + tuple(CAPTURED)
NW_KEYS = set('A B h i j k l m n o p q r s t u v w x z'.split())
XYZ = ((8, 64, 8), (8, 64, 8), (3, 65, 3))
TRANSLATIONS = ((-.25, .5, -.75), (-8, -64, -8), (0, -64, 0))
BOXES = ((0, 0, 0, 1, 1, 1), (.125, .25, .375, .875, .75, .625),
         (-.25, -.5, -.125, 1.5, 1.25, 1.75))
VERTEX_PCS = {2: (578, 612, 646, 680), 3: (442, 474, 506, 538),
              4: (302, 336, 370, 404), 5: (166, 198, 230, 262)}


def require(value, label):
    if not value:
        raise ValueError(label)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key: ' + key)
        result[key] = value
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def lines(path):
    raw = Path(path).read_bytes()
    require(raw.endswith(b'\n') and b'\r' not in raw, 'ASCII LF source records')
    return raw.decode('ascii').splitlines()


def primitive(text):
    result = {}
    for item in text.split(','):
        key, value = item.split('=', 1)
        require(key not in result, 'unique primitive fields')
        result[key] = value
    return result


def dword(value):
    return struct.pack('>d', value).hex()


def double(word):
    require(type(word) is str and re.fullmatch('[0-9a-f]{16}', word), 'raw binary64 operand')
    return struct.unpack('>d', bytes.fromhex(word))[0]


def fword(value):
    return struct.pack('>f', value).hex()


def float_value(word):
    require(type(word) is str and re.fullmatch('[0-9a-f]{8}', word), 'raw binary32 operand')
    return struct.unpack('>f', bytes.fromhex(word))[0]


def raw_nw(state):
    require(set(state) == NW_KEYS, 'complete NW primitive observation')
    for name in 'i j s t u'.split():
        double(state[name])
    for name in 'l m n q w x'.split():
        require(type(state[name]) is bool, 'typed NW boolean')
    for name in NW_KEYS - set('i j s t u l m n q w x'.split()):
        require(type(state[name]) is int, 'typed NW integer')
    require(state['B'] == 2097152 and state['w'] and state['r'] == 7,
            'genuine NW backing and one begun quad batch')
    return {k: str(v).lower() if type(v) is bool else str(v) for k, v in state.items()}


def profiles(directory=HERE):
    result = []
    for text in lines(Path(directory) / 'profiles.txt'):
        fields = text.split(' ')
        require(len(fields) == 14 and re.fullmatch('[a-z0-9-]+', fields[0]),
                'profile name and thirteen signed operands')
        require(all(re.fullmatch(r'-?(?:0|[1-9][0-9]*)', x) for x in fields[1:]),
                'typed profile integers')
        values = list(map(int, fields[1:]))
        require(all(-2**31 <= x < 2**31 for x in values) and values[0] in range(16) and
                values[1] in range(3) and values[4] in range(3) and values[5] in range(4) and
                values[6] in (0, 1) and values[8] in range(3) and values[9] in (0, 1) and
                values[10] in (0, 1) and values[11] in range(4) and values[12] == 0,
                'closed original Ladder operand domain')
        result.append({'index': len(result), 'name': fields[0], 'values': values})
    require(len(result) == 35 and len({p['name'] for p in result}) == 35,
            '35 unique frozen profiles')
    return result


def validate_rows(selected, ps):
    roles = collections.defaultdict(dict)
    lights = None
    guards = []
    for text in selected:
        row = text.split('\t')
        if row[0] == 'LADDER_LIGHT':
            require(lights is None and len(row) == 3 and row[1] == '3', 'actual sky subtraction')
            lights = row[2].split(',')
            require(len(lights) == 16 and all(re.fullmatch('[0-9a-f]{8}', x) for x in lights),
                    'sixteen actual Dimension light words')
        elif row[0] == 'LADDER_STATIC':
            require(row == ['LADDER_STATIC', 'NW.b=true', 'NW.c=false', 'UU.s.length=256',
                            'UU.s.final=true'], 'actual constructor NW and final UU.s state')
        elif row[0] in ('LADDER_INPUT', 'LADDER', 'LADDER_BACKING'):
            index = int(row[1])
            require(row[0] not in roles[index], 'one original row per role and profile')
            roles[index][row[0]] = row
        else:
            guards.append(text)
    require(set(roles) == set(range(35)) and lights is not None, 'complete original profile inventory')
    result, end, unchanged = [], 0, 0
    for profile in ps:
        index, p = profile['index'], profile['values']
        record = roles[index]
        require(set(record) == {'LADDER_INPUT', 'LADDER', 'LADDER_BACKING'}, 'complete source roles')
        inp, row, backing = (record[k] for k in ('LADDER_INPUT', 'LADDER', 'LADDER_BACKING'))
        bm = (83 + p[3] + 2**31) % 2**32 - 2**31
        emission = 0 if p[7] == -2**31 else p[7]
        xyz = list(XYZ[p[8]])
        require(inp == ['LADDER_INPUT', str(index), '65', '83', str(p[3]), str(bm), '0',
                        str(emission), ','.join(map(str, p)), ','.join(map(str, xyz)), '0'],
                'actual registered Ladder/bm/emission/coordinates/pass operands')
        require(len(row) == 16 and row[:4] == ['LADDER', str(index), profile['name'], str(p[0])] and
                row[4:6] == (['UNRETURNED', 'NULL'] if p[5] else ['true', 'NONE']),
                'whole source return and reached NULL state')
        before, after = map(int, row[6:8])
        delta = 48 if not p[5] and p[0] in VERTEX_PCS else 0
        require(before == end and after == before + delta and row[8] == '2097152',
                'uninterrupted original quad batch and metadata visibility')
        require(row[10].split(',') == [dword(x) for x in TRANSLATIONS[p[8]]] and
                row[11].split(',') == [dword(x) for x in BOXES[p[4]]],
                'actual declared translation and retained bounds')
        old, new = map(primitive, row[13:15])
        require(set(old) == set(new) == NW_KEYS and old['o'] == str(before) and new['o'] == str(after),
                'complete original before/after NW fields')
        require(int(new['h']) - int(old['h']) == delta // 8 and
                int(new['p']) - int(old['p']) == delta // 12, 'actual source/expanded vertex counts')
        require(not p[5] or old == new, 'NULL prefix retains all NW primitive fields')
        words = row[15].split(',')
        require(len(words) == max(100, delta) and all(re.fullmatch('[0-9a-f]{8}', x) for x in words),
                'every appended packed word and own inactive tail')
        require(len(backing) == 6 and backing[:3] == ['LADDER_BACKING', str(index), str(2097152-delta)],
                'own-prior complete inactive backing audit')
        result.append({'profile': profile, 'raw': '\t'.join(row), 'input': '\t'.join(inp),
                       'before': old, 'after': new, 'words': words, 'delta': delta,
                       'lights': lights, 'emission': emission, 'xyz': xyz})
        unchanged += 2097152 - delta
        end = after
    require(guards == [
        'LADDER_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=3',
        'LADDER_GUARD controls network=2 native=1 denied-writes=1 execs=1',
        f'LADDER_COMPLETE rows=35 own-unchanged-words={unchanged} genuine-backing=2097152 '
        'begins=1 draw=0 raw-allocations=0 source-replacements=0'],
        'complete original backing/source/denial guard receipts')
    require(end == 960, 'exact original finite appended inventory')
    return result


def validate_events(events, rows, pins, live):
    methods = {cl + '.' + m['name'] + m['descriptor']: m
               for cl, state in pins['classes'].items() for m in state['methods']}
    live_methods = set()
    for line in live:
        tag, owner, name, descriptor, length, code = line.split('\t')
        key = owner + '.' + name + descriptor
        require(tag == 'LW_LIVE_CODE' and key not in live_methods and key in methods and
                methods[key]['code_bytes'] == int(length) and methods[key]['code_sha256'] == code,
                'live method Code equals verified original archive')
        live_methods.add(key)
    def at(text):
        key, pc = text.rsplit('@', 1)
        require(key in live_methods, 'reached pinned live Code')
        return key, int(pc)
    stack, entered, counts = [], {}, collections.Counter()
    roots, ended, failed = set(), set(), set()
    source_vertices = collections.defaultdict(list)
    outer_vertices = collections.Counter()
    for seq, event in enumerate(events):
        require(event['seq'] == seq and type(event['profile']) is int and event['profile'] in range(35),
                'consecutive actual event sequence and profile')
        index = event['profile']; row = rows[index]; p = row['profile']['values']
        kind = event['event']; key, pc = at(event['at'])
        nw = raw_nw(event['nw'])
        if kind == 'entry':
            call = event['call']
            require(type(call) is int and call == len(entered), 'consecutive actual calls')
            counts[key] += 1
            if key == 'cv.h(Luu;III)Z':
                require(not stack and index not in roots and index == len(roots) and
                        event['args'] == [('ref:observedBlock' if p[5] not in (2, 3) else None),
                                          *row['xyz']] and nw == row['before'],
                        'actual whole source entry and NW prefix')
                roots.add(index)
            else:
                require(stack and entered[stack[-1]]['profile'] == index,
                        'properly nested actual source call')
                parent = entered[stack[-1]]
                caller_key, caller_pc = at(event['caller'])
                require(caller_key == parent['at'].rsplit('@', 1)[0], 'actual immediate source parent')
                if key.startswith('ew.'):
                    require(event['receiver'] == 'ref:observedCache' and
                            event['args'][:3] == row['xyz'], 'actual EW coordinates and owner')
                    if key == 'ew.a(IIII)F':
                        require(caller_pc == 13 and event['args'][3] == row['emission'],
                                'actual emission captured before brightness query')
                    if key == 'ew.e(III)I':
                        require(caller_key == 'cv.h(Luu;III)Z' and caller_pc == 117,
                                'metadata query after actual color/UV setup')
                elif key == 'uu.a(I)I':
                    require(caller_pc == 7 and event['receiver'] == 'ref:observedBlock' and
                            event['args'] == [0], 'texture getter executes before override')
                elif key == 'uu.d(Lxp;III)F':
                    require(caller_pc == 34 and event['receiver'] == 'ref:observedBlock' and
                            event['args'] == [('ref:observedCache' if p[5] not in (1, 3) else None),
                                              *row['xyz']], 'actual inherited brightness call')
                elif key.startswith('nw.'):
                    require(event['receiver'] == 'ref:observedTess', 'actual singleton NW call')
                    if key == 'nw.a(DDDDD)V':
                        n = outer_vertices[index]
                        require(p[0] in VERTEX_PCS and n < 4 and caller_pc == VERTEX_PCS[p[0]][n],
                                'actual orientation-specific source vertex PC')
                        outer_vertices[index] += 1
                    if key == 'nw.a(DDD)V':
                        xyz = [fword(double(a) + double(event['nw'][name]))
                               for a, name in zip(event['args'], 'stu')]
                        uv = [fword(double(event['nw'][name])) for name in 'ij']
                        require(event['nw']['l'] and event['nw']['m'] and not event['nw']['n'],
                                'actual source packed vertex flags')
                        source_vertices[index].append(xyz + uv + [f'{event["nw"]["k"] & 0xffffffff:08x}'])
            entered[call] = event
            stack.append(call)
        elif kind == 'return':
            require(stack and event['call'] == stack[-1], 'properly nested source return')
            original = entered[stack.pop()]
            require(original['at'].rsplit('@', 1)[0] == key, 'actual entered/returned method')
            value = event['result']
            if key == 'cv.h(Luu;III)Z':
                require(not stack and value is True and nw == row['after'],
                        'actual returned true and complete final NW prefix')
                ended.add(index)
            elif key == 'uu.a(I)I':
                require(value == (83 + p[3] + 2**31) % 2**32 - 2**31,
                        'actual mutable Block texture getter result')
            elif key == 'ew.e(III)I':
                require(value == p[0], 'actual entered metadata nibble')
            elif key == 'ew.a(III)I':
                require(value == 65, 'actual registered Block in genuine Chunk')
            elif key in ('ew.d(III)I', 'ew.a(IIIZ)I'):
                require(value == (12 if p[1] == 1 else 15 if p[1] == 2 else 0),
                        'actual saved sky/block light level')
            elif key in ('uu.d(Lxp;III)F', 'ew.a(IIII)F'):
                level = max(12 if p[1] == 1 else 15 if p[1] == 2 else 0, row['emission'])
                require(value == row['lights'][level], 'actual source Dimension brightness')
        elif kind == 'field-read':
            require(stack, 'reached source static read')
            if event['field'] == 'nw.a':
                require((key, pc, event['value']) == ('cv.h(Luu;III)Z', 0, 'ref:observedTess'),
                        'actual first singleton capture')
            else:
                require((key, pc, event['field'], event['value'], event['element']) ==
                        ('uu.d(Lxp;III)F', 5, 'uu.s', 'ref:observedEmissions', row['emission']),
                        'actual final emission-array element read')
        elif kind == 'exception':
            require(stack and p[5] and event['type'] == 'java.lang.NullPointerException' and
                    (key, pc) == (('uu.d(Lxp;III)F', 13) if p[5] == 1 else
                                  ('cv.h(Luu;III)Z', 7)) and nw == row['after'] and
                    event['frames'][0] == event['at'] and
                    event['catch'].startswith('LadderObservation.main([Ljava/lang/String;)V@'),
                    'actual reached original NULL PC and retained prefix')
            stack.clear(); failed.add(index); ended.add(index)
        else:
            require(False, 'known read-only original event kind')
    require(not stack and roots == ended == set(range(35)) and failed == {32, 33, 34},
            'complete source calls and reached NULL inventory')
    for index, row in enumerate(rows):
        vertices = source_vertices[index]
        require(len(vertices) == row['delta'] // 12, 'complete actual source vertex inventory')
        words = []
        for start in range(0, len(vertices), 4):
            words.extend(word for i in (0, 1, 2, 0, 2, 3)
                         for word in vertices[start+i] + ['00000000', '00000000'])
        require(words == row['words'][:row['delta']],
                'all packed active words equal actual source arguments and NW state')
    return dict(events=len(events), entries=len(entered), queries=sum(n for k, n in counts.items()
                                                                   if k.startswith('ew.')),
                methods=len(live_methods), counts=dict(counts))


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(type(provenance['profiles']) is int and provenance['profiles'] == 35 and
            provenance['client_jar_sha256'] == JAR and
            provenance['original_inputs_unchanged'] is True and
            provenance['original_helpers_unchanged'] is True and provenance['source_writes_from_jdi'] == 0,
            'unchanged executed original cohort and archive')
    require(FROZEN and provenance['frozen_authored_files_sha256'] == FROZEN and
            provenance['files_sha256'] == {**FROZEN, **CAPTURED}, 'exact frozen source/product pins')
    for name, pin in provenance['files_sha256'].items():
        require(digest(directory / name) == pin, 'changed captured source/product: ' + name)
    ps = profiles(directory)
    observed = validate_rows(lines(directory / 'selected.txt'), ps)
    events = [json.loads(line, object_pairs_hook=unique) for line in lines(directory / 'events.jsonl')]
    pins = load(directory / 'source-pins.json'); live = lines(directory / 'live-code.tsv')
    metrics = validate_events(events, observed, pins, live)
    return {'profiles': ps, 'observed': observed, 'events': events, 'pins': pins, 'live': live,
            'metrics': metrics, 'rows': [r['raw'] for r in observed],
            'inputs': [r['input'] for r in observed], 'provenance': provenance}


def projected_cases(placement, directory=HERE):
    require(placement in (1, 2), 'section or loaded projection')
    packet = read_packet(directory)
    result = [{'name': r['profile']['name'], 'id': 65, 'metadata': r['profile']['values'][0],
               'pass': 0, 'xyz': r['xyz'], 'vertices': r['delta'] // 8,
               'words': r['words'][:r['delta']]}
              for r in packet['observed'] if r['profile']['values'][8] == placement]
    require(len(result) == 4 and [r['metadata'] for r in result] == [2, 3, 4, 5],
            'four actual captured orientation profiles')
    return result
