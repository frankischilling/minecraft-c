"""Exact original Torch rows and read-only query/static-read/vertex evidence."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
FROZEN = {
    'TorchObservation.java': '043e3a59a64af8ac5f23f02b8a9b90107115efbd8c66ac138da7a2d6a5efc66e',
    'TorchJdi.java': 'e772bcd40a18dc77d860a5ee36f33521641d4a805e41ff79e988d7602f43d049',
    'profiles.txt': 'f2dcf11fbbae280abd9e241979bc5f6eb5421e274cf807c93f8d59abbaf94271',
    'declared-inputs.json': '2418a2baeeb025961994fead24d1a42dec78b0f6927ba1dbbe5d1f387b3fd9fb',
    'source-pins.json': '6cfb5b55fa3dace9dd2d983b587c5b06ce3e8aae45c7b7e5dffe211a049cce68',
    'capture_reference.py': 'f821982a626fc1e9cba960f58b48e2c040d6b329c6037fd048a02705c039546b',
}
CAPTURED = {
    'selected.txt': 'dd69b166409edacfe5ed44906ca8317a9d12f264d9be1018ecbbcfb73592a921',
    'events.jsonl': 'fb5d5ac8b80fde6dd1ce17924d22b23155a8feb2b229d5e927c78d92b7fc92ab',
    'live-code.tsv': '51b9a39036e2657d4b2e90124d5a3b8e22c663ec9fd72fd3bd594a4c8ecde3c7',
}
FILES = tuple(FROZEN) + tuple(CAPTURED)
INITIAL = ['bfd0000000000000', '3fe0000000000000', 'bfe8000000000000']
NW_KEYS = set('A B h i j k l m n o p q r s t u v w x z'.split())
FAILURES = {60: 'cv.c(Luu;III)Z@8', 61: 'cv.c(Luu;III)Z@29', 62: 'cv.c(Luu;III)Z@8',
            63: 'cv.a(Luu;DDDDD)V@7', 64: 'cv.a(Luu;DDDDD)V@7'}
VERTEX_PCS = (206, 245, 284, 323, 344, 371, 398, 419, 440, 467, 494, 515,
              537, 565, 593, 615, 637, 665, 693, 715)
ENTRY_COUNTS = {'cv.c(Luu;III)Z': 60, 'cv.a(Luu;DDDDD)V': 63, 'uu.d(Lxp;III)F': 57,
                'uu.a(I)I': 61, 'ew.e(III)I': 58, 'ew.a(IIII)F': 57, 'ew.d(III)I': 57,
                'ew.a(IIIZ)I': 57, 'ew.a(III)I': 57, 'nw.a(FFF)V': 57,
                'nw.a(DDDDD)V': 1220, 'nw.a(DD)V': 1220, 'nw.a(DDD)V': 1220}


def require(value, label):
    if not value:
        raise ValueError(label)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = {}
    for name, value in pairs:
        require(name not in result, 'duplicate JSON field: ' + name)
        result[name] = value
    return result


def load(path):
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique)


def lines(path):
    raw = path.read_bytes()
    require(raw.endswith(b'\n') and b'\r' not in raw, 'selected ASCII LF transport')
    return raw.decode('ascii').splitlines()


def primitive(text):
    result = {}
    for pair in text.split(','):
        name, value = pair.split('=', 1)
        require(name not in result, 'unique primitive fields')
        result[name] = value
    return result


def f32(word):
    return struct.unpack('>f', bytes.fromhex(word))[0]


def dword(value):
    return struct.pack('>d', value).hex()


def arguments_for_geometry(profile, direct):
    p = profile['values']
    a, b = f32('3ecccccd'), f32('3e4ccccd')
    if direct:
        return [dword(v) for v in ((8, 64, 8, 0, 0), (-.5, -.5, -.5, 0, 0),
                                  (-0.0, -0.0, -0.0, -0.0, 0.0), (1.25, -2.5, 3.75, a, -a))[p[12]]]
    h = .5 - a
    values = {1: (8-h, 64+b, 8, -a, 0), 2: (8+h, 64+b, 8, a, 0),
              3: (8, 64+b, 8-h, 0, -a), 4: (8, 64+b, 8+h, 0, a)}.get(p[2], (8, 64, 8, 0, 0))
    return [dword(v) for v in values]


def raw_state(state):
    require(set(state) == NW_KEYS, 'complete NW primitive state')
    require(type(state['B']) is int and state['B'] == 2097152 and state['w'] is True and state['r'] == 7,
            'genuine constructor backing and entered batch')
    for name in ('i', 'j', 's', 't', 'u'):
        require(type(state[name]) is str and re.fullmatch('[0-9a-f]{16}', state[name]), 'raw binary64 state')
    for name in ('l', 'm', 'n', 'q', 'w', 'x'):
        require(type(state[name]) is bool, 'typed NW boolean')
    for name in NW_KEYS - set('i j s t u l m n q w x'.split()):
        require(type(state[name]) is int, 'typed NW integer')
    return {k: str(v).lower() if isinstance(v, bool) else str(v) for k, v in state.items()}


def validate_rows(selected, profiles):
    require(len(selected) == 202 and selected[0] == 'TORCH_STATIC\tNW.b=true\tNW.c=false\tUU.s.length=256\tUU.s.final=true', 'real constructor/static array facts and complete rows')
    observed, end, untouched_total = [], 0, 0
    boxes = ((0, 0, 0, 1, 1, 1), (.125, .25, .375, .875, .75, .625), (-.25, -.5, -.125, 1.5, 1.25, 1.75))
    buffer_state = None
    for index, profile in enumerate(profiles):
        p = profile['values']
        cls, bm, original_emission = {50: ('vm', 80, 14), 75: ('db', 115, 0), 76: ('db', 99, 7)}[p[1]]
        entered_bm = ((bm + p[5] + 2**31) % 2**32) - 2**31
        emission = original_emission if p[4] == -2**31 else p[4]
        inp, row, backing = [line.split('\t') for line in selected[1+3*index:4+3*index]]
        require(inp == ['TORCH_INPUT', str(index), cls, str(p[1]), str(bm), str(p[5]), str(entered_bm),
                        str(original_emission), str(emission), str(p[2]), str(p[3])], 'actual registered Block/emission/bm/metadata inputs')
        require(len(row) == 16 and row[:4] == ['TORCH', str(index), profile['name'], str(p[0])], 'closed sixteen-field source row')
        failed = index in FAILURES
        result = 'UNRETURNED' if failed else 'VOID' if p[0] else 'true'
        require(row[4:6] == [result, 'NULL' if failed else 'NONE'], 'actual return kind and nullable reachability')
        before, after, capacity = map(int, row[6:9])
        delta = 0 if failed else 240
        require(before == end and after-before == delta and capacity == 2097152, 'uninterrupted batch and exact five-quad words')
        require(row[9] == ('1' if not failed and not p[0] and p[3] == 1 else '0') and row[10].split(',') == INITIAL,
                'actual Chunk light flag and retained translation')
        require(row[11].split(',') == [dword(v) for v in boxes[p[8]]], 'retained actual Block bounds')
        cv = primitive(row[12])
        require(cv['m'] == ('true' if p[11] else 'false') and cv['f'] == ('true' if index & 1 else 'false') and
                cv['d'] == str(p[6]) and cv['e'] == ('true' if p[10] else 'false') and
                all(cv[name] == str(p[9]) for name in 'ghijkl'), 'retained actual CV fields')
        old, new = primitive(row[13]), primitive(row[14])
        require(set(old) == set(new) == NW_KEYS and old['o'] == str(before) and new['o'] == str(after), 'complete source NW before/after fields')
        require(int(new['h'])-int(old['h']) == (0 if failed else 30) and
                int(new['p'])-int(old['p']) == (0 if failed else 20) and (not failed or old == new), 'source packed/input vertex counts and exact failed prefix')
        words = row[15].split(',')
        require(len(words) == max(delta, 100) and all(re.fullmatch('[0-9a-f]{8}', word) for word in words), 'exact active and own inactive raw words')
        require(len(backing) == 6 and backing[:3] == ['TORCH_BACKING', str(index), str(capacity-delta)] and
                all(re.fullmatch('-?[0-9]+', word) and -2**31 <= int(word) < 2**31 for word in backing[3:5]) and
                (not failed or backing[3] == backing[4]), 'full own-prior backing checks/hash shape')
        if buffer_state is None:
            buffer_state = backing[5]
        require(backing[5] == buffer_state, 'actual unchanged NIO positions/limits/capacities')
        observed.append({'raw': '\t'.join(row), 'input': '\t'.join(inp), 'backing': '\t'.join(backing),
                         'before': old, 'after': new, 'return': None if failed else 'void' if p[0] else True,
                         'profile': profile, 'emission': emission, 'bm': entered_bm})
        end = after
        untouched_total += capacity-delta
    require(end == 14640 and untouched_total == 138397392 and selected[-3:] == [
        'TORCH_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=3',
        'TORCH_GUARD controls network=2 native=1 denied-writes=1 execs=1',
        'TORCH_COMPLETE rows=66 own-unchanged-words=138397392 genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0'], 'complete original calls, backing audit and denial guards')
    return observed


def validate_events(events, observed, pins, live):
    methods = {owner+'.'+m['name']+m['descriptor']: m for owner, cls in pins['classes'].items() for m in cls['methods']}
    live_methods = set()
    for line in live:
        tag, owner, name, desc, length, code_hash = line.split('\t')
        key = owner+'.'+name+desc
        require(tag == 'TW_LIVE_CODE' and key not in live_methods and key in methods and
                methods[key]['code_bytes'] == int(length) and methods[key]['code_sha256'] == code_hash, 'actual live Code matches original archive')
        live_methods.add(key)
    require(live_methods == set(ENTRY_COUNTS), 'complete thirteen live methods')
    stack, entered, failures, profiles = [], {}, set(), set()
    counts, reads, vertices = collections.Counter(), collections.Counter(), collections.Counter()
    brightness, field_order = {}, collections.defaultdict(list)
    next_call, last_profile = 0, -1
    for seq, event in enumerate(events):
        require(type(event['seq']) is int and event['seq'] == seq and type(event['profile']) is int and 0 <= event['profile'] < 66, 'contiguous typed source events')
        profile = event['profile']
        require(profile >= last_profile, 'ordered source profiles')
        if profile != last_profile:
            require(not stack, 'no incomplete call crosses a profile')
            last_profile = profile
        profiles.add(profile)
        key, pc = event['at'].rsplit('@', 1)
        require(key in live_methods and pc.isdecimal() and int(pc) < methods[key]['code_bytes'], 'actual live source location')
        current = raw_state(event['nw'])
        kind = event['event']
        if kind == 'entry':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'caller', 'receiver', 'args', 'nw'} and
                    type(event['call']) is int and event['call'] == next_call and pc == '0', 'typed ordered actual entry')
            next_call += 1
            counts[key] += 1
            p = observed[profile]['profile']['values']
            block = None if p[7] in (2, 4) else 'ref:observedBlock'
            if not stack:
                require(key == ('cv.a(Luu;DDDDD)V' if p[0] else 'cv.c(Luu;III)Z') and current == observed[profile]['before'], 'actual root method and pre-call prefix')
            if key.startswith('cv.'):
                require(event['receiver'] == 'ref:cv' and event['args'] == ([block]+arguments_for_geometry(observed[profile]['profile'], bool(p[0])) if key.startswith('cv.a') else [block, 8, 64, 8]), 'actual wrapper orientation/direct geometry arguments')
            elif key.startswith('ew.'):
                require(event['receiver'] == 'ref:observedCache' and event['args'][:3] == [8, 64, 8] and all(type(v) is int for v in event['args'][:3]), 'genuine EW receiver and exact coordinates')
                if key == 'ew.a(IIIZ)I':
                    require(event['args'] == [8, 64, 8, True] and type(event['args'][3]) is bool, 'typed EW neighbor argument')
                elif key == 'ew.a(IIII)F':
                    require(event['args'] == [8, 64, 8, observed[profile]['emission']] and type(event['args'][3]) is int, 'actual live minimum-light operand')
                else:
                    require(len(event['args']) == 3, 'exact EW query arity')
            elif key == 'uu.a(I)I':
                require(event['receiver'] == 'ref:observedBlock' and event['args'] == [0], 'actual texture getter before override')
            elif key == 'uu.d(Lxp;III)F':
                require(event['receiver'] == 'ref:observedBlock' and event['args'] == ['ref:observedCache', 8, 64, 8], 'actual Block brightness receiver/arguments')
            else:
                require(event['receiver'] == 'ref:observedTess', 'actual original singleton NW receiver')
                if key == 'nw.a(FFF)V':
                    expected = '3f800000' if observed[profile]['emission'] > 0 else brightness[profile]
                    require(event['args'] == [expected]*3 and field_order[profile][-1] == 'cv.c(Luu;III)Z@34', 'post-brightness fresh emission capture and gray color')
                else:
                    arity = {'nw.a(DDDDD)V': 5, 'nw.a(DD)V': 2, 'nw.a(DDD)V': 3}[key]
                    require(len(event['args']) == arity and all(type(v) is str and re.fullmatch('[0-9a-f]{16}', v) for v in event['args']), 'exact raw NW geometry arguments')
                    if key == 'nw.a(DDDDD)V':
                        require(event['caller'] == 'cv.a(Luu;DDDDD)V@'+str(VERTEX_PCS[vertices[profile] % 20]), 'ordered five-quad source vertex PCs')
                        vertices[profile] += 1
                    else:
                        parent, parent_key = entered[stack[-1]]
                        require(parent_key == 'nw.a(DDDDD)V' and event['args'] == (parent['args'][3:] if key == 'nw.a(DD)V' else parent['args'][:3]), 'actual nested UV/vertex operands')
            entered[event['call']] = (event, key)
            stack.append(event['call'])
        elif kind == 'return':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'result', 'nw'} and stack and stack[-1] == event['call'], 'properly nested actual return')
            entry, method = entered[stack.pop()]
            require(key == method, 'entered/returned method identity')
            if key.startswith(('ew.', 'uu.')):
                require(current == raw_state(entry['nw']), 'queries/getters preserve actual NW prefix')
                result = event['result']
                require(type(result) is (str if key.endswith('F') else int), 'typed actual query result')
                if key.endswith('F'):
                    require(re.fullmatch('[0-9a-f]{8}', result), 'actual binary32 brightness')
                if key == 'ew.e(III)I':
                    require(result == observed[profile]['profile']['values'][2], 'actual nibble metadata result')
                elif key == 'ew.a(III)I':
                    require(result == observed[profile]['profile']['values'][1], 'actual registered center Block result')
                elif key == 'uu.a(I)I':
                    require(result == observed[profile]['bm'], 'actual live bm before override')
                elif key == 'uu.d(Lxp;III)F':
                    require(result == brightness[profile], 'actual Block/EW brightness result forwarding')
                elif key == 'ew.a(IIII)F':
                    brightness[profile] = result
            elif key == 'nw.a(DD)V':
                after = dict(entry['nw'], i=entry['args'][0], j=entry['args'][1], m=True)
                require(event['result'] == 'void' and event['nw'] == after, 'exact UV stores and retained fields')
            elif key == 'nw.a(DDD)V':
                before = entry['nw']
                added = 3 if before['p'] % 4 == 3 else 1
                require(event['result'] == 'void' and event['nw']['o'] == before['o']+8*added and event['nw']['p'] == before['p']+1 and event['nw']['h'] == before['h']+added, 'actual quad expansion/index stores')
            elif key.startswith('cv.') and not stack:
                require(event['result'] == observed[profile]['return'] and current == observed[profile]['after'], 'actual outer return and full retained state')
            else:
                require(event['result'] == 'void', 'actual void return')
        elif kind == 'field-read':
            require(set(event) == {'seq', 'profile', 'event', 'at', 'field', 'value', 'element', 'nw'} and stack, 'typed reached static field capture')
            allowed = {('nw.a', 'cv.c(Luu;III)Z@15'), ('nw.a', 'cv.a(Luu;DDDDD)V@0'),
                       ('uu.s', 'uu.d(Lxp;III)F@5'), ('uu.s', 'cv.c(Luu;III)Z@34')}
            pair = event['field'], event['at']
            require(pair in allowed, 'exact fresh static-read source PC')
            if event['field'] == 'uu.s':
                require(event['value'] == 'ref:observedEmissions' and type(event['element']) is int and event['element'] == observed[profile]['emission'], 'same genuine static-final array and live element')
                if event['at'].startswith('cv.c'):
                    require(profile in brightness, 'fresh wrapper capture occurs after brightness return')
            else:
                require(event['value'] == 'ref:observedTess' and event['element'] is None, 'actual NW singleton capture')
            reads[pair] += 1
            field_order[profile].append(event['at'])
        elif kind == 'exception':
            require(set(event) == {'seq', 'profile', 'event', 'at', 'type', 'catch', 'frames', 'nw'} and profile in FAILURES and profile not in failures and stack,
                    'one reached real nullable exception')
            require(event['at'] == FAILURES[profile] and event['frames'] == [FAILURES[profile]] and event['type'] == 'java.lang.NullPointerException' and
                    event['catch'] == 'TorchObservation.main([Ljava/lang/String;)V@1498' and current == observed[profile]['after'], 'actual NPE throw/catch PCs and retained prefix')
            failures.add(profile)
            stack.clear()
        else:
            require(False, 'unknown source event')
    require(not stack and profiles == set(range(66)) and failures == set(FAILURES) and len(events) == 8723 and next_call == 4244 and counts == ENTRY_COUNTS and
            reads == {('nw.a', 'cv.c(Luu;III)Z@15'): 58, ('nw.a', 'cv.a(Luu;DDDDD)V@0'): 63,
                      ('uu.s', 'uu.d(Lxp;III)F@5'): 57, ('uu.s', 'cv.c(Luu;III)Z@34'): 57} and
            dict(vertices) == {i: 20 for i in range(66) if i not in FAILURES}, 'complete bounded query/static/UV/vertex/exception coverage')


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(provenance['client_jar_sha256'] == JAR and provenance['original_inputs_unchanged'] is True and
            provenance['original_helpers_unchanged'] is True and type(provenance['source_writes_from_jdi']) is int and
            provenance['source_writes_from_jdi'] == 0, 'unchanged original authority and read-only observation')
    for key, expected in (('format', 1), ('profiles', 66), ('ordinary_fresh_vms', 2), ('read_only_jdi_fresh_vms', 1),
                          ('events', 8723), ('query_entries', 286), ('static_reads', 235), ('shared_calls', 63),
                          ('nw_vertex_calls', 1220), ('source_exceptions', 5), ('live_methods', 13), ('input_count', 112)):
        require(type(provenance[key]) is int and provenance[key] == expected, 'exact '+key)
    require(provenance['frozen_authored_files_sha256'] == FROZEN and provenance['files_sha256'] == {**FROZEN, **CAPTURED}, 'exact frozen source/product inventories')
    for name, value in provenance['files_sha256'].items():
        require(digest(directory / name) == value, 'changed captured source/product: '+name)
    require(len(provenance['captured_inputs_sha256']) == 112 and
            all(re.fullmatch('[0-9a-f]{64}', value) for value in provenance['captured_inputs_sha256'].values()), 'complete source/JDK/archive/library fingerprints')
    require('17.0.17' in provenance['jdk_version'] and
            set(provenance['compiled_helpers_sha256']) == {'TorchObservation.class', 'TorchObservation$1.class', 'TorchJdi.class'} and
            all(re.fullmatch('[0-9a-f]{64}', value) for value in provenance['compiled_helpers_sha256'].values()) and
            provenance['original_stdout_sha256'] == ['45c5fb95c6e675aea9e167e960c111ae8c9c4ccccb53ef200f0369c1e8b7fd78']*3,
            'matching helper/JDK and three complete original streams')
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR and set(pins['classes']) == set('cv ew fd lm nw uu vm db ln mx ge xp'.split()), 'complete original class/Code identities')
    profiles = []
    for line in lines(directory / 'profiles.txt'):
        cells = line.split(' ')
        require(len(cells) == 14 and re.fullmatch('[a-z0-9-]+', cells[0]), 'name plus thirteen integer operands')
        values = [int(v) for v in cells[1:]]
        require(all(-2**31 <= value < 2**31 for value in values), 'Java int profile operands')
        profiles.append({'name': cells[0], 'values': values})
    require(len(profiles) == 66 and len({p['name'] for p in profiles}) == 66, 'complete unique source profiles')
    observed = validate_rows(lines(directory / 'selected.txt'), profiles)
    events = [json.loads(line, object_pairs_hook=unique) for line in lines(directory / 'events.jsonl')]
    live = lines(directory / 'live-code.tsv')
    validate_events(events, observed, pins, live)
    return {'rows': [r['raw'] for r in observed], 'inputs': [r['input'] for r in observed], 'profiles': profiles,
            'observed': observed, 'events': events, 'pins': pins, 'live': live, 'provenance': provenance}
