"""Actual source World saves/reload on authored disposable files, independently decoded."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
BOUND = 1048576


def require(value, message):
    if not value:
        raise AssertionError(message)


def read(path):
    require(path.is_file() and path.stat().st_size <= BOUND, 'bounded actual selected file')
    return path.read_bytes()


def fingerprint(path):
    data = read(path)
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def text(value):
    raw = value.encode('ascii')
    return struct.pack('>H', len(raw)) + raw


def named(kind, key, body):
    return bytes([kind]) + text(key) + body


def compound(fields):
    return b''.join(fields) + b'\0'


def stack(count, damage, slot=False):
    fields = [named(2, 'id', struct.pack('>h', 262)), named(1, 'Count', struct.pack('>b', count)),
              named(2, 'Damage', struct.pack('>h', damage)), named(4, 'ItemExtra', struct.pack('>q', 1101))]
    if slot:
        fields.insert(0, named(1, 'Slot', b'\0'))
    return compound(fields)


def item():
    return compound([named(8, 'id', text('Item')), named(9, 'Pos', b'\6' + struct.pack('>i3d', 3, 8.25, 64., 8.25)),
        named(9, 'Motion', b'\6' + struct.pack('>i3d', 3, 0., 0., 0.)),
        named(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 0.)),
        named(2, 'Health', struct.pack('>h', 7)), named(2, 'Age', struct.pack('>h', 19)),
        named(10, 'Item', stack(5, 7)), named(4, 'EntityExtra', struct.pack('>q', 101))])


def chest():
    return compound([named(8, 'id', text('Chest')), named(3, 'x', struct.pack('>i', 8)),
        named(3, 'y', struct.pack('>i', 64)), named(3, 'z', struct.pack('>i', 8)),
        named(9, 'Items', b'\12' + struct.pack('>i', 1) + stack(7, 3, True)),
        named(3, 'TileExtra', struct.pack('>i', 77))])


def seed(mode):
    metadata = compound([named(8, 'LevelName', text('Disk name')), named(4, 'RandomSeed', struct.pack('>q', 23)),
        named(4, 'Time', struct.pack('>q', 6000)), named(4, 'LastPlayed', struct.pack('>q', 1304000000000)),
        named(4, 'SizeOnDisk', struct.pack('>q', 4096)), named(3, 'SpawnX', struct.pack('>i', 8)),
        named(3, 'SpawnY', struct.pack('>i', 70)), named(3, 'SpawnZ', struct.pack('>i', 9)),
        named(3, 'version', struct.pack('>i', 19132)), named(3, 'MetadataExtra', struct.pack('>i', 777))])
    info = named(10, '', compound([named(10, 'Data', metadata), named(3, 'MetadataRootExtra', struct.pack('>i', 99))]))
    blocks = bytearray(32768)
    if mode == 'item-chest':
        blocks[(8 << 11) | (8 << 7) | 64] = 54
    entities = [item()] if mode in ('item-chest', 'null-stack', 'opaque') else []
    if mode == 'opaque':
        entities.append(compound([named(8, 'id', text('NativeUnknown')), named(4, 'UnknownExtra', struct.pack('>q', 10))]))
    tiles = [chest()] if mode == 'item-chest' else []
    fields = [named(3, 'xPos', bytes(4)), named(3, 'zPos', bytes(4)), named(4, 'LastUpdate', struct.pack('>q', 6000))]
    for key, data in [('Blocks', blocks), ('Data', bytes(16384)), ('SkyLight', bytes(16384)),
                      ('BlockLight', bytes(16384)), ('HeightMap', bytes([64]) * 256)]:
        fields.append(named(7, key, struct.pack('>i', len(data)) + data))
    fields += [named(1, 'TerrainPopulated', b'\1'),
               named(9, 'Entities', b'\12' + struct.pack('>i', len(entities)) + b''.join(entities)),
               named(9, 'TileEntities', b'\12' + struct.pack('>i', len(tiles)) + b''.join(tiles)),
               named(4, 'LevelExtra', struct.pack('>q', 902))]
    chunk = named(10, '', compound([named(4, 'RootExtra', struct.pack('>q', 901)), named(10, 'Level', compound(fields))]))
    compressed = zlib.compress(chunk)
    require(len(compressed) + 5 < 4096, 'authored initial record occupies one sector')
    image = bytearray(12288)
    struct.pack_into('>I', image, 0, (2 << 8) | 1)
    struct.pack_into('>i', image, 4096, 123)
    record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
    image[8192:8192 + len(record)] = record
    return gzip.compress(info, mtime=0), image


class Nbt:
    def __init__(self, data):
        self.data, self.at, self.tags = data, 0, 0

    def take(self, n):
        require(0 <= n <= len(self.data) - self.at, 'bounded NBT extent')
        value = self.data[self.at:self.at + n]
        self.at += n
        return value

    def number(self, fmt):
        return struct.unpack('>' + fmt, self.take(struct.calcsize(fmt)))[0]

    def string(self):
        raw, units, at = self.take(self.number('H')), [], 0
        while at < len(raw):
            lead = raw[at]
            at += 1
            if 1 <= lead <= 127:
                units.append(lead)
            elif lead & 224 == 192:
                require(at < len(raw) and raw[at] & 192 == 128, 'MUTF8 continuation')
                value = ((lead & 31) << 6) | (raw[at] & 63)
                at += 1
                require(value == 0 or value >= 128, 'canonical MUTF8 pair')
                units.append(value)
            else:
                require(lead & 240 == 224 and at + 1 < len(raw) and raw[at] & 192 == raw[at + 1] & 192 == 128,
                        'MUTF8 triple')
                value = ((lead & 15) << 12) | ((raw[at] & 63) << 6) | (raw[at + 1] & 63)
                at += 2
                require(value >= 2048, 'canonical MUTF8 triple')
                units.append(value)
        return ''.join(map(chr, units))

    def payload(self, kind, depth):
        self.tags += 1
        require(depth <= 64 and self.tags <= 10000, 'bounded NBT graph')
        if kind in range(1, 7):
            return self.number({1: 'b', 2: 'h', 3: 'i', 4: 'q', 5: 'I', 6: 'Q'}[kind])
        if kind == 7:
            n = self.number('i')
            require(0 <= n <= BOUND, 'bounded ByteArray')
            return self.take(n)
        if kind == 8:
            return self.string()
        if kind == 9:
            child, n = self.number('B'), self.number('i')
            require(0 <= child <= 10 and 0 <= n <= 10000, 'bounded typed list')
            return [child, [self.payload(child, depth + 1) for _ in range(n)]]
        require(kind == 10, 'actual Compound type')
        result = {}
        while True:
            child = self.number('B')
            if child == 0:
                return result
            key = self.string()
            require(key not in result, 'unique Compound names')
            result[key] = [child, self.payload(child, depth + 1)]

    def root(self):
        require(self.number('B') == 10 and self.string() == '', 'single unnamed Compound root')
        result = self.payload(10, 1)
        require(self.at == len(self.data), 'complete NBT/no trailing bytes')
        return result


def inflate(data, window):
    decoder = zlib.decompressobj(window)
    raw = decoder.decompress(data, BOUND + 1)
    require(len(raw) <= BOUND and decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail,
            'complete bounded compressed member/checksum/trailer')
    return Nbt(raw).root()


def region(path):
    data = read(path)
    require(8192 <= len(data) <= BOUND and len(data) % 4096 == 0, 'complete bounded region extent')
    offsets = struct.unpack_from('>1024I', data, 0)
    require(offsets[0] and not any(offsets[1:]), 'one actual selected Chunk slot')
    sector, count = offsets[0] >> 8, offsets[0] & 255
    require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(data), 'complete selected allocation')
    at = sector * 4096
    n = struct.unpack_from('>i', data, at)[0]
    require(1 < n <= count * 4096 - 4 and data[at + 4] == 2, 'actual selector2 body extent')
    return inflate(data[at + 5:at + 4 + n], 15), struct.unpack_from('>i', data, 4096)[0], len(data)


def edited_blocks(objects):
    state, blocks = 0x87654321, bytearray()
    for _ in range(32768):
        state ^= (state << 13) & 0xffffffff
        state ^= state >> 17
        state ^= (state << 5) & 0xffffffff
        blocks.append(state & 1)
    if objects:
        blocks[(8 << 11) | (8 << 7) | 64] = 54
    return blocks


def compact(value):
    if isinstance(value, bytes):
        return {'bytes': len(value), 'sha256': hashlib.sha256(value).hexdigest()}
    if isinstance(value, dict):
        return {key: compact(item) for key, item in value.items()}
    if isinstance(value, list):
        return [compact(item) for item in value]
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve()
    helper = source / 'tests/reference/client/statistics/support.py'
    spec = importlib.util.spec_from_file_location('source_save_transport', helper)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    selected_inputs = [Path(__file__).resolve(), helper, args.executable.resolve(),
        source / 'tests/world/runtime/saving/test_source_saving.c',
        source / 'tests/world/runtime/tile_entities/test_tile_entities.c']
    def freeze():
        require(all(p.is_file() and p.stat().st_size <= 268435456 for p in selected_inputs),
                'bounded fixture/helper/native product inputs')
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in selected_inputs}
    before_inputs = freeze()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='source-world-saving-', dir=args.work_dir.resolve()))
    (work / 'INPUT-FREEZE.json').write_text(json.dumps(before_inputs, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'retained_work': str(work), 'stage': 'FROZEN_BEFORE_NATIVE_EXECUTION'}), flush=True)
    cases = {}
    for mode in ('empty', 'item-chest', 'session-mismatch', 'opaque', 'null-stack', 'null-m', 'null-l'):
        directory = work / mode
        owned = directory / 'owned'
        require(str(owned).isascii(), 'selected native fixture has an ASCII owned path')
        (owned / 'world/region').mkdir(parents=True)
        (owned / 'snapshots').mkdir()
        info, image = seed(mode)
        (owned / 'world/level.dat').write_bytes(info)
        (owned / 'world/region/r.0.0.mcr').write_bytes(image)
        (owned / 'world/preserve.bin').write_bytes(b'unrelated disposable World file\0\xff')
        capture = support.run_bounded([str(args.executable.resolve()), str(owned), mode], directory,
                                      'native', timeout=90, limit=BOUND)
        require(capture.returncode == 0 and capture.stderr == b'', mode + ': native complete exit0/empty stderr')
        lines = capture.stdout.decode('ascii').splitlines()
        console = [line for line in lines if not line.startswith('{')]
        require(all(line in ('16 achievements', '151 recipes', 'Skipping Entity with id NativeUnknown')
                    for line in console), 'only actual Registry/unknown-Entity console lines')
        rows = [json.loads(line) for line in lines if line.startswith('{')]
        require(rows[-1]['event'] == 'complete' and rows[-1]['mode'] == mode and rows[-1]['cleanup'] == 1,
                mode + ': complete actual save/cleanup receipt')
        states = {row['phase']: row for row in rows[:-1]}
        require(len(states) == len(rows) - 1 and 'loaded' in states, 'unique actual state events')
        snapshots = {path.name: fingerprint(path) for path in sorted((owned / 'snapshots').iterdir())}
        decoded = {}
        if mode in ('empty', 'item-chest'):
            require(set(states) == {'loaded', 'saved-first', 'saved-second', 'reloaded'} and rows[-1]['reload'] == 1,
                    'actual World save twice, native teardown, new source World load')
            loaded, first, second = [states[key] for key in ('loaded', 'saved-first', 'saved-second')]
            for phase in ('saved-first', 'saved-second'):
                row = states[phase]
                tree, stamp, extent = region(owned / 'snapshots' / (phase + '-region.mcr'))
                require(set(tree) == {'Level'} and tree['Level'][0] == 10, 'fresh source root/Level without native overlays')
                level = tree['Level'][1]
                expected_keys = {'xPos', 'zPos', 'LastUpdate', 'Blocks', 'Data', 'SkyLight', 'BlockLight',
                                 'HeightMap', 'TerrainPopulated', 'Entities', 'TileEntities'}
                require(set(level) == expected_keys and level['xPos'] == [3, 0] and level['zPos'] == [3, 0] and
                        level['LastUpdate'] == [4, 20000], 'exact fresh source Chunk schema/time')
                require(level['Blocks'] == [7, edited_blocks(mode == 'item-chest')], 'all actual edited Block bytes')
                for key, n in [('Data', 16384), ('SkyLight', 16384), ('BlockLight', 16384), ('HeightMap', 256)]:
                    require(level[key] == [7, bytes([64 if key == 'HeightMap' else 0]) * n],
                            'actual complete authored source array ' + key)
                entities, tiles = level['Entities'][1][1], level['TileEntities'][1][1]
                require(len(entities) == len(tiles) == (mode == 'item-chest'), 'actual finite live Entity/TileEntity output')
                if mode == 'item-chest':
                    require(entities[0]['id'] == [8, 'Item'] and 'EntityExtra' not in entities[0] and
                            entities[0]['Item'][1] == {'id': [2, 262], 'Count': [1, 5], 'Damage': [2, 7]},
                            'direct actual Item and Stack source fields without retained extras')
                    require(tiles[0]['id'] == [8, 'Chest'] and 'TileExtra' not in tiles[0], 'actual fresh Chest source writer')
                    items = tiles[0]['Items'][1][1]
                    require(items == [{'Slot': [1, 0], 'id': [2, 262], 'Count': [1, 7], 'Damage': [2, 3]}],
                            'actual Chest inventory source fields without Stack overlay')
                require(row['first'] // 1000 <= stamp <= row['last'] // 1000 and
                        all(row[key] for key in ('root', 'level', 'attached', 'prefix', 'entities', 'tiles', 'written',
                                                  'closed', 'returned', 'has_output', 'output_closed', 'commit', 'commit_returned')) and
                        not row['failed'] and row['source_closes'] == 1 and not row['dirty'] and row['last_save'] == 20000,
                        'actual returned World/LD/source output and cache save effects')
                metadata = inflate(read(owned / 'snapshots' / (phase + '-level.dat')), 31)
                require(set(metadata) == {'Data'} and metadata['Data'][0] == 10, 'fresh source metadata root')
                data = metadata['Data'][1]
                require(set(data) == {'RandomSeed', 'SpawnX', 'SpawnY', 'SpawnZ', 'Time', 'SizeOnDisk',
                                     'LastPlayed', 'LevelName', 'version', 'rainTime', 'raining',
                                     'thunderTime', 'thundering'} and
                        data['RandomSeed'] == [4, 23] and data['SpawnX'] == [3, 8] and
                        data['SpawnY'] == [3, 70] and data['SpawnZ'] == [3, 9] and
                        data['LevelName'] == [8, 'SourceSave'] and data['Time'] == [4, 20000] and
                        data['version'] == [3, 19132] and 'Player' not in data and 'MetadataExtra' not in data and
                        row['first'] <= data['LastPlayed'][1] <= row['last'], 'actual source metadata before cached Chunk saves')
                decoded[phase] = {'chunk': compact(tree), 'metadata': compact(metadata), 'timestamp': stamp, 'extent': extent}
            growth = decoded['saved-first']['extent'] - len(image)
            require(growth > 0 and first['growth'] == growth and first['size'] == loaded['size'] + growth and
                    second['growth'] == 0 and second['size'] == first['size'], 'actual late signed region growth added to live Info once')
            require(decoded['saved-first']['metadata']['Data'][1]['SizeOnDisk'] == [4, loaded['size']] and
                    decoded['saved-second']['metadata']['Data'][1]['SizeOnDisk'] == [4, first['size']],
                    'first metadata precedes Chunk growth; second save persists grown Info size')
            require(states['reloaded']['size'] == first['size'] and states['reloaded']['time'] == 20000 and
                    snapshots['reloaded-region.mcr'] == snapshots['saved-second-region.mcr'] and
                    snapshots['reloaded-level.dat'] == snapshots['saved-second-level.dat'],
                    'new actual source World/Chunk loads same unchanged committed files')
        else:
            require(set(states) == {'loaded', 'failed'} and rows[-1]['reload'] == 0, 'explicit failing source LD call')
            before, failed = states['loaded'], states['failed']
            require(snapshots['loaded-region.mcr'] == snapshots['failed-region.mcr'] and
                    snapshots['loaded-level.dat'] == snapshots['failed-level.dat'] and failed['size'] == before['size'],
                    'failure preserves actual region timestamp/allocation/payload, metadata and Info size')
            require(not any(failed[key] for key in ('written', 'closed', 'returned', 'output_closed', 'commit',
                                                   'commit_returned', 'source_closes', 'raw_bytes')), 'no invented serializer finally/commit')
            if mode == 'session-mismatch':
                require(failed['error_ld_pc'] == 1 and not failed['has_output'] and not failed['root'] and
                        failed['q'] == before['q'], 'outside-catch first session check precedes stream/fresh root/q')
            else:
                require(all(failed[key] for key in ('root', 'level', 'attached', 'prefix', 'has_output', 'failed')) and
                        failed['entities'] == (mode == 'null-l') and not failed['tiles'],
                        'real opened stream/fresh prefix and reached list attachment retained after failure')
                if mode == 'opaque':
                    require(not failed['q'] and failed['native_policy'] and not failed['diagnostic'],
                            'unconsumed opaque record refusal after actual q105 without preservation overlay')
                elif mode in ('null-m', 'null-l'):
                    pc = 125 if mode == 'null-m' else 224
                    require(not failed['q'] and failed['error_ld_pc'] == 51 and
                            failed['error_serializer_pc'] == failed['cause_pc'] == pc and
                            failed['source_exception'] and failed['diagnostic'] and not failed['native_policy'] and
                            failed['source_frames'] == 2 and all(failed[key] == 0 for key in
                            ('entity_wrapper', 'entity_name', 'entity_base', 'entity_item')),
                            'actual null container receiver reaches only to site and LD102, with no fabricated Entity call')
                else:
                    require(failed['q'] and failed['error_serializer_pc'] == 183 and failed['diagnostic'] and
                            failed['source_exception'] and not failed['native_policy'], 'late Item source failure preserves q167 and LD diagnostic frontier')
            require(fingerprint(owned / 'world/region/r.0.0.mcr') == snapshots['failed-region.mcr'] and
                    fingerprint(owned / 'world/level.dat') == snapshots['failed-level.dat'], 'native teardown does not finish retained failed stream')
        require(read(owned / 'world/preserve.bin') == b'unrelated disposable World file\0\xff', 'unrelated authored file unchanged')
        cases[mode] = {'root': str(owned), 'capture': capture.capture_report, 'summary': rows[-1],
                       'states': states, 'snapshots': snapshots, 'decoded': decoded, 'console': console,
                       'files': {p.relative_to(owned).as_posix(): fingerprint(p) for p in sorted(owned.rglob('*')) if p.is_file()}}
    after_inputs = freeze()
    require(before_inputs == after_inputs, 'actual fixture/helper/product unchanged through native execution')
    report = {'status': 'PASS_AUTHORED_ACTUAL_SOURCE_WORLD_SAVING', 'retained_work': str(work), 'cases': cases,
              'inputs_before_sha256': before_inputs, 'inputs_after_sha256': after_inputs, 'inputs_unchanged': True,
              'controller_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'scope': 'Actual loaded source World save/cache/LD/Store/Info plus real Item/Chest owners and new source World reload on authored disposable files. Independent disk decode. Not natural gameplay generation or original World parity.',
              'teardown_scope': 'Explicit native Entity/Tile/cache/Chunk/World/Handler retirement plus actual shared rj source flush before new World construction; ordinary client pause Save-and-quit detach is separate.',
              'failure_scope': 'Direct actual World Chunk-save entry tests session/opaque/late Item and actual Chunk.m/l null source projections; both projections are immediately restored and native ownership ledgers stay intact. Canonical null Entity occurrences are rejected by native admission and are not runtime-qualified. No full-save metadata failure rollback is asserted.'}
    (work / 'REPORT.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'retained': str(work), 'profiles': len(cases), 'checks': sum(x['summary']['checks'] for x in cases.values())}))


if __name__ == '__main__':
    main()
