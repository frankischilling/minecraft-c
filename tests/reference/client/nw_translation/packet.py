"""Audit raw unchanged-original NW additive calls and their actual ordered stores."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CODE = '1d89a8ffc32bdf34b2e28b4f093b470679fab3cf26acdc79a00e96a3f95ec702'
FILES = ('NwOffsetObservation.java', 'NwOffsetJdi.java', 'profiles.tsv',
         'declared-inputs.json', 'source-pins.json', 'observations.tsv',
         'original-stdout.txt', 'stores.tsv')
PROFILE_NAMES = ('zero', 'cactus-offsets', 'negative-zero', 'positive-zero',
                 'double-not-float-add', 'double-subnormal-start', 'float-max-cancellation',
                 'binary64-ulp-ties', 'double-max-and-float', 'infinite-cancellation',
                 'double-nan-payloads', 'float-nan-widening', 'float-subnormal',
                 'subnormal-cancel', 'float-rounding-boundary', 'repeated-addition',
                 'cactus-large-coordinates', 'near-infinite-nan')


def require(value, message):
    if not value:
        raise ValueError(message)


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


def triple(text, width):
    parts = text.split(',')
    require(len(parts) == 3 and all(re.fullmatch('[0-9a-f]{' + str(width) + '}', part)
                                   for part in parts), 'raw triple words')
    return parts


def count(value, expected, name):
    require(type(value) is int and value == expected, 'exact ' + name)


def read_packet(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(provenance['client_jar_sha256'] == JAR and
            provenance['original_inputs_unchanged'] is True and
            provenance['original_helpers_unchanged'] is True, 'original authority and continuity')
    for key, expected in (('format', 1), ('profiles', 36), ('rows', 106),
                          ('ordinary_fresh_vms', 2), ('read_only_jdi_fresh_vms', 1),
                          ('ordered_stores', 318)):
        count(provenance[key], expected, key)
    require(set(provenance['files_sha256']) == set(FILES), 'complete captured file inventory')
    for name in FILES:
        require(digest(directory / name) == provenance['files_sha256'][name],
                'changed captured input: ' + name)
    raw = (directory / 'original-stdout.txt').read_bytes()
    raw_hash = hashlib.sha256(raw).hexdigest()
    require(provenance['original_stdout_sha256'] == [raw_hash, raw_hash] and
            provenance['jdi_target_stdout_sha256'] == raw_hash, 'three identical raw original streams')
    guards = ['NW_GUARD source network=0 native=0 writes=0 execs=0 rows=106',
              'NW_GUARD controls network=2 native=1 writes=1 execs=1']
    original = raw.decode('ascii').splitlines()
    require(provenance['source_guards'] == guards and
            [line for line in original if line.startswith('NW_GUARD ')] == guards and
            original[-1] == 'NW_COMPLETE rows=106 profiles=36 canonical=1 raw-allocations=0 reflection-writes=0',
            'complete original calls and genuine denial controls')
    require(set(provenance['compiled_helpers_sha256']) ==
            {'NwOffsetObservation.class', 'NwOffsetObservation$1.class', 'NwOffsetJdi.class'} and
            all(re.fullmatch('[0-9a-f]{64}', value)
                for value in provenance['compiled_helpers_sha256'].values()), 'authored helper identities')
    require('17.0.17' in provenance['jdk_version'] and
            {'bin/java.exe', 'bin/javac.exe', 'lib/modules', 'bin/server/jvm.dll'} <=
            set(provenance['jdk_files_sha256']) and
            all(re.fullmatch('[0-9a-f]{64}', value)
                for value in provenance['jdk_files_sha256'].values()), 'recorded observation JDK')
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR and set(pins['classes']) == {'nw', 'ge'},
            'complete source class identities')
    for name, class_hash, method_count in (
            ('nw', '2facc45feb91d0fea0c5888ac6793a87ffa6e4cdf5b0cc4e418c6c162ca8bf33', 19),
            ('ge', '4c302c844a8fae626f6ab56de47dc5b1032131e176e1fac7754869c472b57722', 9)):
        detail = pins['classes'][name]
        require(detail['class_sha256'] == class_hash and len(detail['methods']) == method_count and
                len({(m['name'], m['descriptor']) for m in detail['methods']}) == method_count,
                'complete class/method inventory: ' + name)
    required = [
        ('nw', '<init>', '(I)V', 154, '88e02069d2d57cf3e471ad4dca8627ffa4f23c51c0f2eef14e07fcd3f89f5b7b'),
        ('nw', '<clinit>', '()V', 21, 'f5e5ce584dcd7ec9eb419b0baa06c98fef4666440ffaa05687812c153a20f09e'),
        ('nw', 'b', '(DDD)V', 17, '1ba599c032d6d8dffff459b25ee01aa5238ee10c88058f2abd62f5f1347370e7'),
        ('nw', 'b', '()V', 7, 'd7add7c81bcc22173876373c4aac713d8813d179d45410481d5e5332262981e3'),
        ('nw', 'a', '()V', 415, '6b22ea3c837c5afb9713209d9dea8c5bde822ffa4e6cef179f8c5e87be1b1218'),
        ('nw', 'c', '(FFF)V', 34, CODE),
        ('ge', 'c', '(I)Ljava/nio/ByteBuffer;', 13, '8b14925f1c50ec7f4a49d4779a4a97685543ccaebda31791e0e730b4e69a2f06')]
    for owner, method, descriptor, length, code_hash in required:
        found = [m for m in pins['classes'][owner]['methods']
                 if m['name'] == method and m['descriptor'] == descriptor]
        require(len(found) == 1 and found[0]['code_sha256'] == code_hash and
                type(found[0]['code_bytes']) is int and found[0]['code_bytes'] == length,
                'required exact source Code identity')
    require(provenance['live_code'] == 'NW_LIVE_CODE\tnw\tc\t(FFF)V\t34\t' + CODE,
            'executed original method Code identity')
    declared = load(directory / 'declared-inputs.json')
    count(declared['profiles'], 36, 'declared profiles')
    count(declared['calls'], 106, 'declared calls')
    profiles = []
    for index, line in enumerate(lines(directory / 'profiles.tsv')):
        cells = line.split('\t')
        require(index < 36 and len(cells) == 7 and cells[:3] ==
                [str(index), PROFILE_NAMES[index % 18], str(index // 18)], 'closed profile order/modes')
        for word in cells[3:6]:
            require(re.fullmatch('[0-9a-f]{16}', word), 'initial raw binary64 word')
        calls = cells[6].split(';')
        for call in calls:
            triple(call, 8)
        profiles.append({'index': index, 'name': cells[1], 'building': cells[2],
                         'initial': cells[3:6], 'calls': calls})
    require(len(profiles) == 36 and sum(len(p['calls']) for p in profiles) == 106,
            'complete call inventory')
    for index in range(18):
        require(profiles[index]['initial'] == profiles[index + 18]['initial'] and
                profiles[index]['calls'] == profiles[index + 18]['calls'], 'matching real batch modes')
    rows = lines(directory / 'observations.tsv')
    require(len(rows) == 106 and
            rows == [line for line in original if line.startswith('NW_OFFSET\t')],
            'raw original row selection')
    cursor = 0
    for profile in profiles:
        before = profile['initial']
        for step, delta in enumerate(profile['calls']):
            cells = rows[cursor].split('\t')
            require(len(cells) == 10 and cells[:6] ==
                    ['NW_OFFSET', str(cursor), str(profile['index']), profile['name'],
                     profile['building'], str(step)] and cells[7] == delta,
                    'exact original call identity/arguments')
            require(triple(cells[6], 16) == before, 'actual source setup/previous returned state')
            before = triple(cells[8], 16)
            state_pairs = [pair.split('=', 1) for pair in cells[9].split(',')]
            require(all(len(pair) == 2 for pair in state_pairs) and
                    len({pair[0] for pair in state_pairs}) == len(state_pairs), 'unique unrelated source fields')
            state = dict(state_pairs)
            require(set(state) == set('A B d e f g h i j k l m n o p q r v w x y z'.split()) and
                    state['w'] == ('true' if profile['building'] == '1' else 'false') and
                    state['r'] == ('7' if profile['building'] == '1' else '0') and
                    state['g'] == '2097152/-1140850687' and state['y'] == 'null',
                    'actual CPU constructor/batch/source backing state')
            cursor += 1
    stores = [line.split('\t') for line in lines(directory / 'stores.tsv')]
    require(len(stores) == 318, 'complete actual source stores')
    for index, row in enumerate(rows):
        cells = row.split('\t')
        before, after = triple(cells[6], 16), triple(cells[8], 16)
        for axis, (field, pc) in enumerate((('s', '8'), ('t', '19'), ('u', '30'))):
            store = stores[index * 3 + axis]
            require(len(store) == 9 and store[:6] ==
                    ['NW_STORE', str(index), cells[2], cells[5], pc, field] and
                    triple(store[6], 16) == after[:axis] + before[axis:] and
                    store[7] == before[axis] and store[8] == after[axis],
                    'actual ordered store and preserved current prefix')
    return {'rows': rows, 'profiles': profiles, 'stores': stores, 'provenance': provenance}
