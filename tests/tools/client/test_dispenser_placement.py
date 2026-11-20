"""One SDK BlockItem23 placement, empty AZ/GQ, close and save/reselection.

--self-test runs offline controls only. SDK execution needs the genuine native
product, supplied assets and monitor 2. Authored SDL input is not physical input.
"""
import argparse
import copy
import gzip
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/tools/client'))
import test_dispenser_gameplay as D

G, require, U = D.G, D.require, D.U
write, inputs, fingerprints = D.write, D.inputs, D.fingerprints
capture_owned, natural_custody = D.capture_owned, D.natural_custody
IMAGES = frozenset(('dispenser-placement-placed.png', 'dispenser-placement-title.png',
                    'dispenser-placement-reloaded.png'))
ORIGINAL_COORDINATES = {(x, z) for x in range(-8, 9) for z in range(-8, 9)}
# Actual saved Player chunk (0,-1): Source random-tick radius9 plus one
# nonrecursive neighbor-query Chunk halo, all populated before bootstrap.
COORDINATES = {(x, z) for x in range(-10, 11) for z in range(-11, 10)}


def planes(selected, placed=False):
    require(type(selected) is bool and type(placed) is bool, 'Closed authored plane phase')
    blocks, light, metadata = map(bytearray, D.planes(False))
    if selected:
        blocks[(2 << 7) | 64] = 1
        if placed:
            at = (1 << 7) | 64
            blocks[at], metadata[at // 2] = 23, 2
    return bytes(blocks), bytes(light), bytes(metadata)


def chunk_record(world, x, z):
    blocks, light, metadata = planes((x, z) == (0, 0))
    level = world.tag(3, 'xPos', struct.pack('>i', x)) + world.tag(3, 'zPos', struct.pack('>i', z))
    level += world.tag(4, 'LastUpdate', bytes(8))
    heights = bytearray([64] * 256)
    if (x, z) == (0, 0):
        heights[2 * 16] = 65
    for key, value in (('Blocks', blocks), ('Data', metadata), ('SkyLight', light),
                       ('BlockLight', bytes(16384)), ('HeightMap', heights)):
        level += world.tag(7, key, struct.pack('>i', len(value)) + value)
    level += world.tag(1, 'TerrainPopulated', b'\1')
    level += world.tag(9, 'Entities', b'\x0a' + bytes(4))
    level += world.tag(9, 'TileEntities', b'\x0a' + bytes(4))
    raw = b'\x0a\0\0' + world.tag(10, 'Level', level + b'\0') + b'\0'
    compressed = zlib.compress(raw)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    count = (len(record) + 4095) // 4096
    return record + bytes(count * 4096 - len(record)), count


def regions(world, directory):
    """Append only closed prelaunch flat inputs; no Dispenser/AZ or post-load repair."""
    output = {}
    # Keep all original 289 locations, timestamps, records and NBT wires, then
    # append the 152 missing inputs into formerly empty Region header slots.
    for x, z in sorted(ORIGINAL_COORDINATES) + sorted(COORDINATES - ORIGINAL_COORDINATES):
        record, count = chunk_record(world, x, z)
        region = output.setdefault((x >> 5, z >> 5), bytearray(8192))
        sector = len(region) // 4096
        struct.pack_into('>I', region, ((x & 31) + (z & 31) * 32) * 4, sector * 256 + count)
        region += record
    for (x, z), data in output.items():
        (directory / f'r.{x}.{z}.mcr').write_bytes(data)


def saved_semantics(level, player, placed):
    require(type(placed) is bool and type(level) is dict and type(player) is dict,
            'Actual decoded selected Chunk/Player compounds and closed phase')
    coordinates = [G.typed(level.get(key), 3, 'Chunk coordinate Int width') for key in ('xPos', 'zPos')]
    require(all(type(value) is int for value in coordinates) and coordinates == [0, 0], 'Selected Chunk coordinates')
    blocks = G.typed(level.get('Blocks'), 7, 'Complete block plane')
    metadata = G.typed(level.get('Data'), 7, 'Complete metadata plane')
    expected_blocks, _, expected_metadata = planes(True, placed)
    require(type(blocks) is bytes and type(metadata) is bytes and blocks == expected_blocks and
            metadata == expected_metadata, 'Complete selected Blocks/Data retain every unrelated cell and nibble')
    tiles = G.typed(level.get('TileEntities'), 9, 'Actual Tile list')
    require(type(tiles) is list and len(tiles) == 2 and type(tiles[0]) is int and tiles[0] == 10 and
            type(tiles[1]) is list and len(tiles[1]) == int(placed), 'Exactly one newly placed AZ, or no authored Tile')
    if placed:
        tile = tiles[1][0]
        require(type(tile) is dict and set(tile) == {'id', 'x', 'y', 'z', 'Items'}, 'Closed actual AZ fields')
        coordinates = [G.typed(tile[key], 3, 'AZ coordinate Int width') for key in ('x', 'y', 'z')]
        require(G.typed(tile['id'], 8, 'AZ id String width') == 'Trap' and
                all(type(value) is int for value in coordinates) and coordinates == [0, 64, 1],
                'Exact actual placed AZ id/coordinates/widths')
        items = G.typed(tile['Items'], 9, 'Actual empty Items List')
        require(type(items) is list and len(items) == 2 and type(items[0]) is int and items[0] == 1 and
                type(items[1]) is list and not items[1], 'Actual empty Trap wire List is empty Byte, no manufactured item')
    inventory = G.slot_rows(player['Inventory'])
    dimension = G.typed(player['Dimension'], 3, 'Player Dimension Int width')
    require(inventory == ([] if placed else [(0, 23, 1, 0)]) and type(dimension) is int and dimension == 0,
            'Exactly one authored BlockItem consumed; no extra Player item')
    entities = G.typed(level.get('Entities'), 9, 'Actual selected Entity list')
    require(type(entities) is list and len(entities) == 2 and type(entities[0]) is int and
            type(entities[1]) is list and not entities[1], 'No unexpected selected Entity')
    return {'tile_count': int(placed), 'tile_slots': [], 'player_slots': inventory,
            'block': 23 if placed else 0, 'metadata': 2 if placed else 0,
            'scope': 'One yaw-zero runtime placement; no other cardinal, acquisition, power or firing credit.'}


def terrain_semantics(level, coordinate, placed):
    require(type(coordinate) is tuple and len(coordinate) == 2 and
            all(type(value) is int for value in coordinate) and coordinate in COORDINATES,
            'Declared authored Chunk coordinate')
    values = [G.typed(level[key], 3, 'Actual Chunk coordinate Int') for key in ('xPos', 'zPos')]
    require(all(type(value) is int for value in values) and tuple(values) == coordinate,
            'Actual NBT coordinates match Region slot')
    populated = G.typed(level['TerrainPopulated'], 1, 'Actual populated Byte')
    require(type(populated) is int and populated == 1, 'Authored and saved TerrainPopulated remains true')
    blocks, _, metadata = planes(coordinate == (0, 0), placed)
    require(type(level['Blocks'][1]) is bytes and type(level['Data'][1]) is bytes and
            level['Blocks'] == [7, blocks] and level['Data'] == [7, metadata],
            'Every authored Chunk retains complete Blocks/Data except the single declared placement')
    if coordinate != (0, 0):
        tiles = G.typed(level['TileEntities'], 9, 'Unrelated Tile list')
        require(type(tiles) is list and len(tiles) == 2 and type(tiles[0]) is int and
                type(tiles[1]) is list and not tiles[1], 'No unrelated Tile introduced')


def terrain_gate(nbt, directory, placed):
    paths = sorted(directory.glob('r.*.*.mcr'))
    require({p.name for p in paths} == {f'r.{x}.{z}.mcr' for x in (-1, 0) for z in (-1, 0)} | {'r.9.9.mcr'},
            'Exact authored Region inventory including unrelated empty control')
    seen = set()
    for path in paths:
        require(path.is_file() and not path.is_symlink() and 8192 <= path.stat().st_size <= 16 * 1048576,
                'Bounded ordinary Region')
        raw = path.read_bytes()
        require(len(raw) % 4096 == 0, 'Complete Region allocation')
        if path.name == 'r.9.9.mcr':
            require(raw == bytes(8192), 'Unrelated Region unchanged')
            continue
        _, rx, rz, _ = path.name.split('.')
        allocated = {0, 1}
        for slot in range(1024):
            location, = struct.unpack_from('>I', raw, slot * 4)
            if not location:
                continue
            sector, count = location >> 8, location & 255
            extent = set(range(sector, sector + count))
            require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(raw) and
                    not allocated.intersection(extent), 'Disjoint complete Chunk sectors')
            allocated.update(extent)
            offset = sector * 4096
            size, = struct.unpack_from('>I', raw, offset)
            require(1 < size <= count * 4096 - 4 and raw[offset + 4] == 2, 'Complete zlib Chunk record')
            decoded = nbt.inflate(raw[offset + 5:offset + 4 + size], 15)
            level = G.compound(decoded['Level'], 'Actual Region Level')
            coordinate = (int(rx) * 32 + slot % 32, int(rz) * 32 + slot // 32)
            require(coordinate in COORDINATES and coordinate not in seen, 'Unique complete authored Chunk coordinates')
            seen.add(coordinate)
            terrain_semantics(level, coordinate, placed)
        # Saved Region free sectors are a legitimate allocator detail.
    require(seen == COORDINATES, 'All 441 declared authored Chunks retained, no missing or unexpected coordinates')
    return {'chunks': len(seen), 'block_bytes': len(seen) * 32768, 'metadata_bytes': len(seen) * 16384}


def saved_gate(nbt, mining, directory, placed):
    level = mining.selected_chunk(nbt, directory)
    metadata = nbt.inflate(G.bounded(directory / 'level.dat', nbt.BOUND), 31)
    player = G.compound(G.compound(metadata['Data'], 'Saved Data')['Player'], 'Saved Player')
    return {**saved_semantics(level, player, placed), 'terrain': terrain_gate(nbt, directory / 'region', placed),
            'region_sha256': G.digest(directory / 'region/r.0.0.mcr'), 'metadata_sha256': G.digest(directory / 'level.dat')}


def save_protocol(rows, evidence, interval):
    snapshot = [row for row in rows if row.startswith('WORLD_RELOAD_SNAPSHOT ')]
    require(len(snapshot) == 1 and snapshot[0].startswith('WORLD_RELOAD_SNAPSHOT phase=dispenser-placement-saved '),
            'Unique placement save label')
    # The unchanged common protocol is identical except this finite snapshot label.
    result = D.save_protocol([row.replace('phase=dispenser-placement-saved ', 'phase=dispenser-saved ') for row in rows], None, interval)
    if evidence is not None:
        entries = evidence / 'entries'
        frame = result['entry_frame']
        require(entries.is_dir() and not entries.is_symlink() and
                {p.name for p in entries.iterdir()} == {f'frame-{i}' for i in range(frame + 1)}, 'Complete pre-save copies')
        for i in range(frame + 1):
            G.snapshot_files(entries / f'frame-{i}')
        entry = next(row for row in rows if row.startswith('WORLD_SAVE_TITLE_ENTRY '))
        entry_bytes = int(re.search(r' bytes=([0-9]+) copied-before-frame=1$', entry)[1])
        require(sum(v['bytes'] for v in G.snapshot_files(entries / f'frame-{frame}').values()) == entry_bytes,
                'Selected pre-save copy bytes')
        actual = G.snapshot_files(evidence / 'snapshots/dispenser-placement-saved')
        require((len(actual), sum(v['bytes'] for v in actual.values())) == result['snapshot'], 'Exact saved copy count and bytes')
    return result


def receipts(stdout, stderr, monitor, *, evidence=None, interval=None):
    require(type(stdout) is str and stdout.endswith('\n') and '\r' not in stdout and stderr == '', 'Complete clean decoded streams')
    rows, at = stdout.splitlines(), 2
    require(rows[:2] == ['16 achievements', '151 recipes'], 'One exact startup banner pair')

    def exact(value):
        nonlocal at
        require(at < len(rows) and rows[at] == value, 'Ordered exact placement receipt: ' + value)
        at += 1

    def match(pattern):
        nonlocal at
        value = re.fullmatch(pattern, rows[at]) if at < len(rows) else None
        require(value is not None, 'Complete typed placement receipt')
        at += 1
        return value

    target = monitor['selected']['work']
    require(type(target) in (list, tuple) and len(target) == 4 and all(type(v) is int for v in target) and
            target[2] >= 854 and target[3] >= 480, 'Actual monitor2 work rectangle')

    def host(stage):
        for window in ('frame', 'canvas'):
            row = match(r'WORLD_SELECTION_MONITOR stage=' + stage + ' window=' + window +
                        r' x=(-?[0-9]+) y=(-?[0-9]+) width=854 height=480 target=(\S+)')
            x, y = map(int, row.group(1, 2))
            require(row[3] == ','.join(map(str, target)) and target[0] <= x and target[1] <= y and
                    x + 854 <= target[0] + target[2] and y + 480 <= target[1] + target[3], 'Both windows within monitor2')
        row = match(r'WORLD_SAVE_TITLE_HOST stage=' + stage + r' frame-id=([1-9][0-9]{0,9}) '
                    r'canvas-id=([1-9][0-9]{0,9}) canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1')
        ids = tuple(map(int, row.groups()))
        require(ids[0] != ids[1] and max(ids) <= 4294967295, 'Distinct actual uint32 window IDs')
        return ids

    first = host('open')
    exact('DISPENSER_PLACEMENT_READY epoch=0 distinct-world-player=0 ll-frame-returned=1 picked=0,64,2')
    attempts = 0
    while True:
        attempts += 1
        require(attempts <= 12, 'Finite visible tap schedule')
        aim = match(r'DISPENSER_PLACEMENT_AIM attempt=' + str(attempts) +
                    r' picked=(-?[0-9]+),(-?[0-9]+),(-?[0-9]+) face=([0-5]) target=(-?[0-9]+),(-?[0-9]+),(-?[0-9]+)'
                    r' player-bits=([0-9a-f]{16}),([0-9a-f]{16}),([0-9a-f]{16})'
                    r' entity-eye-bits=([0-9a-f]{16}),([0-9a-f]{16}),([0-9a-f]{16})'
                    r' yaw-bits=([0-9a-f]{8}) pitch-bits=([0-9a-f]{8}) cursor=0')
        cell = tuple(map(int, aim.group(1, 2, 3)))
        face = int(aim[4])
        offset = ((0, -1, 0), (0, 1, 0), (0, 0, -1), (0, 0, 1), (-1, 0, 0), (1, 0, 0))[face]
        target_cell = tuple(a + b for a, b in zip(cell, offset))
        positions = [struct.unpack('>d', bytes.fromhex(aim[i]))[0] for i in range(8, 14)]
        require(cell == (0, 64, 2) and face == 2 and tuple(map(int, aim.group(5, 6, 7))) == target_cell == (0, 64, 1) and
                all(math.isfinite(value) for value in positions) and aim[14] == '00000000' and aim[15] == '41f00000',
                'Actual Ray face independently derives CK target; finite raw Player/returned entity-eye and yaw/pitch bits')
        exact(f'DISPENSER_PLACEMENT_TAP attempt={attempts} sdk-right-down-up=1')
        if at < len(rows) and rows[at].startswith('DISPENSER_PLACEMENT_NO_EFFECT '):
            exact(f'DISPENSER_PLACEMENT_NO_EFFECT attempt={attempts} drained=1 source-action=0')
            continue
        exact(f'DISPENSER_PLACED attempts={attempts} sdk-right=1 block-item=23 count-before=1 count-after=0 count-stores=1 statistic=1 sound=1 support=0,64,2 target=0,64,1 yaw=0 metadata=2 az-slots=9 empty=1')
        break

    def opened(epoch):
        exact(f'DISPENSER_PLACEMENT_OPEN epoch={epoch} sdk-right=1 real-gq-sh=1 slots=45 empty=1 placed=0')
        exact(f'DISPENSER_PLACEMENT_CLOSE epoch={epoch} sdk-key=E inherited-close=1 default-container=1 empty=1')

    opened(0)
    start = at
    while at < len(rows) and rows[at].startswith(('WORLD_SAVE_TITLE_RECOVERY ', 'WORLD_SAVE_TITLE_AUTOPAUSE ',
            'WORLD_SAVE_TITLE_ESCAPE ', 'WORLD_SAVE_TITLE_ENTRY ', 'WORLD_SAVE_TITLE mode=', 'WORLD_RELOAD_SNAPSHOT ')):
        at += 1
    saved = save_protocol(rows[start:at], evidence, interval)
    exact('DISPENSER_PLACEMENT_READY epoch=1 distinct-world-player=1 ll-frame-returned=1 picked=0,64,1')
    opened(1)
    require(host('after-dispenser-placement') == first, 'Same actual host through save/reselection')
    row = match(r'DISPENSER_PLACEMENT_BOUNDED placements=1 opens=2 closes=2 epochs=2 taps=' + U + r' frames=' + U + r' gui-frames=' + U +
                r' field-repair=0 physical-input=0 firing=0 fps-claim=0')
    taps, frames, gui = map(int, row.groups())
    require(taps == attempts and 2 <= gui <= frames <= 65536, 'Exact tap schedule and finite returning World/GUI frames')
    exact('DISPENSER_PLACEMENT_STOP actual-frame-close=1 source-frame-retry=0')
    row = match(r'DISPENSER_PLACEMENT_FINISH cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9}) source-field-repair=0')
    require(int(row[1]) <= 4294967295 and at == len(rows), 'Checked terminal closure and no trailing record')
    return {'window_ids': first, 'attempts': attempts, 'frames': frames, 'gui_frames': gui, 'checks': int(row[1]), 'save': saved,
            'route_scope': 'Runtime Controller/CK/XQ c/h/placed-by and DC/GQ/SH joins inferred from pinned production and real public outcomes; no direct callback-PC observer.',
            'sound_scope': 'One actual reached returning World/Sound continuation, not an audibility assertion.'}


def authored(world, selected, mining, nbt, saves):
    for folder, name, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000),
                                  ('alpha', 'Dispenser placement', 1304000000000)):
        directory = saves / folder
        directory.mkdir(parents=True)
        wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
        if folder == 'alpha':
            empty = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
            require(wire.count(empty) == 1, 'One authored Player Inventory')
            wire = wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + G.stack(world, 0, 23, 1)))
        (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated placement control')
    directory = saves / 'alpha/region'
    directory.mkdir()
    regions(world, directory)
    (directory / 'r.9.9.mcr').write_bytes(bytes(8192))
    return saved_gate(nbt, mining, saves / 'alpha', False)


def self_test():
    # Reexercise the unchanged retained helper's transport interruption and
    # natural-custody controls as well as its complete save protocol controls.
    D.self_test()
    monitor = {'selected': {'work': [100, 200, 1000, 800]}}

    def host(stage):
        return [f'WORLD_SELECTION_MONITOR stage={stage} window={w} x=100 y=200 width=854 height=480 target=100,200,1000,800'
                for w in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']

    rows = ['16 achievements', '151 recipes'] + host('open') + [
        'DISPENSER_PLACEMENT_READY epoch=0 distinct-world-player=0 ll-frame-returned=1 picked=0,64,2',
        'DISPENSER_PLACEMENT_AIM attempt=1 picked=0,64,2 face=2 target=0,64,1 player-bits=3fe0000000000000,405067ae14800000,bfe0000000000000 entity-eye-bits=3fe0000000000000,405067ae14800000,bfe0000000000000 yaw-bits=00000000 pitch-bits=41f00000 cursor=0',
        'DISPENSER_PLACEMENT_TAP attempt=1 sdk-right-down-up=1',
        'DISPENSER_PLACED attempts=1 sdk-right=1 block-item=23 count-before=1 count-after=0 count-stores=1 statistic=1 sound=1 support=0,64,2 target=0,64,1 yaw=0 metadata=2 az-slots=9 empty=1',
        'DISPENSER_PLACEMENT_OPEN epoch=0 sdk-right=1 real-gq-sh=1 slots=45 empty=1 placed=0',
        'DISPENSER_PLACEMENT_CLOSE epoch=0 sdk-key=E inherited-close=1 default-container=1 empty=1',
        'WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=2 resumed=1',
        'WORLD_SAVE_TITLE_ESCAPE keyboard-before=2 keyboard-after=4 fresh-pause=1 distinct-recovered=1',
        'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=84 time=5 size-before=100 size-after=200 entered=1 bytes=10 copied-before-frame=1',
        'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=84 time=5 size-before=100 size-after=200 first=1000 last=1001 entry-frame=0 entry-bytes=10',
        'WORLD_RELOAD_SNAPSHOT phase=dispenser-placement-saved files=1 bytes=10 actual-file-copy=1',
        'DISPENSER_PLACEMENT_READY epoch=1 distinct-world-player=1 ll-frame-returned=1 picked=0,64,1',
        'DISPENSER_PLACEMENT_OPEN epoch=1 sdk-right=1 real-gq-sh=1 slots=45 empty=1 placed=0',
        'DISPENSER_PLACEMENT_CLOSE epoch=1 sdk-key=E inherited-close=1 default-container=1 empty=1'] + host('after-dispenser-placement') + [
        'DISPENSER_PLACEMENT_BOUNDED placements=1 opens=2 closes=2 epochs=2 taps=1 frames=12 gui-frames=2 field-repair=0 physical-input=0 firing=0 fps-claim=0',
        'DISPENSER_PLACEMENT_STOP actual-frame-close=1 source-frame-retry=0',
        'DISPENSER_PLACEMENT_FINISH cleanup=1 windows-absent=1 checks=100 source-field-repair=0']
    valid = '\n'.join(rows) + '\n'
    receipts(valid, '', monitor, interval=(999, 1002))
    aim = next(row for row in rows if row.startswith('DISPENSER_PLACEMENT_AIM '))
    repeated = valid.replace('DISPENSER_PLACEMENT_TAP attempt=1 sdk-right-down-up=1\n',
                            'DISPENSER_PLACEMENT_TAP attempt=1 sdk-right-down-up=1\nDISPENSER_PLACEMENT_NO_EFFECT attempt=1 drained=1 source-action=0\n' +
                            aim.replace('attempt=1', 'attempt=2') + '\nDISPENSER_PLACEMENT_TAP attempt=2 sdk-right-down-up=1\n').replace('DISPENSER_PLACED attempts=1 ', 'DISPENSER_PLACED attempts=2 ').replace('epochs=2 taps=1 ', 'epochs=2 taps=2 ')
    require(receipts(repeated, '', monitor)['attempts'] == 2, 'All no-effect retries retained in schedule')
    bad = []
    for i in range(2, len(rows)):
        bad += ['\n'.join(rows[:i] + rows[i + 1:]) + '\n', '\n'.join(rows[:i] + [rows[i]] + rows[i:]) + '\n']
    for old, new in (('metadata=2', 'metadata=3'), ('count-after=0', 'count-after=1'), ('sound=1', 'sound=0'),
                     ('slots=45', 'slots=44'), ('yaw=0', 'yaw=90'), ('distinct-world-player=1', 'distinct-world-player=0'),
                     ('width=854', 'width=855'), ('x=100 ', 'x=99 '), ('windows-absent=1', 'windows-absent=0'),
                     ('face=2', 'face=3'), ('target=0,64,1', 'target=0,64,3'), ('yaw-bits=00000000', 'yaw-bits=42b40000'),
                     ('player-bits=3fe0000000000000', 'player-bits=7ff8000000000000'),
                     ('first=1000 last=1001', 'first=1002 last=1000'), ('phase=dispenser-placement-saved', 'phase=dispenser-saved')):
        bad.append(valid.replace(old, new))
    bad += [valid[:-1], valid + 'extra\n', repeated.replace('source-action=0', 'source-action=1'),
            repeated.replace('TAP attempt=2', 'TAP attempt=3'), repeated.replace('PLACED attempts=2', 'PLACED attempts=1'),
            repeated.replace('epochs=2 taps=2 ', 'epochs=2 taps=1 ')]
    for value in bad:
        try:
            receipts(value, '', monitor, interval=(999, 1002))
        except (RuntimeError, ValueError, KeyError, TypeError):
            pass
        else:
            raise RuntimeError('Corrupted placement receipt was admitted')
    try:
        receipts(valid, 'error', monitor)
    except RuntimeError:
        pass
    else:
        raise RuntimeError('Actual stderr was admitted')
    blocks, _, metadata = planes(True, True)
    tile = {'id': [8, 'Trap'], 'x': [3, 0], 'y': [3, 64], 'z': [3, 1], 'Items': [9, [1, []]]}
    level = {'xPos': [3, 0], 'zPos': [3, 0], 'Blocks': [7, blocks], 'Data': [7, metadata],
             'TileEntities': [9, [10, [tile]]], 'Entities': [9, [10, []]]}
    player = {'Dimension': [3, 0], 'Inventory': [9, [10, []]]}
    saved_semantics(level, player, True)
    bad_saves = []
    for key, value in (('id', [8, 'Chest']), ('x', [3, False]), ('y', [2, 64]), ('z', [3, 2]),
                       ('Items', [9, [10, []]]), ('Items', [9, [1, [0]]])):
        changed = copy.deepcopy(level)
        changed['TileEntities'][1][1][0][key] = value
        bad_saves.append((changed, player))
    for key, offset, value in (('Blocks', 10000, 0), ('Data', 11000, 1), ('Blocks', (1 << 7) | 64, 0),
                               ('Data', ((1 << 7) | 64) // 2, 3)):
        changed = copy.deepcopy(level)
        raw = bytearray(changed[key][1])
        raw[offset] = value
        changed[key][1] = bytes(raw)
        bad_saves.append((changed, player))
    changed = copy.deepcopy(level)
    changed['TileEntities'][1][1].append(copy.deepcopy(tile))
    bad_saves.append((changed, player))
    changed = copy.deepcopy(player)
    changed['Inventory'][1][1].append({'Slot': [1, 0], 'id': [2, 23], 'Count': [1, 1], 'Damage': [2, 0]})
    bad_saves.append((level, changed))
    changed = copy.deepcopy(player)
    changed['Dimension'] = [3, False]
    bad_saves.append((level, changed))
    for chunk, inventory in bad_saves:
        try:
            saved_semantics(chunk, inventory, True)
        except (RuntimeError, ValueError, KeyError, TypeError):
            pass
        else:
            raise RuntimeError('Corrupted saved placement semantics were admitted')
    blocks, _, metadata = planes(False)
    remote = {'xPos': [3, 2], 'zPos': [3, -3], 'TerrainPopulated': [1, 1], 'Blocks': [7, blocks], 'Data': [7, metadata],
              'TileEntities': [9, [10, []]]}
    terrain_semantics(remote, (2, -3), True)
    bad_remote = []
    for key, offset, value in (('Blocks', 10000, 0), ('Data', 11000, 1)):
        changed = copy.deepcopy(remote)
        raw = bytearray(changed[key][1])
        raw[offset] = value
        changed[key][1] = bytes(raw)
        bad_remote.append(changed)
    changed = copy.deepcopy(remote)
    changed['xPos'] = [3, 0]
    bad_remote.append(changed)
    changed = copy.deepcopy(remote)
    changed['TileEntities'][1][1].append(tile)
    bad_remote.append(changed)
    for chunk in bad_remote:
        try:
            terrain_semantics(chunk, (2, -3), True)
        except (RuntimeError, ValueError, KeyError, TypeError):
            pass
        else:
            raise RuntimeError('Corrupted remote terrain was admitted')
    boundary_negatives = fixture_controls()
    print(f'PASS: Dispenser placement offline reader controls 5 positive {len(bad) + len(bad_saves) + len(bad_remote) + boundary_negatives + 1} negative; inherited transport controls above; no native execution')


def fixture_controls():
    world = G.module('dp_fixture_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    nbt = G.module('dp_fixture_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    rejected = 0
    with tempfile.TemporaryDirectory(prefix='dispenser-flat441-') as temporary:
        directory = Path(temporary)
        regions(world, directory)
        (directory / 'r.9.9.mcr').write_bytes(bytes(8192))
        require(terrain_gate(nbt, directory, False)['chunks'] == 441, 'Physical fresh 441-Chunk fixture readback')
        originals = {path.name: path.read_bytes() for path in directory.iterdir()}

        def refusal(images):
            nonlocal rejected
            for name, raw in images.items():
                (directory / name).write_bytes(raw)
            try:
                terrain_gate(nbt, directory, False)
            except (RuntimeError, ValueError, KeyError, TypeError, AssertionError):
                rejected += 1
            else:
                raise RuntimeError('Corrupted physical 441-Chunk fixture was admitted')
            for name, raw in originals.items():
                (directory / name).write_bytes(raw)

        def clear(coordinates):
            images = {name: bytearray(raw) for name, raw in originals.items()}
            for x, z in coordinates:
                struct.pack_into('>I', images[f'r.{x >> 5}.{z >> 5}.mcr'], ((x & 31) + (z & 31) * 32) * 4, 0)
            return images

        refusal(clear(COORDINATES - ORIGINAL_COORDINATES))
        refusal(clear({(x, -9) for x in range(-10, 11)}))
        refusal(clear({(-10, -11)}))
        images = dict(originals)
        extra = bytearray(images['r.-1.-1.mcr'])
        record, count = chunk_record(world, -11, -11)
        struct.pack_into('>I', extra, ((-11 & 31) + (-11 & 31) * 32) * 4, (len(extra) // 4096) * 256 + count)
        extra += record
        images['r.-1.-1.mcr'] = extra
        refusal(images)
        # Mutate actual complete compressed NBT in an outer authored Chunk;
        # preserve its correct coordinate and all Region allocation extents.
        for key, index, replacement in (('Blocks', 10000, 0), ('Data', 11000, 1), ('TerrainPopulated', None, 0)):
            images = dict(originals)
            raw = bytearray(images['r.-1.-1.mcr'])
            slot = ((-10 & 31) + (-11 & 31) * 32) * 4
            location, = struct.unpack_from('>I', raw, slot)
            at, count = (location >> 8) * 4096, location & 255
            size, = struct.unpack_from('>I', raw, at)
            wire = zlib.decompress(raw[at + 5:at + 4 + size])
            level = nbt.Nbt(wire).root()['Level'][1]
            if index is None:
                before, after = world.tag(1, key, b'\1'), world.tag(1, key, b'\0')
            else:
                old = level[key][1]
                changed = bytearray(old)
                require(old[index] != replacement, 'Meaningful remote plane mutation')
                changed[index] = replacement
                before = world.tag(7, key, struct.pack('>i', len(old)) + old)
                after = world.tag(7, key, struct.pack('>i', len(changed)) + changed)
            require(wire.count(before) == 1, 'Unique physical NBT field mutation')
            compressed = zlib.compress(wire.replace(before, after))
            record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
            require(len(record) <= count * 4096, 'Corruption retains original sector extent')
            raw[at:at + count * 4096] = record + bytes(count * 4096 - len(record))
            images['r.-1.-1.mcr'] = raw
            refusal(images)
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.self_test:
        require(args.executable is None and args.assets is None and args.artifacts is None, 'No SDK operands in offline controls')
        self_test()
        return
    require(os.name == 'nt' and all(v is not None for v in (args.executable, args.assets, args.artifacts)), 'Explicit Windows SDK operands')
    world = G.module('dp_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = G.module('dp_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = G.module('dp_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    nbt = G.module('dp_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    transport = G.module('dp_transport', ROOT / 'tests/reference/client/controller/capture.py')
    product = G.module('dp_products', ROOT / 'tests/tools/client/native_product_capture.py')
    monitor = G.module('dp_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='ordinary-placement-', dir=args.artifacts.resolve()))
    for name in ('snapshots', 'entries'):
        (evidence / name).mkdir()
    before, assets_before = fingerprints(inputs(executable) | {Path(__file__).resolve()}), world.snapshot(assets)
    report = {'status': 'FAIL', 'monitor': monitor, 'inputs_before': before, 'assets_before': assets_before,
              'scope': 'One authored count-one BlockItem23; ordinary runtime placement into authored air, empty AZ/GQ, close and save/reselect. '
                       'No physical input, ordinary acquisition, firing/power, all-cardinal runtime coverage, sustained play, FPS or original pixels/parity.',
              'capture_limit': 'Bounded single actual child with checked closure; no independent descendant or loaded-library census.',
              'python': {'executable': sys.executable, 'version': sys.version}}
    write(evidence / 'run.json', report)
    try:
        with tempfile.TemporaryDirectory(prefix='dispenser-placement-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            report['authored'] = authored(world, selected, mining, nbt, saves)
            before_files = world.save_title_files(saves, evidence / 'before')
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds,
                               MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds, MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'), PYTHONDONTWRITEBYTECODE='1')
            command = [str(executable), str(assets), str(evidence)]
            execution = {'command': command, 'before': before_files, 'actual_environment': environment,
                         'working_directory': str(Path.cwd()), 'process_first_millis': time.time_ns() // 1000000}
            write(evidence / 'execution.json', execution)
            try:
                execution['native_products'] = product.capture_native_products(command, evidence)
                write(evidence / 'execution.json', execution)
                report['capture'] = capture_owned(transport, command, evidence, environment, execution)
                execution['process_last_millis'] = time.time_ns() // 1000000
            except BaseException as error:
                execution['failure'] = {'type': type(error).__name__, 'message': str(error)}
                if getattr(error, 'receipt', None) is not None:
                    execution['native_products'] = error.receipt
                raise
            finally:
                try:
                    execution['after'] = world.save_title_files(saves, evidence / 'after')
                except BaseException as error:
                    execution['after_snapshot_error'] = {'type': type(error).__name__, 'message': str(error)}
                write(evidence / 'execution.json', execution)
            transport.require_success(report['capture'], empty_stderr=True)
            natural_custody(execution)
            require('after' in execution and 'after_snapshot_error' not in execution, 'Complete retained after saves')
            raw_out, raw_err = (evidence / 'native.stdout').read_bytes(), (evidence / 'native.stderr').read_bytes()
            stdout, stderr = raw_out.decode('utf8').replace('\r\n', '\n'), raw_err.decode('utf8')
            print(stdout, end='')
            print(stderr, end='', file=sys.stderr)
            interval = (execution['process_first_millis'], execution['process_last_millis'])
            report['native'] = receipts(stdout, stderr, monitor, evidence=evidence, interval=interval)
            require({path.name for path in evidence.glob('dispenser-placement-*.png')} == IMAGES, 'Exact fresh image inventory')
            report['images'] = {name: G.png_gate(evidence / name, interval[0]) for name in sorted(IMAGES)}
            report['save'] = saved_gate(nbt, mining, evidence / 'snapshots/dispenser-placement-saved/alpha', True)
            report['after_save'] = saved_gate(nbt, mining, evidence / 'after/alpha', True)
            unaffected = lambda values: {name: value for name, value in values.items() if not name.startswith('alpha/') or
                                        name.endswith('/preserve-chunk.dat') or name == 'alpha/region/r.9.9.mcr'}
            require(unaffected(before_files) == unaffected(execution['after']), 'Exact unrelated saves and controls preserved')
        report['status'] = 'PASS'
    except BaseException as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        try:
            report['inputs_after'] = fingerprints(inputs(executable) | {Path(__file__).resolve()})
            report['assets_after'] = world.snapshot(assets)
            report['assets_unchanged'] = report['assets_after'] == assets_before
            if report['inputs_after'] != before or not report['assets_unchanged']:
                report['status'] = 'FAIL'
        except BaseException as error:
            report['status'] = 'FAIL'
            report['continuity_failure'] = {'type': type(error).__name__, 'message': str(error)}
        write(evidence / 'run.json', report)
        print('Dispenser placement artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Dispenser placement protocol or source/assets/products continuity')
    print('PASS: ordinary Dispenser placement SDK save/reselection')


if __name__ == '__main__':
    main()
