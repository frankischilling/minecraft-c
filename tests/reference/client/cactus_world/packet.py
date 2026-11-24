"""Validate exact original Cactus observations and read-only query/store evidence."""
import hashlib
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
FROZEN = {
    'CactusObservation.java': '3ff1bf8aaef33555943200e172ef980b517b79f9220e11145450dcd52af1c5d6',
    'CactusJdi.java': '880cbe2c4b8c7e47545a75418e306aaa375d4ed0e7040f244ec7be6745b0eba2',
    'profiles.txt': '7b494f48c3a72622c2ee901ab1fc8ce0ff6ce8ead3fb2c605a34c9dce5bd4e66',
    'declared-inputs.json': '57891d7d3a73dac053afcb6ec47a3b1f1e1092eebc824d6330408f1c2d1fb525',
    'source-pins.json': 'f9608b5fb6e1bc7ec4fb62064d62197916bef48f42e152f0cdef46d99ba9587a',
    'capture_reference.py': '20c93b8f3990a59639e762cc0bfcf34b08baa29e5a4c98785284b63a3ab2d1a9',
}
CAPTURED = {
    'selected.txt': 'b3f753d6f2114210b400c6368101f3db8abdda8aa18710a779329a1c6713b927',
    'events.jsonl': '60601023f3cbdae325208e00113620c7fb13dbaa50489e82893715158ab42a2d',
    'live-code.tsv': 'aba0987dddf814bcdb93bd56562e45d038c0723f0475cd76ee0b35b27de6de93',
}
FILES = tuple(FROZEN) + tuple(CAPTURED)
INITIAL = ['bfd0000000000000', '3fe0000000000000', 'bfe8000000000000']
NW_KEYS = set('A B h i j k l m n o p q r s t u v w x z'.split())
FAILURES = {7: ('uu.d(Lxp;III)F@13', ['uu.d(Lxp;III)F@13', 'cv.c(Luu;IIIFFF)Z@120']),
            8: ('cv.c(Luu;IIIFFF)Z@120', ['cv.c(Luu;IIIFFF)Z@120']),
            9: ('cv.c(Luu;IIIFFF)Z@268', ['cv.c(Luu;IIIFFF)Z@268']),
            13: ('uu.d(Lxp;III)F@13', ['uu.d(Lxp;III)F@13', 'cv.c(Luu;IIIFFF)Z@120', 'cv.m(Luu;III)Z@140']),
            14: ('cv.m(Luu;III)Z@9', ['cv.m(Luu;III)Z@9'])}


def require(value, label):
    if not value:
        raise ValueError(label)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    out = {}
    for name, value in pairs:
        require(name not in out, 'duplicate JSON field: ' + name)
        out[name] = value
    return out


def load(path):
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique)


def lines(path):
    raw = path.read_bytes()
    require(raw.endswith(b'\n') and b'\r' not in raw, 'selected ASCII LF transport')
    return raw.decode('ascii').splitlines()


def primitive(text):
    result = {}
    for pair in text.split(','):
        key, value = pair.split('=', 1)
        require(key not in result, 'unique primitive fields')
        result[key] = value
    return result


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
    return {name: str(value).lower() if isinstance(value, bool) else str(value) for name, value in state.items()}


def rows(selected, profiles, declared):
    require(len(selected) == 53, 'complete selected source inventory')
    result = []
    end = 0
    for index, profile in enumerate(profiles):
        inp = selected[index * 2].split('\t')
        p = profile['values']
        bm = ((70 + p[12] + 2**31) % 2**32) - 2**31
        require(inp == ['CACTUS_INPUT', str(index), '70', str(p[12]), str(bm)], 'actual late bm authoring')
        row = selected[index * 2 + 1].split('\t')
        require(len(row) == 16 and row[:4] == ['CACTUS', str(index), profile['name'], str(p[0])], 'closed CACTUS row schema/identity')
        fail = index in FAILURES
        require(row[4:6] == (['-1', 'NULL'] if fail else ['0' if index == 1 else '1', 'NONE']), 'exact source result/nullable reachability')
        begin, after, capacity = map(int, row[6:9])
        expected = 0 if index in (1, 7, 8, 13, 14) else 48 if index == 9 else 144 if index in (2, 22) else 288
        require(begin == end and after - begin == expected and capacity == 2097152, 'exact uninterrupted batch and emitted word count')
        require(row[9] == ('0' if expected == 0 else '1') and row[10].split(',') == INITIAL, 'actual Chunk light flag and retained translation')
        box = []
        for word in declared['bounds_float32'][p[3]]:
            value = struct.unpack('>f', bytes.fromhex(word))[0]
            box.append(struct.pack('>d', value).hex())
        require(row[11].split(',') == box, 'actual float-widened authored bounds')
        cv = primitive(row[12])
        require(cv['m'] == ('true' if p[4] else 'false') and cv['f'] == ('true' if p[2] else 'false') and
                cv['d'] == str(p[8]) and cv['e'] == ('true' if p[7] else 'false'), 'retained actual CV source fields')
        before, after_state = primitive(row[13]), primitive(row[14])
        require(set(before) == set(after_state) == NW_KEYS and before['o'] == str(begin) and after_state['o'] == str(after), 'complete raw NW before/after state')
        words = row[15].split(',')
        require(len(words) == max(expected, 100) and all(re.fullmatch('[0-9a-f]{8}', word) for word in words), 'actual active/backing-tail words')
        result.append({'raw': '\t'.join(row), 'input': '\t'.join(inp), 'before': before, 'after': after_state,
                       'return': None if fail else index != 1, 'profile': profile})
        end = after
    require(end == 5232, 'exact final word index')
    require(selected[-3:] == ['CACTUS_GUARD source network=0 native=0 denied-writes=0 execs=0 world-writes=3',
                            'CACTUS_GUARD controls network=2 native=1 denied-writes=1 execs=1',
                            'CACTUS_COMPLETE rows=25 malformed=0 draw=0 raw-allocations=0 source-replacements=0'], 'active source/denial controls and full completion')
    return result


def validate_events(events, observed, pins, live):
    """Semantic controls independent of the packet's byte-identity binding."""
    methods = {}
    for owner, detail in pins['classes'].items():
        for method in detail['methods']:
            methods[owner + '.' + method['name'] + method['descriptor']] = method
    live_methods = set()
    for line in live:
        tag, owner, name, descriptor, length, code_hash = line.split('\t')
        key = owner + '.' + name + descriptor
        require(tag == 'CW_LIVE_CODE' and key not in live_methods and key in methods and
                methods[key]['code_bytes'] == int(length) and methods[key]['code_sha256'] == code_hash, 'actual live method Code matches archive')
        live_methods.add(key)
    require(len(live_methods) == 10, 'complete live method set')
    stack, entered, exceptions, profile_seen = [], {}, set(), set()
    next_call = query_count = store_count = 0
    last_profile = -1
    for seq, event in enumerate(events):
        require(type(event['seq']) is int and event['seq'] == seq and type(event['profile']) is int and 0 <= event['profile'] < 25, 'contiguous typed event sequence')
        profile = event['profile']
        require(profile >= last_profile, 'ordered complete source profiles')
        if profile != last_profile:
            require(not stack, 'no unfinished call crosses a source profile')
            last_profile = profile
        profile_seen.add(profile)
        key, pc_text = event['at'].rsplit('@', 1)
        require(key in live_methods and pc_text.isdecimal() and 0 <= int(pc_text) < methods[key]['code_bytes'], 'event actual live Code location')
        current = raw_state(event['nw'])
        kind = event['event']
        if kind == 'entry':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'caller', 'receiver', 'args', 'nw'} and
                    type(event['call']) is int and event['call'] == next_call and pc_text == '0', 'typed ordered source entry')
            next_call += 1
            if not stack:
                require(key == ('cv.m(Luu;III)Z' if observed[profile]['profile']['values'][0] else 'cv.c(Luu;IIIFFF)Z') and
                        current == observed[profile]['before'], 'actual outer method and entry prefix')
            if key.startswith('ew.'):
                require(event['receiver'] == 'ref:observedCache' and len(event['args']) in (3, 4) and
                        all(type(v) is int for v in event['args'][:3]), 'genuine EW receiver and typed query arguments')
                if key == 'ew.a(IIIZ)I':
                    require(type(event['args'][3]) is bool, 'actual EW boolean argument')
                elif len(event['args']) == 4:
                    require(type(event['args'][3]) is int, 'actual EW integer argument')
                require(event['args'][0] in (7, 8, 9) and event['args'][1] in (63, 64, 65) and event['args'][2] in (7, 8, 9), 'bounded authored query coordinates')
                caller, caller_pc = event['caller'].rsplit('@', 1)
                require(caller in methods and caller_pc.isdecimal() and int(caller_pc) < methods[caller]['code_bytes'], 'actual source query caller PC')
                parent, parent_key = entered[stack[-1]]
                if parent_key.startswith('ew.'):
                    require(event['args'][:3] == parent['args'][:3] and caller == parent_key, 'nested EW query preserves actual coordinates/caller')
                if key == 'ew.a(IIII)F':
                    require(event['caller'] == 'uu.d(Lxp;III)F@13' and event['args'][3] == 0, 'actual UU brightness query')
                elif key == 'ew.e(III)I':
                    require(event['caller'] == 'uu.a(Lxp;IIII)I@8' and event['args'] == [8, 64, 8], 'actual World texture metadata query')
                elif key == 'ew.g(III)Z':
                    require(event['caller'] == 'uu.b(Lxp;IIII)Z@106', 'actual side visibility query')
                query_count += 1
            elif key == 'nw.c(FFF)V':
                require(event['receiver'] == 'ref:observedTess' and len(event['args']) == 3 and
                        all(word in ('00000000', '3d800000', 'bd800000') for word in event['args']), 'actual Cactus float offset arguments')
                event = dict(event, store_prefix=0, state_after=dict(event['nw']))
            else:
                require(event['receiver'] == 'ref:cv', 'actual CV receiver')
            entered[event['call']] = (event, key)
            stack.append(event['call'])
        elif kind == 'return':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'result', 'nw'} and stack and stack[-1] == event['call'], 'properly nested actual return')
            entry, method = entered[stack.pop()]
            require(key == method, 'return matches entered method')
            if key.startswith('ew.'):
                require(current == raw_state(entry['nw']), 'EW queries preserve actual NW prefix')
                result = event['result']
                require(type(result) is (str if key.endswith('F') else bool if key.endswith('Z') else int), 'typed EW result')
                if key.endswith('F'):
                    require(re.fullmatch('[0-9a-f]{8}', result), 'actual binary32 brightness result')
                x, y, z = entry['args'][:3]
                pattern = observed[profile]['profile']['values'][1]
                block_id = 81 if [x, y, z] == [8, 64, 8] else 0 if pattern == 0 else 1 if pattern == 1 else int((x + 2*y + 4*z) & 2 != 0)
                if key == 'ew.a(III)I':
                    require(result == block_id, 'actual authored block query result')
                elif key == 'ew.e(III)I':
                    require(result == (x+y+z) & 3, 'actual authored metadata query result')
                elif key == 'ew.g(III)Z':
                    require(result is (block_id == 1), 'actual opaque-neighbor query result')
            elif key == 'nw.c(FFF)V':
                require(entry['store_prefix'] == 3 and event['result'] == 'void' and event['nw'] == entry['state_after'], 'three actual stores before additive return')
            elif not stack:
                require(event['result'] is observed[profile]['return'] and current == observed[profile]['after'], 'ordinary outer return and exact retained NW state')
        elif kind == 'store':
            require(set(event) == {'seq', 'profile', 'event', 'call', 'at', 'field', 'old', 'new', 'nw'} and stack and stack[-1] == event['call'], 'store in actual active offset call')
            entry, method = entered[event['call']]
            axis = entry['store_prefix']
            require(method == 'nw.c(FFF)V' and axis < 3 and (event['field'], int(pc_text)) == (('s', 8), ('t', 19), ('u', 30))[axis], 'actual s8/t19/u30 store order')
            field = event['field']
            require(event['nw'] == entry['state_after'] and event['old'] == entry['state_after'][field], 'actual current pre-store prefix')
            old = struct.unpack('>d', bytes.fromhex(event['old']))[0]
            delta = struct.unpack('>f', bytes.fromhex(entry['args'][axis]))[0]
            require(event['new'] == struct.pack('>d', old + delta).hex(), 'float widening then binary64 addition')
            entry['state_after'][field] = event['new']
            entry['store_prefix'] += 1
            store_count += 1
        elif kind == 'exception':
            require(set(event) == {'seq', 'profile', 'event', 'at', 'type', 'catch', 'frames', 'nw'} and profile in FAILURES and profile not in exceptions and stack,
                    'one reached nullable source exception')
            expected_at, frames = FAILURES[profile]
            require(event['at'] == expected_at and event['frames'] == frames and event['type'] == 'java.lang.NullPointerException' and
                    event['catch'] == 'CactusObservation.main([Ljava/lang/String;)V@1206' and current == observed[profile]['after'], 'exact source throw/catch PCs and retained prefix')
            exceptions.add(profile)
            stack.clear()
        else:
            require(False, 'unknown source event')
    require(not stack and profile_seen == set(range(25)) and exceptions == set(FAILURES) and
            (len(events), next_call, query_count, store_count) == (2133, 845, 669, 444), 'complete bounded query/store/exception cohort')


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(provenance['client_jar_sha256'] == JAR and provenance['original_inputs_unchanged'] is True and
            provenance['original_helpers_unchanged'] is True and provenance['source_writes_from_jdi'] == 0, 'unchanged original authority and read-only observer')
    for key, expected in (('format', 1), ('profiles', 25), ('ordinary_fresh_vms', 2), ('read_only_jdi_fresh_vms', 1),
                          ('input_count', 109), ('events', 2133), ('query_entries', 669), ('nw_additive_calls', 148),
                          ('nw_stores', 444), ('source_exceptions', 5), ('live_methods', 10)):
        require(type(provenance[key]) is int and provenance[key] == expected, 'exact ' + key)
    require(provenance['frozen_authored_files_sha256'] == FROZEN and provenance['files_sha256'] == {**FROZEN, **CAPTURED}, 'exact frozen source and captured product inventories')
    for name, pin in provenance['files_sha256'].items():
        require(digest(directory / name) == pin, 'changed captured source/product: ' + name)
    require(len(provenance['captured_inputs_sha256']) == 109 and all(re.fullmatch('[0-9a-f]{64}', value) for value in provenance['captured_inputs_sha256'].values()), 'complete recorded JDK/archive/library/source hashes')
    require(len(provenance['compiled_helpers_sha256']) == 3 and '17.0.17' in provenance['jdk_version'] and
            provenance['original_stdout_sha256'] == ['bcb85b3be658b260b89b0e5abbc5cd40609a08401ed14881bccb52a617d7ce34'] * 3, 'matching JDK/helper and three identical complete original streams')
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR and set(pins['classes']) == set('cv ew fd lm nw uu or ln px mx'.split()), 'complete original class/Code inventory')
    declared = load(directory / 'declared-inputs.json')
    profiles = []
    for line in lines(directory / 'profiles.txt'):
        fields = line.split(' ')
        require(len(fields) == 14 and re.fullmatch('[a-z0-9-]+', fields[0]), 'named profile plus thirteen integer inputs')
        values = [int(v) for v in fields[1:]]
        require(all(-2**31 <= v < 2**31 for v in values) and values[11] == 0, 'bounded genuine-backing profile inputs')
        profiles.append({'name': fields[0], 'values': values})
    require(len(profiles) == 25 and len({p['name'] for p in profiles}) == 25, 'closed unique profile inventory')
    observed = rows(lines(directory / 'selected.txt'), profiles, declared)
    events = [json.loads(line, object_pairs_hook=unique) for line in lines(directory / 'events.jsonl')]
    live = lines(directory / 'live-code.tsv')
    validate_events(events, observed, pins, live)
    return {'rows': [row['raw'] for row in observed], 'inputs': [row['input'] for row in observed],
            'profiles': profiles, 'observed': observed, 'events': events, 'pins': pins, 'live': live, 'provenance': provenance}
