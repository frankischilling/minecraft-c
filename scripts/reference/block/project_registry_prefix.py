"""Finite raw-original projection; never reads C source or a native executable."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
NAMES = ('u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z '
         'aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az '
         'aA aB aC aD aE aF aG aH aI aJ aK aL aM aN aO aP aQ aR aS aT aU aV aW aX aY aZ '
         'ba bb bc bd be bf bg bh bi bj bk bl').split()
BASE = [('bm', 'I'), ('bn', 'I'), ('bo', 'F'), ('bp', 'F'), ('bq', 'Z'), ('br', 'Z'),
        *[(name, 'D') for name in ('bs', 'bt', 'bu', 'bv', 'bw', 'bx')],
        ('by', 'Lct;'), ('bz', 'F'), ('bA', 'Lln;'), ('bB', 'F'), ('a', 'Ljava/lang/String;')]
LENGTHS = {'BLOCK_REGISTRY_PREFIX_V1': 2, 'POLICY': 5, 'METHOD_BINDING': 8, 'LOADER_ROOT': 3,
           'TYPE': 8, 'FIELD_DECL': 6, 'METHOD_DECL': 6, 'HELPER_BINDING': 6, 'STATIC': 7,
           'BLOCK_ALIAS': 5, 'PRIVATE_RANDOM': 4, 'PROFILE_UNIQUIFIER': 2, 'THREAD': 6,
           'FRAME': 7, 'OBJECT': 6, 'ARRAY': 4, 'ELEMENT': 4, 'STRING': 4, 'INSTANCE': 7,
           'CLASS_MIRROR': 7, 'LOADER_MIRROR': 4, 'THREAD_MIRROR': 3, 'END': 7}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def fingerprint(path):
    raw = path.read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def project(raw, side):
    require(raw.endswith(b'\n') and b'\r' not in raw and len(raw) <= 4 * 1024 * 1024, 'raw byte framing')
    rows = [line.split('\t') for line in raw.decode('ascii').splitlines()]
    require(len(rows) <= 20000 and rows[0] == ['BLOCK_REGISTRY_PREFIX_V1', side], 'raw header')
    for row in rows:
        if row[0] == 'MATH_HOLDER':
            require(len(row) in (2, 5), 'Math row shape')
        else:
            require(row[0] in LENGTHS and len(row) == LENGTHS[row[0]], 'raw row schema ' + row[0])
        require(all(all(32 <= ord(c) <= 126 for c in part) for part in row), 'ASCII columns')
    require(rows[-1][0:3] == ['END', '96', '4'] and int(rows[-1][-1]) == len(rows) - 1, 'raw complete footer')
    base, material, sound = ('uu', 'ln', 'ct') if side == 'client' else ('na', 'hj', 'bu')
    binding = [r for r in rows if r[0] == 'METHOD_BINDING']
    require(len(binding) == 1 and binding[0][1:6] == [base, '<clinit>', '()V', '8', '3678']
            and binding[0][-1] == '3398', 'actual initializer binding')
    objects, statics, instance, arrays, elements, strings, aliases = {}, {}, {}, {}, {}, {}, {}
    for row in rows:
        tag = row[0]
        if tag == 'OBJECT':
            key = int(row[1]); destination = objects; value = row[2:]
        elif tag == 'STATIC':
            key = tuple(row[1:4]); destination = statics; value = row[4:]
        elif tag == 'INSTANCE':
            key = (int(row[1]), row[2], row[3]); destination = instance; value = row[4:]
        elif tag == 'ARRAY':
            key = int(row[1]); destination = arrays; value = row[2:]
        elif tag == 'ELEMENT':
            key = (int(row[1]), int(row[2])); destination = elements; value = row[3]
        elif tag == 'STRING':
            key = int(row[1]); destination = strings; value = row[2:]
        elif tag == 'BLOCK_ALIAS':
            key = int(row[1]); destination = aliases; value = row[2:]
        else:
            continue
        require(key not in destination, 'duplicate raw ' + tag)
        destination[key] = value
    require(set(objects) == set(range(1, len(objects) + 1)) and len(objects) == int(rows[-1][3]), 'object rank inventory')
    require(len({v[0] for v in objects.values()}) == len(objects), 'actual unique object IDs')
    require(len(elements) == int(rows[-1][4]) == sum(int(v[0]) for v in arrays.values()), 'full physical cells')
    for rank, (length, signature) in arrays.items():
        require(set(i for owner, i in elements if owner == rank) == set(range(int(length))), 'array element coverage')
        require(objects[rank][2] == signature, 'array object descriptor')
    for rank, (length, content) in strings.items():
        require(objects[rank][1] == 'java.lang.String', 'String actual object')
        require((length == '0' and content == '-') or
                (int(length) > 0 and len(content) == int(length) * 4 and re.fullmatch('[0-9a-f]+', content)), 'complete UTF16')
    require(sum(int(v[0]) for v in strings.values()) == int(rows[-1][5]), 'all raw UTF16 units')
    require(len([k for k in statics if k[0] == 'BLOCK']) == 113, 'all raw Block static cells')
    require(set(aliases) == set(range(96)), 'all raw named aliases')
    enrolled, string_order, output = {}, [], [('REGISTRY_BASE_PREFIX_V1', side)]

    def add(*columns):
        output.append(tuple(str(value) for value in columns))

    def reference(token, is_string=False):
        if token == 'NULL':
            return token
        require(re.fullmatch(r'R:[1-9][0-9]*', token), 'raw reference token')
        rank = int(token[2:])
        require(rank in objects, 'referenced actual mirror exists')
        if rank not in enrolled:
            enrolled[rank] = len(enrolled) + 1
        if is_string:
            require(rank in strings, 'referenced actual String exists')
            if rank not in string_order:
                string_order.append(rank)
        return 'R:' + str(enrolled[rank])

    def typed(token, descriptor):
        if descriptor[0] in '[L':
            return reference(token, descriptor == 'Ljava/lang/String;')
        widths = {'I': 8, 'F': 8, 'D': 16}
        if descriptor == 'Z':
            require(token in ('Z:0', 'Z:1'), 'raw boolean')
        else:
            require(descriptor in widths and re.fullmatch(descriptor + ':[0-9a-f]{' + str(widths[descriptor]) + '}', token), 'raw primitive bits')
        return token

    def static(role, declaring, name):
        require((role, declaring, name) in statics, 'required actual static ' + declaring + '.' + name)
        return statics[(role, declaring, name)]

    def field(rank, declaring, name, descriptor):
        require((rank, declaring, name) in instance, 'required actual instance ' + declaring + '.' + name)
        actual, modifiers, token = instance[(rank, declaring, name)]
        require(actual == descriptor, 'actual field descriptor')
        return typed(token, descriptor)

    for name in [*'defghijkl', *'mnopqrst', *NAMES]:
        descriptor, modifiers, token = static('BLOCK', base, name)
        require(modifiers == '25', 'actual Block static modifiers')
        add('STATIC', name, reference(token))
    for name in [*'abcdefghijklmnopqrstuvwxyz', 'A', 'B']:
        descriptor, modifiers, token = static('REACHED_CLASS', material, name)
        require(descriptor == 'L' + material + ';', 'actual Material descriptor')
        add('MATERIAL', name, reference(token))
    for name in 'defghijkl':
        token = static('BLOCK', base, name)[2]
        rank = int(token[2:])
        for member, descriptor in [('a', 'Ljava/lang/String;'), ('b', 'F'), ('c', 'F')]:
            add('SOUND', name, member, field(rank, sound, member, descriptor))
    for index, name in enumerate(NAMES):
        token = static('BLOCK', base, name)[2]
        rank = int(token[2:])
        require(aliases[index][0:2] == [name, str(rank)], 'actual named/alias mirror equality')
        for member, descriptor in BASE:
            if member == 'by': descriptor = 'L' + sound + ';'
            if member == 'bA': descriptor = 'L' + material + ';'
            add('BLOCK', name, member, field(rank, base, member, descriptor))
    for name in 'mnopqrst':
        descriptor, modifiers, token = static('BLOCK', base, name)
        rank = int(token[2:])
        require(rank in arrays and arrays[rank] == ['256', descriptor], 'actual complete registry array')
        for index in range(256):
            add('ELEMENT', name, index, typed(elements[(rank, index)], descriptor[1:]))
    for rank in string_order:
        add('STRING', enrolled[rank], *strings[rank])
    add('END', 113, 96, 2048, len(string_order), len(enrolled), len(output))
    return ('\n'.join('\t'.join(row) for row in output) + '\n').encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture-dir', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    capture_dir = args.capture_dir.resolve(strict=True)
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(exist_ok=False)
    capture = json.loads((capture_dir / 'capture.json').read_text())
    require(capture['status'] == 'captured' and capture['inputs_unchanged'] and capture['inputs_before'] == capture['inputs_after'], 'accepted immutable raw capture')
    report = {'status': 'failed', 'scope': 'All17 base fields,113 static reference aliases,28 Material identities,9 sound fields,8 complete arrays and encountered String identities/content; excludes subtype/JDK internals/clock state/class initialization phase',
              'capture_manifest': fingerprint(capture_dir / 'capture.json'), 'projector': fingerprint(Path(__file__).resolve()), 'sides': {}}
    try:
        for side in ('client', 'server'):
            source = capture_dir / side / 'snapshot.raw.tsv'
            require(fingerprint(source) == capture['sides'][side]['snapshot'], 'frozen raw snapshot')
            raw = source.read_bytes()
            projected = project(raw, side)
            destination = output_dir / (side + '.expected.tsv')
            destination.write_bytes(projected)
            # Rebind only framing counts so structural mutations reach their
            # own intended checks instead of all failing at the row footer.
            lines = raw.splitlines(keepends=True)
            instance_at = next(i for i, line in enumerate(lines) if line.startswith(b'INSTANCE\t'))
            element_at = next(i for i, line in enumerate(lines) if line.startswith(b'ELEMENT\t'))
            def frame(changed):
                footer = changed[-1].rstrip(b'\n').split(b'\t')
                footer[-1] = str(len(changed) - 1).encode('ascii')
                return b''.join(changed[:-1] + [b'\t'.join(footer) + b'\n'])
            mutations = [(raw[:-1], 'raw byte framing'),
                         (frame(lines[:instance_at] + [lines[instance_at]] + lines[instance_at:]), 'duplicate raw INSTANCE'),
                         (frame(lines[:element_at] + lines[element_at + 1:]), 'full physical cells')]
            for mutated, reason in mutations:
                try:
                    project(mutated, side)
                except ValueError as error:
                    require(str(error) == reason, 'mutation reached incorrect guard: ' + str(error))
                else:
                    raise ValueError('corrupt raw snapshot accepted')
            report['sides'][side] = {'raw': fingerprint(source), 'projected': fingerprint(destination),
                                     'rows': projected.count(b'\n'), 'raw_guards': len(mutations)}
        report['status'] = 'projected'
    finally:
        with (output_dir / 'projection.json').open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
