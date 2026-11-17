"""Prelaunch saved Signs through real Timer/World/Font/model frames and save/reselection."""
import argparse
import copy
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import unittest
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/tools/client'))
import test_chest_loop as G

require = G.require
PROFILES = ('standing-0', 'standing-4', 'standing-8', 'standing-12',
            'wall-2', 'wall-3', 'wall-4', 'wall-5')
LINES = ('Native Sign', 'Beta 1.7.3', '0123456789', '')
IMAGES = ('sign-loaded.png', 'sign-title.png', 'sign-reloaded.png', 'held-dirt.png')
U = r'(0|[1-9][0-9]{0,19})'
S = r'(0|-?[1-9][0-9]{0,18})'


def modules():
    return G.module('sign_base_inputs', ROOT / 'tests/tools/client/test_lever_loop.py').modules()


def parameters(profile):
    require(profile in PROFILES, 'Declared saved Sign profile')
    standing, metadata = profile.startswith('standing-'), int(profile.split('-')[1])
    angle = metadata * 360 / 16 if standing else {2: 180, 3: 0, 4: 90, 5: -90}[metadata]
    # These are saved Player operands, selected before bootstrap. The camera
    # approaches the corresponding Sign face; runtime rotation is never repaired.
    side = ({0: 'south', 4: 'west', 8: 'north', 12: 'east'} if standing else
            {2: 'north', 3: 'south', 4: 'west', 5: 'east'})[metadata]
    x, z, yaw = {'north': (3.5, .5, 0.), 'south': (3.5, 6.5, 180.),
                 'west': (.5, 3.5, 270.), 'east': (6.5, 3.5, 90.)}[side]
    support = None if standing else {2: (3, 64, 4), 3: (3, 64, 2),
                                    4: (4, 64, 3), 5: (2, 64, 3)}[metadata]
    return {'standing': standing, 'block': 63 if standing else 68, 'metadata': metadata,
            'angle_bits': struct.pack('>f', angle).hex(), 'camera': (x, z, yaw), 'support': support}


def row_digest(lines):
    value = 14695981039346656037
    for row, text in enumerate(lines):
        units = b'' if text is None else text.encode('utf-16le', 'surrogatepass')
        wire = struct.pack('<IBQ', row, int(text is None), len(units) // 2) + units
        for byte in wire:
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f'{value:016x}'


def metadata(world, torch, name, profile):
    wire = torch.metadata(world, name, 1304000000000, 3)
    count = world.tag(1, 'Count', b'\2')
    require(wire.count(count) == 1, 'One declared prelaunch held Stack Count')
    wire = wire.replace(count, world.tag(1, 'Count', b'\1'))
    old_position = world.tag(9, 'Pos', b'\6' + struct.pack('>i3d', 3, 3.5,
        65. + struct.unpack('>f', struct.pack('>f', 1.62))[0], .5))
    old_rotation = world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 26.))
    require(wire.count(old_position) == wire.count(old_rotation) == 1, 'Exact inherited saved camera fields')
    x, z, yaw = parameters(profile)['camera']
    wire = wire.replace(old_position, world.tag(9, 'Pos', b'\6' + struct.pack('>i3d', 3, x,
        64. + struct.unpack('>f', struct.pack('>f', 1.62))[0], z)))
    return wire.replace(old_rotation, world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, yaw, 20.)))


def nibble_set(values, index, value):
    shift = (index & 1) * 4
    values[index // 2] = (values[index // 2] & ~(15 << shift)) | (value << shift)


def expected_tile():
    return {'id': [8, 'Sign'], 'x': [3, 3], 'y': [3, 64], 'z': [3, 3],
            **{f'Text{i+1}': [8, text] for i, text in enumerate(LINES)}}


def sign_semantics(level, blocks, data, profile):
    setting = parameters(profile)
    require(len(blocks) == 32768 and len(data) == 16384, 'Complete decoded saved planes')
    at = (3 << 11) | (3 << 7) | 64
    require(blocks[at] == setting['block'] and (data[at // 2] >> ((at & 1) * 4)) & 15 == setting['metadata'],
            'Independent exact saved Sign Block/metadata')
    require(level.get('TileEntities') == [9, [10, [expected_tile()]]],
            'Exactly one full saved Sign compound with precise field widths and current text')
    require(blocks[at - 1] in (1, 2, 3), 'Stable standing support beneath loaded Sign')
    if setting['support'] is not None:
        x, y, z = setting['support']
        require(blocks[(x << 11) | (z << 7) | y] == 1, 'Stable declared wall attachment support')
    return {'block': setting['block'], 'metadata': setting['metadata'], 'lines': list(LINES),
            'units': 31, 'text_digest': row_digest(LINES), 'tile_entities': 1}


def regions(m, directory, profile):
    """The only authored Sign, text, support and pose exist before bootstrap."""
    world, decoder, farming, torch = (m[key] for key in ('world', 'parser', 'farming', 'torch'))
    base = torch.authored_regions(world, decoder, farming, directory)
    old, images = torch.region_records(decoder, directory)
    name, location, stamp, allocation, payload, wire, level = old[(0, 0)]
    arrays = {key: bytearray(level[key][1]) for key in ('Blocks', 'Data', 'SkyLight', 'HeightMap')}
    setting = parameters(profile)
    index = lambda x, y, z: (x << 11) | (z << 7) | y
    # Remove the inherited north camera's one-high mining platform so every
    # declared camera starts on the flat Grass surface at feet y64.
    cells = [(3, 64, 3, setting['block'], setting['metadata']), (3, 64, 0, 0, 0)]
    for x, y, z in ((3, 64, 2), (3, 64, 4), (2, 64, 3), (4, 64, 3)):
        cells.append((x, y, z, 1 if (x, y, z) == setting['support'] else 0, 0))
    for x, y, z, block, meta in cells:
        at = index(x, y, z)
        arrays['Blocks'][at] = block
        nibble_set(arrays['Data'], at, meta)
    for x, z in {(x, z) for x, _, z, _, _ in cells}:
        highest = max(y for y in range(128) if arrays['Blocks'][index(x, y, z)] not in (0, 63, 68))
        arrays['HeightMap'][z * 16 + x] = highest + 1
        for y in range(128):
            nibble_set(arrays['SkyLight'], index(x, y, z), 15 if y > highest else 0)
    for key, value in arrays.items():
        before = world.tag(7, key, struct.pack('>i', len(level[key][1])) + level[key][1])
        require(wire.count(before) == 1, 'One declared selected-Chunk array field')
        wire = wire.replace(before, world.tag(7, key, struct.pack('>i', len(value)) + value))
    tile = world.tag(8, 'id', world.string('Sign'))
    for key, value in (('x', 3), ('y', 64), ('z', 3)):
        tile += world.tag(3, key, struct.pack('>i', value))
    for i, text in enumerate(LINES):
        tile += world.tag(8, f'Text{i+1}', world.string(text))
    empty = world.tag(9, 'TileEntities', b'\x0a' + bytes(4))
    require(wire.count(empty) == 1, 'One declared selected empty Tile List')
    wire = wire.replace(empty, world.tag(9, 'TileEntities', b'\x0a' + struct.pack('>i', 1) + tile + b'\0'))
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    sector, count = location >> 8, location & 255
    require(len(record) <= count * 4096, 'Selected saved Chunk retains finite allocation')
    images[name][sector * 4096:(sector + count) * 4096] = record + bytes(count * 4096 - len(record))
    (directory / name).write_bytes(images[name])
    for coordinate, row in old.items():
        if coordinate == (0, 0):
            continue
        region, slot, _, raw, _, _, _ = row
        start, length = (slot >> 8) * 4096, (slot & 255) * 4096
        require((directory / region).read_bytes()[start:start + length] == raw,
                'All 440 other allocated compressed records remain byte-exact')
    current_image = (directory / name).read_bytes()
    require(current_image[:8192] == images[name][:8192], 'All Region slots/timestamps preserved')
    reader = G.module('sign_selected_chunk_reader', ROOT / 'tests/client/test_furnace_gui.py')
    current, blocks, data = reader.selected_chunk(decoder, directory.parent)
    require(all(current[key] == [7, bytes(value)] for key, value in arrays.items()) and
            all(current[key] == level[key] for key in set(level) - set(arrays) - {'TileEntities'}),
            'Exact independently decoded selected planes and unchanged other fields')
    semantics = sign_semantics(current, blocks, data, profile)
    return {'base': base, **semantics, 'chunks': 441, 'changed_chunks': [[0, 0]],
            'other_compressed_records_preserved': 440, 'cells': [list(cell) for cell in cells],
            'selected_wire_sha256': hashlib.sha256(wire).hexdigest()}


def save_protocol(rows, *, evidence=None, interval=None):
    """Consume the genuine ordered recovery/Escape/save/copy protocol."""
    at = 0

    def take(pattern, message):
        nonlocal at
        require(at < len(rows), message)
        row = re.fullmatch(pattern, rows[at])
        require(row is not None, message)
        at += 1
        return row

    def nums(pattern, message):
        values = tuple(map(int, take(pattern, message).groups()))
        require(all(-(1 << 63) <= value < 1 << 64 for value in values), message)
        return values

    recovery = nums(r'WORLD_SAVE_TITLE_RECOVERY focus-pauses=' + U +
                    r' keyboard-before-escape=' + U + r' resumed=1', 'Actual save recovery precedes Escape')
    require(recovery[0] <= 4, 'Bounded focus recovery ledger')
    retries = 0
    while at < len(rows) and rows[at].startswith('WORLD_SAVE_TITLE_AUTOPAUSE'):
        pause = nums(r'WORLD_SAVE_TITLE_AUTOPAUSE attempt=' + U + r' focus-pauses=' + U +
                     r' old-oz-keys=2 real-id4=1 resumed=1 frames=' + U, 'Actual source automatic-pause recovery')
        retries += 1
        require(pause[0] == retries <= 3 and pause[1] == recovery[0] + 1 <= 4 and
                1 <= pause[2] < 64, 'Bounded contiguous recovery attempts')
        following = nums(r'WORLD_SAVE_TITLE_RECOVERY focus-pauses=' + U +
                         r' keyboard-before-escape=' + U + r' resumed=1', 'Recovery before next Escape pair')
        require(following == (pause[1], recovery[1] + 2), 'Recovery keyboard continuity')
        recovery = following
    escape = nums(r'WORLD_SAVE_TITLE_ESCAPE keyboard-before=' + U + r' keyboard-after=' + U +
                  r' fresh-pause=1 distinct-recovered=1', 'Actual SDK Escape opens a fresh Pause')
    require(escape[0] == recovery[1] and escape[1] >= escape[0] + 2, 'Escape follows recovery boundary')
    entry = nums(r'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=' + U + r' eligible=' + U +
                 r' time=' + S + r' size-before=' + S + r' size-after=' + S +
                 r' entered=1 bytes=' + U + r' copied-before-frame=1', 'Complete real save entry')
    saved = nums(r'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 '
                 r'i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 '
                 r'original-load-returned=1 eligible=' + U + r' time=' + S +
                 r' size-before=' + S + r' size-after=' + S + r' first=' + S + r' last=' + S +
                 r' entry-frame=' + U + r' entry-bytes=' + U, 'Complete successful real save continuation')
    frame, eligible, world_time, before, after, entry_bytes = entry
    require(0 <= frame < 64 and 0 < eligible <= 1024 and 0 <= world_time < 1 << 63 and
            0 <= before <= after < 1 << 63 and 0 < entry_bytes <= 16 * 1048576 and
            saved[:4] == entry[1:5] and saved[6:] == (frame, entry_bytes) and
            0 < saved[4] <= saved[5] < 1 << 63, 'All entry/continuation signed64 fields and clocks agree')
    if interval is not None:
        require(interval[0] <= saved[4] <= saved[5] <= interval[1], 'Save clock lies inside actual process')
    snapshot = nums(r'WORLD_RELOAD_SNAPSHOT phase=sign-saved files=' + U + r' bytes=' + U +
                    r' actual-file-copy=1', 'Actual saved copy before reselection')
    require(0 < snapshot[0] <= 1024 and 0 < snapshot[1] <= 16 * 1048576 and at == len(rows),
            'Complete unique bounded save/copy protocol')
    if evidence is not None:
        entries = evidence / 'entries'
        require(entries.is_dir() and not entries.is_symlink() and
                {p.name for p in entries.iterdir()} == {f'frame-{i}' for i in range(frame + 1)},
                'Exact actual pre-save candidate inventory')
        for i in range(frame + 1):
            inventory = G.snapshot_files(entries / f'frame-{i}')
            if i == frame:
                require(sum(v['bytes'] for v in inventory.values()) == entry_bytes, 'Selected entry byte receipt')
        inventory = G.snapshot_files(evidence / 'snapshots/sign-saved')
        require((len(inventory), sum(v['bytes'] for v in inventory.values())) == snapshot,
                'Saved snapshot file count and bytes independently agree')
    return {'frame': frame, 'time': world_time, 'size_before': before,
            'first': saved[4], 'last': saved[5], 'snapshot': list(snapshot)}



def receipts(stdout, stderr, monitor, profile, *, evidence=None, interval=None):
    setting = parameters(profile)
    require(type(stdout) is str and stdout.endswith('\n') and '\r' not in stdout and stderr == '',
            'Complete clean positive SDK transport')
    rows = stdout.splitlines()
    require(rows[:2] == ['16 achievements', '151 recipes'], 'Exact single Registry startup banner pair')
    target = monitor['selected']['work']
    require(len(target) == 4 and all(type(v) is int for v in target) and target[2] > 0 and target[3] > 0,
            'Physical monitor rectangle')
    at, index, hosts, phases = 2, 0, [], []

    def exact(value):
        nonlocal at
        require(at < len(rows) and rows[at] == value, 'Exact ordered native receipt: ' + value)
        at += 1

    def host(stage):
        nonlocal at
        for window in ('frame', 'canvas'):
            require(at < len(rows), 'Complete geometry lifecycle')
            row = re.fullmatch(r'WORLD_SELECTION_MONITOR stage=' + stage + ' window=' + window +
                               r' x=(-?[0-9]+) y=(-?[0-9]+) width=854 height=480 target=(\S+)', rows[at])
            require(row is not None and row[3] == ','.join(map(str, target)), 'Actual monitor2 geometry')
            x, y = map(int, row.group(1, 2))
            require(target[0] <= x and target[1] <= y and x + 854 <= target[0] + target[2] and
                    y + 480 <= target[1] + target[3], 'Both native windows contained on monitor2')
            at += 1
        row = re.fullmatch(r'WORLD_SAVE_TITLE_HOST stage=' + stage +
                           r' frame-id=([1-9][0-9]{0,9}) canvas-id=([1-9][0-9]{0,9}) '
                           r'canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1',
                           rows[at]) if at < len(rows) else None
        require(row is not None and int(row[1]) != int(row[2]) and
                max(map(int, row.groups())) <= 4294967295, 'Actual distinct bound Frame/Canvas IDs')
        hosts.append(tuple(map(int, row.groups())))
        at += 1

    def phase(name):
        nonlocal at, index
        draws = []
        while at < len(rows) and rows[at].startswith('SIGN_DRAW '):
            row = re.fullmatch(r'SIGN_DRAW index=' + U + r' phase=' + name + ' profile=' + profile +
                r' standing=([01]) metadata=' + U + r' angle=([0-9a-f]{8})'
                r' rows-measured=4 rows-drawn=4 units=31 null-rows=0 text-digest=([0-9a-f]{16})'
                r' model-parts=2 compiled=' + U + ' faces=' + U + ' vertices=' + U +
                r' tiles=1 sign-returned=1 sign-retired=1 row=3 width=0 x=0 y=10'
                r' text-scale=3c360b62 texture-bound=1 depth-restored=1 color-restored=1'
                r' matrix-popped=1 actual-tile=1 occurrence=1 world-returned=1 hud-returned=1'
                r' timer-ticks=' + U + ' world-ticks=' + U, rows[at])
            require(row is not None, 'Complete ordered real PO draw receipt')
            ordinal, standing, meta = map(int, row.group(1, 2, 3))
            compiled, faces, vertices, ticks, world_ticks = map(int, row.group(6, 7, 8, 9, 10))
            require(ordinal == index < 1024 and standing == int(setting['standing']) and meta == setting['metadata'] and
                    row[4] == setting['angle_bits'] and row[5] == row_digest(LINES) and
                    0 <= compiled <= (2 if setting['standing'] else 1) and faces == compiled * 6 and
                    vertices == faces * 4 and 0 <= ticks <= 10 and 0 <= world_ticks < 1 << 64,
                    'Exact actual orientation/text arguments, lazy PS counts and Timer bounds')
            require(not draws or draws[-1]['world_ticks'] <= world_ticks, 'Monotone actual World ticks')
            draws.append({'index': index, 'compiled': compiled, 'faces': faces, 'vertices': vertices,
                          'timer_ticks': ticks, 'world_ticks': world_ticks})
            at += 1
            index += 1
        require(at < len(rows) and len(draws) >= 3, 'At least three completed actual Sign frames')
        row = re.fullmatch(r'SIGN_PHASE phase=' + name + ' profile=' + profile + ' draws=' + U +
                           ' world-ticks=' + U + ' actual-world-tick-delta=' + U + r' current-text=1', rows[at])
        require(row is not None, 'Unique completed real Sign phase')
        count, ticks, delta = map(int, row.groups())
        require(count == len(draws) and ticks == draws[-1]['world_ticks'] and 2 <= delta <= ticks < 1 << 64 and
                ticks - delta <= draws[0]['world_ticks'], 'Phase baseline precedes observed draws and two actual World ticks returned')
        at += 1
        phases.append({'phase': name, 'draws': draws, 'world_ticks': ticks, 'world_tick_delta': delta})

    host('open')
    phase('loaded')
    start = at
    while at < len(rows) and rows[at].startswith(('WORLD_SAVE_TITLE_RECOVERY', 'WORLD_SAVE_TITLE_AUTOPAUSE',
                                                'WORLD_SAVE_TITLE_ESCAPE', 'WORLD_SAVE_TITLE_ENTRY',
                                                'WORLD_SAVE_TITLE mode=', 'WORLD_RELOAD_SNAPSHOT')):
        at += 1
    saved = save_protocol(rows[start:at], evidence=evidence, interval=interval)
    phase('reloaded')
    require(phases[1]['draws'][0]['world_ticks'] >= phases[0]['world_ticks'], 'Continuous actual frame tick ledger')
    exact('SIGN_RELOAD ordinary-save-reselection=1 distinct-world-player=1 current-text=1 rows=4')
    host('after-sign')
    require(hosts[0] == hosts[1], 'Same native host across actual save/reselection')
    exact('SIGN_STOP actual-frame-close=1 source-frame-retry=0')
    row = re.fullmatch(r'SIGN_FINISH profile=' + profile +
                      r' cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9}) source-field-repair=0',
                      rows[at]) if at < len(rows) else None
    require(row is not None and int(row[1]) <= 4294967295 and at + 1 == len(rows),
            'Unique final uint32 cleanup result and no trailing output')
    return {'checks': int(row[1]), 'phases': phases, 'window_ids': list(hosts[0]), 'save': saved,
            'text_digest': row_digest(LINES), 'lines': list(LINES)}


def saved_gate(decoder, directory, profile, actions):
    root = decoder.inflate(G.bounded(directory / 'level.dat', decoder.BOUND), 31)
    data = G.compound(root.get('Data'), 'Saved metadata Data')
    require(data.get('LevelName') == [8, 'Sign world'] and data.get('Time') == [4, actions['time']] and
            data.get('SizeOnDisk') == [4, actions['size_before']] and
            data.get('LastPlayed', [None])[0] == 4 and actions['first'] <= data['LastPlayed'][1] <= actions['last'],
            'Independent metadata agrees with actual signed64 save transport')
    player = G.compound(data.get('Player'), 'Saved actual Player')
    require(player.get('Dimension') == [3, 0] and player.get('Health') == [2, 5] and
            G.slot_rows(player.get('Inventory')) == [(0, 3, 1, 0)], 'Independent unchanged actual Player Stack/Health')
    reader = G.module('sign_saved_chunk_reader', ROOT / 'tests/client/test_furnace_gui.py')
    level, blocks, nibbles = reader.selected_chunk(decoder, directory)
    return {**sign_semantics(level, blocks, nibbles, profile), 'time': actions['time'],
            'scope': 'Independent selected saved Chunk Sign text/Block and Player; actual reload separately witnessed.'}


def controls():
    class Controls(unittest.TestCase):
        @staticmethod
        def host(stage):
            return [f'WORLD_SELECTION_MONITOR stage={stage} window={window} x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040'
                    for window in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']

        @staticmethod
        def save():
            return ['WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=2 resumed=1',
                    'WORLD_SAVE_TITLE_ESCAPE keyboard-before=2 keyboard-after=4 fresh-pause=1 distinct-recovered=1',
                    'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=84 time=5 size-before=100 size-after=200 entered=1 bytes=10 copied-before-frame=1',
                    'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=84 time=5 size-before=100 size-after=200 first=1000 last=1001 entry-frame=0 entry-bytes=10',
                    'WORLD_RELOAD_SNAPSHOT phase=sign-saved files=1 bytes=10 actual-file-copy=1']

        @classmethod
        def protocol(cls, profile):
            setting = parameters(profile)
            rows = ['16 achievements', '151 recipes'] + cls.host('open')
            for phase_index, phase in enumerate(('loaded', 'reloaded')):
                if phase_index:
                    rows += cls.save()
                for i in range(3):
                    rows.append(f'SIGN_DRAW index={phase_index * 3 + i} phase={phase} profile={profile} '
                                f'standing={int(setting["standing"])} metadata={setting["metadata"]} angle={setting["angle_bits"]} '
                                f'rows-measured=4 rows-drawn=4 units=31 null-rows=0 text-digest={row_digest(LINES)} '
                                'model-parts=2 compiled=0 faces=0 vertices=0 tiles=1 sign-returned=1 sign-retired=1 '
                                'row=3 width=0 x=0 y=10 text-scale=3c360b62 texture-bound=1 depth-restored=1 '
                                'color-restored=1 matrix-popped=1 actual-tile=1 occurrence=1 world-returned=1 '
                                f'hud-returned=1 timer-ticks=1 world-ticks={phase_index * 4 + i + 1}')
                rows.append(f'SIGN_PHASE phase={phase} profile={profile} draws=3 world-ticks={phase_index * 4 + 3} actual-world-tick-delta=2 current-text=1')
            rows += ['SIGN_RELOAD ordinary-save-reselection=1 distinct-world-player=1 current-text=1 rows=4'] + cls.host('after-sign') + [
                'SIGN_STOP actual-frame-close=1 source-frame-retry=0',
                f'SIGN_FINISH profile={profile} cleanup=1 windows-absent=1 checks=100 source-field-repair=0']
            return '\n'.join(rows) + '\n'

        def test_defined_utf16_digest(self):
            self.assertEqual(row_digest(LINES), '3e05f47480edd3fa')
            self.assertNotEqual(row_digest((None,)), row_digest(('',)))
            self.assertNotEqual(row_digest(('ab', 'c')), row_digest(('a', 'bc')))
            self.assertNotEqual(row_digest(('A\ud800',)), row_digest(('A\ufffd',)))

        def test_closed_ordered_native_protocol(self):
            monitor = {'selected': {'work': [-1920, 0, 1920, 1040]}}
            for profile in PROFILES:
                raw = self.protocol(profile)
                self.assertEqual(receipts(raw, '', monitor, profile, interval=(999, 1002))['checks'], 100)
                changes = (raw.rstrip('\n'), raw + 'unknown\n', raw + raw.splitlines()[-1] + '\n',
                           raw.replace('index=5 ', 'index=4 '), raw.replace('rows-drawn=4', 'rows-drawn=3'),
                           raw.replace('units=31', 'units=30'), raw.replace('text-digest=3e05f47480edd3fa', 'text-digest=0000000000000000'),
                           raw.replace('model-parts=2', 'model-parts=1'), raw.replace('tiles=1', 'tiles=2'),
                           raw.replace('occurrence=1', 'occurrence=2'), raw.replace('sign-retired=1', 'sign-retired=0'),
                           raw.replace('current-text=1', 'current-text=0'), raw.replace('timer-ticks=1', 'timer-ticks=11'),
                           raw.replace('actual-world-tick-delta=2', 'actual-world-tick-delta=1'),
                           raw.replace('depth-restored=1', 'depth-restored=0'), raw.replace('x=-1387', 'x=0'),
                           raw.replace('canvas-id=4', 'canvas-id=3'), raw.replace('size-after=200 first=', 'size-after=201 first='),
                           raw.replace('keyboard-after=4', 'keyboard-after=3'), raw.replace('source-field-repair=0', 'source-field-repair=1'))
                for changed in changes:
                    with self.subTest(profile=profile, changed=changed), self.assertRaises(RuntimeError):
                        receipts(changed, '', monitor, profile, interval=(999, 1002))
                with self.assertRaises(RuntimeError):
                    receipts(raw, 'hidden failure\n', monitor, profile)

        def test_phase_baseline_can_precede_first_observed_draw(self):
            monitor = {'selected': {'work': [-1920, 0, 1920, 1040]}}
            raw = self.protocol('standing-0')
            # sign_frames captures its baseline before the first completed
            # observer draw. Timer ticks may return while terrain is loading.
            earlier = raw.replace('actual-world-tick-delta=2', 'actual-world-tick-delta=3')
            self.assertEqual(receipts(earlier, '', monitor, 'standing-0')['checks'], 100)
            after_first = raw.replace('timer-ticks=1 world-ticks=1\n',
                                      'timer-ticks=1 world-ticks=0\n', 1)
            with self.assertRaises(RuntimeError):
                receipts(after_first, '', monitor, 'standing-0')

        def test_exact_saved_sign_semantics(self):
            for profile in PROFILES:
                setting = parameters(profile)
                level = {'TileEntities': [9, [10, [expected_tile()]]]}
                blocks, data = bytearray(32768), bytearray(16384)
                at = (3 << 11) | (3 << 7) | 64
                blocks[at], blocks[at-1] = setting['block'], 2
                nibble_set(data, at, setting['metadata'])
                if setting['support']:
                    x, y, z = setting['support']; blocks[(x << 11) | (z << 7) | y] = 1
                self.assertEqual(sign_semantics(level, blocks, data, profile)['lines'], list(LINES))
                for mutate in (lambda v: v['TileEntities'][1][1][0]['Text1'].__setitem__(1, 'changed'),
                               lambda v: v['TileEntities'][1][1][0]['x'].__setitem__(0, 2),
                               lambda v: v['TileEntities'][1][1].append(expected_tile()),
                               lambda v: v['TileEntities'][1][1][0].update(EditRow=[3, -1])):
                    altered = copy.deepcopy(level); mutate(altered)
                    with self.assertRaises(RuntimeError): sign_semantics(altered, blocks, data, profile)

        def test_authored_inputs_and_saved_camera(self):
            m = modules()
            for profile in PROFILES:
                with tempfile.TemporaryDirectory() as temporary:
                    directory = Path(temporary) / 'alpha/region'; directory.mkdir(parents=True)
                    result = regions(m, directory, profile)
                    self.assertEqual((result['chunks'], result['tile_entities']), (441, 1))
                    wire = metadata(m['world'], m['torch'], 'Sign world', profile)
                    player = m['parser'].Nbt(wire).root()['Data'][1]['Player'][1]
                    self.assertEqual(G.slot_rows(player['Inventory']), [(0, 3, 1, 0)])
                    self.assertEqual(player['Health'], [2, 5])
                    x, z, yaw = parameters(profile)['camera']
                    bits = lambda value: int.from_bytes(struct.pack('>d', value), 'big')
                    self.assertEqual(player['Pos'], [9, [6, [bits(x), bits(64. + struct.unpack('>f', struct.pack('>f', 1.62))[0]), bits(z)]]])
                    self.assertEqual(player['Rotation'], [9, [5, [int.from_bytes(struct.pack('>f', value), 'big') for value in (yaw, 20.)]]])

        def test_retained_snapshot_bytes(self):
            with tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                for name in ('entries/frame-0', 'snapshots/sign-saved'):
                    (directory / name).mkdir(parents=True)
                    (directory / name / 'file').write_bytes(b'0123456789')
                self.assertEqual(save_protocol(self.save(), evidence=directory)['snapshot'], [1, 10])
                (directory / 'entries/frame-0/file').write_bytes(b'012345678')
                with self.assertRaises(RuntimeError): save_protocol(self.save(), evidence=directory)
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    require(result.wasSuccessful(), 'Offline Sign reader controls; no hosted or frame evidence')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--profile', choices=PROFILES, default='standing-0')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.controls:
        controls()
        return
    require(os.name == 'nt' and all((args.executable, args.assets, args.artifacts)),
            'Actual Windows SDK executable/assets/evidence required')
    m = modules()
    world, torch, nbt = (m[key] for key in ('world', 'torch', 'parser'))
    monitor = G.module('sign_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='sign-sdk-', dir=args.artifacts.resolve()))
    before_inputs, before_assets = G.fingerprints(G.source_inputs(executable)), world.snapshot(assets)
    require(before_inputs and all(type(value) is str for value in before_inputs.values()), 'Readable selected inputs before process')
    report = {'status': 'FAIL', 'profile': args.profile, 'monitor': monitor, 'inputs_before': before_inputs,
              'scope': 'One prelaunch saved Sign, current text, genuine Timer/World/HUD frames, actual PO/RF/Font and save/reselection. No live Source/clock/RNG repair, editor, placement/removal, original pixels, FPS or sustained-play claim.'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'; game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            for folder, name in (('older', 'Old world'), ('bravo', ''), ('alpha', 'Sign world')):
                directory = saves / folder; directory.mkdir(parents=True)
                wire = metadata(world, torch, name, args.profile) if folder == 'alpha' else world.metadata(name, 1303000000000, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve.bin').write_bytes(b'independent Sign fixture bytes\0\xff')
            region = saves / 'alpha/region'; region.mkdir()
            report['saved_input'] = regions(m, region, args.profile)
            before = world.save_title_files(saves, evidence / 'before')
            for name in ('entries', 'snapshots'): (evidence / name).mkdir()
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds, MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds,
                               MC_WORLD_RELOAD_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'), MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first = time.time_ns() // 1000000
            child = world.retained_native_run([str(executable), str(assets), str(evidence), args.profile], environment,
                                               evidence, saves, before, 'sign-' + args.profile, first, 600)
            last = time.time_ns() // 1000000
            print(child.stdout, end=''); print(child.stderr, end='', file=sys.stderr)
            report['native_exit'] = child.returncode
            require(child.returncode == 0, 'Actual native SDK profile and checked cleanup passed')
            report['native'] = receipts(child.stdout, child.stderr, monitor, args.profile, evidence=evidence, interval=(first, last))
            require({p.name for p in evidence.glob('*.png')} == set(IMAGES), 'Exact four fresh process image inventory')
            report['images'] = {name: G.png_gate(evidence / name, first) for name in IMAGES}
            report['saved'] = saved_gate(nbt, evidence / 'snapshots/sign-saved/alpha', args.profile, report['native']['save'])
            require(report['saved']['lines'] == report['native']['lines'] == report['saved_input']['lines'] and
                    report['saved']['text_digest'] == report['native']['text_digest'] == report['saved_input']['text_digest'],
                    'Prelaunch, actual Font arguments and independently decoded saved text agree')
            after = world.save_title_files(saves, evidence / 'after-verified')
            require(all(after.get(n) == value for n, value in before.items() if not n.startswith('alpha/') or n == 'alpha/preserve.bin') and
                    all(n.startswith('alpha/') for n in set(after) - set(before)), 'All unrelated saved bytes and names preserved')
            selected, final = G.snapshot_files(evidence / 'snapshots/sign-saved'), G.snapshot_files(saves)
            token = 'alpha/session.lock'
            require(set(selected) == set(final) and all(final[n] == value for n, value in selected.items() if n != token) and
                    final[token]['bytes'] == 8, 'Ordinary reselection preserves saved bytes except actual eight-byte SessionLock')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = G.fingerprints(G.source_inputs(executable))
        report['assets_unchanged'] = world.snapshot(assets) == before_assets
        if report['inputs_after'] != before_inputs or not report['assets_unchanged']:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Selected source/helper/product/DLL or imported assets changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('Sign SDK artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Actual Sign SDK continuity')


if __name__ == '__main__':
    main()
