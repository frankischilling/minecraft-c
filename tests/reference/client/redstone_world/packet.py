"""Sealed original Redstone cohorts and joins between independent observations.

This audit packs observed NW arguments; it does not regenerate CV geometry or
use the native renderer as an oracle. Native callback controls are separate.
"""
import collections
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
MANIFEST_SHA256 = '0865d29e8088ec9dd67899aaabf52c78514ab08172a75a3836d5e4f874698301'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
COHORTS = {'flat': ('REDSTONE', 56, 'RW'), 'vertical': ('VERTICAL', 20, 'RV'),
           'capability': ('CAPABILITY', 10, 'RC'), 'signed': ('SIGNED', 8, 'RS'),
           'nullable': ('NULLABLE', 6, 'RN')}
CAPACITY = 2097152
INITIAL = ['bfd0000000000000', '3fe0000000000000', 'bfe8000000000000']
NW_KEYS = set('A B h i j k l m n o p q r s t u v w x z'.split())
NW_BOOL = set('l m n q w x'.split())
NW_DOUBLE = set('i j s t u'.split())
CV_FLOAT = set('ABCDEFGHIJKLMNOPQRSTUVWXYZ') - {'O'} | set('nopqrstuvwxyz') | {'aa'}
CV_BOOL = set('b e f m ab ac ad ae af ag ah ai aj ak al am'.split())
CV_INT = set('O d g h i j k l'.split())
BUFFER = ('java.nio.DirectByteBuffer:0:8388608:8388608,'
          'java.nio.DirectIntBufferU:0:2097152:2097152,'
          'java.nio.DirectFloatBufferU:0:2097152:2097152,int[]:2097152,NULL')
FLOORS = ((901, 922, 943, 964, 1000, 1025, 1050, 1075),
          (1105, 1126, 1147, 1168, 1204, 1229, 1254, 1279),
          (1309, 1330, 1351, 1372, 1408, 1433, 1458, 1483))
WALLS = ((1641, 1665, 1689, 1717, 1760, 1788, 1816, 1848),
         (1938, 1968, 1998, 2024, 2065, 2099, 2133, 2163),
         (2251, 2279, 2307, 2331, 2370, 2402, 2434, 2462),
         (2556, 2582, 2608, 2638, 2683, 2713, 2743, 2777))
GRAY = (975, 1179, 1383)
WALL_COLOR = ((1613, 1728), (1912, 2035), (2227, 2342), (2526, 2649))
REFERENCES = {'ref:' + x for x in ('cv observedCache observedBlock observedTess '
              'observedEmissions observedBlocks observedMaterial observedStone observedWest '
              'observedStoneMaterial observedWestMaterial observedRepeaterOff observedRepeaterOn '
              'observedUpper actual-JJ.b ln').split()}
SIGNED_REQUESTS = ((0, 0), (0, 1), (1, 0), (1, 1),
                   (-1048577, 0), (-1048577, 1), (-1048576, 0), (-1048576, 1),
                   (1048575, 0), (1048575, 1), (1048576, 0), (1048576, 1))
SIGNED_OWNER_CODE = {
    'yy.a(II)I': (37, '65d453cd12f16eebb67f57f3989d8c19384d4877168a5ed060eef661e5d374c7'),
    'ok.b(II)Llm;': (34, 'ef1ce4e47bf2aa234367946b6f54b1bf549dcbd54cb8b215e2182223b14f66ec'),
}


def require(value, label):
    if not value:
        raise ValueError(label)


def unique(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, 'duplicate JSON key: ' + key)
        out[key] = value
    return out


def loads(text):
    return json.loads(text, object_pairs_hook=unique)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def lf(raw):
    require(raw.endswith(b'\n') and b'\r' not in raw, 'canonical ASCII LF transport')
    return raw.decode('ascii').splitlines()


def integer(value):
    return type(value) is int and -2**31 <= value < 2**31


def wrap(value):
    return (value + 2**31) % 2**32 - 2**31


def number(text):
    require(type(text) is str and re.fullmatch(r'0|-?[1-9][0-9]*', text), 'canonical integer')
    value = int(text)
    require(integer(value), 'signed Java integer')
    return value


def bits(text, width):
    require(type(text) is str and re.fullmatch('[0-9a-f]{' + str(width) + '}', text),
            'raw binary' + str(width * 4))
    return text


def primitive(text):
    pairs = [part.split('=') for part in text.split(',')]
    require(all(len(pair) == 2 for pair in pairs), 'primitive field syntax')
    return unique(pairs)


def nw(state):
    require(type(state) is dict and set(state) == NW_KEYS, 'complete NW primitive keys')
    for key, value in state.items():
        require(type(value) is bool if key in NW_BOOL else
                bool(bits(value, 16)) if key in NW_DOUBLE else integer(value), 'typed NW ' + key)
    require(state['A'] == 10 and state['B'] == CAPACITY and state['r'] == 7 and
            state['w'] is True and [state[k] for k in 'stu'] == INITIAL and
            state['n'] is False and state['q'] is False and state['x'] is False and
            state['v'] == 0 and state['z'] == 0,
            'genuine retained batch/backing/translation')
    return state


def parse_nw(text):
    raw = primitive(text)
    require(set(raw) == NW_KEYS, 'complete selected NW state')
    out = {}
    for k, v in raw.items():
        if k in NW_BOOL:
            require(v in ('true', 'false'), 'selected NW boolean')
            out[k] = v == 'true'
        else:
            out[k] = bits(v, 16) if k in NW_DOUBLE else number(v)
    return nw(out)


def profiles(lines):
    out = []
    for line in lines:
        cells = line.split(' ')
        require(len(cells) == 14 and re.fullmatch('[a-z0-9-]+', cells[0]), 'profile operand roster')
        out.append({'name': cells[0], 'values': [number(x) for x in cells[1:]]})
    require(len({p['name'] for p in out}) == len(out), 'unique profile names')
    return out


def validate_signed_owners(lines):
    """Join actual request-slot/LM identity observations; do not repair aliases."""
    require(type(lines) is list and len(lines) == 1, 'one actual signed-owner receipt')
    cells = lines[0].split('\t')
    require(len(cells) == 15 and cells[:3] == ['SIGNED_OWNERS', '12', '10'],
            'closed twelve-request/ten-owner receipt')
    slots = []
    first_key = {}
    for index, cell in enumerate(cells[3:]):
        values = cell.split(',')
        require(len(values) == 6, 'closed signed-owner slot tuple')
        values = [number(v) for v in values]
        x, z = SIGNED_REQUESTS[index]
        # Original yy.a: sign and low fifteen bits on each axis. The receipt
        # observes equality, rather than assigning a new object per request.
        key = ((0x80000000 if x < 0 else 0) | ((x & 0x7fff) << 16) |
               (0x8000 if z < 0 else 0) | (z & 0x7fff))
        first = first_key.setdefault(key, index)
        require(values[:3] == [index, x, z], 'ordered actual signed requests')
        require(values[5] == first and tuple(values[3:5]) == SIGNED_REQUESTS[first],
                'actual signed owner identity/coordinates and packed-key alias')
        slots.append(values)
    require(len(first_key) == 10 and slots[10][5] == 0 and slots[11][5] == 1 and
            len({v[5] for v in slots}) == 10, 'ten actual owners, including both origin aliases')
    return {'raw': lines[0], 'slots': slots, 'requests': 12, 'unique': 10}


def validate_owner_contract(contract, pins):
    require(set(contract) == {'scope', 'lm_constructor', 'packed_key', 'cached_lookup',
                              'receipt', 'owner_aliases', 'projection'} and
            contract['scope'] == 'static field/method binding; no original Code bytes or authored owner values',
            'closed static owner-contract scope')
    constructor = contract['lm_constructor']
    constructor_hash = '60116fb183c78fe9ff90312f09df640f02475e357de4f097516ec57ad2d0c844'
    require(set(constructor) == {'descriptor', 'code_bytes', 'code_sha256', 'x', 'z'} and
            constructor['descriptor'] == '(Lfd;II)V' and type(constructor['code_bytes']) is int and
            constructor['code_bytes'] == 102 and constructor['code_sha256'] == constructor_hash,
            'actual LM coordinate constructor identity')
    for axis, field, argument, load, store in (('x', 'j', 2, 50, 51), ('z', 'k', 3, 55, 56)):
        binding = constructor[axis]
        require(binding == {'field': field, 'descriptor': 'I', 'argument_local': argument,
                             'load_pc': load, 'store_pc': store} and
                all(type(binding[k]) is int for k in ('argument_local', 'load_pc', 'store_pc')),
                'actual LM coordinate field/argument/store binding')
    methods = {cls + '.' + m['name'] + m['descriptor']: m
               for cls, value in pins['classes'].items() for m in value['methods']}
    key = 'lm.<init>' + constructor['descriptor']
    require(key in methods and methods[key].get('code_bytes') == 102 and
            methods[key].get('code_sha256') == constructor_hash,
            'original LM coordinate constructor Code pin')
    for role, owner, method, descriptor in (('packed_key', 'yy', 'a', '(II)I'),
                                           ('cached_lookup', 'ok', 'b', '(II)Llm;')):
        key = owner + '.' + method + descriptor
        size, sha = SIGNED_OWNER_CODE[key]
        require(contract[role] == {'class': owner, 'method': method, 'descriptor': descriptor,
                                   'code_bytes': size, 'code_sha256': sha} and
                type(contract[role]['code_bytes']) is int and key in methods and
                methods[key].get('code_bytes') == size and methods[key].get('code_sha256') == sha,
                'original signed owner key/GET Code pins')
    require(contract['receipt'] == 'SIGNED_OWNERS\\t12\\tobservedUniqueCount then12 TABtokens '
            'slot,requestX,requestZ,actualX,actualZ,firstEqualRequestSlot' and
            contract['owner_aliases'] == 'actual returned LM references, zero-based earliest '
            'request slot by linear ==; no coordinate consistency repair' and
            contract['projection'] == 'owners.tsv contains one actual receipt with declared LF '
            'ending; selected.txt unchanged geometry envelope28lines',
            'actual owner receipt identity/projection contract')


def coordinates(name, values):
    return values[7:10] if name == 'signed' else [8, 64, 8]


def declared_cells(name, p):
    """Authored operand cells, not regenerated renderer output."""
    x, y, z = coordinates(name, p)
    cells = {(x, y, z): (55, p[2]), (x, wrap(y-1), z): (1, 0)}
    directions = ((-1, 0), (1, 0), (0, -1), (0, 1))
    if name in ('flat', 'vertical'):
        for i, (dx, dz) in enumerate(directions):
            if p[7] & (1 << i):
                nx, nz = wrap(x+dx), wrap(z+dz)
                cells[nx, y, nz] = (55 if name == 'flat' else p[8], 0)
                if name == 'vertical':
                    cells[nx, wrap(y-1), nz] = p[9], 0
                    cells[nx, wrap(y+1), nz] = p[10], 0
        if name == 'vertical':
            cells[x, wrap(y+1), z] = p[11], 0
    elif name == 'capability':
        cells[wrap(x-1), y, z] = p[7], p[8]
    elif name == 'signed' and p[10]:
        cells[wrap(x-1), y, z] = 1, 0
        cells[wrap(x-1), wrap(y+1), z] = 55, 0
    elif name == 'nullable' and p[9]:
        cells[(wrap(x-1), y, z) if p[9] == 1 else (x, wrap(y+1), z)] = 1, 0
    return cells


def query_operands(xyz):
    x, y, z = xyz
    queries = {13: xyz, 462: [x, wrap(y+1), z], 1496: [x, wrap(y+1), z]}
    for dx, dz, side, initial, solid, lower, upper_solid, upper, wall_solid, wall_id in (
            (-1, 0, 1, 219, 235, 256, 480, 501, 1562, 1582),
            (1, 0, 3, 280, 296, 317, 520, 541, 1861, 1881),
            (0, -1, 2, 341, 357, 378, 560, 581, 2176, 2196),
            (0, 1, 0, 402, 418, 439, 600, 621, 2475, 2495)):
        nx, nz = wrap(x+dx), wrap(z+dz)
        queries[initial] = [nx, y, nz, side]
        queries[lower] = [nx, wrap(y-1), nz, -1]
        queries[upper] = [nx, wrap(y+1), nz, -1]
        for pc in (solid, upper_solid, wall_solid):
            queries[pc] = [nx, y, nz]
        queries[wall_id] = [nx, wrap(y+1), nz]
    return queries


def validate_rows(name, selected, ps):
    tag, count, _ = COHORTS[name]
    require(len(ps) == count and len(selected) == 3 * count + 4, 'closed selected row count')
    require(selected[0] == tag + '_STATIC\tNW.b=true\tNW.c=false\tUU.s.length=256\tUU.s.final=true',
            'actual NW/static-final array facts')
    observed, backing, prefix_hash, cursor, untouched = [], [], 1, 0, 0
    for index, profile in enumerate(ps):
        p = profile['values']
        inp, row, own = [s.split('\t') for s in selected[1 + 3*index:4 + 3*index]]
        require(inp[:4] == [tag + '_INPUT', str(index), 'sm', '55'], 'registered Redstone input')
        require(len(inp) >= 11 and [number(x) for x in inp[4:11]] ==
                [164, p[5], wrap(164 + p[5]), 0, 0, p[2], p[3]], 'literal bm/emission/metadata operands')
        if name == 'flat':
            require(inp[11:] == [str(p[7])], 'flat declared mask')
        elif name == 'vertical':
            require(inp[11:] == [str(x) for x in p[7:12]], 'vertical declared operands')
        elif name == 'capability':
            require(inp[11:13] == [str(x) for x in p[7:9]] and
                    inp[13:] == [{93: 'wo', 94: 'wo', 70: 'bv', 1: 'eo'}[p[7]]],
                    'actual capability receiver and nibble')
        elif name == 'signed':
            require(inp[11:15] == [str(x) for x in p[7:11]] and len(inp) == 16 and
                    number(inp[15]) == (-16777232 if p[7] < 0 else 16777200),
                    'signed coordinates, wall and actual EW minimum Chunk')
        elif name == 'nullable':
            require(inp[11:] == [str(p[7]), str(p[8]), str(p[9]), str(p[6]),
                                str(int(p[9] != 0))], 'declared nullable operands/registered snapshot owner')
        require(len(row) == 16 and row[:4] == [tag, str(index), profile['name'], str(p[0])],
                'closed literal effect row')
        require(row[4:6] == ['true', 'NONE'] if name != 'nullable' else
                row[4] in ('true', 'UNRETURNED') and row[5] in ('NONE', 'NULL', 'INDEX'),
                'literal outcome/exception kind')
        before, after, capacity = [number(x) for x in row[6:9]]
        delta = after - before
        require(before == cursor and 0 <= delta <= 480 and delta % 48 == 0 and
                capacity == CAPACITY and after < capacity, 'continuous returned packed cursor')
        require(row[9] in ('0', '1') and row[10].split(',') == INITIAL, 'Chunk flag and translation')
        require(row[11].split(',') == ['0000000000000000'] * 3 +
                ['3ff0000000000000', '3fb0000000000000', '3ff0000000000000'],
                'complete raw Block bounds')
        cv = primitive(row[12])
        require(set(cv) == CV_FLOAT | CV_BOOL | CV_INT, 'complete CV primitive keys')
        require(all(bits(cv[k], 8) == '00000000' for k in CV_FLOAT) and
                all(cv[k] == ('true' if k == 'b' else 'false') for k in CV_BOOL) and
                all(number(cv[k]) == (1 if k == 'O' else p[6] if k == 'd' else 0)
                    for k in CV_INT), 'retained declared CV fields')
        old, new = parse_nw(row[13]), parse_nw(row[14])
        require(old['o'] == before and new['o'] == after and old['h']*8 == before and
                new['h']*8 == after and new['p'] >= old['p'] and
                new['h'] - old['h'] == (new['p'] - old['p']) * 3 // 2,
                'literal NW input/expanded vertex counters')
        require(old == (observed[-1]['after'] if observed else
                        dict(old, h=0, o=0, p=0, k=0, l=False, m=False,
                             i='0000000000000000', j='0000000000000000')),
                'own prior complete NW state')
        words = [int(bits(x, 8), 16) for x in row[15].split(',')]
        require(len(words) == max(delta, 100) and all(x == 0 for x in words[delta:]),
                'own inactive tail remains untouched')
        require(len(own) == 6 and own[:3] == [tag + '_BACKING', str(index), str(capacity-delta)] and
                own[5] == BUFFER, 'actual own-prior/reference/buffer facts')
        previous_hash = wrap(prefix_hash * pow(31, capacity-len(backing), 2**32))
        for word in words[:delta]:
            prefix_hash = (31 * prefix_hash + wrap(word)) % 2**32
        backing.extend(words[:delta])
        current_hash = wrap(prefix_hash * pow(31, capacity-len(backing), 2**32))
        require([number(x) for x in own[3:5]] == [previous_hash, current_hash],
                'full backing Arrays.hashCode continuation')
        observed.append({'raw': row, 'input': inp, 'profile': profile, 'before': old, 'after': new,
                         'return': row[4], 'exception': row[5], 'words': words[:delta],
                         'emission': number(inp[8]), 'bm': number(inp[6])})
        cursor, untouched = after, untouched + capacity-delta
    require(selected[-3] == tag + '_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=' +
            ('6' if name == 'signed' else '3') and
            selected[-2] == tag + '_GUARD controls network=2 native=1 denied-writes=1 execs=1',
            'original-only successful denial receipts')
    require(selected[-1] == tag + '_COMPLETE rows=' + str(count) + ' own-unchanged-words=' +
            str(untouched) + ' genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0',
            'complete bounded original cohort')
    return observed


def methods_from(pins, live, prefix):
    require(set(pins) == {'client_jar_sha256', 'classes'} and pins['client_jar_sha256'] == JAR,
            'original archive authority')
    methods = {}
    for owner, cls in pins['classes'].items():
        require(set(cls) == {'class_bytes', 'class_sha256', 'major', 'minor', 'methods'} and
                type(cls['class_bytes']) is int and cls['class_bytes'] > 0 and
                type(cls['major']) is int and cls['major'] == 49 and
                type(cls['minor']) is int and cls['minor'] == 0 and bool(bits(cls['class_sha256'], 64)),
                'complete original Class identity')
        for m in cls['methods']:
            base = {'name', 'descriptor', 'access'}
            require(type(m['access']) is int and
                    (set(m) == base | {'code_bytes', 'code_sha256'} or
                     set(m) == base and m['access'] & (0x100 | 0x400)),
                    'closed original method pin')
            if 'code_bytes' in m:
                require(type(m['code_bytes']) is int and 0 < m['code_bytes'] < 65536 and
                        bits(m['code_sha256'], 64), 'typed complete method Code pin')
            key = owner + '.' + m['name'] + m['descriptor']
            require(key not in methods, 'unique pinned method')
            methods[key] = m
    captured = set()
    for line in live:
        cells = line.split('\t')
        require(len(cells) == 6, 'closed live Code row')
        tag, owner, method, desc, size, sha = cells
        key = owner + '.' + method + desc
        require(tag == prefix + '_LIVE_CODE' and key not in captured and key in methods and
                'code_bytes' in methods[key] and type(methods[key]['code_bytes']) is int and
                methods[key]['code_bytes'] == number(size) and
                methods[key]['code_sha256'] == bits(sha, 64), 'live method Code joins archive pin')
        captured.add(key)
    require(methods['cv.g(Luu;III)Z']['code_sha256'] ==
            'cd319d16871692c2f789b5887ffa05e978f3839f3a8dcee6fa12138c3f177fd2',
            'unchanged actual CV.g Code')
    return methods, captured


def location(text, methods, live):
    key, pc = text.rsplit('@', 1)
    require(key in live and pc.isdecimal() and 0 <= int(pc) < methods[key]['code_bytes'],
            'reached pinned live Code location')
    return key, int(pc)


def fvalue(text):
    return struct.unpack('>f', bytes.fromhex(bits(text, 8)))[0]


def dvalue(text):
    return struct.unpack('>d', bytes.fromhex(bits(text, 16)))[0]


def fword(value):
    try:
        return int.from_bytes(struct.pack('>f', value), 'big')
    except OverflowError:
        return 0xff800000 if value < 0 else 0x7f800000


def color_word(args):
    channels = []
    for arg in args:
        value = fvalue(arg)
        multiplied = struct.unpack('>f', fword(value * 255.0).to_bytes(4, 'big'))[0]
        channel = (0 if math.isnan(multiplied) else -2**31 if multiplied <= -2**31 else
                   2**31-1 if multiplied >= 2**31-1 else int(multiplied))
        channels.append(max(0, min(255, channel)))
    return wrap(0xff000000 | channels[0] | channels[1] << 8 | channels[2] << 16)


def source_tokens(floor, walls):
    result = [('c', 146)] + [('v', p) for p in FLOORS[floor][:4]] + [('c', GRAY[floor])]
    result += [('v', p) for p in FLOORS[floor][4:]]
    for wall in walls:
        result += [('c', WALL_COLOR[wall][0])] + [('v', p) for p in WALLS[wall][:4]]
        result += [('c', WALL_COLOR[wall][1])] + [('v', p) for p in WALLS[wall][4:]]
    return result


def validate_events(name, events, observed, pins, live):
    methods, captured = methods_from(pins, live, COHORTS[name][2])
    entered, stack, used, profiles_seen, exceptions = {}, [], set(), set(), set()
    next_call, last_profile, state = 0, -1, None
    packed, tokens, vertices = [], collections.defaultdict(list), collections.defaultdict(list)
    brightness, colors, roots, reads = {}, {}, collections.Counter(), collections.Counter()
    class_state = 0
    for seq, event in enumerate(events):
        require(type(event['seq']) is int and event['seq'] == seq and
                type(event['profile']) is int and 0 <= event['profile'] < len(observed),
                'contiguous typed event sequence/profile')
        index = event['profile']
        require(index >= last_profile, 'ordered independent profile journal')
        if index != last_profile:
            require(not stack, 'no pending method crosses profile boundary')
            if last_profile >= 0:
                require(state == observed[last_profile]['after'], 'last retained profile state')
            state, last_profile = observed[index]['before'], index
        row = observed[index]
        kind = event['event']
        if kind == 'class-state':
            require(set(event) == {'seq', 'profile', 'event', 'class', 'loaded', 'initialized'} and
                    name in ('capability', 'signed', 'nullable') and seq == 0 and event['class'] == 'jj' and
                    event['loaded'] is False and event['initialized'] is None,
                    'observed cold JJ before its first real active use')
            class_state += 1
            continue
        key, pc = location(event['at'], methods, captured)
        current = nw(event['nw'])
        profiles_seen.add(index)
        if kind == 'entry':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'caller', 'receiver', 'args', 'nw'} and
                    type(event['call']) is int and event['call'] == next_call and pc == 0 and
                    current == state and type(event['args']) is list, 'actual ordered method entry/state')
            next_call += 1
            used.add(key)
            if stack:
                require(event['caller'].rsplit('@', 1)[0] == entered[stack[-1]][1],
                        'actual caller/entry stack join')
            else:
                require(key == 'cv.g(Luu;III)Z' and event['receiver'] == 'ref:cv' and
                        current == row['before'] and
                        re.fullmatch('Redstone[A-Za-z]*Observation.main\\(\\[Ljava/lang/String;\\)V@[0-9]+',
                                     event['caller']), 'actual authored direct root call')
                roots[index] += 1
            args, receiver = event['args'], event['receiver']
            require(receiver is None or receiver in REFERENCES, 'classified genuine receiver identity')
            xyz = coordinates(name, row['profile']['values'])
            if key == 'cv.g(Luu;III)Z':
                require(args == [None if name == 'nullable' and row['profile']['values'][8]
                                 else 'ref:observedBlock'] + xyz, 'actual root operands')
            elif key.startswith('ew.'):
                require(receiver == 'ref:observedCache' and len(args) in (3, 4) and
                        all(integer(v) for v in args[:3]), 'actual EW/query integer operands')
                parent = entered[stack[-1]][0]
                if parent['at'] == 'cv.g(Luu;III)Z@0':
                    caller_pc = int(event['caller'].rsplit('@', 1)[1])
                    require(caller_pc in query_operands(xyz) and
                            args == query_operands(xyz)[caller_pc], 'fresh CV query operands/PC')
                if parent['at'].startswith('sm.e('):
                    require(args[:3] == parent['args'][1:4], 'same captured SM XP/coordinates')
                if key == 'ew.a(IIIZ)I':
                    require(len(args) == 4 and args[3] is True, 'actual neighbor-light boolean')
                if key == 'ew.a(IIII)F':
                    require(len(args) == 4 and integer(args[3]) and args[3] == row['emission'],
                            'actual captured emission minimum')
            elif key == 'uu.d(Lxp;III)F':
                require(receiver == 'ref:observedBlock' and args == ['ref:observedCache'] + xyz,
                        'captured genuine brightness XP')
            elif key == 'sm.a(II)I':
                require(receiver == 'ref:observedBlock' and args == [1, row['profile']['values'][2]],
                        'metadata texture precedes override')
            elif key == 'sm.e(Lxp;IIII)Z':
                require(receiver is None and len(args) == 5 and args[0] == 'ref:observedCache' and
                        all(integer(v) for v in args[1:]), 'actual static connectivity XP')
                caller_pc = int(event['caller'].rsplit('@', 1)[1])
                require(caller_pc in query_operands(xyz) and
                        args[1:] == query_operands(xyz)[caller_pc], 'captured connectivity operands/PC')
            elif key.startswith('nw.'):
                require(receiver == 'ref:observedTess', 'same captured genuine NW singleton')
                if key == 'nw.a(FFF)V':
                    require(len(args) == 3 and all(bits(v, 8) for v in args), 'raw source colors')
                    caller_pc = int(event['caller'].rsplit('@', 1)[1])
                    tokens[index].append(('c', caller_pc))
                    if caller_pc == 146:
                        colors[index] = args
                    elif caller_pc in GRAY or caller_pc in [x[1] for x in WALL_COLOR]:
                        require(args == [brightness[index]] * 3, 'captured brightness gray operands')
                    else:
                        require(args == colors[index], 'captured power colors repeated at upright')
                else:
                    arity = {'nw.a(DDDDD)V': 5, 'nw.a(DD)V': 2, 'nw.a(DDD)V': 3}[key]
                    require(len(args) == arity and all(bits(v, 16) for v in args), 'raw NW operands')
                    if key == 'nw.a(DDDDD)V':
                        require(event['caller'].startswith('cv.g(Luu;III)Z@'), 'actual CV vertex caller')
                        tokens[index].append(('v', int(event['caller'].rsplit('@', 1)[1])))
                        vertices[index].append(args)
                    else:
                        parent = entered[stack[-1]][0]
                        require(parent['at'] == 'nw.a(DDDDD)V@0' and args ==
                                (parent['args'][3:] if arity == 2 else parent['args'][:3]),
                                'nested observed UV/position forwarding')
            elif key.endswith('()Z') or key.endswith('()V'):
                require(args == [], 'actual zero-argument child arity')
            entered[event['call']] = event, key
            stack.append(event['call'])
        elif kind == 'return':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'result', 'nw'} and
                    type(event['call']) is int and stack and event['call'] == stack[-1],
                    'properly nested return')
            entry, method = entered[stack.pop()]
            require(key == method, 'returned actual method identity')
            if key in ('cv.g(Luu;III)Z', 'uu.d(Lxp;III)F', 'sm.a(II)I',
                       'nw.a(FFF)V', 'nw.a(DDDDD)V', 'nw.a(DD)V', 'nw.a(DDD)V'):
                require(pc == methods[key]['code_bytes'] - 1, 'actual child return instruction')
            expected = dict(state)
            if key == 'nw.a(FFF)V':
                expected.update(k=color_word(entry['args']), l=True)
            elif key == 'nw.a(DD)V':
                expected.update(i=entry['args'][0], j=entry['args'][1], m=True)
            elif key == 'nw.a(DDD)V':
                require(state['m'] and state['l'] and not state['n'], 'observed packed vertex flags')
                vertex = [fword(dvalue(a) + dvalue(state[k])) for a, k in zip(entry['args'], 'stu')]
                vertex += [fword(dvalue(state['i'])), fword(dvalue(state['j'])), state['k'] % 2**32, 0, 0]
                added = 1
                if state['p'] % 4 == 3:
                    packed += packed[-24:-16] + packed[-8:]
                    added = 3
                packed += vertex
                expected.update(o=state['o'] + 8*added, h=state['h'] + added, p=state['p'] + 1)
            else:
                require(current == state, 'callee preserves current NW prefix')
            require(current == expected, 'complete observed NW effect transition')
            state = current
            result = event['result']
            if key.startswith('nw.') or key.endswith('()V'):
                require(result == 'void', 'observed void return')
            elif key == 'cv.g(Luu;III)Z':
                require(not stack and result is True and row['return'] == 'true' and
                        row['exception'] == 'NONE' and current == row['after'], 'actual root return/state')
            elif key.endswith('F'):
                bits(result, 8)
                if key == 'ew.a(IIII)F':
                    brightness[index] = result
                if key == 'uu.d(Lxp;III)F':
                    require(result == brightness[index], 'brightness result forwarding')
            elif key.endswith('Z'):
                require(type(result) is bool, 'typed source boolean result')
            else:
                require(integer(result), 'typed source integer result')
                if key == 'sm.a(II)I':
                    require(result == row['bm'], 'actual live texture result')
                if key == 'ew.e(III)I' and entry['caller'] == 'cv.g(Luu;III)Z@13':
                    require(result == row['profile']['values'][2], 'actual center nibble result')
                if key in ('ew.a(III)I', 'ew.e(III)I'):
                    cell = declared_cells(name, row['profile']['values']).get(tuple(entry['args']), (0, 0))
                    require(result == cell[1 if key == 'ew.e(III)I' else 0],
                            'observed query joins declared saved-cell operands')
        elif kind == 'field-read':
            require(set(event) == {'seq', 'profile', 'event', 'at', 'field', 'value', 'element', 'nw'} and
                    stack and key == entered[stack[-1]][1] and current == state,
                    'passive static read joins actual caller state')
            field, value, element = event['field'], event['value'], event['element']
            allowed = {'nw.a': ('cv.g(Luu;III)Z@0',), 'uu.s': ('uu.d(Lxp;III)F@5',),
                       'uu.m': ('ew.h(III)Z@0', 'sm.e(Lxp;IIII)Z@31'),
                       'uu.aw': ('sm.e(Lxp;IIII)Z@13',) + tuple('cv.g(Luu;III)Z@' + str(p)
                                                               for p in (1587, 1886, 2201, 2500)),
                       'uu.bi': ('sm.e(Lxp;IIII)Z@47',), 'uu.bj': ('sm.e(Lxp;IIII)Z@58',),
                       'jj.b': ('sm.e(Lxp;IIII)Z@80',)}
            require(field in allowed and event['at'] in allowed[field] and
                    (value is None or value in REFERENCES), 'reached static identity/source PC')
            expected_ref = {'nw.a': 'ref:observedTess', 'uu.s': 'ref:observedEmissions',
                            'uu.m': 'ref:observedBlocks', 'uu.aw': 'ref:observedBlock',
                            'jj.b': 'ref:actual-JJ.b',
                            'uu.bi': 'ref:observedWest' if name == 'capability' and
                                     row['profile']['values'][7] == 93 else 'ref:observedRepeaterOff',
                            'uu.bj': 'ref:observedWest' if name == 'capability' and
                                     row['profile']['values'][7] == 94 else 'ref:observedRepeaterOn'}
            require(value == expected_ref[field], 'genuine static occurrence identity')
            require(element == row['emission'] and integer(element) if field == 'uu.s' and value is not None
                    else element == [2, 3, 0, 1] if field == 'jj.b' else element is None,
                    'captured static-array element observation')
            reads[index] += field == 'nw.a'
        elif kind == 'exception':
            require(set(event) == {'seq', 'profile', 'event', 'at', 'type', 'catch', 'frames', 'nw'} and
                    name == 'nullable' and index not in exceptions and stack and current == state and
                    current == row['after'] and row['return'] == 'UNRETURNED', 'actual retained exception prefix')
            expected_frames = [event['at']] + [entered[x][0]['caller'] for x in reversed(stack)
                                               if entered[x][0]['caller'].rsplit('@', 1)[0] in captured]
            require(event['frames'] == expected_frames and
                    event['type'] == {'NULL': 'java.lang.NullPointerException',
                                      'INDEX': 'java.lang.ArrayIndexOutOfBoundsException'}[row['exception']] and
                    event['catch'].startswith('RedstoneNullableObservation.main([Ljava/lang/String;)V@'),
                    'actual throw/catch/frame identity')
            p = row['profile']['values']
            actual_seam = (['cv.g(Luu;III)Z@13'] if p[7] else
                           ['cv.g(Luu;III)Z@24'] if p[8] else
                           ['ew.h(III)Z@25', 'cv.g(Luu;III)Z@' + ('235' if p[9] == 1 else '462')])
            require(event['frames'] == actual_seam, 'declared nullable source throw prefix')
            exceptions.add(index)
            stack.clear()
        else:
            require(False, 'unknown journal event')
    require(not stack and profiles_seen == set(range(len(observed))) and
            dict(roots) == {i: 1 for i in range(len(observed))} and used == captured and
            state == observed[-1]['after'], 'complete live method/profile/stack coverage')
    require(class_state == (1 if name in ('capability', 'signed', 'nullable') else 0),
            'closed class-state observations')
    require(packed == [x for row in observed for x in row['words']], 'observed NW append words join literal effects')
    for index, row in enumerate(observed):
        require(reads[index] == 1, 'one actual NW capture per body')
        successful = row['exception'] == 'NONE'
        require(successful == (index not in exceptions), 'literal/event exception join')
        legal = [source_tokens(floor, [i for i in range(4) if mask & (1 << i)])
                 for floor, mask in itertools.product(range(3), range(16))]
        require(any(tokens[index] == p if successful else tokens[index] == p[:len(tokens[index])]
                    for p in legal), 'source color/56 vertex-PC group ordering')
        for offset in range(0, len(vertices[index]) - 7, 8):
            base, overlay = vertices[index][offset:offset+4], vertices[index][offset+4:offset+8]
            require(all(a[:4] == b[:4] and struct.pack('>d', dvalue(a[4]) + .0625).hex() == b[4]
                        for a, b in zip(base, overlay)), 'observed overlay operands preserve positions/U')
    return {'events': len(events), 'live_methods': len(captured), 'source_exceptions': len(exceptions),
            'queries': sum(e['event'] == 'entry' and e['at'].startswith('ew.') for e in events),
            'field_reads': sum(e['event'] == 'field-read' for e in events)}


def load_cohort(directory):
    directory = Path(directory)
    data = {'selected': lf((directory / 'selected.txt').read_bytes()),
            'profiles': lf((directory / 'profiles.txt').read_bytes()),
            'events': [loads(x) for x in lf((directory / 'events.jsonl').read_bytes())],
            'pins': loads((directory / 'source-pins.json').read_text(encoding='utf-8')),
            'live': lf((directory / 'live-code.tsv').read_bytes()),
            'declared': loads((directory / 'declared-inputs.json').read_text(encoding='utf-8')),
            'stdout': (directory / 'original.stdout').read_bytes()}
    if directory.name == 'signed':
        data['owners'] = lf((directory / 'owners.tsv').read_bytes())
        data['owner_contract'] = loads((directory / 'owner-contract.json').read_text(encoding='utf-8'))
    return data


def validate_cohort(name, data):
    require(name in COHORTS, 'known independent cohort')
    tag, count, _ = COHORTS[name]
    declaration = data['declared']
    require(type(declaration['schema']) is int and declaration['schema'] == 1 and
            type(declaration['profiles']) is int and declaration['profiles'] == count and
            len(declaration['integer_fields']) == 13 and
            len(set(declaration['integer_fields'])) == 13, 'typed declared input count/columns')
    ps = profiles(data['profiles'])
    require(all(p['values'][:2] == [0, 55] and p['values'][4] == -2**31 for p in ps),
            'direct genuine Redstone/preserved emission profiles')
    for profile in ps:
        p = profile['values']
        require(0 <= p[2] <= 15 and 0 <= p[3] <= 2, 'authored nibble/light input')
        if name == 'flat':
            require(0 <= p[7] < 16 and p[8:] == [0] * 5, 'flat mask/reserved fields')
        else:
            require((p[2] in (0, 7, 15) if name == 'vertical' else p[2] == 7) and
                    p[3] == 0 and p[5] == 0, 'later-cohort declared center/light/texture')
            if name == 'vertical':
                require(p[6] == -1 and p[7] in (1, 2, 4, 8, 15) and p[8] in (0, 1) and
                        p[9] in (0, 55) and p[10] in (0, 55, 75) and p[11] in (0, 1) and p[12] == 0,
                        'vertical declared cell operands')
            elif name == 'capability':
                require(p[6] == -1 and p[7] in (93, 94, 70, 1) and 0 <= p[8] <= 3 and
                        p[9:] == [0] * 4, 'capability declared cell operands')
            elif name == 'signed':
                require(p[6] == -1 and p[7] in (-16777217, -16777216, 16777215, 16777216) and
                        p[8:10] == [64, 8] and p[10] in (0, 1) and p[11:] == [0, 0],
                        'signed saved operand context')
            else:
                require(p[6] in (-1, 0) and p[7] in (0, 1) and p[8] in (0, 1) and
                        p[9] in (0, 1, 2) and p[10:] == [0] * 3,
                        'nullable actual reference/material operands')
    owner = validate_signed_owners(data['owners']) if name == 'signed' else None
    if name == 'signed':
        validate_owner_contract(data['owner_contract'], data['pins'])
    require(data['stdout'] == ('16 achievements\r\n151 recipes\r\n' +
            (owner['raw'] + '\r\n' if owner else '') +
            '\r\n'.join(data['selected']) + '\r\n').encode('ascii'), 'full exact raw stdout projection')
    observed = validate_rows(name, data['selected'], ps)
    counts = validate_events(name, data['events'], observed, data['pins'], data['live'])
    return {'rows': [r['raw'] for r in observed], 'inputs': [r['input'] for r in observed],
            'profiles': ps, 'observed': observed, 'events': data['events'], 'pins': data['pins'],
            'live': data['live'], 'counts': counts, 'tag': tag, 'owners': owner}


def validate_provenance(name, provenance, data, observed, files):
    require(provenance['client_jar_sha256'] == JAR and provenance['original_inputs_unchanged'] is True and
            provenance['original_helpers_unchanged'] is True and
            type(provenance['source_writes_from_jdi']) is int and provenance['source_writes_from_jdi'] == 0,
            'unchanged original execution and passive JDI scope')
    for key, value in {'format': 1, 'profiles': COHORTS[name][1], 'ordinary_fresh_vms': 2,
                       'read_only_jdi_fresh_vms': 1, **observed['counts'],
                       'ordinary_target_timeout_seconds': 90, 'jdi_internal_timeout_seconds': 150,
                       'jdi_outer_timeout_seconds': 180, 'jdi_event_limit': 20000,
                       'jdi_journal_byte_limit': 16777216}.items():
        require(type(provenance[key]) is int and provenance[key] == value, 'actual provenance ' + key)
    raw_hash = hashlib.sha256(data['stdout']).hexdigest()
    require(provenance['original_stdout_sha256'] == [raw_hash] * 3,
            'three complete identical original streams')
    captured = {'selected.txt', 'events.jsonl', 'live-code.tsv', 'original.stdout'}
    if name == 'signed':
        captured.add('owners.tsv')
    require(provenance['captured_files_sha256'] == {p: files[p] for p in captured} and
            provenance['frozen_authored_files_sha256'] ==
            {p: sha for p, sha in files.items() if p not in captured | {'provenance.json'}},
            'exact authored and captured provenance inventories')
    require(len(provenance['captured_inputs_sha256']) >= 100 and
            all(bits(v, 64) for v in provenance['captured_inputs_sha256'].values()),
            'captured JDK/archive/helper/dependency closure fingerprints')
    java = {Path(p).stem for p in files if p.endswith('.java')}
    observation = next(p for p in java if p.endswith('Observation'))
    require(set(provenance['compiled_authored_helpers_sha256']) ==
            {p + '.class' for p in java} | {observation + '$1.class'} and
            all(bits(v, 64) for v in provenance['compiled_authored_helpers_sha256'].values()) and
            '17.0.17' in provenance['java_version'] and '17.0.17' in provenance['javac_version'],
            'actual authored helper/JDK fingerprints')
    c = observed['counts']
    completion = (COHORTS[name][2] + '_JDI_COMPLETE direct-g=' + str(COHORTS[name][1]) +
                  ' events=' + str(c['events']) + ' entries=' +
                  str(sum(e['event'] == 'entry' for e in data['events'])) +
                  ' queries=' + str(c['queries']) + ' reads=' + str(c['field_reads']) +
                  ' exceptions=' + str(c['source_exceptions']) + ' target-exit=0 source-writes=0')
    require(provenance['jdi_completion'] == completion, 'complete raw JDI journal accounting')
    require(len(provenance['runs']) == 3, 'three independent retained VM receipts')
    for index, run in enumerate(provenance['runs']):
        capture = run['capture']
        require(type(capture['exit']) is int and capture['exit'] == 0 and
                capture['timed_out'] is False and capture['pipe_capture_complete'] is True and
                capture['reader_errors'] == {} and capture['output_overflow'] is False and
                all(type(capture[k]) is int for k in ('timeout_seconds', 'stdout_bytes',
                                                      'stderr_bytes', 'stdout_limit', 'stderr_limit')) and
                capture['timeout_seconds'] == (90 if index < 2 else 180) and
                0 < capture['stdout_bytes'] < capture['stdout_limit'] == 4194304 and
                0 <= capture['stderr_bytes'] < capture['stderr_limit'] == 1048576 and
                bits(capture['stdout_sha256'], 64) and bits(capture['stderr_sha256'], 64),
                'actual completed bounded VM/pipe receipt')
        if index < 2:
            require(capture['stdout_sha256'] == raw_hash and capture['stdout_bytes'] == len(data['stdout']),
                    'ordinary full stdout byte/identity join')
        before, after = run['world_before_sha256'], run['world_after_sha256']
        require(set(before) == set(after) and 'session.lock' in before and
                all(bits(v, 64) for v in list(before.values()) + list(after.values())) and
                all(before[p] == after[p] for p in before if p != 'session.lock'),
                'retained saved inputs; only actual session lock may change')


def read_packet(directory=HERE):
    directory = Path(directory)
    require(digest(directory / 'manifest.json') == MANIFEST_SHA256, 'sealed packet manifest')
    manifest = loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    require(set(manifest) == {'format', 'cohorts'} and type(manifest['format']) is int and
            manifest['format'] == 1 and set(manifest['cohorts']) == set(COHORTS), 'closed five-cohort manifest')
    require({p.name for p in directory.iterdir() if p.is_dir()} == set(COHORTS) and
            {p.name for p in directory.iterdir() if p.is_file()} <=
            {'manifest.json', 'packet.py', 'test_packet.py', 'README.md', '.gitattributes'},
            'closed public packet root roster')
    out = {}
    for name, (tag, count, _) in COHORTS.items():
        declared = manifest['cohorts'][name]
        require(set(declared) == {'tag', 'profiles', 'files_sha256'} and declared['tag'] == tag and
                type(declared['profiles']) is int and declared['profiles'] == count,
                'typed manifest cohort identity/count')
        files = declared['files_sha256']
        common = {'generate_profiles.py', 'capture_reference.py', 'declared-inputs.json',
                  'source-pins.json', 'profiles.txt', 'selected.txt', 'events.jsonl',
                  'live-code.tsv', 'original.stdout', 'provenance.json'}
        prefix = 'Redstone' + ('' if name == 'flat' else name.title())
        java = {prefix + suffix for suffix in ('Observation.java', 'Jdi.java')}
        require(set(files) == common | java | ({'world_inputs.py', 'owners.tsv', 'owner-contract.json'}
                                              if name == 'signed' else set()),
                'closed captured/helper file roster')
        require({p.name for p in (directory / name).iterdir()} == set(files), 'no unlisted cohort files')
        for basename, sha in files.items():
            require(Path(basename).name == basename and bits(sha, 64) and
                    digest(directory / name / basename) == sha, 'immutable cohort file: ' + name + '/' + basename)
        data = load_cohort(directory / name)
        out[name] = validate_cohort(name, data)
        provenance = loads((directory / name / 'provenance.json').read_text(encoding='utf-8'))
        validate_provenance(name, provenance, data, out[name], files)
    return {'cohorts': out, 'manifest': manifest}


if __name__ == '__main__':
    result = read_packet()
    print('PASS: original Redstone packet; ' + str(sum(len(x['rows']) for x in result['cohorts'].values())) +
          ' rows in five separate cohorts')
