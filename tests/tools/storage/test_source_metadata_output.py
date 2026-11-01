"""Check genuine source metadata output on fresh owned native paths."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
HEADER = bytes.fromhex('1f8b08000000000000ff')


def require(condition, message):
    if not condition:
        raise AssertionError(message)


class Nbt:
    """Independent bounded decoder; no production encoder/parser is imported."""
    def __init__(self, data):
        self.data, self.at, self.tags = data, 0, 0

    def take(self, count):
        require(0 <= count <= len(self.data) - self.at, 'NBT extent')
        value = self.data[self.at:self.at + count]
        self.at += count
        return value

    def number(self, fmt):
        return struct.unpack('>' + fmt, self.take(struct.calcsize(fmt)))[0]

    def text(self):
        raw, units, at = self.take(self.number('H')), [], 0
        while at < len(raw):
            byte = raw[at]
            if 1 <= byte <= 127:
                units.append(byte)
                at += 1
            elif byte & 224 == 192:
                require(at + 1 < len(raw) and raw[at + 1] & 192 == 128, 'MUTF8 pair')
                value = ((byte & 31) << 6) | (raw[at + 1] & 63)
                require(value == 0 or value >= 128, 'MUTF8 canonical pair')
                units.append(value)
                at += 2
            else:
                require(byte & 240 == 224 and at + 2 < len(raw) and
                        raw[at + 1] & 192 == 128 and raw[at + 2] & 192 == 128, 'MUTF8 triple')
                value = ((byte & 15) << 12) | ((raw[at + 1] & 63) << 6) | (raw[at + 2] & 63)
                require(value >= 2048, 'MUTF8 canonical triple')
                units.append(value)
                at += 3
        return units

    def payload(self, kind, depth):
        self.tags += 1
        require(depth <= 128 and self.tags <= 100000, 'NBT bounded graph')
        if kind in (1, 2, 3, 4, 5, 6):
            return self.number({1: 'b', 2: 'h', 3: 'i', 4: 'q', 5: 'I', 6: 'Q'}[kind])
        if kind == 7:
            length = self.number('i')
            require(0 <= length <= 1048576, 'byte array bound')
            return list(self.take(length))
        if kind == 8:
            return self.text()
        if kind == 9:
            element, length = self.number('B'), self.number('i')
            require(0 <= length <= 100000 and 0 <= element <= 10, 'list bound/type')
            return [element, [[element, self.payload(element, depth + 1)] for _ in range(length)]]
        require(kind == 10, 'compound type')
        fields = {}
        while True:
            child = self.number('B')
            if child == 0:
                return fields
            name = ''.join(chr(unit) for unit in self.text())
            require(name not in fields, 'unique compound names')
            fields[name] = [child, self.payload(child, depth + 1)]

    def root(self):
        kind, name = self.number('B'), self.text()
        require(kind == 10 and name == [], 'fresh unnamed Compound root')
        result = [kind, self.payload(kind, 1)]
        require(self.at == len(self.data), 'complete NBT/no trailing bytes')
        return result


def decode(path):
    require(path.stat().st_size <= 1048576, 'bounded compressed file extent')
    encoded = path.read_bytes()
    require(10 < len(encoded) <= 1048576 and encoded[:10] == HEADER, 'real exact GZIP header')
    reader = zlib.decompressobj(31)
    raw = reader.decompress(encoded, 2097153)
    require(len(raw) <= 2097152 and reader.eof and not reader.unused_data and
            not reader.unconsumed_tail, 'bounded complete GZIP/CRC/trailer')
    return Nbt(raw).root()


def expected(row):
    second = row['time'] == -1234567890123
    name = list(map(ord, 'Second' if second else 'First')) + [0, 937, 55357, 56832]
    result = [10, {'Data': [10, {
        'RandomSeed': [4, -9223372036854775807], 'SpawnX': [3, -17], 'SpawnY': [3, 65],
        'SpawnZ': [3, 1003], 'Time': [4, row['time']], 'SizeOnDisk': [4, 9876543210],
        'LastPlayed': [4, row['last_played']], 'LevelName': [8, name], 'version': [3, 19132],
        'rainTime': [3, -345], 'raining': [1, 1], 'thunderTime': [3, 678], 'thundering': [1, 0]
    }]}]
    if row['mode'] == 'player-array':
        result[1]['Data'][1]['Player'] = [10, {'Bytes': [7, list(array_payload())]}]
    return result


def array_payload():
    rng, data = 0x6d2b79f5, bytearray()
    for _ in range(65536):
        rng ^= (rng << 13) & 0xffffffff
        rng ^= rng >> 17
        rng ^= (rng << 5) & 0xffffffff
        data.append(rng & 255)
    return bytes(data)


def mutf8(units):
    encoded = bytearray()
    for value in units:
        if 1 <= value <= 127:
            encoded.append(value)
        elif value <= 2047:
            encoded.extend((192 | value >> 6, 128 | value & 63))
        else:
            encoded.extend((224 | value >> 12, 128 | value >> 6 & 63, 128 | value & 63))
    require(len(encoded) <= 65535, 'authored MUTF8 extent')
    return struct.pack('>H', len(encoded)) + encoded


def head(kind, name):
    return bytes((kind,)) + mutf8(tuple(map(ord, name)))


def authored_array_prefix(row):
    """Native supplied compound order before the one failing ByteArray span."""
    prefix = bytearray(head(10, '') + head(10, 'Data'))
    for name, (kind, value) in expected(row)[1]['Data'][1].items():
        require(kind in (1, 3, 4, 8), 'authored ordinary metadata scalar')
        prefix.extend(head(kind, name))
        prefix.extend(mutf8(value) if kind == 8 else struct.pack({1: '>b', 3: '>i', 4: '>q'}[kind], value))
    prefix.extend(head(10, 'Player') + head(7, 'Bytes') + struct.pack('>i', 65536))
    return bytes(prefix)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    source = args.source_root.resolve()
    helper = source / 'tests/reference/client/statistics/support.py'
    spec = importlib.util.spec_from_file_location('metadata_output_transport', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='metadata-output-', dir=args.work_dir.resolve()))
    snapshots = work / 'snapshots'
    snapshots.mkdir()
    cases = {}
    operation_names = ['old_exists', 'old_delete', 'old_rename', 'current_exists',
                       'current_delete', 'new_rename', 'new_exists', 'new_delete']

    def run(label, mode, root=None):
        case_dir = work / label
        case_dir.mkdir()
        if root is None:
            root = case_dir / 'owned'
            root.mkdir()
        command = [str(args.executable.resolve()), str(root), mode]
        result = support.run_bounded(command, case_dir, 'native', timeout=30, limit=1048576)
        require(result.returncode == 0 and result.stderr == b'', f'{label}: complete native exit0/empty stderr')
        rows = result.stdout.replace(b'\r\n', b'\n').splitlines()
        require(len(rows) == 1 and rows[0].startswith(b'METADATA '), f'{label}: one native receipt')
        parsed = dict(piece.split('=', 1) for piece in rows[0].decode('ascii').split()[1:])
        require(parsed['mode'] == mode and re.fullmatch('[01]{8}', parsed['calls']) and
                re.fullmatch('[01]{8}', parsed['values']), f'{label}: exact receipt schema')
        row = {key: value if key in ('mode', 'calls', 'values') else int(value)
               for key, value in parsed.items()}
        require(row['checks'] > 0 and row['first'] <= row['last_played'] <= row['last'],
                f'{label}: genuine independently bounded LastPlayed clock')
        world = root / 'world'
        require((world / 'session.lock').is_file() and (world / 'session.lock').stat().st_size == 8,
                f'{label}: real source session output')
        emitted_files = [path for path in sorted(world.rglob('*')) if path.is_file()]
        for path in emitted_files:
            require(path.stat().st_size <= 1048576, f'{label}: bounded emitted file extent')
        files = {path.relative_to(world).as_posix(): support.fingerprint(path)
                 for path in emitted_files}
        decoded = {}
        for name in ('level.dat', 'level.dat_old', 'level.dat_new'):
            path = world / name
            if path.is_file():
                shutil.copyfile(path, snapshots / f'{label}-{name}')
        item = {'mode': mode, 'root': str(root), 'world': str(world), 'result': row,
                'source_file_returns': [[name, parsed['values'][i] == '1']
                                        for i, name in enumerate(operation_names) if parsed['calls'][i] == '1'],
                'files': files, 'decoded': decoded, 'capture': result.capture_report,
                'stdout': support.fingerprint(case_dir / 'native.stdout'),
                'stderr': support.fingerprint(case_dir / 'native.stderr')}
        cases[label] = item
        return item

    def checked_current(item):
        tree = decode(Path(item['world']) / 'level.dat')
        require(tree == expected(item['result']), f"{item['mode']}: complete typed Data-only tree")
        item['decoded']['level.dat'] = tree
        require(not (Path(item['world']) / 'level.dat_new').exists(), 'no leftover source new file')
        return (Path(item['world']) / 'level.dat').read_bytes()

    fresh = run('fresh', 'first')
    checked_current(fresh)
    existing_root = work / 'existing-owned'
    (existing_root / 'world').mkdir(parents=True)
    (existing_root / 'world/level.dat').write_bytes(b'obsolete current')
    (existing_root / 'world/level.dat_old').write_bytes(b'obsolete old')
    initial = run('existing-first', 'existing-first', existing_root)
    initial_bytes = checked_current(initial)
    require((existing_root / 'world/level.dat_old').read_bytes() == b'obsolete current', 'first existing old exact bytes')
    newer = run('existing-second', 'second', existing_root)
    checked_current(newer)
    require((existing_root / 'world/level.dat_old').read_bytes() == initial_bytes, 'exact preceding current preserved as old')
    newer['decoded']['level.dat_old'] = decode(existing_root / 'world/level.dat_old')

    old_root = work / 'old-obstruction-owned'
    (old_root / 'world/level.dat_old').mkdir(parents=True)
    (old_root / 'world/level.dat_old/sentinel').write_bytes(b'old sentinel')
    (old_root / 'world/level.dat').write_bytes(initial_bytes)
    old = run('old-directory', 'old-obstruction', old_root)
    checked_current(old)
    require((old_root / 'world/level.dat_old/sentinel').read_bytes() == b'old sentinel', 'old directory sentinel preserved')

    both_root = work / 'both-obstructions-owned'
    for name in ('level.dat', 'level.dat_old'):
        (both_root / 'world' / name).mkdir(parents=True, exist_ok=True)
        (both_root / 'world' / name / 'sentinel').write_bytes((name + ' sentinel').encode('ascii'))
    both = run('both-directories', 'both-obstructions', both_root)
    require(not (both_root / 'world/level.dat_new').exists(), 'ignored final rename refusal still deletes new')
    for name in ('level.dat', 'level.dat_old'):
        require((both_root / 'world' / name / 'sentinel').read_bytes() == (name + ' sentinel').encode('ascii'), 'obstruction sentinels unchanged')

    failed_root = work / 'open-failure-owned'
    (failed_root / 'world/level.dat_new').mkdir(parents=True)
    (failed_root / 'world/level.dat_new/sentinel').write_bytes(b'new sentinel')
    run('open-failure', 'open-failure', failed_root)
    require((failed_root / 'world/level.dat_new/sentinel').read_bytes() == b'new sentinel' and
            not (failed_root / 'world/level.dat').exists(), 'FOS failure has no rotation')
    checked_current(run('null-first', 'null-first'))
    checked_current(run('retained', 'retained'))
    checked_current(run('player-array', 'player-array'))
    repeated = run('repeat', 'repeat')
    repeated_bytes = checked_current(repeated)
    require((Path(repeated['world']) / 'level.dat_old').read_bytes() == repeated_bytes,
            'same Handler repeated completed-record output and prior receipt cleanup')

    header = run('header-failure', 'header-failure')
    header_world = Path(header['world'])
    require(not (header_world / 'level.dat').exists() and not (header_world / 'level.dat_old').exists(),
            'failed constructor/header has no rotation')
    require((header_world / 'level.dat_new').read_bytes() == HEADER[:3] and
            header['result']['calls'] == '00000000' and header['result']['prior_pc'] == 9 and
            header['result']['close_pc'] == -1 and header['result']['write_fault'] == 1 and
            header['result']['close_fault'] == 0 and header['result']['raw_bytes'] == 0 and
            header['result']['compressed_bytes'] == 3,
            'real constructor/header three-byte prefix, no DOS/finally/source close')

    for mode in ('write-finally', 'write-finally-close'):
        item = run(mode, mode)
        row, world = item['result'], Path(item['world'])
        require(not (world / 'level.dat').exists() and not (world / 'level.dat_old').exists() and
                row['calls'] == '00000000', 'source exception/finally has no rotation')
        new_file = world / 'level.dat_new'
        require(new_file.stat().st_size <= 1048576, 'bounded retained partial gzip')
        data = new_file.read_bytes()
        prefix = authored_array_prefix(row)
        raw_count = len(prefix) + 65536
        crc = zlib.crc32(prefix)
        require(len(data) > 13 and data[:10] == HEADER and len(data) == row['compressed_bytes'],
                'actual header plus three-byte write prefix and same-stream finally output')
        require(row['raw_bytes'] == raw_count and row['written'] == len(prefix) and row['crc'] == crc and
                struct.unpack('<II', data[-8:]) == (crc, raw_count) and row['write_fault'] == 1 and
                row['close_pc'] == 30 and row['prior_pc'] == (30 if mode.endswith('-close') else 18) and
                row['close_fault'] == int(mode.endswith('-close')),
                'finally30 drains the same remaining array, retains failed-call CRC/count and replacement site')
        item['partial_output'] = {'raw_input_bytes_consumed': raw_count, 'crc_before_failed_array': crc,
                                  'completed_NBT_or_valid_gzip_claim': False,
                                  'scope': 'Real partial IO removes bytes; retained trailer and source close state only.'}

    for mode in ('codec-limit', 'cleanup-failure', 'finish-limit', 'utf-limit', 'gzip-limit',
                 'literal-4', 'literal-5', 'literal-6', 'literal-7', 'close-failure'):
        item = run(mode, mode)
        world = Path(item['world'])
        require(not (world / 'level.dat').exists() and not (world / 'level.dat_old').exists(),
                f'{mode}: no falsely credited rotation')
        new_file = world / 'level.dat_new'
        if mode in ('codec-limit', 'cleanup-failure', 'finish-limit', 'utf-limit'):
            require(new_file.read_bytes() == HEADER, f'{mode}: exact reached header, no native cleanup finish')
        elif mode == 'gzip-limit':
            require(new_file.read_bytes() == b'', 'deflater limit before header')
        elif mode.startswith('literal-'):
            require(not new_file.exists(), 'literal prefix before FOS')
        else:
            tree = decode(new_file)
            require(tree == expected(item['result']), 'consumed close failure retains complete unrotated file')
            item['decoded']['level.dat_new'] = tree

    report = {'status': 'PASS_AUTHORED_NATIVE_SOURCE_METADATA_OUTPUT', 'cases': cases,
              'source_root': str(source), 'executable': support.fingerprint(args.executable.resolve()),
              'fixture_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'retained_work': str(work), 'original_parity_claim': False,
              'scope': 'Genuine source Store/Files/FOS output from actual completed Info records; independent bounded decoder; native fault controls. Existing-first/second use separate process handlers; repeat uses one actual Handler. Selected Player ByteArray is authored, not an actual LocalPlayer/List serializer. Constructor/header, write/finally30, replacement finally-close and normal close22 controls are distinct. Native World/List/LocalPlayer source callers and original comparison remain separate.'}
    (work / 'REPORT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'retained': str(work), 'profiles': len(cases),
                      'checks': sum(item['result']['checks'] for item in cases.values())}))


if __name__ == '__main__':
    main()
