#!/usr/bin/env python3
"""Project a finite Item base-state slice from an untouched-original JDI capture.

No C source, native output, original JAR or installed JDK is read. Original
INITIALIZING and native FAILED states are explicitly outside the projection.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

NAMES = ('d e f g h i j k l m n o p q r s t u v w x y z '
         'A B C D E F G H I J K L M N O P Q R S T U V W X Y Z '
         'aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az '
         'aA aB aC aD aE aF aG aH aI aJ aK aL aM aN aO aP aQ aR aS aT aU aV aW aX aY aZ '
         'ba bb bc bd be').split()
BLOCK_NAMES = ('u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z '
               'aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az '
               'aA aB aC aD aE aF aG aH aI aJ aK aL aM aN aO aP aQ aR aS aT aU aV aW aX aY aZ '
               'ba bb bc bd be bf bg bh bi bj bk bl').split()
BASE = [('bf', 'I', '17'), ('bg', 'I', '4'), ('a', 'I', '2'), ('bh', 'I', '4'),
        ('bi', 'Z', '4'), ('bj', 'Z', '4'), ('bk', None, '2'),
        ('bl', 'Ljava/lang/String;', '2')]
LENGTHS = {'ITEM_REGISTRY_PREFIX_V1': 2, 'POLICY': 5, 'METHOD_BINDING': 8,
           'OUTER_BLOCK_METHOD_BINDING': 8, 'LOADER_ROOT': 3, 'TYPE': 8,
           'FIELD_DECL': 6, 'METHOD_DECL': 6, 'HELPER_BINDING': 7,
           'ITEM_ARRAY_ROOT': 2, 'STATIC': 7, 'ITEM_ALIAS': 6, 'BLOCK_ALIAS': 5,
           'PRIVATE_RANDOM': 4, 'ITEM_STATIC_RANDOM': 3, 'PROFILE_UNIQUIFIER': 2,
           'THREAD': 6, 'FRAME': 7, 'OBJECT': 6, 'ARRAY': 4, 'ELEMENT': 4,
           'STRING': 4, 'INSTANCE': 7, 'CLASS_MIRROR': 7, 'LOADER_MIRROR': 4,
           'THREAD_MIRROR': 3, 'END': 8}
JARS = {'client': (1465375, 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'),
        'server': (503100, '033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d')}
# Actual original initializer bytecode hashes, observed independently of C.
METHODS = {'client': ('d839820ccc643d594ee77ca3f701d60e4741249305b01b63edcca8a49d58b269',
                      '77074b7960127602b17c6446e26ec3b058092fce89324f6614899ede8c28418c'),
           'server': ('9128ead52f41be380498216ff8c6738bbbdae4b0a3323ace59bafd8931c9cb4b',
                      '3375f07538eea27afee133bb30ea3e8bc00ea1536938eb8c710e996b654cce47')}
SCOPE = ('106 Items: eight base fields,107 static references excluding clock b,'
         ' full current32000-element array, and106 distinct name String identities/UTF16; '
         'excludes initialization phase,constructor/subtype/class metadata,JDK/clock/profile state')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def decimal(text, maximum=2**63 - 1):
    require(re.fullmatch(r'0|[1-9][0-9]*', text) is not None, 'canonical unsigned decimal')
    value = int(text)
    require(value <= maximum, 'decimal bound')
    return value


def fingerprint(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def modifiers(text):
    # JDI exposes a signed Java int. The pinned VM reports additional high
    # bits on the actual synthetic enum-array declarations; retain them all.
    require(re.fullmatch(r'0|[1-9][0-9]*|-[1-9][0-9]*', text) is not None,
            'canonical signed modifier int')
    value = int(text)
    require(-(2**31) <= value < 2**31, 'signed modifier int width')
    return value


def valid_fingerprint(value):
    return (isinstance(value, dict) and type(value.get('bytes')) is int and value['bytes'] >= 0
            and isinstance(value.get('sha256'), str)
            and re.fullmatch('[0-9a-f]{64}', value['sha256']) is not None)


def utf16(length, content):
    size = decimal(length, 65536)
    require((size == 0 and content == '-') or
            (size > 0 and len(content) == 4 * size and re.fullmatch('[0-9a-f]+', content)),
            'complete lowercase UTF16 units')
    return size


def descriptor(text, method=False):
    atom = r'(?:[ZBCSIJFD]|L[A-Za-z0-9_$/]+;|\[+(?:[ZBCSIJFD]|L[A-Za-z0-9_$/]+;))'
    pattern = r'\(' + atom + r'*\)(?:' + atom + r'|V)' if method else atom
    require(re.fullmatch(pattern, text) is not None, 'raw descriptor syntax')


def project(raw, side):
    require(side in JARS, 'explicit original side')
    require(0 < len(raw) <= 8 * 1024 * 1024 and raw.endswith(b'\n') and b'\r' not in raw,
            'raw byte framing')
    rows = [line.split('\t') for line in raw.decode('ascii').split('\n')[:-1]]
    require(1 <= len(rows) <= 100000 and rows[0] == ['ITEM_REGISTRY_PREFIX_V1', side], 'raw header')
    for row in rows:
        tag = row[0]
        require((tag == 'MATH_HOLDER' and len(row) in (2, 5)) or
                (tag in LENGTHS and len(row) == LENGTHS[tag]), 'raw row schema ' + tag)
        require(all(all(32 <= ord(c) <= 126 for c in value) for value in row), 'raw ASCII columns')
    require(rows[-1][:4] == ['END', '106', '96', '5']
            and decimal(rows[-1][7]) == len(rows) - 1, 'complete raw footer')

    def only(tag):
        found = [r for r in rows if r[0] == tag]
        require(len(found) == 1, 'unique raw ' + tag)
        return found[0]

    require(only('ITEM_REGISTRY_PREFIX_V1') == rows[0] and only('END') == rows[-1], 'unique framing rows')
    require(only('POLICY')[1:] == ['ACTUAL_JDI_READS', 'SUSPEND_ALL',
                                  'BLOCK_FIRST_BEFORE_STATISTICS_ACTIVE_USE', '2664'], 'actual stop policy')
    base, block = ('gm', 'uu') if side == 'client' else ('ej', 'na')
    require(only('METHOD_BINDING')[1:] == [base, '<clinit>', '()V', '8', '2668', METHODS[side][0], '2664'],
            'actual Item initializer binding')
    require(only('OUTER_BLOCK_METHOD_BINDING')[1:] ==
            [block, '<clinit>', '()V', '8', '3678', METHODS[side][1], '3398'], 'actual outer Block binding')
    tables = {tag: {} for tag in ('OBJECT', 'TYPE', 'FIELD_DECL', 'METHOD_DECL', 'STATIC', 'INSTANCE',
                                  'ARRAY', 'ELEMENT', 'STRING', 'ITEM_ALIAS', 'BLOCK_ALIAS',
                                  'FRAME', 'THREAD', 'CLASS_MIRROR', 'LOADER_MIRROR', 'THREAD_MIRROR')}
    for row in rows:
        tag = row[0]
        if tag not in tables:
            continue
        if tag in ('OBJECT', 'ARRAY', 'STRING', 'ITEM_ALIAS', 'BLOCK_ALIAS', 'FRAME', 'THREAD',
                   'CLASS_MIRROR', 'LOADER_MIRROR', 'THREAD_MIRROR'):
            key = decimal(row[1])
        elif tag == 'TYPE':
            key = row[1]
        elif tag in ('FIELD_DECL', 'STATIC'):
            key = (row[2], row[3])
        elif tag == 'METHOD_DECL':
            key = (row[2], row[3], row[4])
        elif tag == 'INSTANCE':
            key = (decimal(row[1]), row[2], row[3])
        else:
            key = (decimal(row[1]), decimal(row[2]))
        require(key not in tables[tag], 'duplicate raw ' + tag)
        tables[tag][key] = row
    objects, types, declarations = tables['OBJECT'], tables['TYPE'], tables['FIELD_DECL']
    statics, instances = tables['STATIC'], tables['INSTANCE']
    arrays, elements, strings = tables['ARRAY'], tables['ELEMENT'], tables['STRING']
    require(set(objects) == set(range(1, len(objects) + 1)) and len(objects) <= 2048
            and len(objects) == decimal(rows[-1][4]), 'actual object rank inventory')
    require(len({r[2] for r in objects.values()}) == len(objects), 'unique actual JDI IDs')

    def rank(token, nullable=True):
        if token == 'NULL':
            require(nullable, 'nonnull actual reference')
            return None
        require(re.fullmatch(r'R:[1-9][0-9]*', token) is not None, 'actual reference token')
        value = decimal(token[2:], 2048)
        require(value in objects, 'referenced actual mirror exists')
        return value

    def typed(token, signature):
        descriptor(signature)
        if signature[0] in '[L':
            rank(token)
        elif signature == 'Z':
            require(token in ('Z:0', 'Z:1'), 'raw boolean bits')
        else:
            width = {'B': 2, 'C': 4, 'S': 4, 'I': 8, 'J': 16, 'F': 8, 'D': 16}[signature]
            require(re.fullmatch(signature + ':[0-9a-f]{' + str(width) + '}', token) is not None,
                    'raw primitive bit width')
        return token

    def loader(value):
        number = decimal(value, 2048)
        require(number == 0 or number in tables['LOADER_MIRROR'], 'actual loader mirror')
        return number

    for r in objects.values():
        require(decimal(r[2]) > 0, 'positive actual JDI ID')
        descriptor(r[4]); loader(r[5])
        if not r[4].startswith('['):
            require(r[4] == 'L' + r[3].replace('.', '/') + ';', 'object class descriptor')
    for r in types.values():
        descriptor(r[2]); modifiers(r[3]); loader(r[4])
        require(r[2] == 'L' + r[1].replace('.', '/') + ';'
                and all(v in ('true', 'false') for v in r[5:]), 'type metadata')
    for r in declarations.values():
        require(r[1] == r[2] and r[1] in types, 'actual declaring field owner')
        descriptor(r[4]); modifiers(r[5])
    for r in tables['METHOD_DECL'].values():
        require(r[1] == r[2] and r[1] in types, 'actual declaring method owner')
        descriptor(r[4], method=True); modifiers(r[5])
    for r in statics.values():
        require(r[1] in ('ITEM', 'BLOCK', 'REACHED_CLASS', 'OBSERVED_MATH_HOLDER'), 'actual static root role')
        require((r[2], r[3]) in declarations and declarations[r[2], r[3]][4:] == r[4:6]
                and modifiers(r[5]) & 8, 'actual static declaration metadata')
        typed(r[6], r[4])
    static_owners = {r[2] for r in statics.values()}
    for owner in static_owners:
        require({key for key in statics if key[0] == owner} ==
                {key for key, r in declarations.items() if key[0] == owner and int(r[5]) & 8},
                'complete declared static field set')
    for r in instances.values():
        require(decimal(r[1]) in objects and (r[2], r[3]) in declarations
                and declarations[r[2], r[3]][4:] == r[4:6] and not int(r[5]) & 8,
                'actual instance declaration metadata')
        typed(r[6], r[4])
    instance_owners = {(key[0], key[1]) for key in instances}
    for object_rank, owner in instance_owners:
        require({key[2] for key in instances if key[:2] == (object_rank, owner)} ==
                {key[1] for key, r in declarations.items() if key[0] == owner and not int(r[5]) & 8},
                'complete observed declaring instance field set')
    require(set(rank_ for rank_, _ in elements) <= set(arrays), 'no orphan array elements')
    require(len(elements) == decimal(rows[-1][5]) <= 262144, 'raw physical cell total')
    for owner, r in arrays.items():
        length = decimal(r[2], 65536)
        require(owner in objects and objects[owner][4] == r[3] and r[3].startswith('['), 'actual array descriptor')
        require({index for rank_, index in elements if rank_ == owner} == set(range(length)), 'complete physical array indices')
        for index in range(length):
            typed(elements[owner, index][3], r[3][1:])
    require(sum(decimal(r[2], 65536) for r in arrays.values()) == len(elements), 'physical array length sum')
    require(set(arrays) == {key for key, r in objects.items() if r[4].startswith('[')}, 'all actual arrays expanded')
    for owner, r in strings.items():
        require(owner in objects and objects[owner][3:5] == ['java.lang.String', 'Ljava/lang/String;'], 'actual String object')
        utf16(r[2], r[3])
    require(set(strings) == {key for key, r in objects.items() if r[3] == 'java.lang.String'}, 'all actual Strings expanded')
    require(sum(decimal(r[2], 65536) for r in strings.values()) == decimal(rows[-1][6]) <= 262144, 'complete raw UTF16 total')
    for tag in ('CLASS_MIRROR', 'LOADER_MIRROR', 'THREAD_MIRROR'):
        for owner, r in tables[tag].items():
            require(owner in objects, 'terminal mirror object exists')
            if tag == 'CLASS_MIRROR':
                require(objects[owner][3] == 'java.lang.Class', 'actual Class mirror')
                descriptor(r[3]); loader(r[4])
                require(all(v in ('true', 'false') for v in r[5:]), 'reflected type flags')
            elif tag == 'LOADER_MIRROR':
                require(r[2:4] == [objects[owner][2], objects[owner][3]], 'actual loader ID/class')
            else:
                utf16(str(0 if r[2] == '-' else len(r[2]) // 4), r[2])
    game_loader = decimal(only('LOADER_ROOT')[1], 2048)
    require(game_loader in objects and objects[game_loader][2] == only('LOADER_ROOT')[2]
            and objects[game_loader][3] == 'java.net.URLClassLoader', 'actual held original loader')
    helper = only('HELPER_BINDING')
    require(helper[1] == 'ItemRegistryPrefixObservation' and loader(helper[2]) != game_loader
            and rank(helper[3], False) == game_loader and helper[5] == JARS[side][1]
            and helper[6] == ('00750075' if side == 'client' else '006e0061'), 'actual helper entry/JAR/loader')
    utf16(str(len(helper[4]) // 4), helper[4])
    for owner in (base, block):
        require(owner in types and types[owner][4:] == [str(game_loader), 'true', 'false', 'false'],
                'original suspended initializer metadata')
        require((owner, '<clinit>', '()V') in tables['METHOD_DECL'], 'declared bound initializer')
    frames = tables['FRAME']
    require(1 <= len(frames) <= 128 and set(frames) == set(range(len(frames))), 'complete ordered stack')
    for r in frames.values():
        descriptor(r[4], method=True); loader(r[6])
        require(r[5] == '-1' or decimal(r[5]) >= 0, 'raw frame code index')
    require(frames[0][2:6] == [base, '<clinit>', '()V', '2664'] and frames[0][6] == str(game_loader)
            and any(r[2:6] == [block, '<clinit>', '()V', '3398'] and r[6] == str(game_loader) for r in frames.values())
            and any(r[2:4] == ['ItemRegistryPrefixObservation', 'main'] for r in frames.values()), 'actual nested Block-first stack')
    require(1 <= len(tables['THREAD']) <= 64, 'raw thread inventory bound')
    for owner, r in tables['THREAD'].items():
        require(owner in objects and r[2] == objects[owner][2] and owner in tables['THREAD_MIRROR']
                and r[3] == tables['THREAD_MIRROR'][owner][2] and decimal(r[5]) > 0, 'actual suspended thread metadata')
        require(r[4] == '-1' or decimal(r[4], 5) >= 0, 'thread status')
    require(set(tables['ITEM_ALIAS']) == set(range(106)) and set(tables['BLOCK_ALIAS']) == set(range(96)), 'complete actual alias inventories')

    expected = {'b': ('Ljava/util/Random;', '12'), 'c': ('[L' + base + ';', '9')}
    for name in NAMES:
        concrete = ('wr' if side == 'client' else 'ol') if name == 'bb' else (
                    ('bl' if side == 'client' else 'as') if name == 'bc' else base)
        expected[name] = ('L' + concrete + ';', '9')
    expected_fields = {name: (signature or 'L' + base + ';', modifiers) for name, signature, modifiers in BASE}
    require({key[1]: tuple(r[4:6]) for key, r in declarations.items() if key[0] == base} ==
            dict(expected, **expected_fields), 'all116 exact Item declarations')
    require({key[1] for key in statics if key[0] == base} == set(expected)
            and all(statics[base, name][1] == 'ITEM' for name in expected), 'all108 actual Item static cells')
    require(len([r for r in statics.values() if r[1] == 'BLOCK']) == 113, 'all113 raw Block static cells')
    for index, name in enumerate(BLOCK_NAMES):
        r = tables['BLOCK_ALIAS'][index]
        require((block, name) in statics and r[2] == name and rank(statics[block, name][6], False) == decimal(r[3])
                and objects[decimal(r[3])][3] == r[4], 'actual Block named alias')
    rng = [r for r in rows if r[0] == 'PRIVATE_RANDOM']
    require(len(rng) == 4 and len({decimal(r[1], 95) for r in rng}) == 4, 'four actual Block Random rows')
    item_rng = only('ITEM_STATIC_RANDOM')
    for r in rng:
        require(decimal(r[2], 2048) in objects and decimal(r[3], 2048) in objects, 'raw Block Random identities')
    random_ranks = [decimal(r[2], 2048) for r in rng] + [decimal(item_rng[1], 2048)]
    seed_ranks = [decimal(r[3], 2048) for r in rng] + [decimal(item_rng[2], 2048)]
    require(len(set(random_ranks)) == 5 and len(set(seed_ranks)) == 5 and
            all(r in objects and objects[r][3] == 'java.util.Random' for r in random_ranks) and
            all(r in objects and objects[r][3] == 'java.util.concurrent.atomic.AtomicLong' for r in seed_ranks), 'raw ordinary Random/seed owners')
    require(rank(statics[base, 'b'][6], False) == random_ranks[-1], 'actual excluded Item clock root')
    rank(only('PROFILE_UNIQUIFIER')[1], False)
    math = only('MATH_HOLDER')
    require(math == ['MATH_HOLDER', 'ABSENT'] or
            (len(math) == 5 and math[1] == 'PRESENT' and all(v in ('true', 'false') for v in math[2:])), 'observed Math holder metadata')

    named = []
    for index, name in enumerate(NAMES):
        actual = rank(statics[base, name][6], False)
        alias = tables['ITEM_ALIAS'][index]
        require(alias[2:5] == [name, str(actual), objects[actual][3]] and objects[actual][5] == str(game_loader), 'actual named Item alias/type/loader')
        fields = {key[2]: r for key, r in instances.items() if key[:2] == (actual, base)}
        require(set(fields) == set(expected_fields) and alias[5] == fields['bf'][6], 'all eight actual base fields and bf alias')
        named.append(actual)
    require(len(set(named)) == 106, '106 distinct actual named Item owners')
    array = rank(statics[base, 'c'][6], False)
    require(rank(only('ITEM_ARRAY_ROOT')[1], False) == array and array not in named
            and arrays[array][2:] == ['32000', '[L' + base + ';'], 'actual full current Item array')
    enrolled = {owner: index + 1 for index, owner in enumerate(named)}
    enrolled[array] = 107
    output = [('ITEM_BASE_PREFIX_V1', side)]
    string_order = []

    def add(*columns):
        output.append(tuple(str(value) for value in columns))

    def item_reference(token):
        owner = rank(token)
        require(owner is None or owner in named, 'projected Item reference must name an actual named owner')
        return 'NULL' if owner is None else 'R:' + str(enrolled[owner])

    add('STATIC', 'c', 'R:107')
    for index, name in enumerate(NAMES):
        add('STATIC', name, 'R:' + str(index + 1))
    for owner in named:
        for name, signature, _ in BASE:
            token = instances[owner, base, name][6]
            if name == 'bk':
                token = item_reference(token)
            elif name == 'bl':
                actual = rank(token, False)
                require(actual in strings and actual not in enrolled, 'distinct actual Item name String')
                enrolled[actual] = len(enrolled) + 1
                string_order.append(actual)
                token = 'R:' + str(enrolled[actual])
            else:
                typed(token, signature)
            add('FIELD', enrolled[owner], name, token)
        identifier = int(instances[owner, base, 'bf'][6][2:], 16)
        require(identifier < 32000 and rank(elements[array, identifier][3], False) == owner, 'actual bf/current-array alias')
    add('ARRAY', 107, 32000)
    for index in range(32000):
        add('ELEMENT', index, item_reference(elements[array, index][3]))
    for owner in string_order:
        add('STRING', enrolled[owner], *strings[owner][2:])
    add('END', len(output))
    require(len(output) == 33064, 'complete finite projection row count')
    return ('\n'.join('\t'.join(row) for row in output) + '\n').encode('ascii')


def relative_leaf(name):
    path = PurePosixPath(name)
    require(isinstance(name, str) and '\\' not in name and not path.is_absolute()
            and path.parts and all(part not in ('', '.', '..') for part in path.parts)
            and ':' not in name, 'safe relative artifact label')
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    options = parser.parse_args()
    capture_dir = options.capture_dir.resolve(strict=True)
    manifest_raw = (capture_dir / 'capture.json').read_bytes()
    capture = json.loads(manifest_raw)
    require(capture['status'] == 'captured' and capture['inputs_unchanged'] is True
            and capture['inputs_before'] == capture['inputs_after'] and capture['inputs_before'], 'stable accepted producer inputs')
    inputs = capture['inputs_before']
    for name in ('java', 'javac', 'modules', 'jvm', 'java_native', 'release', 'source_archive',
                 'bounded_runner', 'capture_driver', 'ItemRegistryPrefixObservation.java',
                 'ItemRegistryPrefixController.java', 'client_jar', 'server_jar'):
        require(name in inputs, 'complete source/JDK/original input closure')
    require(all(valid_fingerprint(value) for value in inputs.values()) and any(name.startswith('runtime/') for name in inputs), 'full typed input fingerprints')
    for side in JARS:
        require((inputs[side + '_jar']['bytes'], inputs[side + '_jar']['sha256']) == JARS[side], 'pinned original JAR metadata')
    artifacts = capture['artifacts']
    require(isinstance(artifacts, dict) and artifacts and len(artifacts) <= 256, 'finite actual artifact ledger')
    for name, expected in artifacts.items():
        relative_leaf(name)
        source = (capture_dir / name).resolve(strict=True)
        require(source.is_relative_to(capture_dir) and valid_fingerprint(expected)
                and source.stat().st_size <= 16 * 1024 * 1024 and fingerprint(source.read_bytes()) == expected,
                'complete unchanged producer artifact ' + name)
    compiled = capture['compiled_classes']
    require(isinstance(compiled, dict) and compiled and
            all(valid_fingerprint(value) and artifacts.get('classes/' + name) == value for name, value in compiled.items()), 'compiled class before/after artifact closure')
    require('ItemRegistryPrefixObservation.class' in compiled and 'ItemRegistryPrefixController.class' in compiled, 'actual compiled entry/controller')
    children = capture['children']
    require(isinstance(children, list) and len(children) == 3, 'compile plus two controller producers')
    for child, label in zip(children, ('compile', 'client-controller', 'server-controller')):
        require(child['reaped'] is True and type(child['exit_code']) is int and child['exit_code'] == 0
                and child['stdout'] == artifacts[label + '.stdout'] and child['stderr'] == artifacts[label + '.stderr']
                and child['stdout']['bytes'] == child['stderr']['bytes'] == 0, 'actual clean producer lifecycle')
    provenance = {'schema': 'ITEM_BASE_PREFIX_PROVENANCE_V1', 'scope': SCOPE,
                  'termination_policy': 'Requested debugger exit0 at Item2664 with outer Block3398; not natural completion or successful bootstrap',
                  'capture_manifest': fingerprint(manifest_raw), 'projector': fingerprint(Path(__file__).read_bytes()),
                  'inputs': {name: {'bytes': value['bytes'], 'sha256': value['sha256']} for name, value in sorted(inputs.items())},
                  'compiled_classes': compiled, 'artifacts': artifacts, 'sides': {}}
    projections = {}
    for side in ('client', 'server'):
        result = capture['sides'][side]
        source = capture_dir / side / 'snapshot.raw.tsv'
        raw = source.read_bytes()
        require(result['status'] == 'captured' and fingerprint(raw) == result['snapshot'] == artifacts[side + '/snapshot.raw.tsv'], 'accepted exact raw snapshot')
        receipt_raw = (capture_dir / side / 'receipt.tsv').read_bytes()
        require(fingerprint(receipt_raw) == result['receipt'] == artifacts[side + '/receipt.tsv']
                and receipt_raw.endswith(b'\n') and b'\r' not in receipt_raw, 'actual controller receipt')
        receipt = [line.split('\t') for line in receipt_raw.decode('ascii').split('\n')[:-1]]
        require(receipt[0] == ['ITEM_PREFIX_PROCESS_V1'] and ['STATUS', 'PASS'] in receipt
                and receipt[-1] == ['END', str(len(receipt) - 1)]
                and ['WATCHDOG', 'false', 'false'] in receipt, 'complete accepted receipt')
        child = [r for r in receipt if r[0] == 'CHILD']
        require(len(child) == 1 and child[0][1] == str(result['child_pid'])
                and child[0][2:] == ['true', '0', 'true', 'true'], 'actual requested-exit/reap closure')
        require(['SNAPSHOT_RAW', str(len(raw)), fingerprint(raw)['sha256'], 'true'] in receipt
                and ['JAR_SHA256', JARS[side][1], JARS[side][1]] in receipt, 'actual raw/JAR receipt joins')
        projected = project(raw, side)
        require(result['rows'] == raw.count(b'\n'), 'actual raw physical rows')
        projections[side] = projected
        provenance['sides'][side] = {'raw': fingerprint(raw), 'receipt': fingerprint(receipt_raw),
                                     'fixture': fingerprint(projected), 'rows': projected.count(b'\n')}
    require((capture_dir / 'capture.json').read_bytes() == manifest_raw, 'capture manifest unchanged during projection')
    output = options.output_dir.resolve()
    output.mkdir(exist_ok=False)
    for side, projected in projections.items():
        with (output / (side + '.expected.tsv')).open('xb') as stream:
            stream.write(projected)
    with (output / 'provenance.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(provenance, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'status': 'projected', 'scope': SCOPE, 'sides': provenance['sides']}, indent=2))


if __name__ == '__main__':
    main()
