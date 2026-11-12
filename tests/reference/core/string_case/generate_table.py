"""Turn qualified JDK17 simple-fold scalar observations into the runtime table."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def observations(path):
    data = path.read_bytes()
    if len(data) != 0x110000 * 4:
        raise ValueError('Complete Unicode codepoint inventory required')
    values = [word[0] for word in struct.iter_unpack('>I', data)]
    if any(value > 0x10ffff or values[value] != value for value in values):
        raise ValueError('Bounded idempotent scalar folds required')
    return data, values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fold', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    data, values = observations(args.fold)
    changed = [(point, value) for point, value in enumerate(values) if point != value]
    non_ascii = [(point, value) for point, value in changed if point >= 0x80]
    fnv = 0xcbf29ce484222325
    for value in data:
        fnv = ((fnv ^ value) * 0x100000001b3) & 0xffffffffffffffff
    lines = [
        '/* Qualified JDK17 Character.toLowerCase(Character.toUpperCase(int))',
        ' * scalar observations. ASCII folds are handled directly by the caller;',
        ' * other unlisted codepoints map to themselves.',
        ' * Reproduce with tests/reference/core/string_case/. */',
        'static const mc_string_case_pair mc_string_case_folds[] = {',
    ]
    entries = [f'{{UINT32_C(0x{point:06x}), UINT32_C(0x{value:06x})}},'
               for point, value in non_ascii]
    lines.extend('    ' + ' '.join(entries[index:index + 2])
                 for index in range(0, len(entries), 2))
    lines.append('};')
    args.output.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'codepoints': len(values), 'changed': len(changed), 'non_ascii': len(non_ascii),
                      'fold_sha256': hashlib.sha256(data).hexdigest(),
                      'fold_fnv64': f'{fnv:016x}',
                      'table_sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()}))


if __name__ == '__main__':
    main()
