"""Real Timer/World.g Spawner activation, publication, save/reselection or failed App cleanup."""
import argparse
from collections import Counter
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
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/tools/client'))
import test_chest_loop as G

require = G.require
PROFILES = ('countdown', 'success', 'failure')
DELAY = {'countdown': 32767, 'success': 0, 'failure': 20}
PIG_LIMIT = 128
FAILURE_STDERR = ('Client menu: authored SDK Spawner post-publication smoke failure '
                  '(net/minecraft/client/Minecraft.k:1216)\n')


def reload_receipts(rows):
    """Strict passive Chunk witnesses followed by paired actual Entity reads."""
    chunks, pigs = [], []
    while rows and rows[0].startswith('SPAWNER_TICK_CHUNK '):
        row = re.fullmatch(r'SPAWNER_TICK_CHUNK index=([0-9]+) x=(-?[0-9]+) z=(-?[0-9]+) '
                           r'loaded=([01]) nonloading=1', rows.pop(0))
        require(row is not None, 'Complete nonloading actual-provider presence witness')
        index, x, z, loaded = map(int, row.groups())
        require(index == len(chunks) < PIG_LIMIT and -(1 << 27) <= x < 1 << 27 and
                -(1 << 27) <= z < 1 << 27 and [x, z] not in [r['chunk'] for r in chunks],
                'Unique contiguous bounded pre-leave Pig Chunk witnesses')
        chunks.append({'chunk': [x, z], 'loaded': bool(loaded)})
    require(chunks, 'At least one actual pre-leave Pig Chunk classified without loading')
    pattern = (r'SPAWNER_TICK_PIG phase=(runtime|input) index=([0-9]+) chunk-x=(-?[0-9]+) chunk-z=(-?[0-9]+) '
               r'pos=([0-9a-f]{16},[0-9a-f]{16},[0-9a-f]{16}) '
               r'motion=([0-9a-f]{16},[0-9a-f]{16},[0-9a-f]{16}) '
               r'rotation=([0-9a-f]{8},[0-9a-f]{8}) '
               r'health=(-?[0-9]+) hurt=(-?[0-9]+) death=(-?[0-9]+) attack=(-?[0-9]+) '
               r'fire=(-?[0-9]+) air=(-?[0-9]+) grounded=([01]) saddle=([01])')
    while rows:
        pair = {}
        for phase in ('runtime', 'input'):
            require(rows, 'Complete paired runtime and immutable-import Pig readbacks')
            row = re.fullmatch(pattern, rows.pop(0))
            require(row is not None and row[1] == phase and int(row[2]) == len(pigs) < PIG_LIMIT,
                    'Ordered contiguous genuine Pig readback phases and indices')
            x, z = int(row[3]), int(row[4])
            vectors = [[int(word, 16) for word in row[i].split(',')] for i in (5, 6, 7)]
            for words, width, fmt in ((vectors[0], 'Q', 'd'), (vectors[1], 'Q', 'd'), (vectors[2], 'I', 'f')):
                require(all(math.isfinite(struct.unpack('>' + fmt, struct.pack('>' + width, word))[0])
                            for word in words), 'Finite actual current/imported Pig floating fields')
            scalars = list(map(int, row.group(8, 9, 10, 11, 12, 13)))
            require(-(1 << 27) <= x < 1 << 27 and -(1 << 27) <= z < 1 << 27 and
                    all(-(1 << 31) <= value < 1 << 31 for value in scalars) and scalars[0] > 0,
                    'Bounded real Chunk membership and live Pig scalar readback')
            pair[phase] = dict(zip(('pos', 'motion', 'rotation', 'health', 'hurt', 'death', 'attack',
                                    'fire', 'air', 'grounded', 'saddle'),
                                   vectors + scalars + [int(row[14]), int(row[15])]))
            pair[phase]['chunk'] = [x, z]
        require(pair['runtime']['chunk'] == pair['input']['chunk'], 'Same actual Entity membership for paired reads')
        pigs.append(pair)
    require(pigs, 'Actual nonempty World.b Pig readback')
    return {'chunks': chunks, 'pigs': pigs}


def pig_signature(row):
    return tuple(tuple(row[key]) if isinstance(row[key], list) else row[key] for key in
                 ('pos', 'motion', 'rotation', 'health', 'hurt', 'death', 'attack', 'fire', 'air', 'grounded', 'saddle'))


def saved_pig_fields(record):
    entity = record['entity']
    vectors = [entity[key][1][1] for key in ('Pos', 'Motion', 'Rotation')]
    scalars = []
    for key in ('Health', 'HurtTime', 'DeathTime', 'AttackTime', 'Fire', 'Air'):
        value = entity.get(key)
        require(isinstance(value, list) and len(value) == 2 and value[0] == 2 and
                type(value[1]) is int and -32768 <= value[1] <= 32767, 'Typed saved Pig ' + key)
        scalars.append(value[1])
    for key in ('OnGround', 'Saddle'):
        require(entity.get(key) in ([1, 0], [1, 1]), 'Canonical saved Pig Boolean ' + key)
    return dict(zip(('pos', 'motion', 'rotation', 'health', 'hurt', 'death', 'attack', 'fire', 'air', 'grounded', 'saddle'),
                    vectors + scalars + [entity['OnGround'][1], entity['Saddle'][1]]))


def match_reload_pigs(saved, native):
    """Disk completeness and loaded-subset persistence are separate facts."""
    require(saved and native['reloaded_pigs'] == len(native['readback']['pigs']), 'Bounded complete runtime readback count')
    classified = {tuple(row['chunk']): row['loaded'] for row in native['readback']['chunks']}
    require(set(classified) == {tuple(row['chunk']) for row in saved},
            'Every actual saved-Pig Chunk, and only those Chunks, has a nonloading reload presence witness')
    loaded = [row for row in saved if classified[tuple(row['chunk'])]]
    unloaded = [row for row in saved if not classified[tuple(row['chunk'])]]
    require(loaded and len(loaded) == native['reloaded_pigs'], 'All Pigs from actually loaded saved Chunks accounted for')
    expected = Counter(pig_signature(saved_pig_fields(row)) for row in loaded)
    imported = Counter(pig_signature(row['input']) for row in native['readback']['pigs'])
    require(expected == imported, 'Exact saved/imported Pig fields match one-for-one, including raw pose/motion/rotation bits')
    for row in native['readback']['pigs']:
        require(row['runtime']['health'] == row['input']['health'] and
                row['runtime']['saddle'] == row['input']['saddle'], 'Actual current Pig Health and Saddle agree with imported saved fields')
    return {'disk_pigs': saved, 'loaded_chunk_pigs': loaded, 'unloaded_chunk_pigs': unloaded,
            'runtime_readback': native['readback'],
            'scope': 'All disk Pig records retained; exact imported fields checked only for witnessed-loaded saved Chunks. Current physics fields are separately observed after ordinary ticks.'}


def regions(world, directory, profile):
    """Pre-bootstrap wire inputs; Grass exists only near the authored Spawner."""
    source = G.module('spawner_tick_region_base', ROOT / 'tests/client/test_spawner_frame.py')
    coordinates = [None, None]

    class Inputs:
        string = staticmethod(world.string)

        @staticmethod
        def tag(kind, name, payload):
            if name in ('xPos', 'zPos'):
                coordinates[name == 'zPos'], = struct.unpack('>i', payload)
            if name == 'Blocks':
                raw = bytearray(payload)
                for x in range(16):
                    for z in range(16):
                        wx, wz = coordinates[0] * 16 + x, coordinates[1] * 16 + z
                        if -5 <= wx <= 5 and -3 <= wz <= 7:
                            raw[4 + (x << 11) + (z << 7) + 63] = 2
                payload = bytes(raw)
            elif name == 'Delay':
                payload = struct.pack('>h', DELAY[profile])
            return world.tag(kind, name, payload)

    source.regions(Inputs, directory)


def receipts(stdout, stderr, monitor, profile):
    expected_stderr = FAILURE_STDERR if profile == 'failure' else ''
    require(type(stdout) is str and stderr == expected_stderr,
            'Exact expected failure diagnostic or clean positive-profile stderr transport')
    lines = stdout.splitlines()
    final = [line for line in lines if line.startswith('SPAWNER_TICK_FINISH')]
    require(len(final) == 1, 'Unique SDK cleanup marker')
    finish = re.fullmatch(r'SPAWNER_TICK_FINISH profile=' + profile +
                         r' cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9}) source-field-repair=0', final[0])
    require(finish is not None and int(finish[1]) <= 4294967295 and lines[-1] == final[0],
            'Complete final positive uint32 native cleanup result')
    target = monitor['selected']['work']
    require(len(target) == 4 and all(type(v) is int for v in target) and target[2] > 0 and target[3] > 0,
            'Physical monitor work rectangle')
    geometry, hosts = [], []
    positions = {}
    for position, line in enumerate(lines):
        if line.startswith('WORLD_SELECTION_MONITOR'):
            row = re.fullmatch(r'WORLD_SELECTION_MONITOR stage=(open|after-spawner-tick) window=(frame|canvas) '
                               r'x=(-?[0-9]+) y=(-?[0-9]+) width=854 height=480 target=(\S+)', line)
            require(row is not None and row[5] == ','.join(map(str, target)), 'Exact monitor witness')
            x, y = map(int, row.group(3, 4))
            require(target[0] <= x and target[1] <= y and x + 854 <= target[0] + target[2] and
                    y + 480 <= target[1] + target[3], 'Both source windows fit physical monitor2')
            geometry.append((row[1], row[2]))
            positions[(row[1], row[2])] = position
        elif line.startswith('WORLD_SAVE_TITLE_HOST'):
            row = re.fullmatch(r'WORLD_SAVE_TITLE_HOST stage=(open|after-spawner-tick) '
                               r'frame-id=([1-9][0-9]{0,9}) canvas-id=([1-9][0-9]{0,9}) '
                               r'canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1', line)
            require(row is not None and int(row[2]) != int(row[3]) and
                    max(int(row[2]), int(row[3])) <= 4294967295, 'Distinct actual source-host window IDs')
            hosts.append((row[1], int(row[2]), int(row[3])))
            positions[(row[1], 'host')] = position
    require(geometry == [(stage, window) for stage in ('open', 'after-spawner-tick') for window in ('frame', 'canvas')] and
            len(hosts) == 2 and hosts[0][0] == 'open' and hosts[1][0] == 'after-spawner-tick' and
            hosts[0][1:] == hosts[1][1:], 'Stable source host and exact ordered geometry lifecycle')
    for stage in ('open', 'after-spawner-tick'):
        require(positions[(stage, 'frame')] < positions[(stage, 'canvas')] < positions[(stage, 'host')],
                'Native geometry precedes each live host witness')
    selected = [(i, line) for i, line in enumerate(lines) if line.startswith('SPAWNER_TICK_')]
    allowed = ('SPAWNER_TICK_ACTIVE ', 'SPAWNER_TICK_CHUNK ', 'SPAWNER_TICK_PIG ', 'SPAWNER_TICK_RELOAD ', 'SPAWNER_TICK_FAILURE ',
               'SPAWNER_TICK_STOP ', 'SPAWNER_TICK_PREPARE ', 'SPAWNER_TICK_FINISH ')
    require(all(not line.startswith('SPAWNER_') or line.startswith('SPAWNER_TICK_') for line in lines) and
            all(line.startswith(allowed) for _, line in selected) and
            positions[('open', 'host')] < selected[0][0] and selected[-1][1] == final[0],
            'Only complete bounded Spawner protocol after live startup')
    stop = 'SPAWNER_TICK_STOP actual-frame-close=1 source-retry=0'
    require([line for _, line in selected if line.startswith('SPAWNER_TICK_STOP')] == [stop] and
            positions[('after-spawner-tick', 'host')] < lines.index(stop) < lines.index(final[0]),
            'Actual Frame close follows final host and precedes cleanup')
    result = {'checks': int(finish[1]), 'window_ids': list(hosts[0][1:])}
    if profile == 'failure':
        failure = ('SPAWNER_TICK_FAILURE world-pc=390 cy-pc=571 published-pig=1 preview-distinct=1 '
                   'observer-self-removed=1 delay-reset=0 owned-child=1')
        prepared = ('SPAWNER_TICK_PREPARE source-unchanged=1 world-time-unchanged=1 ticks-unchanged=1 '
                    'candidate-still-published=1 owned-child-ended=1')
        require([line for _, line in selected] == [failure, stop, prepared, final[0]],
                'Exact retained failure, actual stop and checked App cleanup sequence')
    else:
        require(len(selected) >= 7 if profile == 'success' else len(selected) == 3,
                'Exact countdown or complete bounded successful native protocol')
        row = re.fullmatch(r'SPAWNER_TICK_ACTIVE profile=' + profile +
                           r' entity-calls=([1-9][0-9]{0,9}) attempts=([0-9]{1,10}) factory=([0-9]{1,10}) '
                           r'predicates=([0-9]{1,10}) accepted=([0-9]{1,10}) inserted=([0-9]{1,10}) '
                           r'smoke=([1-9][0-9]{0,9}) flame=([1-9][0-9]{0,9}) explode=([0-9]{1,10}) resets=([0-9]{1,10}) '
                           r'pigs=([0-9]{1,10}) delay=([0-9]{1,5}) time=([0-9]{1,19}) active-tile=1 real-timer=1', selected[0][1])
        require(row is not None, 'Actual World.g/Timer/Tile observation with exact fields')
        values = tuple(map(int, row.groups()))
        calls, attempts, factory, predicates, accepted, inserted, smoke, flame, explode, resets, pigs, delay, world_time = values
        require(0 <= world_time < 1 << 63 and calls > 0 and attempts == factory and
                predicates <= factory and accepted <= predicates and inserted <= accepted and smoke == flame,
                'Actual copied call observations have coherent bounded counts')
        if profile == 'success':
            require(pigs > 0 and inserted > 0 and explode >= 20 and resets > 0 and 0 < delay <= 799,
                    'Genuine expired Pig publication, full LS.V and later delay reset')
            reload = re.fullmatch(r'SPAWNER_TICK_RELOAD pigs=([1-9][0-9]{0,9}) new-world=1 preview-distinct=1 '
                                  r'ordinary-save-reselect=1 fresh-insertions=0', selected[-3][1])
            require(reload is not None and selected[-2][1] == stop and
                    selected[0][0] < selected[-3][0] < positions[('after-spawner-tick', 'frame')],
                    'Reselected actual World Pigs precede final host with no replacement spawns')
            result['reloaded_pigs'] = int(reload[1])
            result['readback'] = reload_receipts([line for _, line in selected[1:-3]])
            require(result['reloaded_pigs'] == len(result['readback']['pigs']), 'Every runtime Pig has exactly one paired readback')
        else:
            require((attempts, factory, predicates, accepted, inserted, explode, resets, pigs) == (0,) * 8 and
                    0 < delay < 32767 and selected[1][1] == stop,
                    'Long delay actually decrements in registered active Tile ticks')
        result['active'] = dict(zip(('entity_calls', 'attempts', 'factory', 'predicates', 'accepted',
            'inserted', 'smoke', 'flame', 'explode', 'resets', 'pigs', 'delay', 'time'), values))
    return result


def saved_pigs(decoder, directory):
    result, chunks, total = [], 0, 0
    for path in sorted((directory / 'region').glob('r.*.*.mcr')):
        match = re.fullmatch(r'r\.(-?[0-9]+)\.(-?[0-9]+)\.mcr', path.name)
        require(match is not None, 'Canonical saved region name')
        raw = G.bounded(path, 1048576)
        total += len(raw)
        require(total <= 16 * 1048576 and len(raw) >= 8192 and len(raw) % 4096 == 0, 'Bounded saved region inventory')
        rx, rz = map(int, match.groups())
        occupied = {0, 1}
        for slot, location in enumerate(struct.unpack_from('>1024I', raw)):
            if not location:
                continue
            sector, sectors = location >> 8, location & 255
            require(sector >= 2 and sectors > 0 and (sector + sectors) * 4096 <= len(raw) and
                    not occupied.intersection(range(sector, sector + sectors)), 'Contained nonoverlapping saved Chunk allocation')
            occupied.update(range(sector, sector + sectors))
            at = sector * 4096
            size, = struct.unpack_from('>I', raw, at)
            require(1 < size <= sectors * 4096 - 4 and raw[at + 4] == 2, 'Complete actual saved Chunk compression envelope')
            root = decoder.inflate(raw[at + 5:at + size + 4], 15)
            level = G.compound(root.get('Level'), 'Saved Chunk Level')
            require(level.get('xPos') == [3, rx * 32 + slot % 32] and level.get('zPos') == [3, rz * 32 + slot // 32],
                    'Saved Chunk coordinates match actual region slot')
            child, entities = G.typed(level.get('Entities'), 9, 'Saved Entity List')
            require(child == 10 or not entities, 'Actual nonempty Entity Compound List type')
            chunks += 1
            require(chunks <= 1024, 'Bounded saved Chunk count')
            for entity in entities:
                if entity.get('id') != [8, 'Pig']:
                    continue
                require(entity.get('Saddle') == [1, 0] and entity.get('Health', [None])[0] == 2,
                        'Actual Pig subtype and Living NBT fields')
                for key, kind, count, fmt, width in (('Pos', 6, 3, 'd', 'Q'), ('Motion', 6, 3, 'd', 'Q'), ('Rotation', 5, 2, 'f', 'I')):
                    tag, words = G.typed(entity.get(key), 9, 'Actual Pig ' + key)
                    require(tag == kind and len(words) == count and all(type(word) is int for word in words),
                            'Typed Pig floating List width and extent')
                    values = [struct.unpack('>' + fmt, struct.pack('>' + width, word))[0] for word in words]
                    require(all(math.isfinite(value) for value in values), 'Finite actual Pig ' + key)
                result.append({'chunk': [level['xPos'][1], level['zPos'][1]], 'entity': entity})
    require(chunks > 0, 'Actual saved Chunk inventory')
    return result


def save_receipts(lines, evidence, first, last):
    entries = [line for line in lines if line.startswith('WORLD_SAVE_TITLE_ENTRY ')]
    saves = [line for line in lines if line.startswith('WORLD_SAVE_TITLE mode=')]
    snapshots = [line for line in lines if line.startswith('WORLD_RELOAD_SNAPSHOT')]
    require(len(entries) == len(saves) == len(snapshots) == 1, 'Unique genuine save entry, continuation and saved copy')
    entry = re.fullmatch(r'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=([0-9]+) eligible=([1-9][0-9]*) '
                         r'time=([0-9]+) size-before=([0-9]+) size-after=([0-9]+) entered=1 bytes=([1-9][0-9]*) copied-before-frame=1', entries[0])
    saved = re.fullmatch(r'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 '
                         r'old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=([1-9][0-9]*) time=([0-9]+) '
                         r'size-before=([0-9]+) size-after=([0-9]+) first=([0-9]+) last=([0-9]+) entry-frame=([0-9]+) entry-bytes=([1-9][0-9]*)', saves[0])
    snapshot = re.fullmatch(r'WORLD_RELOAD_SNAPSHOT phase=spawner-tick-saved files=([1-9][0-9]*) bytes=([1-9][0-9]*) actual-file-copy=1', snapshots[0])
    require(entry is not None and saved is not None and snapshot is not None, 'Complete actual save transport rows')
    e, s = tuple(map(int, entry.groups())), tuple(map(int, saved.groups()))
    require(0 <= e[0] < 64 and 0 < e[1] <= 1024 and 0 <= e[2] < 1 << 63 and 0 <= e[3] <= e[4] < 1 << 63 and
            0 < e[5] <= 16 * 1048576 and s[:4] == e[1:5] and s[6:] == (e[0], e[5]) and
            first <= s[4] <= s[5] <= last and lines.index(entries[0]) < lines.index(saves[0]) < lines.index(snapshots[0]),
            'Selected native save entry and continuation agree on signed64 time/size/clock and bytes')
    chunk_rows = [i for i, line in enumerate(lines) if line.startswith('SPAWNER_TICK_CHUNK ')]
    reload_rows = [i for i, line in enumerate(lines) if line.startswith('SPAWNER_TICK_RELOAD ')]
    require(chunk_rows and len(reload_rows) == 1 and lines.index(snapshots[0]) < min(chunk_rows) <
            max(chunk_rows) + 1 < reload_rows[0], 'Actual saved copy precedes passive reload readback and final reload witness')
    inventory = G.snapshot_files(evidence / 'snapshots/spawner-tick-saved')
    require((len(inventory), sum(item['bytes'] for item in inventory.values())) == tuple(map(int, snapshot.groups())),
            'Native saved snapshot count and bytes match retained files')
    require({path.name for path in (evidence / 'entries').iterdir()} == {f'frame-{i}' for i in range(e[0] + 1)},
            'Exact bounded native entry candidate inventory')
    selected = G.snapshot_files(evidence / 'entries' / f'frame-{e[0]}')
    require(sum(item['bytes'] for item in selected.values()) == e[5], 'Actual selected entry bytes match native receipt')
    return {'time': e[2], 'size_before': e[3], 'first': s[4], 'last': s[5], 'frame': e[0]}


def controls():
    class Controls(unittest.TestCase):
        @staticmethod
        def pig_row(phase='runtime', index=0, x=0, z=0):
            return (f'SPAWNER_TICK_PIG phase={phase} index={index} chunk-x={x} chunk-z={z} '
                    'pos=3fe0000000000000,4050000000000000,4004000000000000 '
                    'motion=0000000000000000,0000000000000000,0000000000000000 '
                    'rotation=42b40000,00000000 health=10 hurt=0 death=0 attack=0 fire=0 air=300 grounded=1 saddle=0')

        @classmethod
        def readback(cls):
            return ['SPAWNER_TICK_CHUNK index=0 x=0 z=0 loaded=1 nonloading=1',
                    'SPAWNER_TICK_CHUNK index=1 x=-2 z=-9 loaded=0 nonloading=1',
                    cls.pig_row(), cls.pig_row('input')]

        @staticmethod
        def saved_record(chunk, x):
            bits = lambda value: struct.unpack('>Q', struct.pack('>d', value))[0]
            return {'chunk': chunk, 'entity': {'id': [8, 'Pig'],
                    'Pos': [9, [6, [bits(x), bits(64), bits(2.5)]]],
                    'Motion': [9, [6, [0, 0, 0]]], 'Rotation': [9, [5, [0x42b40000, 0]]],
                    'Health': [2, 10], 'HurtTime': [2, 0], 'DeathTime': [2, 0],
                    'AttackTime': [2, 0], 'Fire': [2, 0], 'Air': [2, 300],
                    'OnGround': [1, 1], 'Saddle': [1, 0]}}

        def test_authored_inputs(self):
            world = G.module('spawner_tick_control_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
            decoder = G.module('spawner_tick_control_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
            chunk = G.module('spawner_tick_control_chunk', ROOT / 'tests/client/test_furnace_gui.py')
            for profile in PROFILES:
                with tempfile.TemporaryDirectory(prefix='spawner-tick-input-') as root:
                    directory = Path(root)
                    (directory / 'region').mkdir()
                    regions(world, directory / 'region', profile)
                    level, blocks, _ = chunk.selected_chunk(decoder, directory)
                    self.assertEqual(blocks[(2 << 7) + 64], 52)
                    self.assertEqual(blocks[(2 << 7) + 63], 2)
                    self.assertEqual(level['TileEntities'][1][1][0]['Delay'], [2, DELAY[profile]])
                    self.assertEqual(saved_pigs(decoder, directory), [])

        def test_ordered_complete_native_protocol(self):
            monitor = {'selected': {'work': [-1920, 0, 1920, 1040]}}
            def host(stage):
                return [f'WORLD_SELECTION_MONITOR stage={stage} window={window} x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040'
                        for window in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']
            stop = 'SPAWNER_TICK_STOP actual-frame-close=1 source-retry=0'
            for profile in PROFILES:
                lines = host('open')
                if profile == 'failure':
                    lines += host('after-spawner-tick') + [
                        'SPAWNER_TICK_FAILURE world-pc=390 cy-pc=571 published-pig=1 preview-distinct=1 observer-self-removed=1 delay-reset=0 owned-child=1', stop,
                        'SPAWNER_TICK_PREPARE source-unchanged=1 world-time-unchanged=1 ticks-unchanged=1 candidate-still-published=1 owned-child-ended=1']
                else:
                    detail = ('attempts=4 factory=4 predicates=4 accepted=1 inserted=1 smoke=22 flame=22 explode=20 resets=1 pigs=1 delay=300'
                              if profile == 'success' else 'attempts=0 factory=0 predicates=0 accepted=0 inserted=0 smoke=2 flame=2 explode=0 resets=0 pigs=0 delay=32765')
                    lines += [f'SPAWNER_TICK_ACTIVE profile={profile} entity-calls=2 {detail} time=2 active-tile=1 real-timer=1']
                    if profile == 'success':
                        lines += self.readback() + ['SPAWNER_TICK_RELOAD pigs=1 new-world=1 preview-distinct=1 ordinary-save-reselect=1 fresh-insertions=0']
                    lines += host('after-spawner-tick') + [stop]
                finish = f'SPAWNER_TICK_FINISH profile={profile} cleanup=1 windows-absent=1 checks=100 source-field-repair=0'
                lines.append(finish)
                raw = '\n'.join(lines) + '\n'
                stderr = FAILURE_STDERR if profile == 'failure' else ''
                self.assertEqual(receipts(raw, stderr, monitor, profile)['checks'], 100)
                for changed in (raw + finish + '\n', raw.replace('checks=100', 'checks=0'),
                                raw.replace('x=-1387', 'x=10'), raw.replace('frame-id=3', 'frame-id=4'),
                                raw.replace('source-retry=0', 'source-retry=1'), raw.replace(finish, ''),
                                'SPAWNER_FAKE returned=1\n' + raw, raw + 'trailing\n'):
                    with self.subTest(profile=profile), self.assertRaises(RuntimeError):
                        receipts(changed, stderr, monitor, profile)
                invalid_stderr = (('', FAILURE_STDERR * 2, FAILURE_STDERR + 'extra\n',
                                   FAILURE_STDERR.replace(':1216)', ':1215)'), FAILURE_STDERR.rstrip('\n'))
                                  if profile == 'failure' else
                                  (FAILURE_STDERR, 'failure hidden in stderr', '\n'))
                for changed in invalid_stderr:
                    with self.subTest(profile=profile, stderr=changed), self.assertRaises(RuntimeError):
                        receipts(raw, changed, monitor, profile)

        def test_complete_nonloading_chunk_classification(self):
            rows = self.readback()
            native = {'reloaded_pigs': 1, 'readback': reload_receipts(rows[:])}
            saved = [self.saved_record([0, 0], 0.5), self.saved_record([-2, -9], -16.5)]
            report = match_reload_pigs(saved, native)
            self.assertEqual(len(report['disk_pigs']), 2)
            self.assertEqual(len(report['loaded_chunk_pigs']), 1)
            self.assertEqual(len(report['unloaded_chunk_pigs']), 1)
            # Current physics may evolve; immutable actual import must still
            # match the disk record exactly. No pose is rewritten to compare.
            native['readback']['pigs'][0]['runtime']['pos'][0] = 0x3fe8000000000000
            self.assertEqual(len(match_reload_pigs(saved, native)['runtime_readback']['pigs']), 1)
            for changed in (rows[1:], rows[:1] + rows[2:], rows + rows[-1:], rows[:-1],
                            [rows[1], rows[0]] + rows[2:], rows[:2] + list(reversed(rows[2:])),
                            [row.replace('nonloading=1', 'nonloading=0') for row in rows],
                            [row.replace('phase=input index=0', 'phase=input index=1') for row in rows],
                            [row.replace('3fe0000000000000', '7ff0000000000000') for row in rows]):
                with self.subTest(rows=changed), self.assertRaises(RuntimeError):
                    readback = reload_receipts(changed[:])
                    match_reload_pigs(saved, {'reloaded_pigs': 1, 'readback': readback})
            mutations = [[row.replace(old, new) for row in rows] for old, new in
                         (('x=-2 z=-9', 'x=-3 z=-9'), ('loaded=0', 'loaded=1'),
                          ('phase=input index=0 chunk-x=0', 'phase=input index=0 chunk-x=1'))]
            mutations.append(rows[:-1] + [rows[-1].replace('3fe0000000000000', '3fe0000000000001')])
            for changed in mutations:
                with self.subTest(rows=changed), self.assertRaises(RuntimeError):
                    match_reload_pigs(saved, {'reloaded_pigs': 1, 'readback': reload_receipts(changed)})
            with self.assertRaises(RuntimeError):
                match_reload_pigs(saved + saved[:1], native)
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    require(result.wasSuccessful(), 'Authored native SDK input controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--profile', choices=PROFILES, default='success')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.controls:
        controls()
        return
    require(os.name == 'nt' and all((args.executable, args.assets, args.artifacts)), 'Actual Windows SDK executable/assets/evidence required')
    world = G.module('spawner_tick_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = G.module('spawner_tick_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = G.module('spawner_tick_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    decoder = G.module('spawner_tick_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    monitor = G.module('spawner_tick_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='spawner-tick-', dir=args.artifacts.resolve()))
    before, assets_before = G.fingerprints(G.source_inputs(executable)), world.snapshot(assets)
    require(before and all(type(value) is str for value in before.values()), 'Readable selected inputs before process')
    report = {'status': 'FAIL', 'profile': args.profile, 'monitor': monitor, 'inputs_before': before,
              'scope': 'Authored Spawner through actual Windows Timer/World.g frames and native ownership; no live Source/RNG repair, original entropy, original graphics/pixels, FPS or sustained-play qualification.'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            for folder, name in (('older', 'Old world'), ('bravo', ''), ('alpha', 'Spawner tick world')):
                directory = saves / folder
                directory.mkdir(parents=True)
                wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, 1303000000000, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
            region = saves / 'alpha/region'
            region.mkdir()
            regions(world, region, args.profile)
            before_files = world.save_title_files(saves, evidence / 'before')
            (evidence / 'entries').mkdir()
            (evidence / 'snapshots').mkdir()
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds, MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds,
                               MC_WORLD_RELOAD_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'), MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first = time.time_ns() // 1000000
            child = world.retained_native_run([str(executable), str(assets), str(evidence), args.profile], environment,
                                               evidence, saves, before_files, 'spawner-tick-' + args.profile, first, 360)
            last = time.time_ns() // 1000000
            print(child.stdout, end='')
            print(child.stderr, end='', file=sys.stderr)
            require(child.returncode == 0, 'Actual native SDK branch and terminal ownership passed')
            report['native'] = receipts(child.stdout, child.stderr, monitor, args.profile)
            images = ('spawner-tick-before-failure.png',) if args.profile == 'failure' else (
                ('spawner-tick.png', 'spawner-tick-title.png', 'spawner-tick-reloaded.png') if args.profile == 'success' else ('spawner-tick.png',))
            require({p.name for p in evidence.glob('*.png')} == set(images), 'Exact source profile image inventory')
            report['images'] = {name: G.png_gate(evidence / name, first) for name in images}
            if args.profile == 'success':
                actions = save_receipts(child.stdout.splitlines(), evidence, first, last)
                directory = evidence / 'snapshots/spawner-tick-saved/alpha'
                pigs = saved_pigs(decoder, directory)
                report['saved'] = match_reload_pigs(pigs, report['native'])
                root = decoder.inflate(G.bounded(directory / 'level.dat', decoder.BOUND), 31)
                data = G.compound(root.get('Data'), 'Actual saved metadata')
                require(data.get('LevelName') == [8, 'Spawner tick world'] and data.get('Time') == [4, actions['time']] and
                        data.get('SizeOnDisk') == [4, actions['size_before']] and actions['first'] <= data['LastPlayed'][1] <= actions['last'],
                        'Independent signed64 metadata agrees with source save transport')
                player = G.compound(data.get('Player'), 'Actual saved Player')
                require(player.get('Dimension') == [3, 0] and player.get('Health') == [2, 20] and
                        player.get('Score') == [3, 7] and G.slot_rows(player.get('Inventory')) == [], 'Saved actual independent Player fields')
                report['saved']['metadata'] = actions
        report['status'] = 'PASS'
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        report['inputs_after'] = G.fingerprints(G.source_inputs(executable))
        report['assets_unchanged'] = world.snapshot(assets) == assets_before
        if report['inputs_after'] != before or not report['assets_unchanged']:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Selected source/helper/product/DLL or imported asset changed during native execution'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('Spawner SDK artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Native Spawner process/input continuity')


if __name__ == '__main__':
    main()
