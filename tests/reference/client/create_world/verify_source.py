"""Pin original Create World identities offline; never execute or rewrite game classes."""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SPEC = importlib.util.spec_from_file_location(
    'create_world_classfile', HERE.parent / 'resource_lookup_common/offline_classfile.py')
READER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(READER)
JAR_BYTES = 1465375
JAR_SHA256 = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
MAX_PINS_BYTES = 2097152
APP = 'net/minecraft/client/Minecraft'
TEXT = '(Lsj;Ljava/lang/String;III)V'
UNIQUE = '(Lnl;Ljava/lang/String;)Ljava/lang/String;'
RO_CONSTRUCTOR = '(Lda;Lsj;IIIILjava/lang/String;)V'

# None selects every declaration, including abstract interface declarations.
# Every listed class also has its entire field/method inventory pinned below.
SELECTED = {
    'fj': None,
    'ro': None,
    'fp': None,
    'da': None,
    'ub': None,
    'nh': None,
    'nl': None,
    'tq': [('<init>', '(Ljava/io/File;)V'), ('b', '(Ljava/lang/String;)Lei;')],
    'hi': [('<init>', '(Ljava/io/File;)V'), ('a', '(Ljava/lang/String;Z)Lwt;')],
    'ke': [('<init>', '(IIILjava/lang/String;)V'),
           ('<init>', '(IIIIILjava/lang/String;)V')],
    'in': [('a', '(Ljava/lang/String;)Z')],
    'sj': [('a', '(Ljava/lang/String;III)V'), ('a', '(Ljava/lang/String;)I')],
    'rq': [('a', '(Lke;)V')],
    APP: [('a', '()V'), ('c', '()Lnl;'), ('a', '(Lda;)V'),
          ('a', '(Ljava/lang/String;Ljava/lang/String;J)V')],
}

# These are resolved constant-pool identities at particular source PCs, not
# instruction dumps. Code hashes independently cover all intervening opcodes.
REFERENCES = [
    ('fj', '<init>', '(Lda;)V', [1, 6]),
    ('fj', 'a', '()V', [4, 11]),
    ('fj', 'b', '()V', [0, 5, 12, 21, 50, 53, 56, 66, 95, 98, 101,
                       108, 136, 139, 142, 150, 159, 163, 190, 193, 197]),
    ('fj', 'k', '()V', [5, 8, 11, 14, 42, 45, 58, 67, 75, 82, 85]),
    ('fj', 'a', UNIQUE, [2, 18, 23, 26]),
    ('fj', 'h', '()V', [1]),
    ('fj', 'a', '(Lke;)V', [24, 42, 55, 58, 62, 65, 73, 80, 88, 110,
                          119, 127, 130, 145, 149, 157]),
    ('fj', 'a', '(CI)V', [16, 28, 43, 51, 59, 71, 74, 85, 89]),
    ('fj', 'a', '(III)V', [4, 14, 24]),
    ('fj', 'a', '(IIF)V', [0, 6, 18, 41, 53, 69, 88, 122, 134, 150,
                         162, 179, 186, 193, 200]),
    ('fj', 'j', '()V', [15, 23, 34, 42]),
    ('ro', '<init>', RO_CONSTRUCTOR, [1, 16, 21, 50]),
    ('ro', 'a', '(Ljava/lang/String;)V', [2]),
    ('ro', 'a', '()Ljava/lang/String;', [1]),
    ('ro', 'b', '()V', [7]),
    ('ro', 'a', '(CI)V', [25, 34, 51, 60, 67, 89, 96, 99, 102, 105,
                        118, 134, 139, 142, 145, 149, 159, 188, 192, 195, 198]),
    ('ro', 'a', '(III)V', [59]),
    ('ro', 'a', '(Z)V', [13, 18]),
    ('ro', 'c', '()V', [37, 69, 120, 134, 137, 162, 199]),
    ('ro', 'a', '(I)V', [2]),
    ('fp', 'a', '()Ljava/lang/String;', [16, 21, 24, 32, 43, 72]),
    ('fp', '<clinit>', '()V', [0, 3, 93]),
    ('da', 'd', '()Ljava/lang/String;', [0, 3, 7, 16, 19, 28, 31, 36]),
    ('da', 'a', '(Lnet/minecraft/client/Minecraft;II)V', [14, 19, 22, 39, 45]),
    ('ub', 'a', TEXT, [5, 15]),
    ('ub', 'b', TEXT, [7]),
    ('nh', '<init>', '()V', [23, 26, 37, 40]),
    ('nh', 'a', '()Lnh;', [0]),
    ('nh', 'a', '(Ljava/lang/String;)Ljava/lang/String;', [6]),
    ('nh', '<clinit>', '()V', [0, 4, 7]),
    ('hi', '<init>', '(Ljava/io/File;)V', [2]),
    ('tq', 'b', '(Ljava/lang/String;)Lei;', [9, 14, 29, 34, 45, 48, 57, 68,
                                         76, 86, 91, 102, 105, 114, 125, 133]),
    ('rq', 'a', '(Lke;)V', [162, 167, 170]),
    (APP, 'a', '()V', [167, 184, 187]),
    (APP, 'c', '()Lnl;', [1]),
]

DECLARATIONS = [
    ('fj', 'b', '()V'), ('fj', 'a', '()V'), ('fj', 'h', '()V'),
    ('fj', 'j', '()V'), ('fj', 'a', '(Lke;)V'), ('fj', 'a', '(CI)V'),
    ('fj', 'a', '(III)V'), ('fj', 'a', '(IIF)V'), ('fj', 'c', '()Z'),
    ('fj', 'i', '()V'), ('fj', 'a', TEXT), ('fj', 'b', TEXT),
    ('ro', 'a', '(IIIII)V'), ('ro', 'b', TEXT),
    ('hi', 'b', '(Ljava/lang/String;)Lei;'),
    ('hi', 'a', '(Ljava/lang/String;Z)Lwt;'),
    ('nl', 'b', '(Ljava/lang/String;)Lei;'),
]


def _bounded_read(path, limit):
    with path.open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Input exceeds size bound: ' + str(path))
    return data


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate metadata key: ' + key)
        result[key] = value
    return result


def load_pins(path):
    return json.loads(_bounded_read(path, MAX_PINS_BYTES).decode('utf-8'),
                      object_pairs_hook=_unique_object)


def _method(classes, owner, name, descriptor):
    found = [row for row in classes[owner]['methods']
             if (row['name'], row['descriptor']) == (name, descriptor)]
    if len(found) != 1:
        raise ValueError('Unique original declaration required: ' + owner + '.' + name + descriptor)
    return found[0]


def make_pins(jar):
    raw = _bounded_read(jar, JAR_BYTES)
    if len(raw) != JAR_BYTES or hashlib.sha256(raw).hexdigest() != JAR_SHA256:
        raise ValueError('Exact original client archive required')
    # Parse the same immutable bytes that passed the archive identity check.
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        members = archive.infolist()
        names = [entry.filename for entry in members]
        if len(members) > 4096 or len(names) != len(set(names)):
            raise ValueError('Bounded unique original archive members required')
        classes = {}
        for name in SELECTED:
            entry = archive.getinfo(name + '.class')
            if entry.file_size > 2097152:
                raise ValueError('Bounded selected class required')
            value = READER.read_class(archive.read(entry))
            if value['class'] != name:
                raise ValueError('Actual archive class identity required')
            classes[name] = value

    result = {
        'schema': 1,
        'scope': 'Offline class inventories, hierarchy, Code hashes and selected references; no runtime observations.',
        'jar_bytes': len(raw), 'jar_sha256': JAR_SHA256,
        'archive_member_count': len(members),
        'archive_class_count': sum(name.endswith('.class') for name in names),
        'classes': {},
    }
    for owner, keys in SELECTED.items():
        value = classes[owner]
        inventory = [{key: row[key] for key in ('name', 'descriptor', 'access')}
                     for row in value['methods']]
        selected = value['methods'] if keys is None else [
            _method(classes, owner, name, descriptor) for name, descriptor in keys]
        selected = [{key: row[key] for key in (
            'name', 'descriptor', 'access', 'code_bytes', 'code_sha256',
            'code_attribute_sha256', 'max_stack', 'max_locals', 'exceptions') if key in row}
            for row in selected]
        result['classes'][owner] = {
            key: value[key] for key in (
                'class_bytes', 'class_sha256', 'access', 'major', 'minor', 'interfaces')}
        result['classes'][owner].update(
            superclass=value['super'], fields=value['fields'],
            methods=inventory, selected_methods=selected)

    resolved = {}
    for owner, name, descriptor in DECLARATIONS:
        current, visited = owner, set()
        while current in classes:
            if current in visited:
                raise ValueError('Acyclic original hierarchy required')
            visited.add(current)
            if any((row['name'], row['descriptor']) == (name, descriptor)
                   for row in classes[current]['methods']):
                resolved[owner + '.' + name + descriptor] = current
                break
            current = classes[current]['super']
        else:
            raise ValueError('Unresolved original declaration: ' + owner + '.' + name + descriptor)
    result['resolved_declarations'] = resolved

    references = {}
    for owner, name, descriptor, pcs in REFERENCES:
        method = _method(classes, owner, name, descriptor)
        instructions = {row['pc']: row for row in method['instructions']}
        for pc in pcs:
            row = instructions[pc]
            references[f'{owner}.{name}{descriptor}:{pc}'] = {
                'operation': row['name'], 'reference': row['reference']}
    result['selected_references'] = references
    return result


def _compare(actual, expected, path='$'):
    # Python considers True == 1; identity metadata must not accept that coercion.
    if type(actual) is not type(expected):
        raise ValueError('Static identity type differs at ' + path)
    if isinstance(actual, dict):
        if actual.keys() != expected.keys():
            raise ValueError('Static identity keys differ at ' + path)
        for key in actual:
            _compare(actual[key], expected[key], path + '.' + key)
    elif isinstance(actual, list):
        if len(actual) != len(expected):
            raise ValueError('Static identity length differs at ' + path)
        for index, (left, right) in enumerate(zip(actual, expected)):
            _compare(left, right, path + '[' + str(index) + ']')
    elif actual != expected:
        raise ValueError('Static identity differs at ' + path)


def verify(jar, metadata):
    actual = make_pins(jar)
    _compare(actual, metadata)
    return {'status': 'PASS_STATIC_IDENTITIES', 'classes': len(actual['classes']),
            'selected_methods': sum(len(row['selected_methods']) for row in actual['classes'].values()),
            'selected_references': len(actual['selected_references']),
            'game_class_execution': False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--pins', type=Path, default=HERE / 'source-pins.json')
    args = parser.parse_args(argv)
    if not args.jar.is_file():
        print('UNAVAILABLE: original client archive is missing')
        return 77
    try:
        result = verify(args.jar, load_pins(args.pins))
    except (AssertionError, OSError, ValueError, KeyError, IndexError,
            TypeError, zipfile.BadZipFile) as error:
        print('FAIL: ' + str(error))
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
