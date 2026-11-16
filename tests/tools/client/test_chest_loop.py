"""Authored Windows Chest inputs through SDK transfer, save and actual reselection."""
import argparse
import gzip
import hashlib
import importlib.util
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

ROOT = Path(__file__).resolve().parents[3]
sys.dont_write_bytecode = True
PROFILES = frozenset(('single', 'double', 'dense', 'blocked', 'shutdown', 'placement'))
PLACEMENT = ('CHEST_PLACEMENT placed=2 rejected=1 transferred=5 held=1 '
             'sdk-strafe=1 source-field-repair=0')
OPEN_PREPARE = ('CHEST_OPEN_PREPARE source-unchanged=1 world-time-unchanged=1 '
                'ticks-unchanged=1 stacks-unchanged=1 native-prefix-only=1')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1048576), b''):
            value.update(block)
    return value.hexdigest()


def source_inputs(executable):
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
              ROOT / 'tests/tools/world/test_source_saving.py'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(executable.parent.glob('*.dll'))
    return inputs


def fingerprints(inputs):
    result = {}
    for path in sorted(inputs):
        try:
            result[str(path)] = digest(path)
        except OSError as error:
            result[str(path)] = {'error': type(error).__name__}
    return result


def bounded(path, maximum):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= maximum,
            'Bounded regular evidence file ' + path.name)
    with path.open('rb') as source:
        raw = source.read(maximum + 1)
    require(len(raw) <= maximum, 'Evidence file grew beyond bound')
    return raw


def typed(value, kind, message):
    require(type(value) is list and len(value) == 2 and type(value[0]) is int and value[0] == kind,
            message)
    return value[1]


def compound(value, message):
    result = typed(value, 10, message)
    require(type(result) is dict, message)
    return result


def placement_receipts(lines, open_host, reload_frame, process_interval=None, evidence=None):
    """Validate the authored placement branch, including its actual save transport."""
    prefixes = ('CHEST_PLACE_TAP', 'CHEST_STRAFE', 'WORLD_SAVE_TITLE', 'WORLD_RELOAD_SNAPSHOT')
    events = [(position, line) for position, line in enumerate(lines)
              if line.startswith(prefixes) and not line.startswith('WORLD_SAVE_TITLE_HOST')]
    require(events and all(open_host < position < reload_frame for position, _ in events),
            'Placement actions and save precede the reloaded host')
    at = 0

    def take(pattern, message):
        nonlocal at
        require(at < len(events), message)
        found = re.fullmatch(pattern, events[at][1])
        require(found is not None, message)
        at += 1
        return found

    def tap(x, y, placed):
        row = take(r'CHEST_PLACE_TAP target=([0-9]+),([0-9]+),2 placed=([01]) '
                   r'attempts=([1-9][0-9]?) clicks=1 block-use=1 admitted=([01]) '
                   r'setters=([01]) sounds=([01]) count-stores=([01])',
                   'Exact ordered placement/activation tap and counter deltas')
        require(tuple(map(int, row.group(1, 2, 3))) == (x, y, placed) and
                1 <= int(row[4]) <= 12 and all(int(row[i]) == placed for i in range(5, 9)),
                'Declared tap target, outcome, bounded attempts and exact mutations')

    def strafe(x, y):
        count, position = 0, None
        while at < len(events) and events[at][1].startswith('CHEST_STRAFE'):
            row = take(r'CHEST_STRAFE attempt=([1-9][0-9]?) target=([12]),(63|64),2 '
                       r'key=([AD]) x=([+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)'
                       r'(?:[eE][+-]?[0-9]+)?)', 'Exact finite SDK strafe receipt')
            count += 1
            position = float(row[5])
            require(count <= 20 and int(row[1]) == count and
                    tuple(map(int, row.group(2, 3))) == (x, y) and math.isfinite(position),
                    'Ordered nonempty bounded strafe group with contiguous attempts')
        require(count > 0 and x + .1 < position < x + .9,
                'SDK strafe finishes inside the next picked cell')

    tap(0, 63, 1)
    tap(0, 64, 0)
    for x, y, placed in ((1, 63, 1), (2, 63, 0), (1, 64, 0)):
        strafe(x, y)
        tap(x, y, placed)

    unsigned = r'(0|[1-9][0-9]{0,19})'
    signed = r'(0|-?[1-9][0-9]{0,18})'

    def numbers(pattern, message):
        row = take(pattern, message)
        values = tuple(map(int, row.groups()))
        require(all(-(1 << 63) <= value < 1 << 64 for value in values), message)
        return values

    recovery = numbers(r'WORLD_SAVE_TITLE_RECOVERY focus-pauses=' + unsigned +
                       r' keyboard-before-escape=' + unsigned + r' resumed=1',
                       'Actual save/title recovery boundary')
    require(recovery[0] <= 4, 'Bounded focus recovery count')
    retries = 0
    while at < len(events) and events[at][1].startswith('WORLD_SAVE_TITLE_AUTOPAUSE'):
        pause = numbers(r'WORLD_SAVE_TITLE_AUTOPAUSE attempt=' + unsigned + r' focus-pauses=' +
                        unsigned + r' old-oz-keys=2 real-id4=1 resumed=1 frames=' + unsigned,
                        'Exact source automatic-pause recovery')
        retries += 1
        require(pause[0] == retries <= 3 and pause[1] == recovery[0] + 1 <= 4 and
                1 <= pause[2] < 64, 'Bounded ordered source recovery attempts')
        following = numbers(r'WORLD_SAVE_TITLE_RECOVERY focus-pauses=' + unsigned +
                            r' keyboard-before-escape=' + unsigned + r' resumed=1',
                            'Recovery precedes each retried Escape pair')
        require(following == (pause[1], recovery[1] + 2), 'Recovery keyboard/count continuity')
        recovery = following
    escape = numbers(r'WORLD_SAVE_TITLE_ESCAPE keyboard-before=' + unsigned +
                     r' keyboard-after=' + unsigned + r' fresh-pause=1 distinct-recovered=1',
                     'Actual SDK Escape opens a fresh source Pause')
    require(escape[0] == recovery[1] and escape[1] >= escape[0] + 2,
            'Escape follows the final recovery keyboard boundary')
    entry = numbers(r'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=' + unsigned +
                    r' eligible=' + unsigned + r' time=' + signed + r' size-before=' + signed +
                    r' size-after=' + signed + r' entered=1 bytes=' + unsigned +
                    r' copied-before-frame=1', 'Exact actual source save entry')
    saved = numbers(r'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 '
                    r'i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 '
                    r'original-load-returned=1 eligible=' + unsigned + r' time=' + signed +
                    r' size-before=' + signed + r' size-after=' + signed + r' first=' + signed +
                    r' last=' + signed + r' entry-frame=' + unsigned + r' entry-bytes=' + unsigned,
                    'Exact actual successful source save/title continuation')
    frame, eligible, world_time, before, after, entry_bytes = entry
    require(0 <= frame < 64 and eligible > 0 and 0 <= world_time < 1 << 63 and
            0 <= before <= after < 1 << 63 and 0 < entry_bytes <= 16 * 1048576 and
            saved[:4] == (eligible, world_time, before, after) and saved[6:] == (frame, entry_bytes),
            'Save entry and successful continuation share all observed fields')
    first, last = saved[4:6]
    require(0 < first <= last < 1 << 63, 'Ordered actual save timestamps')
    if process_interval is not None:
        require(type(process_interval) in (tuple, list) and len(process_interval) == 2 and
                all(type(value) is int for value in process_interval) and
                0 < process_interval[0] <= first <= last <= process_interval[1],
                'Save timestamps fall inside this native process interval')
    snapshot = numbers(r'WORLD_RELOAD_SNAPSHOT phase=chest-saved files=' + unsigned +
                       r' bytes=' + unsigned + r' actual-file-copy=1',
                       'Unique actual Chest save snapshot precedes reselection')
    require(0 < snapshot[0] <= 1024 and 0 < snapshot[1] <= 16 * 1048576,
            'Bounded actual source save snapshot receipt')
    tap(1, 64, 0)
    require(at == len(events), 'No duplicate, extra or moved placement/save records')
    if evidence is not None:
        entries = evidence / 'entries'
        require(entries.is_dir() and not entries.is_symlink() and
                {path.name for path in entries.iterdir()} == {f'frame-{i}' for i in range(frame + 1)},
                'Exact native pre-save entry candidate inventory')
        for index in range(frame + 1):
            inventory = snapshot_files(entries / f'frame-{index}')
            if index == frame:
                require(sum(item['bytes'] for item in inventory.values()) == entry_bytes,
                        'Selected actual entry bytes match source receipt')
        inventory = snapshot_files(evidence / 'snapshots/chest-saved')
        require((len(inventory), sum(item['bytes'] for item in inventory.values())) == snapshot,
                'Retained actual snapshot files and bytes match native receipt')


def native_receipts(stdout, profile, monitor, stderr='', *, process_interval=None, evidence=None):
    """Decode this process's exact protocol; synthetic inputs prove only the decoder."""
    require(profile in PROFILES and type(stdout) is str and stderr == '', 'Clean successful native transport')
    lines = stdout.splitlines()
    finish = [line for line in lines if line.startswith('CHEST_FINISH')]
    require(len(finish) == 1, 'Unique native Chest cleanup receipt')
    row = re.fullmatch(r'CHEST_FINISH profile=(single|double|dense|blocked|shutdown|placement) '
                       r'cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9})', finish[0])
    require(row is not None and row[1] == profile and int(row[2]) <= 4294967295,
            'Complete positive uint32 Chest cleanup receipt')
    require(lines and lines[-1] == finish[0], 'Cleanup receipt is the final native line')
    sdk = [line for line in lines if line.startswith('CHEST_SDK')]
    shutdown = [line for line in lines if line.startswith('CHEST_OPEN_SHUTDOWN')]
    prepared = [line for line in lines if line.startswith('CHEST_OPEN_PREPARE')]
    placement = [line for line in lines if line.startswith('CHEST_PLACEMENT')]
    require(placement == ([PLACEMENT] if profile == 'placement' else []),
            'Declared SDK placement, transfer, neighbor and rejection receipt')
    if profile == 'shutdown':
        require(not sdk and shutdown == ['CHEST_OPEN_SHUTDOWN actual-open-hp=1 source-close=0'] and
                prepared == [OPEN_PREPARE], 'Actual open Chest and passive terminal prefix receipts')
        require(lines.index(prepared[0]) < lines.index(shutdown[0]), 'Prepared prefix precedes open shutdown receipt')
        stages = ('open', 'open-chest-terminal')
    else:
        require(sdk == [f'CHEST_SDK profile={profile} ordinary-save-reload=1 epochs=2 source-field-repair=0'] and
                not shutdown and not prepared, 'Actual two-epoch Chest save/reload receipt')
        stages = ('open', 'after-chest-reload')
    bounds = monitor['selected']['work']
    require(type(bounds) in (list, tuple) and len(bounds) == 4 and all(type(value) is int for value in bounds) and
            bounds[2] > 0 and bounds[3] > 0, 'Physical monitor work rectangle')
    x, y, width, height = bounds
    target = ','.join(map(str, bounds))
    observations, monitor_positions = [], {}
    for position, line in enumerate(lines):
        if not line.startswith('WORLD_SELECTION_MONITOR'):
            continue
        item = re.fullmatch(r'WORLD_SELECTION_MONITOR stage=(\S+) window=(frame|canvas) '
                            r'x=(-?[0-9]+) y=(-?[0-9]+) width=([1-9][0-9]*) height=([1-9][0-9]*) target=(\S+)', line)
        require(item is not None and item[7] == target, 'Exact native monitor witness')
        wx, wy, ww, wh = map(int, item.group(3, 4, 5, 6))
        require((ww, wh) == (854, 480) and wx >= x and wy >= y and
                wx + ww <= x + width and wy + wh <= y + height, 'Source window fits physical monitor 2')
        observations.append({'stage': item[1], 'window': item[2], 'rectangle': [wx, wy, ww, wh]})
        monitor_positions[(item[1], item[2])] = position
    require([(item['stage'], item['window']) for item in observations] ==
            [(stage, window) for stage in stages for window in ('frame', 'canvas')],
            'Both source windows witnessed at both lifecycle stages')
    hosts, host_positions = [], {}
    for position, line in enumerate(lines):
        if not line.startswith('WORLD_SAVE_TITLE_HOST'):
            continue
        item = re.fullmatch(r'WORLD_SAVE_TITLE_HOST stage=(\S+) frame-id=([1-9][0-9]{0,9}) '
                            r'canvas-id=([1-9][0-9]{0,9}) canvas=1 display-parent=1 helper-parent=1 '
                            r'bound-app=1 borrowed-window=1', line)
        require(item is not None, 'Exact live source-host witness')
        ids = tuple(map(int, item.group(2, 3)))
        require(max(ids) <= 4294967295 and ids[0] != ids[1], 'Distinct nonzero uint32 source window IDs')
        hosts.append((item[1], ids))
        host_positions[item[1]] = position
    require(len(hosts) == 2 and [item[0] for item in hosts] == list(stages) and
            hosts[0][1] == hosts[1][1], 'Stable source host through native continuation')
    require(all(monitor_positions[(stage, 'frame')] < monitor_positions[(stage, 'canvas')] <
                host_positions[stage] for stage in stages) and
            host_positions[stages[0]] < monitor_positions[(stages[1], 'frame')],
            'Ordered native geometry and source-host lifecycle witnesses')
    require(host_positions[stages[1]] < lines.index(prepared[0] if profile == 'shutdown' else sdk[0]),
            'Continuation receipt follows the final live source-host witness')
    if profile == 'placement':
        require(host_positions[stages[1]] < lines.index(placement[0]) < lines.index(sdk[0]),
                'Placement receipt follows actual reload host and precedes continuation')
        placement_receipts(lines, host_positions['open'],
                           monitor_positions[('after-chest-reload', 'frame')],
                           process_interval, evidence)
    return {'checks': int(row[2]), 'window_ids': list(hosts[0][1]), 'monitor_witnesses': observations}


def png_gate(path, first_millis):
    raw = bounded(path, 4194304)
    require(path.stat().st_mtime_ns >= first_millis * 1000000, 'Fresh process-owned image')
    require(raw.startswith(b'\x89PNG\r\n\x1a\n'), 'PNG signature')
    at, chunks, compressed, header = 8, [], bytearray(), None
    while at < len(raw):
        require(len(raw) - at >= 12, 'Complete PNG chunk framing')
        size, = struct.unpack_from('>I', raw, at)
        require(size <= len(raw) - at - 12, 'Bounded PNG chunk')
        kind = raw[at + 4:at + 8]
        body = raw[at + 8:at + 8 + size]
        crc, = struct.unpack_from('>I', raw, at + 8 + size)
        require(re.fullmatch(b'[A-Za-z]{4}', kind) is not None and zlib.crc32(kind + body) == crc,
                'PNG chunk type and checksum')
        chunks.append(kind)
        if kind == b'IHDR':
            require(len(chunks) == 1 and len(body) == 13 and header is None, 'Single initial PNG header')
            header = struct.unpack('>IIBBBBB', body)
            require(header[:3] == (854, 480, 8) and header[3] in (2, 6) and header[4:] == (0, 0, 0),
                    'Actual source image dimensions and RGB/RGBA encoding')
        elif kind == b'IDAT':
            require(header is not None, 'PNG header precedes image data')
            require(b'IDAT' not in chunks[:-1] or chunks[-2] == b'IDAT', 'Consecutive PNG image data chunks')
            compressed += body
        elif kind == b'IEND':
            require(size == 0 and at + 12 == len(raw), 'Final empty PNG end marker')
        else:
            require(kind[0] & 32, 'Known critical PNG chunk')
        at += size + 12
    require(chunks and chunks[-1] == b'IEND' and header is not None and compressed, 'Complete PNG image')
    channels = 3 if header[3] == 2 else 4
    stride = 854 * channels + 1
    decoder = zlib.decompressobj()
    pixels = decoder.decompress(compressed, stride * 480 + 1)
    require(len(pixels) == stride * 480 and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail and all(pixels[index * stride] <= 4 for index in range(480)),
            'Complete bounded PNG scanlines')
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(), 'width': 854, 'height': 480,
            'color_type': header[3]}


def image_gate(evidence, profile, first_millis):
    names = {'chest-title.png'} if profile == 'blocked' else {'chest-open.png'}
    if profile not in ('blocked', 'shutdown'):
        names.update(('chest-title.png', 'chest-reloaded.png'))
    if profile == 'dense':
        names.add('chest-dense-cursor.png')
    if profile == 'placement':
        names.update(('chest-placed-single.png', 'chest-placement-double.png', 'chest-third-rejected.png'))
    require({path.name for path in evidence.glob('chest-*.png')} == names, 'Exact fresh Chest image inventory')
    return {name: png_gate(evidence / name, first_millis) for name in sorted(names)}


def snapshot_files(root):
    require(root.is_dir() and not root.is_symlink(), 'Existing regular native snapshot root')
    resolved, result, total = root.resolve(strict=True), {}, 0
    for path in sorted(root.rglob('*')):
        require(not path.is_symlink() and path.resolve(strict=True).is_relative_to(resolved),
                'Contained regular snapshot inventory')
        if path.is_dir():
            continue
        require(len(result) < 1024, 'Bounded snapshot file count')
        raw = bounded(path, 1048576)
        total += len(raw)
        require(total <= 16 * 1048576, 'Bounded snapshot aggregate bytes')
        result[path.relative_to(root).as_posix()] = {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}
    require(result, 'Nonempty native snapshot')
    return result


def stack(world, slot, item_id, count):
    return (world.tag(1, 'Slot', struct.pack('>b', slot)) +
            world.tag(2, 'id', struct.pack('>h', item_id)) +
            world.tag(1, 'Count', struct.pack('>b', count)) +
            world.tag(2, 'Damage', b'\0\0') + b'\0')


def authored_slots(dense, neighbor=False):
    if dense:
        return [(slot, 264, 2, 0) for slot in range(27)]
    return [(0, 265, 7, 0)] if neighbor else [(0, 264, 5, 0), (1, 266, 3, 0)]


def chest(world, x, dense):
    slots = authored_slots(dense, x != 0)
    value = world.tag(8, 'id', world.string('Chest'))
    for key, number in (('x', x), ('y', 64), ('z', 2)):
        value += world.tag(3, key, struct.pack('>i', number))
    value += world.tag(9, 'Items', b'\x0a' + struct.pack('>i', len(slots)) +
                       b''.join(stack(world, slot, item_id, count) for slot, item_id, count, _ in slots))
    return value + b'\0'


def regions(world, directory, profile):
    """Authored flat input only; no production save or runtime setter creates it."""
    output = {}
    dense = profile == 'dense'
    for x in range(-8, 9):
        for z in range(-8, 9):
            blocks, light = bytearray(32768), bytearray(16384)
            heights = bytearray([64] * 256)
            for bx in range(16):
                for bz in range(16):
                    at = (bx << 11) | (bz << 7)
                    blocks[at] = 7
                    blocks[at + 1:at + 64] = bytes([3] * 63)
                    light[(at + 64) // 2:(at + 128) // 2] = bytes([255] * 32)
            tiles = []
            if (x, z) == (0, 0) and profile != 'placement':
                for bx in range(1 if profile == 'single' else 2):
                    at = (bx << 11) | (2 << 7) | 64
                    blocks[at] = 54
                    tiles.append(chest(world, bx, dense))
                if profile == 'blocked':
                    at = (1 << 11) | (2 << 7) | 65
                    blocks[at] = 1
                    heights[2 * 16 + 1] = 66
                    light[at // 2] = 0
            level = world.tag(3, 'xPos', struct.pack('>i', x)) + world.tag(3, 'zPos', struct.pack('>i', z))
            level += world.tag(4, 'LastUpdate', bytes(8))
            for key, value in (('Blocks', blocks), ('Data', bytes(16384)), ('SkyLight', light),
                               ('BlockLight', bytes(16384)), ('HeightMap', heights)):
                level += world.tag(7, key, struct.pack('>i', len(value)) + value)
            level += world.tag(1, 'TerrainPopulated', b'\1')
            level += world.tag(9, 'Entities', b'\x0a' + bytes(4))
            level += world.tag(9, 'TileEntities', b'\x0a' + struct.pack('>i', len(tiles)) + b''.join(tiles))
            raw = b'\x0a\0\0' + world.tag(10, 'Level', level + b'\0') + b'\0'
            compressed = zlib.compress(raw)
            record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
            count = (len(record) + 4095) // 4096
            region = output.setdefault((x >> 5, z >> 5), bytearray(8192))
            sector = len(region) // 4096
            struct.pack_into('>I', region, ((x & 31) + (z & 31) * 32) * 4, sector * 256 + count)
            region += record + bytes(count * 4096 - len(record))
    for (x, z), data in output.items():
        (directory / f'r.{x}.{z}.mcr').write_bytes(data)


def slot_rows(tag):
    values = typed(tag, 9, 'Compound Stack list')
    require(type(values) is list and len(values) == 2 and type(values[0]) is int and
            0 <= values[0] <= 10 and type(values[1]) is list and len(values[1]) <= 64 and
            (values[0] == 10 or not values[1]), 'Bounded Compound Stack list')
    rows = []
    for item in values[1]:
        require(type(item) is dict and set(item) == {'Slot', 'id', 'Count', 'Damage'}, 'Exact Stack fields')
        row = tuple(typed(item[key], kind, 'Exact Stack tag widths') for key, kind in
                    (('Slot', 1), ('id', 2), ('Count', 1), ('Damage', 2)))
        require(all(type(value) is int and -(1 << (bits - 1)) <= value < 1 << (bits - 1)
                    for value, bits in zip(row, (8, 16, 8, 16))), 'Raw signed Stack field values')
        rows.append(row)
    require(len({row[0] for row in rows}) == len(rows), 'Unique Stack slots')
    return sorted(rows)


def saved_gate(decoder, directory, profile):
    require(profile in PROFILES, 'Declared Chest save profile')
    metadata = decoder.inflate(bounded(directory / 'level.dat', decoder.BOUND), 31)
    require(type(metadata) is dict and 'Data' in metadata, 'Saved metadata Data root')
    data = compound(metadata['Data'], 'Saved actual metadata Compound')
    require('Player' in data, 'Saved actual Player')
    player = compound(data['Player'], 'Saved actual Player Compound')
    dense, blocked, shutdown = profile == 'dense', profile == 'blocked', profile == 'shutdown'
    placement = profile == 'placement'
    expected_player = [(slot, 264, 2, 0) for slot in range(36)] if dense else (
        [(0, 54, 1, 0)] if placement else [] if blocked or shutdown else [(8, 266, 3, 0)])
    require('Inventory' in player and typed(player.get('Dimension'), 3, 'Saved Player Dimension width') == 0,
            'Saved Overworld Player Inventory')
    require(slot_rows(player['Inventory']) == expected_player, 'Saved actual Player Inventory')
    if placement:
        position = typed(player.get('Pos'), 9, 'Saved placement Player Pos List')
        rotation = typed(player.get('Rotation'), 9, 'Saved placement Player Rotation List')
        require(type(position) is list and len(position) == 2 and position[0] == 6 and
                type(position[1]) is list and len(position[1]) == 3 and
                all(type(value) is int and 0 <= value < 1 << 64 for value in position[1]),
                'Exact saved placement position widths')
        pose = [struct.unpack('>d', struct.pack('>Q', value))[0] for value in position[1]]
        require(1 < pose[0] < 2 and 64 < pose[1] < 66 and pose[2] == -.5,
                'Saved SDK strafe position remains at the observed second Chest')
        require(type(rotation) is list and len(rotation) == 2 and rotation[0] == 5 and
                type(rotation[1]) is list and len(rotation[1]) == 2 and
                all(type(value) is int and 0 <= value < 1 << 32 for value in rotation[1]) and
                [struct.unpack('>f', struct.pack('>I', value))[0] for value in rotation[1]] == [0., 30.],
                'Saved authored look remains unchanged through genuine strafe')
    path = directory / 'region/r.0.0.mcr'
    require(path.is_file() and not path.is_symlink(), 'Regular actual region file')
    file_size = path.stat().st_size
    require(12288 <= file_size <= decoder.BOUND and file_size % 4096 == 0, 'Complete bounded actual region extent')
    with path.open('rb') as source:
        header = source.read(8192)
        require(len(header) == 8192, 'Complete actual region header')
        allocations = []
        for location in struct.unpack_from('>1024I', header):
            if not location:
                continue
            sector, sectors = location >> 8, location & 255
            require(sector >= 2 and sectors > 0 and (sector + sectors) * 4096 <= file_size,
                    'Every allocated record fits this authored region')
            allocations.append((sector, sector + sectors))
        allocations.sort()
        require(all(previous[1] <= following[0] for previous, following in
                    zip(allocations, allocations[1:])), 'No overlapping actual Chunk allocations')
        location, = struct.unpack_from('>I', header)
        sector, sectors = location >> 8, location & 255
        require(sector >= 2 and sectors > 0 and (sector + sectors) * 4096 <= file_size,
                'Selected Chunk allocation lies beyond header and within region')
        source.seek(sector * 4096)
        record = source.read(5)
        require(len(record) == 5, 'Selected Chunk record header')
        size, = struct.unpack_from('>I', record)
        require(1 < size <= sectors * 4096 - 4 and size - 1 <= decoder.BOUND and record[4] == 2,
                'Bounded actual Chunk record')
        payload = source.read(size - 1)
        require(len(payload) == size - 1, 'Complete selected Chunk compressed payload')
    root = decoder.inflate(payload, 15)
    require(type(root) is dict and 'Level' in root, 'Saved Chunk Level root')
    level = compound(root['Level'], 'Saved actual Chunk Level Compound')
    require(typed(level.get('xPos'), 3, 'Saved Chunk X width') == 0 and
            typed(level.get('zPos'), 3, 'Saved Chunk Z width') == 0, 'Selected Chunk coordinates')
    blocks = typed(level.get('Blocks'), 7, 'Saved Blocks ByteArray')
    nibbles = typed(level.get('Data'), 7, 'Saved metadata ByteArray')
    require(type(blocks) is bytes and len(blocks) == 32768 and
            type(nibbles) is bytes and len(nibbles) == 16384, 'Complete saved block arrays')
    for x in (0, 1):
        at = (x << 11) | (2 << 7) | 64
        require(blocks[at] == (54 if x == 0 or profile != 'single' else 0) and
                ((nibbles[at // 2] >> ((at & 1) * 4)) & 15) == 0,
                'Actual saved Chest block and metadata')
        require(blocks[at + 1] == (1 if blocked and x == 1 else 0), 'Authored Chest obstruction state')
    if placement:
        at = (2 << 11) | (2 << 7) | 64
        require(blocks[at] == 0 and ((nibbles[at // 2] >> ((at & 1) * 4)) & 15) == 0 and
                blocks[at - 1] == 3 and blocks[at + 1] == 0,
                'Rejected third Chest left actual air, support and metadata unchanged')
    entities = typed(level.get('Entities'), 9, 'Saved actual Entity list')
    require(type(entities) is list and len(entities) == 2 and type(entities[0]) is int and
            0 <= entities[0] <= 10 and type(entities[1]) is list and not entities[1],
            'No unexpected dropped Item or Entity in selected Chest Chunk')
    tiles = typed(level.get('TileEntities'), 9, 'Saved Chest TileEntities')
    require(type(tiles) is list and len(tiles) == 2 and tiles[0] == 10 and
            type(tiles[1]) is list and 1 <= len(tiles[1]) <= 2, 'Saved actual Chest Compound list')
    observed = {}
    for tile in tiles[1]:
        require(type(tile) is dict and set(tile) == {'id', 'x', 'y', 'z', 'Items'}, 'Exact saved Chest fields')
        require(typed(tile['id'], 8, 'Saved Chest id width') == 'Chest', 'Actual saved Chest type')
        position = tuple(typed(tile[key], 3, 'Exact saved Chest coordinate widths') for key in ('x', 'y', 'z'))
        require(all(type(value) is int for value in position), 'Actual signed Chest coordinates')
        require(position not in observed, 'Unique saved Chest position')
        observed[position] = slot_rows(tile['Items'])
    expected = {(0, 64, 2): authored_slots(dense)}
    if not dense and not blocked and not shutdown:
        expected[(0, 64, 2)] = [(0, 264, 5, 0)]
    if profile != 'single':
        expected[(1, 64, 2)] = [] if placement else authored_slots(dense, True)
    require(observed == expected, 'Saved actual Chest slots after source transfers')
    return {'player': expected_player, 'chests': [{'position': list(key), 'slots': value}
                                                for key, value in sorted(observed.items())]}


def shutdown_gate(decoder, evidence):
    stopped = evidence / 'snapshots/chest-open-stopped'
    prepared = evidence / 'snapshots/chest-open-prepared'
    before, after = snapshot_files(stopped), snapshot_files(prepared)
    require(before == after, 'Native terminal preparation leaves every saved byte unchanged')
    return {'stopped': saved_gate(decoder, stopped / 'alpha', 'shutdown'),
            'prepared': saved_gate(decoder, prepared / 'alpha', 'shutdown'),
            'files': before, 'source_bytes_unchanged': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--profile', choices=sorted(PROFILES), required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'Native Windows Chest SDK fixture')
    world = module('chest_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = module('chest_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = module('chest_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    decoder = module('chest_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    monitor = module('chest_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=args.profile + '-', dir=args.artifacts.resolve()))
    for name in ('snapshots', 'entries'):
        (evidence / name).mkdir()
    inputs = source_inputs(executable)
    before = fingerprints(inputs)
    require(all(type(pin) is str for pin in before.values()), 'Readable declared current inputs')
    assets_before = world.snapshot(assets)
    scope = ('Authored Windows open HP/CD, real Frame stop, passive terminal prefix and unchanged saved bytes'
             if args.profile == 'shutdown' else 'Authored Windows HP/CD SDK, save/reselection and ownership')
    report = {'status': 'FAIL', 'profile': args.profile, 'monitor': monitor, 'inputs_before': before,
              'scope': scope + '; no original pixels, sustained play or FPS claim.',
              'input_scope': 'Selected current source/helpers/product/DLLs and assets; excludes compiler, full dependencies and desktop state.'}
    (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    try:
        with tempfile.TemporaryDirectory(prefix='chest-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000), ('alpha', 'Chest world', 1304000000000)):
                directory = saves / folder
                directory.mkdir(parents=True)
                wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                if folder == 'alpha' and args.profile in ('dense', 'placement'):
                    old = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
                    rows = ([(slot, 264, 2) for slot in range(36)] if args.profile == 'dense'
                            else [(0, 54, 3), (9, 264, 5)])
                    new = world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', len(rows)) +
                                    b''.join(stack(world, *row) for row in rows))
                    require(wire.count(old) == 1, 'One authored Player Inventory')
                    wire = wire.replace(old, new)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Chest control')
            region = saves / 'alpha/region'
            region.mkdir()
            regions(world, region, args.profile)
            unrelated = region / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            before_files = world.save_title_files(saves, evidence / 'before')
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds,
                               MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds, MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first_millis = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence), args.profile],
                environment, evidence, saves, before_files, 'chest-' + args.profile, first_millis, 600)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native Chest SDK source loop returned')
            report['native'] = native_receipts(result.stdout, args.profile, monitor, result.stderr,
                process_interval=(first_millis, time.time_ns() // 1000000), evidence=evidence)
            report['images'] = image_gate(evidence, args.profile, first_millis)
            if args.profile != 'shutdown':
                report['save'] = saved_gate(decoder, evidence / 'snapshots/chest-saved/alpha', args.profile)
                require(unrelated.read_bytes() == bytes(8192), 'Unrelated region preserved')
            else:
                report['shutdown'] = shutdown_gate(decoder, evidence)
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = fingerprints(source_inputs(executable))
        report['assets_unchanged'] = world.snapshot(assets) == assets_before
        if report['inputs_after'] != before or not report['assets_unchanged']:
            report['status'] = 'FAIL'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('Chest artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Chest source/assets changed during observation')
    print('PASS: Chest ' + args.profile)


if __name__ == '__main__':
    main()
