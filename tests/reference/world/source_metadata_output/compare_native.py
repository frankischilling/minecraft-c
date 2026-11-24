"""Compare reached source metadata transport with independently observed original calls."""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import struct
import sys
import zlib

sys.dont_write_bytecode = True


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fingerprint(path):
    require(path.stat().st_size <= 16777216, 'bounded comparison artifact')
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def decode(path):
    """Independent selected metadata decoder; no production/native controller import."""
    require(path.stat().st_size <= 65536, 'bounded compressed metadata extent')
    packed = path.read_bytes()
    require(10 < len(packed) <= 65536 and packed[:10] == bytes.fromhex('1f8b08000000000000ff'),
            'bounded gzip file with reached JDK header')
    reader = zlib.decompressobj(31)
    raw = reader.decompress(packed, 1048577)
    require(len(raw) <= 1048576 and reader.eof and not reader.unused_data and
            not reader.unconsumed_tail, 'complete gzip member, CRC and trailer')
    position, tags = 0, 0

    def take(count):
        nonlocal position
        require(0 <= count <= len(raw) - position, 'bounded NBT extent')
        value = raw[position:position + count]
        position += count
        return value

    def number(fmt):
        return struct.unpack(fmt, take(struct.calcsize(fmt)))[0]

    def text():
        data, units, at = take(number('>H')), [], 0
        while at < len(data):
            byte = data[at]
            at += 1
            if 1 <= byte <= 127:
                units.append(byte)
            elif byte & 224 == 192:
                require(at < len(data) and data[at] & 192 == 128, 'MUTF8 pair')
                value = ((byte & 31) << 6) | (data[at] & 63)
                at += 1
                require(value == 0 or value >= 128, 'shortest MUTF8 pair')
                units.append(value)
            else:
                require(byte & 240 == 224 and at + 1 < len(data) and
                        data[at] & 192 == 128 and data[at + 1] & 192 == 128, 'MUTF8 triple')
                value = ((byte & 15) << 12) | ((data[at] & 63) << 6) | (data[at + 1] & 63)
                at += 2
                require(value >= 2048, 'shortest MUTF8 triple')
                units.append(value)
        return units

    def payload(kind, depth):
        nonlocal tags
        tags += 1
        require(depth <= 64 and tags <= 10000, 'bounded metadata graph')
        if kind in (1, 3, 4):
            return [kind, number({1: '>b', 3: '>i', 4: '>q'}[kind])]
        if kind == 8:
            return [kind, text()]
        require(kind == 10, 'selected metadata tag domain')
        result = {}
        while True:
            child = number('>B')
            if child == 0:
                return [kind, result]
            units = text()
            require(all(0 < unit < 128 for unit in units), 'ASCII metadata keys')
            name = ''.join(map(chr, units))
            require(name not in result, 'unique metadata keys')
            result[name] = payload(child, depth + 1)

    kind, name = number('>B'), text()
    tree = payload(kind, 1)
    require(kind == 10 and name == [] and position == len(raw), 'single unnamed complete root')
    return tree


def without_sampled_clock(tree):
    value = copy.deepcopy(tree)
    require(value[0] == 10 and set(value[1]) == {'Data'} and value[1]['Data'][0] == 10,
            'actual Data-only compound root')
    data = value[1]['Data'][1]
    require(data['LastPlayed'][0] == 4, 'sampled clock retains Long type')
    del data['LastPlayed']
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-receipt', type=Path, required=True)
    parser.add_argument('--native-report', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    reference_path, native_path = args.reference_receipt.resolve(), args.native_report.resolve()
    require(reference_path.stat().st_size <= 16777216 and
            native_path.stat().st_size <= 16777216, 'bounded comparison receipts')
    reference = json.loads(reference_path.read_text())
    native = json.loads(native_path.read_text())
    require(reference['status'] == 'OBSERVED_AND_AUDITED_ORIGINAL_METADATA_OUTPUT_PREPARED_BOUNDARY' and
            reference['inputs_unchanged'] and reference['ordinary_exit'] == reference['debugger_exit'] == 0,
            'qualified ordinary/independent-JDI original receipt')
    require(native['status'] == 'PASS_AUTHORED_NATIVE_SOURCE_METADATA_OUTPUT',
            'complete authored source metadata native controller receipt')
    mapping = {'fresh': 0, 'existing-first': 1, 'existing-second': 2, 'old-directory': 3,
               'both-directories': 4, 'open-failure': 5, 'null-first': 6}
    names = {84: 'old_exists', 92: 'old_delete', 100: 'old_rename', 106: 'current_exists',
             114: 'current_delete', 122: 'new_rename', 128: 'new_exists', 136: 'new_delete'}
    source_paths = {'old_exists': 'level.dat_old', 'old_delete': 'level.dat_old',
                    'old_rename': 'level.dat', 'current_exists': 'level.dat',
                    'current_delete': 'level.dat', 'new_rename': 'level.dat_new',
                    'new_exists': 'level.dat_new', 'new_delete': 'level.dat_new'}
    comparisons = {}
    for label, phase in mapping.items():
        item = native['cases'][label]
        row = item['result']
        require(row['first'] <= row['last_played'] <= row['last'],
                label + ': native real clock bounded within its own call')
        expected_calls = []
        for call in reference['source_file_returns'][str(phase)]:
            key = names[call['caller']['pc']]
            require(call['path'].replace('\\', '/').rsplit('/', 1)[-1] == source_paths[key],
                    label + ': actual source File receiver path')
            expected_calls.append([key, call['result']])
        require(item['source_file_returns'] == expected_calls,
                label + ': exact ordered ignored source booleans')
        comparison = {'original_phase': phase, 'file_returns': expected_calls,
                      'native_clock_interval': [row['first'], row['last']],
                      'native_last_played': row['last_played']}
        if label in ('fresh', 'existing-first', 'existing-second', 'old-directory', 'null-first'):
            path = native_path.parent / 'snapshots' / (label + '-level.dat')
            require(fingerprint(path) == item['files']['level.dat'],
                    label + ': retained native snapshot matches actual emitted File receipt')
            tree = decode(path)
            require(tree == item['decoded']['level.dat'], label + ': independent native re-decode')
            require(tree[1]['Data'][1]['LastPlayed'] == [4, row['last_played']],
                    label + ': emitted native sampled clock equals reached record value')
            for mode in ('ordinary', 'debugger'):
                original = reference[mode]['decoded'][str(phase)]
                original_row = reference[mode]['calls'][phase]
                original_path = reference_path.parent / mode / 'snapshots' / f'{phase}-level.dat'
                require(fingerprint(original_path) == {key: original_row['files']['level.dat'][key]
                                                       for key in ('bytes', 'sha256')},
                        label + ': retained original snapshot matches actual emitted File receipt')
                require(decode(original_path) == original, label + ': independent original re-decode')
                require(original_row['beforeMillis'] <= original[1]['Data'][1]['LastPlayed'][1] <=
                        original_row['afterMillis'], label + ': independent original clock interval')
                require(without_sampled_clock(tree) == without_sampled_clock(original),
                        label + ': exact typed values against independent original VM')
            comparison['native_file'] = fingerprint(path)
            comparison['exact_stable_typed_values'] = True
        # Each controller independently checked original/native before/after
        # sentinels. Compare their retained actual content fingerprints too.
        original_files = reference['ordinary']['calls'][phase]['files']
        sentinels = [name for name in original_files if name.endswith('/sentinel')]
        original_file_paths = {name for name, value in original_files.items() if value['kind'] == 'file'}
        require(set(item['files']) == original_file_paths,
                label + ': exact resulting file paths, including no leftover new file')
        for name in original_file_paths - {'session.lock', 'level.dat', 'level.dat_old', 'level.dat_new'}:
            require(name.endswith('/sentinel'), label + ': only controlled obstruction files')
        for name in sentinels:
            require(item['files'][name] == {key: original_files[name][key]
                                           for key in ('bytes', 'sha256')},
                    label + ': exact actual obstruction sentinel content')
        comparison['exact_sentinel_paths'] = sentinels
        comparisons[label] = comparison
    current = native_path.parent / 'snapshots/existing-first-level.dat'
    old = native_path.parent / 'snapshots/existing-second-level.dat_old'
    require(current.read_bytes() == old.read_bytes(), 'native exact intra-run preceding-current backup')
    for mode in ('ordinary', 'debugger'):
        prior = reference_path.parent / mode / 'snapshots/1-level.dat'
        backup = reference_path.parent / mode / 'snapshots/2-level.dat_old'
        require(prior.read_bytes() == backup.read_bytes(),
                mode + ': original exact intra-run preceding-current backup')
    report = {
        'status': 'PASS_REACHED_SOURCE_METADATA_TRANSPORT_ORIGINAL_COMPARISON',
        'reference_receipt': fingerprint(reference_path), 'native_report': fingerprint(native_path),
        'comparison_recipe': fingerprint(Path(__file__).resolve()), 'cases': comparisons,
        'clock_policy': 'Only LastPlayed differs across processes; every actual value retains Long type and is checked within its own actual call interval.',
        'backup_policy': 'Exact preceding-current bytes preserved within each original/native cohort; gzip bytes/compound iteration are not equated across implementations.',
        'boolean_receipt_policy': 'Original call order comes from direct fm caller MethodExit events. Native receipts expose reached File call flags/results and the controller lists them in the reviewed implementation source order; no separate native per-primitive trace is claimed.',
        'scope': 'Prepared completed-Info source File/FOS metadata transport and source ignored-booleans/effects only. Native empty/null-first selected-null records match original List omission values; no native List or LocalPlayer serializer call is qualified. Existing-first/second native calls use separate process Handlers; native repeat control separately exercises one Handler.',
        'failure_limit': 'Original FOS-open failure catches/prints/returns150. Native C retains that actual FOS cause at explicit fm147 printStackTrace dependency. Only the common pre-rotation prefix/effects and lack of File rotations are compared; native catch return completion is not credited.',
        'not_qualified': ['whole World/Chunk save or reload', 'nonempty live Player serialization',
                          'HashMap order or compressed byte identity', 'native codec/JDK direct payload controls',
                          'authored source write/close fault OS equivalence']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'cases': len(mapping), 'output': str(args.output)}))


if __name__ == '__main__':
    main()
