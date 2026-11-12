"""Loaded saved-Bed respawn through real Death-screen SDK buttons and direct source save."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True


def require(value, message):
    if not value:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def bed_metadata(fixture, display, played):
    raw = fixture.metadata(display, played, 19132, player_score=0)
    require(raw.endswith(b'\0\0\0'), 'genuine authored Player/Data/root ends')
    coordinates = b''.join(fixture.tag(3, key, struct.pack('>i', value)) for key, value in
                           (('SpawnX', 3), ('SpawnY', 64), ('SpawnZ', 4)))
    return raw[:-3] + coordinates + raw[-3:]


def region_records(directory):
    """Read every complete existing compressed record without native runtime state."""
    records = {}
    for path in sorted(directory.glob('r.*.*.mcr')):
        match = re.fullmatch(r'r\.(-?[0-9]+)\.(-?[0-9]+)\.mcr', path.name)
        require(match is not None, 'ordinary region name')
        rx, rz = map(int, match.groups())
        data = path.read_bytes()
        require(8192 <= len(data) <= 1048576 and len(data) % 4096 == 0,
                'bounded complete saved region')
        occupied = set()
        for index, location in enumerate(struct.unpack_from('>1024I', data)):
            if location == 0:
                continue
            sector, count = location >> 8, location & 255
            require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(data),
                    'complete Chunk record allocation')
            allocation = set(range(sector, sector + count))
            require(not occupied.intersection(allocation), 'disjoint live Chunk allocations')
            occupied.update(allocation)
            offset = sector * 4096
            size = struct.unpack_from('>I', data, offset)[0]
            require(1 < size <= count * 4096 - 4 and data[offset + 4] == 2,
                    'complete normal zlib Chunk record')
            coord = rx * 32 + (index & 31), rz * 32 + (index >> 5)
            require(coord not in records, 'unique actual Chunk slot')
            records[coord] = dict(body=data[offset:offset + size + 4],
                                  timestamp=data[4096 + index * 4:4100 + index * 4])
    return records


def record_raw(record):
    body = record['body']
    decoder = zlib.decompressobj()
    raw = decoder.decompress(body[5:], 1048577)
    require(len(raw) <= 1048576 and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, 'complete bounded selected Chunk compressed member')
    return raw


def array_offset(fixture, raw, name, count):
    prefix = fixture.tag(7, name, struct.pack('>i', count))
    require(raw.count(prefix) == 1, 'one authored selected ByteArray ' + name)
    offset = raw.index(prefix) + len(prefix)
    require(offset + count <= len(raw), 'complete authored selected ByteArray ' + name)
    return offset


def author_bed(fixture, directory, profile):
    fixture.saved_regions(directory, generate=False)
    before = region_records(directory)
    require(set(before) == {(x, z) for x in range(-8, 9) for z in range(-8, 9)},
            'all 289 genuine stored Chunk inputs')
    raw = bytearray(record_raw(before[0, 0]))
    if profile == 'valid':
        blocks = array_offset(fixture, raw, 'Blocks', 32768)
        metadata = array_offset(fixture, raw, 'Data', 16384)
        for z, value in ((3, 0), (4, 8)):
            index = (3 << 11) | (z << 7) | 64
            require(raw[blocks + index] == 0 and raw[metadata + index // 2] == 0,
                    'initial Bed cells are ordinary air')
            raw[blocks + index] = 26
            raw[metadata + index // 2] = value
        compressed = zlib.compress(raw)
        replacement = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
        sectors = (len(replacement) + 4095) // 4096
        path = directory / 'r.0.0.mcr'
        region = bytearray(path.read_bytes())
        # Append the one changed record and repoint its existing header slot.
        # Every other Chunk record and timestamp stays byte-identical.
        struct.pack_into('>I', region, 0, ((len(region) // 4096) << 8) | sectors)
        region.extend(replacement + bytes(sectors * 4096 - len(replacement)))
        path.write_bytes(region)
    after = region_records(directory)
    require(before.keys() == after.keys(), 'unchanged complete Chunk membership')
    require(all(before[key] == after[key] for key in before if key != (0, 0)),
            'all other 288 compressed Chunk records and timestamps preserved')
    require(before[0, 0]['timestamp'] == after[0, 0]['timestamp'],
            'authored Bed input preserves selected Chunk timestamp')
    return before, after


def input_check(parser, level_path, records, before, profile):
    player = parser.inflate(parser.read(level_path), 31)['Data'][1]['Player'][1]
    require([player.get(key) for key in ('SpawnX', 'SpawnY', 'SpawnZ')] ==
            [[3, 3], [3, 64], [3, 4]], 'independent saved Player SpawnXYZ input')
    require(player['Score'] == [3, 0] and player['Health'] == [2, 20] and
            player['Inventory'] == [9, [10, []]], 'ordinary saved Player score/health/empty Inventory')
    saved_y = 64.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0]
    require(player['Dimension'] == [3, 0] and
            player['Pos'] == [9, [6, [int.from_bytes(struct.pack('>d', n), 'big') for n in
                                     (.5, saved_y, .5)]]] and
            player['Rotation'] == [9, [5, [0, 0]]] and
            player['Motion'] == [9, [6, [0, 0, 0]]], 'genuine unchanged saved Player pose input')
    selected = parser.Nbt(record_raw(records[0, 0])).root()['Level'][1]
    original = parser.Nbt(record_raw(before[0, 0])).root()['Level'][1]
    blocks, metadata = selected['Blocks'][1], selected['Data'][1]
    wanted_blocks, wanted_metadata = bytearray(original['Blocks'][1]), bytearray(original['Data'][1])
    for z, value in ((3, 0), (4, 8)):
        index = (3 << 11) | (z << 7) | 64
        require(blocks[index] == (26 if profile == 'valid' else 0) and
                (metadata[index // 2] & 15) == (value if profile == 'valid' else 0),
                'independent Bed/air Block and orientation/head metadata input')
        if profile == 'valid':
            wanted_blocks[index] = 26
            wanted_metadata[index // 2] = value
        require(selected['HeightMap'][1][z * 16 + 3] == 64 and
                selected['SkyLight'][1][index // 2] == 255 and
                blocks[index - 1] == 2, 'Bed opacity0 keeps grass support, height64 and skylight15')
    require(blocks == bytes(wanted_blocks) and metadata == bytes(wanted_metadata),
            'only two declared Bed Block/metadata cells differ')
    require(all(selected[key] == original[key] for key in original if key not in ('Blocks', 'Data')),
            'all other selected Chunk fields and light planes preserved')
    for (x, z), record in records.items():
        chunk = parser.Nbt(record_raw(record)).root()['Level'][1]
        require(chunk['xPos'] == [3, x] and chunk['zPos'] == [3, z] and
                chunk['TerrainPopulated'] == [1, 1] and
                chunk['Entities'] == [9, [10, []]] and chunk['TileEntities'] == [9, [10, []]],
                '289 independently decoded ordinary populated Entity-empty Chunk inputs')
    return dict(chunks=289, saved_spawn=[3, 64, 4], health=20, inventory_empty=True,
                foot=dict(position=[3, 64, 3], id=26 if profile == 'valid' else 0, metadata=0),
                head=dict(position=[3, 64, 4], id=26 if profile == 'valid' else 0,
                          metadata=8 if profile == 'valid' else 0), other_records_preserved=288)


def property_unescape(value):
    result, i = [], 0
    while i < len(value):
        ch = value[i]
        i += 1
        if ch != '\\':
            result.append(ch)
            continue
        require(i < len(value), 'complete selected Java property escape')
        ch = value[i]
        i += 1
        if ch == 'u':
            digits = value[i:i + 4]
            require(re.fullmatch(r'[0-9a-fA-F]{4}', digits) is not None,
                    'complete selected Java Unicode property escape')
            result.append(chr(int(digits, 16)))
            i += 4
        else:
            result.append({'t': '\t', 'n': '\n', 'r': '\r', 'f': '\f'}.get(ch, ch))
    return ''.join(result)


def imported_notice(assets):
    # The imported original property supplies the expected text. No English
    # notice is supplied to the native client or embedded in this reader.
    path = assets / 'jar/lang/en_US.lang'
    lines = path.read_bytes().decode('iso-8859-1').splitlines()
    selected = [line.split('=', 1)[1] for line in lines
                if line.startswith('tile.bed.notValid=')]
    require(len(selected) == 1, 'one actual imported missing-Bed translation key')
    text = property_unescape(selected[0])
    require(text and '\n' not in text and '\r' not in text, 'single imported notice line')
    return text.encode('utf-16-be', errors='surrogatepass').hex()


def native_receipts(stdout, profile, notice):
    rows = stdout.splitlines()
    cycles = 2 if profile == 'valid' else 1
    declared = (f'BED_RESPAWN_INPUT profile={profile} player=pre-bootstrap-nbt saved-spawn=3,64,4 '
                f'Bed=pre-bootstrap-region direct-fatal-body=component-input cycles={cycles}')
    require(rows.count(declared) == 1, 'exact authored saved-input scope receipt')
    actual = [row for row in rows if row.startswith('BED_RESPAWN cycle=')]
    valid = profile == 'valid'
    require(len(actual) == cycles, 'every complete SDK Death/Respawn cycle')
    for cycle, row in enumerate(actual):
        require(row == (f'BED_RESPAWN cycle={cycle} profile={profile} saved-read=1 checked=1 '
                        f'valid={int(valid)} notified={int(not valid)} stored={int(valid)} '
                        f'positioned={int(valid)} Q={"distinct-3,64,4" if valid else "null"} '
                        'new-player=1 new-inventory=1 new-input=1 same-world=1 same-id=1 '
                        'health=20 source-return=372 wrapper-return=1 terrain=289 '
                        'old-coordinate-test-pin=0'), 'actual coordinator flags and Q identity receipt')
    notices = [row for row in rows if row.startswith('BED_RESPAWN_NOTICE ')]
    if valid:
        require(not notices, 'valid Bed emits no missing-Bed notice')
    else:
        require(len(notices) == 1, 'one actual UQ missing-Bed message')
        match = re.fullmatch(r'BED_RESPAWN_NOTICE age=([0-9]+) utf16=([0-9a-f]+)', notices[0])
        require(match is not None and 0 <= int(match[1]) < 200 and match[2] == notice,
                'live-clock visible UQ message text equals actual imported translation')
    require(rows.count(f'BED_RESPAWN_SAVE profile={profile} path=direct-world-full-save '
                       'SDK-save-menu=0 live-player=1 source-returned=1') == 1,
            'explicit actual direct World source full-save receipt')
    require(rows.count('BED_RESPAWN_TERMINAL borrowed-display-before=1 terminal-prefix=1 '
                       'borrowed-display-cleared=1 common-cleanup-next=1') == 1,
            'display borrow ended before common root retirement')
    footer = [row for row in rows if row.startswith('WORLD_SELECTION mode=repeat cleanup=')]
    require(len(footer) == 1 and re.fullmatch(
        r'WORLD_SELECTION mode=repeat cleanup=1 checks=[1-9][0-9]*', footer[0]) is not None,
        'common checked root/Loop/App/bootstrap retirement')
    return dict(cycles=cycles, missing_notice=notices, footer=footer[0])


def saved_player_check(parser, path, profile):
    data = parser.inflate(parser.read(path), 31)['Data'][1]
    player = data['Player'][1]
    # Original sp.a(DataOutput)35..45 writes Byte1 for every empty list.
    require(data['version'] == [3, 19132] and player['Health'] == [2, 20] and
            player['Inventory'] == [9, [1, []]], 'actual full-save live healthy empty-Inventory Player')
    names = ('SpawnX', 'SpawnY', 'SpawnZ')
    if profile == 'valid':
        require([player.get(key) for key in names] == [[3, 3], [3, 64], [3, 4]],
                'independent readback of valid saved-Bed coordinates')
    else:
        require(all(key not in player for key in names), 'missing Bed Player save omits SpawnXYZ')
    return dict(health=20, inventory_empty=True, inventory_list_type=1,
                saved_spawn=[3, 64, 4] if profile == 'valid' else None,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--profile', choices=('valid', 'missing'), required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'This private APPDATA loaded integration profile requires Windows')
    fixture = module('bed_world_fixture', Path(__file__).with_name('test_world_selection_loop.py'))
    nbt = module('bed_independent_nbt', Path(__file__).resolve().parents[1] / 'world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    notice = imported_notice(assets)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=f'bed-respawn-{args.profile}-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'
    game = appdata / '.minecraft'
    fixture.repeat_statistics(game, assets)
    saves = game / 'saves'
    saves.mkdir()
    (saves / 'preserve-me.bin').write_bytes(b'unrelated save-directory file\x00\xff')
    for folder, display, played in (('older', 'Old world', 1303000000000),
                                    ('bravo', '', 1304000000000),
                                    ('alpha', 'Alpha world', 1304000000000)):
        directory = saves / folder
        directory.mkdir()
        raw = bed_metadata(fixture, display, played) if folder == 'alpha' else fixture.metadata(display, played, 19132)
        (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
            gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored untouched chunk')
    regions = saves / 'alpha/region'
    regions.mkdir()
    original, authored = author_bed(fixture, regions, args.profile)
    inputs = input_check(nbt, saves / 'alpha/level.dat', authored, original, args.profile)
    before_resources = fixture.snapshot(assets)
    before_files = fixture.save_title_files(saves, evidence / 'before')
    environment = os.environ.copy()
    environment['APPDATA'] = str(appdata)
    environment['MC_TEST_BED_RESPAWN_PROFILE'] = args.profile
    capture = evidence / 'last-returned-frame.png'
    report = dict(status='FAIL', profile=args.profile, inputs=inputs,
                  scope='Native loaded saved-Bed integration; direct fatal-body component input; real Death-screen SDK buttons; direct World full save; no SDK save-menu/reload, hazard, original whole-frame or FPS qualification')
    try:
        result = fixture.retained_native_run(
            [str(executable), str(assets), 'repeat', str(capture)], environment, evidence,
            saves, before_files, f'bed-respawn-{args.profile}', time.time_ns() // 1000000, 240)
        print(result.stdout, end='')
        print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0 and result.stderr == '', 'actual loaded Bed Death/Respawn native failure')
        report['native'] = native_receipts(result.stdout, args.profile, notice)
        report['saved_player'] = saved_player_check(nbt, saves / 'alpha/level.dat', args.profile)
        # Source full-save is allowed to update selected World metadata/Chunks.
        # Independent nonselected Worlds and unrelated authored files survive.
        after_files = {name.replace('\\', '/'): digest
                       for name, digest in fixture.snapshot(saves).items()}
        for name, value in before_files.items():
            if name.startswith(('older/', 'bravo/')) or name == 'preserve-me.bin' or name.endswith('/preserve-chunk.dat'):
                require(after_files.get(name) == value['sha256'], 'unrelated saved input unchanged: ' + name)
        shots = [capture, Path(str(capture) + '-before-0.png')]
        for cycle in range(2 if args.profile == 'valid' else 1):
            shots.extend(Path(str(capture) + f'-{stage}-{cycle}.png') for stage in ('death', 'returned'))
        report['screenshots'] = {}
        for path in shots:
            data = path.read_bytes()
            require(data.startswith(b'\x89PNG\r\n\x1a\n'), 'retained actual before/death/returned frame PNG')
            report['screenshots'][path.name] = hashlib.sha256(data).hexdigest()
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = dict(type=type(error).__name__, message=str(error))
        raise
    finally:
        unchanged = fixture.snapshot(assets) == before_resources
        report['resources_unchanged'] = unchanged
        if not unchanged:
            report['status'] = 'FAIL'
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        require(unchanged, 'imported original resource bytes unchanged')
    print(f'Bed Respawn scene ({args.profile}): PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
