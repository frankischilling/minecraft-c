"""Saved stable Lever/Piston through real SDK input, Timer frames and save/reselection."""
import argparse
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
PROFILES = ('pushed-stone', 'empty-front')
IMAGES = ('piston-before.png', 'piston-extended.png', 'piston-retracted.png',
          'piston-title.png', 'piston-reloaded.png', 'held-dirt.png')
U = r'(0|[1-9][0-9]{0,19})'
S = r'(0|-?[1-9][0-9]{0,18})'


def modules():
    lever = G.module('piston_lever_inputs', ROOT / 'tests/tools/client/test_lever_loop.py')
    return lever, lever.modules()


def metadata(world, torch, name):
    # The inherited genuine ready-frame observer requires held Dirt3/count1.
    # Lever activation consumes the right click before the held Item is used.
    wire = torch.metadata(world, name, 1304000000000, 3)
    count = world.tag(1, 'Count', b'\2')
    require(wire.count(count) == 1, 'One declared prelaunch held Stack Count')
    return wire.replace(count, world.tag(1, 'Count', b'\1'))


def regions(lever, m, directory, profile):
    """Only saved, settled Blocks are authored, before bootstrap exists."""
    require(profile in PROFILES, 'Declared stable Piston profile')
    base = lever.authored_regions(m, directory)
    world, parser, torch = (m[key] for key in ('world', 'parser', 'torch'))
    old, images = torch.region_records(parser, directory)
    name, location, stamp, allocation, payload, wire, level = old[(0, 0)]
    arrays = {key: bytearray(level[key][1]) for key in ('Blocks', 'Data', 'SkyLight', 'HeightMap')}
    cells = ((3, 65, 2, 69, 4), (4, 65, 2, 33, 3),
             (4, 65, 3, 1 if profile == 'pushed-stone' else 0, 0), (4, 65, 4, 0, 0))
    index = lambda x, y, z: (x << 11) | (z << 7) | y
    for x, y, z, block, meta in cells:
        at = index(x, y, z)
        arrays['Blocks'][at] = block
        lever.nibble_set(arrays['Data'], at, meta)
    for x, z in {(x, z) for x, _, z, _, _ in cells}:
        # These selected columns contain only inherited Stone/Dirt/Grass,
        # Air, Lever and Piston. Both actual Lever and Piston registrations
        # have zero light opacity; their saved Sky/height input stays open.
        highest = max(y for y in range(128) if arrays['Blocks'][index(x, y, z)] not in (0, 33, 69))
        arrays['HeightMap'][z * 16 + x] = highest + 1
        for y in range(128):
            lever.nibble_set(arrays['SkyLight'], index(x, y, z), 15 if y > highest else 0)
    for key, value in arrays.items():
        before = world.tag(7, key, struct.pack('>i', len(level[key][1])) + level[key][1])
        require(wire.count(before) == 1, 'One declared selected-Chunk array field')
        wire = wire.replace(before, world.tag(7, key, struct.pack('>i', len(value)) + value))
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    sector, count = location >> 8, location & 255
    require(len(record) <= count * 4096, 'Selected saved Chunk retains finite allocation')
    images[name][sector * 4096:(sector + count) * 4096] = record + bytes(count * 4096 - len(record))
    (directory / name).write_bytes(images[name])
    actual, _ = torch.region_records(parser, directory)
    require(set(actual) == set(old) and len(actual) == 441 and
            all(actual[c][:-1] == old[c][:-1] for c in old if c != (0, 0)),
            'All 440 other compressed records, slots, stamps and allocation bytes preserved')
    current = actual[(0, 0)][-1]
    require(actual[(0, 0)][1:3] == (location, stamp), 'Selected Chunk slot and timestamp preserved')
    require(all(current[key] == [7, bytes(value)] for key, value in arrays.items()) and
            all(current[key] == level[key] for key in set(level) - set(arrays)),
            'Independent exact selected-Chunk arrays and unchanged other NBT fields')
    require(all(36 not in row[-1]['Blocks'][1] and row[-1]['TileEntities'] == [9, [10, []]]
                for row in actual.values()), 'No authored MovingPiston Block, Tile or progress anywhere')
    return {'base': base, 'chunks': 441, 'changed_chunks': [[0, 0]],
            'other_compressed_records_preserved': 440, 'moving_blocks': 0, 'tile_entities': 0,
            'cells': [list(cell) for cell in cells], 'selected_wire_sha256': hashlib.sha256(wire).hexdigest()}


def f32(word):
    value = struct.unpack('>f', bytes.fromhex(word))[0]
    require(math.isfinite(value), 'Finite actual Piston progress')
    return value


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
    snapshot = nums(r'WORLD_RELOAD_SNAPSHOT phase=piston-saved files=' + U + r' bytes=' + U +
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
        inventory = G.snapshot_files(evidence / 'snapshots/piston-saved')
        require((len(inventory), sum(v['bytes'] for v in inventory.values())) == snapshot,
                'Saved snapshot file count and bytes independently agree')
    return {'frame': frame, 'time': world_time, 'size_before': before,
            'first': saved[4], 'last': saved[5], 'snapshot': list(snapshot)}


def receipts(stdout, stderr, monitor, profile, *, evidence=None, interval=None):
    require(profile in PROFILES and type(stdout) is str and stdout.endswith('\n') and
            '\r' not in stdout and stderr == '', 'Complete clean positive SDK transport')
    rows = stdout.splitlines()
    require(rows[:2] == ['16 achievements', '151 recipes'], 'Exact single Registry startup banner pair')
    target = monitor['selected']['work']
    require(len(target) == 4 and all(type(v) is int for v in target) and target[2] > 0 and target[3] > 0,
            'Physical monitor rectangle')
    at, index, draws, hosts = 2, 0, [], []

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
                           r'canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1', rows[at]) if at < len(rows) else None
        require(row is not None and int(row[1]) != int(row[2]) and
                max(map(int, row.groups())) <= 4294967295, 'Actual distinct bound Frame and Canvas IDs')
        hosts.append(tuple(map(int, row.groups())))
        at += 1

    host('open')
    times = []
    for phase, extending in (('extend', 1), ('retract', 0)):
        exact(f'PISTON_TAP phase={phase} sdk-down-up=1')
        inputs, phase_draws = 0, []
        while at < len(rows) and not rows[at].startswith('PISTON_SETTLED'):
            if rows[at].startswith('PISTON_INPUT'):
                exact(f'PISTON_INPUT phase={phase} sdk-right-click=1 block-use=1 placed=0 count-store=0 mouse-events=2')
                inputs += 1
                require(inputs == 1, 'Exactly one real SDK tap admitted per phase')
                continue
            draw = re.fullmatch(r'PISTON_DRAW index=([0-9]+) phase=' + phase +
                                r' carried=([0-9]+) metadata=([0-9]+) facing=3 extending=([01]) render-head=([01]) '
                                r'progress=([0-9a-f]{8}) current=([0-9a-f]{8}) previous=([0-9a-f]{8}) '
                                r'vertices=([1-9][0-9]*) tiles=([1-9][0-9]*) head=([01]) base=([01]) block=([01]) '
                                r'world-returned=1 hud-returned=1 ll-retired=1 translation-zero=1 actual-tile=1', rows[at])
            require(draw is not None, 'Complete ordered genuine HY transient drawing receipt')
            require(int(draw[1]) == index < 512 and int(draw[4]) == extending and int(draw[5]) == 1 - extending and
                    0 <= int(draw[3]) <= 15 and int(draw[9]) <= 1048576 and int(draw[10]) <= 2,
                    'Contiguous bounded actual transient and branch values')
            progress, current, previous = (f32(draw[i]) for i in (6, 7, 8))
            require(0 <= previous <= current <= 1 and current in (0, .5, 1) and previous in (0, .5, 1) and
                    0 <= progress < 1, 'Actual constructor/tick progress and interpolated non-settled draw')
            carried, metadata = int(draw[2]), int(draw[3])
            if extending:
                branch = (1, 0, 0) if carried == 34 and progress < .5 else (0, 0, 1)
                require((carried, metadata) in ((34, 3), (1, 0)) and
                        (profile == 'pushed-stone' or carried == 34) and
                        (int(draw[11]), int(draw[12]), int(draw[13])) == branch,
                        'Actual extending Head or carried Stone geometry branch')
            else:
                require((carried, metadata) == (33, 3) and
                        (int(draw[11]), int(draw[12]), int(draw[13])) == (1, 1, 0),
                        'Actual retraction renders both Head and Base')
            phase_draws.append({'index': index, 'carried': carried, 'metadata': metadata,
                                'progress_bits': draw[6], 'current_bits': draw[7], 'previous_bits': draw[8],
                                'vertices': int(draw[9]), 'tiles': int(draw[10])})
            index += 1
            at += 1
        require(inputs == 1 and phase_draws and at < len(rows), 'Both admitted input and genuine transient drawing are required')
        settled = re.fullmatch(r'PISTON_SETTLED phase=' + phase + r' base=33 metadata=' + ('11' if extending else '3') +
                               r' head=' + ('34' if extending else '0') + ' pushed-stone=' + str(int(profile == 'pushed-stone')) +
                               r' moving-tiles=0 transient-draws=([1-9][0-9]*) actual-world-ticks=([1-9][0-9]*)', rows[at])
        require(settled is not None and int(settled[1]) == len(phase_draws) and
                0 < int(settled[2]) < 1 << 64, 'Actual stable Block state, empty live Tile List and draw count')
        times.append(int(settled[2]))
        draws.append({'phase': phase, 'draws': phase_draws})
        at += 1
    require(times[1] >= times[0], 'Monotone actual World tick returns')
    start = at
    while at < len(rows) and rows[at].startswith(('WORLD_SAVE_TITLE_RECOVERY', 'WORLD_SAVE_TITLE_AUTOPAUSE',
                                                'WORLD_SAVE_TITLE_ESCAPE', 'WORLD_SAVE_TITLE_ENTRY',
                                                'WORLD_SAVE_TITLE mode=', 'WORLD_RELOAD_SNAPSHOT')):
        at += 1
    saved = save_protocol(rows[start:at], evidence=evidence, interval=interval)
    exact('PISTON_RELOAD ordinary-save-reselection=1 distinct-world-player=1 stable-piston=1 lever-off=1 actual-inventory=1')
    host('after-piston')
    require(hosts[0] == hosts[1], 'Same native host across actual save/reselection')
    exact('PISTON_STOP actual-frame-close=1 source-frame-retry=0')
    finish = re.fullmatch(r'PISTON_FINISH profile=' + profile +
                          r' cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9}) source-field-repair=0', rows[at]) if at < len(rows) else None
    require(finish is not None and int(finish[1]) <= 4294967295 and at + 1 == len(rows),
            'Unique final uint32 cleanup result, no trailing or unknown output')
    return {'checks': int(finish[1]), 'draws': draws, 'world_ticks': times,
            'window_ids': list(hosts[0]), 'save': saved}


def saved_gate(parser, directory, profile, actions):
    root = parser.inflate(G.bounded(directory / 'level.dat', parser.BOUND), 31)
    data = G.compound(root.get('Data'), 'Saved metadata Data')
    require(data.get('LevelName') == [8, 'Piston world'] and data.get('Time') == [4, actions['time']] and
            data.get('SizeOnDisk') == [4, actions['size_before']] and
            data.get('LastPlayed', [None])[0] == 4 and actions['first'] <= data['LastPlayed'][1] <= actions['last'],
            'Independent metadata agrees with signed64 actual save transport')
    player = G.compound(data.get('Player'), 'Saved actual Player')
    require(player.get('Dimension') == [3, 0] and player.get('Health') == [2, 5] and
            G.slot_rows(player.get('Inventory')) == [(0, 3, 1, 0)], 'Independent actual unchanged Player Stack/Health')
    chunk_reader = G.module('piston_saved_chunk', ROOT / 'tests/client/test_furnace_gui.py')
    level, blocks, metadata = chunk_reader.selected_chunk(parser, directory)
    cells = ((3, 65, 2, 69, 4), (4, 65, 2, 33, 3), (4, 65, 3, 0, 0),
             (4, 65, 4, 1 if profile == 'pushed-stone' else 0, 0))
    for x, y, z, block, meta in cells:
        at = (x << 11) | (z << 7) | y
        require(blocks[at] == block and (metadata[at // 2] >> ((at & 1) * 4)) & 15 == meta,
                'Independent saved stable Lever/Piston/Head/carried Block cell')
    require(not G.typed(level.get('TileEntities'), 9, 'Saved TileEntity List')[1],
            'Settled selected saved Chunk has no moving Tile')
    return {'cells': [list(cell) for cell in cells], 'player_inventory': [[0, 3, 1, 0]],
            'time': actions['time'], 'scope': 'Independent selected saved Chunk and Player; actual reload witnessed separately.'}


def controls():
    class Controls(unittest.TestCase):
        @staticmethod
        def save():
            return ['WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=2 resumed=1',
                    'WORLD_SAVE_TITLE_ESCAPE keyboard-before=2 keyboard-after=4 fresh-pause=1 distinct-recovered=1',
                    'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=84 time=5 size-before=100 size-after=200 entered=1 bytes=10 copied-before-frame=1',
                    'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=84 time=5 size-before=100 size-after=200 first=1000 last=1001 entry-frame=0 entry-bytes=10',
                    'WORLD_RELOAD_SNAPSHOT phase=piston-saved files=1 bytes=10 actual-file-copy=1']

        @staticmethod
        def host(stage):
            return [f'WORLD_SELECTION_MONITOR stage={stage} window={window} x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040'
                    for window in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']

        @classmethod
        def protocol(cls, profile):
            rows = ['16 achievements', '151 recipes'] + cls.host('open')
            for i, phase in enumerate(('extend', 'retract')):
                detail = ('carried=34 metadata=3 facing=3 extending=1 render-head=0' if i == 0 else
                          'carried=33 metadata=3 facing=3 extending=0 render-head=1')
                rows += [f'PISTON_TAP phase={phase} sdk-down-up=1',
                         f'PISTON_DRAW index={i} phase={phase} {detail} progress=3e800000 current=3f000000 previous=00000000 vertices=120 tiles=1 head=1 base={i} block=0 world-returned=1 hud-returned=1 ll-retired=1 translation-zero=1 actual-tile=1',
                         f'PISTON_INPUT phase={phase} sdk-right-click=1 block-use=1 placed=0 count-store=0 mouse-events=2',
                         f'PISTON_SETTLED phase={phase} base=33 metadata={11 if i == 0 else 3} head={34 if i == 0 else 0} pushed-stone={int(profile == "pushed-stone")} moving-tiles=0 transient-draws=1 actual-world-ticks={i+4}']
            rows += cls.save() + ['PISTON_RELOAD ordinary-save-reselection=1 distinct-world-player=1 stable-piston=1 lever-off=1 actual-inventory=1'] + cls.host('after-piston') + [
                'PISTON_STOP actual-frame-close=1 source-frame-retry=0',
                f'PISTON_FINISH profile={profile} cleanup=1 windows-absent=1 checks=100 source-field-repair=0']
            return '\n'.join(rows) + '\n'

        def test_closed_ordered_native_protocol(self):
            monitor = {'selected': {'work': [-1920, 0, 1920, 1040]}}
            for profile in PROFILES:
                raw = self.protocol(profile)
                self.assertEqual(receipts(raw, '', monitor, profile, interval=(999, 1002))['checks'], 100)
                changes = (raw + raw.splitlines()[-1] + '\n', raw.rstrip('\n'), raw + 'unknown\n',
                           raw.replace('PISTON_TAP phase=extend sdk-down-up=1\n', ''),
                           raw.replace('index=1 phase=retract', 'index=0 phase=retract'),
                           raw.replace('progress=3e800000', 'progress=7fc00000'),
                           raw.replace('current=3f000000', 'current=3e800000'),
                           raw.replace('vertices=120', 'vertices=0'), raw.replace('head=1 base=1', 'head=1 base=0'),
                           raw.replace('mouse-events=2', 'mouse-events=1'), raw.replace('translation-zero=1', 'translation-zero=0'),
                           raw.replace('x=-1387', 'x=0'), raw.replace('canvas-id=4', 'canvas-id=3'),
                           raw.replace('size-after=200 first=', 'size-after=201 first='),
                           raw.replace('keyboard-after=4', 'keyboard-after=3'),
                           raw.replace('source-field-repair=0', 'source-field-repair=1'))
                for changed in changes:
                    with self.subTest(profile=profile, changed=changed), self.assertRaises(RuntimeError):
                        receipts(changed, '', monitor, profile, interval=(999, 1002))
                rows = raw.splitlines()
                i = next(i for i, row in enumerate(rows) if row.startswith('PISTON_TAP phase=retract'))
                changed = rows[:i] + [rows[i+1], rows[i]] + rows[i+2:]
                with self.assertRaises(RuntimeError):
                    receipts('\n'.join(changed) + '\n', '', monitor, profile)
                with self.assertRaises(RuntimeError):
                    receipts(raw, 'hidden failure\n', monitor, profile)

        def test_retained_snapshot_bytes(self):
            with tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                for name in ('entries/frame-0', 'snapshots/piston-saved'):
                    (directory / name).mkdir(parents=True)
                    (directory / name / 'file').write_bytes(b'0123456789')
                self.assertEqual(save_protocol(self.save(), evidence=directory)['snapshot'], [1, 10])
                (directory / 'entries/frame-0/file').write_bytes(b'012345678')
                with self.assertRaises(RuntimeError):
                    save_protocol(self.save(), evidence=directory)
                (directory / 'entries/frame-0/file').write_bytes(b'0123456789')
                (directory / 'snapshots/piston-saved/extra').write_bytes(b'!')
                with self.assertRaises(RuntimeError):
                    save_protocol(self.save(), evidence=directory)

        def test_authored_settled_input_only(self):
            lever, m = modules()
            for profile in PROFILES:
                with tempfile.TemporaryDirectory() as temporary:
                    directory = Path(temporary)
                    result = regions(lever, m, directory, profile)
                    self.assertEqual(result['chunks'], 441)
                    self.assertEqual(result['moving_blocks'], 0)
                    wire = metadata(m['world'], m['torch'], 'Piston world')
                    player = m['parser'].Nbt(wire).root()['Data'][1]['Player'][1]
                    self.assertEqual(G.slot_rows(player['Inventory']), [(0, 3, 1, 0)])
                    self.assertEqual(player['Health'], [2, 5])
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    require(result.wasSuccessful(), 'Piston SDK reader controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--profile', choices=PROFILES, default='pushed-stone')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.controls:
        controls()
        return
    require(os.name == 'nt' and all((args.executable, args.assets, args.artifacts)), 'Actual Windows SDK executable/assets/evidence required')
    lever, m = modules()
    world, torch, nbt = (m[key] for key in ('world', 'torch', 'parser'))
    monitor = G.module('piston_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='piston-sdk-', dir=args.artifacts.resolve()))
    before_inputs, before_assets = G.fingerprints(G.source_inputs(executable)), world.snapshot(assets)
    require(before_inputs and all(type(value) is str for value in before_inputs.values()), 'Readable selected inputs before process')
    report = {'status': 'FAIL', 'profile': args.profile, 'monitor': monitor, 'inputs_before': before_inputs,
              'scope': 'Authored settled Lever/Piston Blocks, genuine SDK input and actual Timer/World/HUD frames. No authored moving Tile/progress, live Source/clock/RNG repair, original pixel equality, FPS or sustained-play claim.'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            for folder, name in (('older', 'Old world'), ('bravo', ''), ('alpha', 'Piston world')):
                directory = saves / folder
                directory.mkdir(parents=True)
                wire = metadata(world, torch, name) if folder == 'alpha' else world.metadata(name, 1303000000000, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve.bin').write_bytes(b'independent Piston fixture bytes\0\xff')
            region = saves / 'alpha/region'
            region.mkdir()
            report['saved_input'] = regions(lever, m, region, args.profile)
            before = world.save_title_files(saves, evidence / 'before')
            for name in ('entries', 'snapshots'):
                (evidence / name).mkdir()
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds, MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds,
                               MC_WORLD_RELOAD_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'), MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first = time.time_ns() // 1000000
            child = world.retained_native_run([str(executable), str(assets), str(evidence), args.profile], environment,
                                               evidence, saves, before, 'piston-' + args.profile, first, 600)
            last = time.time_ns() // 1000000
            print(child.stdout, end='')
            print(child.stderr, end='', file=sys.stderr)
            report['native_exit'] = child.returncode
            # Failed Source prefixes remain raw retained evidence. They do not
            # satisfy the positive profile or acquire a synthetic success tag.
            report['prefix_lines'] = [row for row in child.stdout.splitlines() if row.startswith('PISTON_PREFIX')]
            require(child.returncode == 0, 'Actual native SDK profile and checked cleanup passed')
            report['native'] = receipts(child.stdout, child.stderr, monitor, args.profile, evidence=evidence, interval=(first, last))
            require({p.name for p in evidence.glob('*.png')} == set(IMAGES), 'Exact six fresh process image inventory')
            report['images'] = {name: G.png_gate(evidence / name, first) for name in IMAGES}
            report['saved'] = saved_gate(nbt, evidence / 'snapshots/piston-saved/alpha', args.profile, report['native']['save'])
            after = world.save_title_files(saves, evidence / 'after-verified')
            require(all(after.get(n) == value for n, value in before.items() if not n.startswith('alpha/') or n == 'alpha/preserve.bin') and
                    all(n.startswith('alpha/') for n in set(after) - set(before)), 'All unrelated saved bytes and names preserved')
            selected, final = G.snapshot_files(evidence / 'snapshots/piston-saved'), G.snapshot_files(saves)
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
        print('Piston SDK artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Actual Piston SDK continuity')


if __name__ == '__main__':
    main()
