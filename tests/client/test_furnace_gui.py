"""Placed Furnace through genuine SDK/World ticks, save/reselection and open-screen shutdown."""
import argparse
import copy
import gzip
import importlib.util
import json
import math
import zlib
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value

G = module('furnace_common_gates', ROOT / 'tests/tools/client/test_chest_loop.py')
require, typed, compound, bounded = G.require, G.typed, G.compound, G.bounded
SHARED = 'FURNACE_SHARED placed=1 real-ticks=1 shared-sk=1 output-taken=2 save-burn=1 epochs=2 source-field-repair=0'
SDK = 'FURNACE_SDK profile=cycle ordinary-save-reload=1 epochs=2 source-field-repair=0'
PREPARED = ('FURNACE_OPEN_PREPARE source-unchanged=1 world-time-unchanged=1 ticks-unchanged=1 '
            'stacks-unchanged=1 native-prefix-only=1')
STOPPED = 'FURNACE_OPEN_SHUTDOWN actual-open-ov=1 source-close=0'
IMAGES = {'furnace-placed.png', 'furnace-smelted.png', 'furnace-title.png', 'furnace-reloaded.png'}


def native_receipts(stdout, monitor, stderr='', *, process_interval=None, evidence=None):
    require(type(stdout) is str and stderr == '', 'Clean successful native Furnace transport')
    lines = stdout.splitlines()
    finish = [line for line in lines if line.startswith('FURNACE_FINISH')]
    require(len(finish) == 1, 'Unique Furnace cleanup receipt')
    row = re.fullmatch(r'FURNACE_FINISH profile=(cycle) cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9})', finish[0])
    require(row is not None and int(row[2]) <= 4294967295 and lines[-1] == finish[0], 'Final complete positive uint32 cleanup receipt')
    for prefix, exact in (('FURNACE_SHARED', SHARED), ('FURNACE_SDK', SDK),
                          ('FURNACE_OPEN_PREPARE', PREPARED), ('FURNACE_OPEN_SHUTDOWN', STOPPED)):
        require([line for line in lines if line.startswith(prefix)] == [exact], 'Unique complete ' + prefix)
    require(lines.index(PREPARED) < lines.index(STOPPED) < lines.index(SHARED) < lines.index(SDK) < lines.index(finish[0]),
            'Actual stopped/prepared/shared/SDK/cleanup receipt order')
    stages = ('open', 'after-furnace-reload')
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
    require(host_positions[stages[1]] < lines.index(PREPARED), 'Final source-host witness precedes actual Frame shutdown')
    actions = action_receipts(lines, host_positions['open'], monitor_positions[(stages[1], 'frame')],
                              host_positions[stages[1]], process_interval, evidence)
    return {'checks': int(row[2]), 'window_ids': list(hosts[0][1]), 'monitor_witnesses': observations, 'actions': actions}



def action_receipts(lines, open_host, final_frame, final_host, process_interval=None, evidence=None):
    """Bind actual source SDK actions and ordinary save transport, without guessed movement."""
    allowed = ('FURNACE_TAP ', 'FURNACE_CLICK ', 'FURNACE_COOK ', 'FURNACE_AWARD ', 'FURNACE_SHARED ', 'FURNACE_SDK ',
               'FURNACE_OPEN_PREPARE ', 'FURNACE_OPEN_SHUTDOWN ', 'FURNACE_FINISH ')
    require(all(not line.startswith('FURNACE_') or line.startswith(allowed) for line in lines) and
            not any(line.startswith(('CHEST_', 'FURNACE_STRAFE')) for line in lines),
            'Only actual Furnace protocol; this scene has no strafe or movement receipts')
    options = [(i, line) for i, line in enumerate(lines) if line.startswith('WORLD_SAVE_TITLE_OPTIONS')]
    require(not options or options == [(options[0][0], 'WORLD_SAVE_TITLE_OPTIONS view-distance=3 lines=1 bytes=15 source-loaded=1')] and
            options[0][0] < open_host, 'Optional actual startup Options receipt is unique and early')
    events = [(position, line) for position, line in enumerate(lines)
              if line.startswith(('FURNACE_TAP', 'FURNACE_CLICK', 'FURNACE_COOK', 'FURNACE_AWARD', 'WORLD_SAVE_TITLE', 'WORLD_RELOAD_')) and
              not line.startswith(('WORLD_SAVE_TITLE_HOST', 'WORLD_SAVE_TITLE_OPTIONS'))]
    require(events and all(open_host < position < lines.index(PREPARED) for position, _ in events),
            'All source actions and snapshots follow the live initial host and precede prepare completion')
    at = 0
    def take(pattern, message):
        nonlocal at
        require(at < len(events), message)
        found = re.fullmatch(pattern, events[at][1])
        require(found is not None, message)
        at += 1
        return found
    def tap(y, placed):
        row = take(r'FURNACE_TAP target=0,(63|64),2 placed=([01]) attempts=([1-9][0-9]?) clicks=1 block-use=1 '
                   r'admitted=([01]) setters=([01]) sounds=([01]) count-stores=([01])', 'Exact ordered Furnace SDK placement/activation tap')
        require((int(row[1]), int(row[2])) == (y, placed) and 1 <= int(row[3]) <= 12 and
                all(int(row[i]) == placed for i in range(4, 8)), 'Bounded tap attempts and exact reached mutation deltas')
    def click(index, shift, cursor, before):
        row = take(r'FURNACE_CLICK index=(0|[1-9][0-9]?) right=([01]) shift=([01]) cursor=([01]) '
                   r'calls-before=(0|[1-9][0-9]{0,19}) calls-after=(0|[1-9][0-9]{0,19})', 'Exact ordered actual ID slot click')
        values = tuple(map(int, row.groups()))
        require(values == (index, 0, shift, cursor, before, before + 1), 'Actual input/fuel/output slots and contiguous per-screen source calls')
    def cook(epoch):
        row = take(r'FURNACE_COOK epoch=([12]) start-ticks=(0|[1-9][0-9]{0,19}) start-cook=(0|[1-9][0-9]{0,2}) '
                   r'end-ticks=(0|[1-9][0-9]{0,19}) end-cook=(0|[1-9][0-9]{0,2}) burn=([1-9][0-9]{0,3}) '
                   r'output=1 shared-sk=1 draw-progress=1 draw-flame=1', 'Actual epoch-specific World ticks, cook progress, shared SK and draws')
        observed, first, start_cook, last, end_cook, burn = map(int, row.groups())
        require(observed == epoch and 0 <= first < last < 1 << 64 and 0 <= start_cook < 200 and
                0 <= end_cook < 200 and last - first >= 200 - start_cook and 0 < burn < 1600,
                'Actual unsigned64 tick extent completes the remaining cook without a synthetic timer')
        return (first, last, burn)
    tap(63, 1)
    tap(64, 0)
    for index, cursor, before in ((3, 1, 0), (0, 0, 1), (4, 1, 2), (1, 0, 3)):
        click(index, 0, cursor, before)
    first_cook = cook(1)
    click(2, 1, 0, 4)
    take(r'FURNACE_AWARD parent-present=0 parent-value=0 iron-value=0 writer-same=1',
         'Authored held Furnace has no EP4 crafting history; actual DC prerequisite gate skips EP5')
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
    require(0 <= frame < 64 and 0 < eligible <= 1024 and 0 <= world_time < 1 << 63 and
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
    snapshot = numbers(r'WORLD_RELOAD_SNAPSHOT phase=furnace-saved files=' + unsigned +
                       r' bytes=' + unsigned + r' actual-file-copy=1',
                       'Unique actual Furnace save snapshot precedes reselection')
    require(0 < snapshot[0] <= 1024 and 0 < snapshot[1] <= 16 * 1048576,
            'Bounded actual source save snapshot receipt')
    tap(64, 0)
    second_cook = cook(2)
    require(first_cook[1] <= second_cook[0] and second_cook[2] < first_cook[2],
            'Reload follows actual first output; the same saved burn continues counting down')
    click(2, 1, 0, 0)
    require(events[at - 1][0] < final_frame, 'All resumed source actions precede the final live reloaded host')
    snapshots = {'furnace-saved': snapshot}
    for name in ('furnace-open-stopped', 'furnace-open-prepared'):
        position = events[at][0] if at < len(events) else -1
        counts = numbers(r'WORLD_RELOAD_SNAPSHOT phase=' + name + r' files=' + unsigned + r' bytes=' + unsigned +
                         r' actual-file-copy=1', 'Exact ordered actual stopped/prepared file snapshot')
        require(final_host < position and 0 < counts[0] <= 1024 and 0 < counts[1] <= 16 * 1048576,
                'Stopped/prepared copies follow the live reloaded host and fit retained bounds')
        snapshots[name] = counts
    require(at == len(events), 'Every tap/click/cook/save/reload receipt occurs once in source order')
    if evidence is not None:
        entries = evidence / 'entries'
        require(entries.is_dir() and not entries.is_symlink() and
                {path.name for path in entries.iterdir()} == {f'frame-{i}' for i in range(frame + 1)},
                'Exact native pre-save candidate inventory')
        for index in range(frame + 1):
            inventory = G.snapshot_files(entries / f'frame-{index}')
            if index == frame:
                require(sum(item['bytes'] for item in inventory.values()) == entry_bytes, 'Actual chosen pre-save bytes match source entry')
        require({path.name for path in (evidence / 'snapshots').iterdir()} == set(snapshots), 'Exact three native Furnace snapshots')
        for name, counts in snapshots.items():
            inventory = G.snapshot_files(evidence / 'snapshots' / name)
            require((len(inventory), sum(item['bytes'] for item in inventory.values())) == counts,
                    'Retained native snapshot count/bytes match ' + name)
        require(G.snapshot_files(evidence / 'snapshots/furnace-open-stopped') ==
                G.snapshot_files(evidence / 'snapshots/furnace-open-prepared'), 'Native-only prepare leaves every actual saved byte unchanged')
    return {'frame': frame, 'eligible': eligible, 'world_time': world_time, 'size_before': before, 'size_after': after,
            'first': first, 'last': last, 'entry_bytes': entry_bytes, 'snapshots': snapshots,
            'cook_ticks': [list(first_cook), list(second_cook)]}

def selected_chunk(decoder, directory):
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
    require(type(root) is dict and set(root) == {'Level'}, 'Saved Chunk Level root')
    level = compound(root['Level'], 'Saved actual Chunk Level Compound')
    require(typed(level.get('xPos'), 3, 'Saved Chunk X width') == 0 and
            typed(level.get('zPos'), 3, 'Saved Chunk Z width') == 0, 'Selected Chunk coordinates')
    blocks = typed(level.get('Blocks'), 7, 'Saved Blocks ByteArray')
    nibbles = typed(level.get('Data'), 7, 'Saved metadata ByteArray')
    require(type(blocks) is bytes and len(blocks) == 32768 and
            type(nibbles) is bytes and len(nibbles) == 16384, 'Complete saved block arrays')
    for key, length in (('SkyLight', 16384), ('BlockLight', 16384), ('HeightMap', 256)):
        value = typed(level.get(key), 7, 'Complete actual Chunk ' + key)
        require(type(value) is bytes and len(value) == length, 'Exact actual Chunk array extent')
    return level, blocks, nibbles



DATA_BASE = {'RandomSeed', 'SpawnX', 'SpawnY', 'SpawnZ', 'SizeOnDisk', 'LastPlayed', 'LevelName', 'version', 'Player'}
DATA_SAVED = DATA_BASE | {'Time', 'rainTime', 'raining', 'thunderTime', 'thundering'}


def metadata_gate(decoder, path, *, saved):
    root = decoder.inflate(bounded(path, decoder.BOUND), 31)
    require(type(root) is dict and set(root) == {'Data'}, 'Complete actual metadata root')
    data = compound(root['Data'], 'Complete metadata Data Compound')
    require(set(data) == (DATA_SAVED if saved else DATA_BASE), 'Complete authored/source metadata fields; partial seed is not admitted')
    for key, expected in {'RandomSeed': [4, 123], 'SpawnX': [3, 0], 'SpawnY': [3, 64], 'SpawnZ': [3, 0],
                          'LevelName': [8, 'Furnace world'], 'version': [3, 19132]}.items():
        require(data.get(key) == expected, 'Pinned actual defining World metadata ' + key)
    for key in ('SizeOnDisk', 'LastPlayed') + (('Time',) if saved else ()):
        value = typed(data.get(key), 4, 'Exact source signed64 ' + key)
        require(type(value) is int and 0 <= value < 1 << 63, 'Nonnegative signed64 ' + key)
    if saved:
        for key in ('rainTime', 'thunderTime'):
            value = typed(data.get(key), 3, 'Exact weather int ' + key)
            require(type(value) is int and -(1 << 31) <= value < 1 << 31, 'Actual weather int range')
        for key in ('raining', 'thundering'):
            require(typed(data.get(key), 1, 'Exact weather byte ' + key) in (0, 1), 'Actual weather Boolean')
    player = compound(data['Player'], 'Complete actual Player Compound')
    require({'Dimension', 'Health', 'Score', 'Pos', 'Motion', 'Rotation', 'Inventory'} <= set(player),
            'Source Player scalars, floating lists and inventory all present')
    require(player['Dimension'] == [3, 0] and player['Health'] == [2, 20] and player['Score'] == [3, 7],
            'Actual authored local Player health, score and Dimension')
    for key, kind, count, width, fmt in (('Pos', 6, 3, 64, 'd'), ('Motion', 6, 3, 64, 'd'), ('Rotation', 5, 2, 32, 'f')):
        values = typed(player.get(key), 9, 'Exact Player ' + key + ' List')
        require(type(values) is list and len(values) == 2 and values[0] == kind and type(values[1]) is list and
                len(values[1]) == count and all(type(value) is int and 0 <= value < 1 << width for value in values[1]),
                'Exact raw floating widths and list extent for ' + key)
        numbers = [struct.unpack('>' + fmt, struct.pack('>Q' if width == 64 else '>I', value))[0] for value in values[1]]
        require(all(math.isfinite(value) for value in numbers), 'Finite real Player ' + key)
    G.slot_rows(player['Inventory'])
    return data, player


def continuity_gate(before, after, assets_before, assets_after):
    for name, first, last in (('source/helper/product/DLL', before, after), ('assets', assets_before, assets_after)):
        require(type(first) is dict and first and type(last) is dict and last and
                all(type(key) is str and key and type(value) is str and re.fullmatch(r'[0-9a-f]{64}', value)
                    for mapping in (first, last) for key, value in mapping.items()) and first == last,
                'Exact readable nonempty ' + name + ' input hash inventory throughout native process')
    return True


def retained_save_gate(decoder, evidence, actions, process_interval):
    """Join source receipt fields to retained bytes and independent NBT, not to runtime memory."""
    before, entry, saved = evidence / 'before', evidence / 'entries' / f"frame-{actions['frame']}", evidence / 'snapshots/furnace-saved'
    original, original_player = metadata_gate(decoder, before / 'alpha/level.dat', saved=False)
    require(G.slot_rows(original_player['Inventory']) == [(0, 61, 1, 0), (9, 15, 2, 0), (10, 263, 2, 0)],
            'Independent initial held Furnace, ore and fuel NBT')
    level, blocks, _ = selected_chunk(decoder, before / 'alpha')
    initial_tiles = typed(level.get('TileEntities'), 9, 'Authored initial Tile List')
    require(initial_tiles == [10, []] and blocks[(2 << 7) | 64] == 0 and blocks[((2 << 7) | 64) - 1] == 3,
            'Actual authored initial region has no seeded Furnace or Tile')
    entry_root = decoder.inflate(bounded(entry / 'alpha/level.dat', decoder.BOUND), 31)
    entry_data = compound(entry_root.get('Data'), 'Actual pre-save metadata Compound')
    entry_data, _ = metadata_gate(decoder, entry / 'alpha/level.dat', saved='Time' in entry_data)
    if 'Time' in entry_data:
        require(entry_data['Time'][1] <= actions['world_time'] and
                original['SizeOnDisk'][1] <= entry_data['SizeOnDisk'][1] <= actions['size_before'] and
                process_interval[0] <= entry_data['LastPlayed'][1] <= actions['first'], 'Genuine pre-save partial metadata prefix interval')
    before_files, entry_files, after_files = G.snapshot_files(before), G.snapshot_files(entry), G.snapshot_files(saved)
    def world_product(name):
        return name in {'alpha/level.dat', 'alpha/level.dat_old', 'alpha/session.lock'} or name.startswith('alpha/region/')
    protected = {name: value for name, value in before_files.items() if not world_product(name)}
    require(all(entry_files.get(name) == value and after_files.get(name) == value for name, value in protected.items()),
            'Unrelated authored save bytes remain identical through entry and full save')
    require(all(name in before_files or world_product(name) for name in entry_files) and
            all(name in entry_files or name == 'alpha/level.dat_old' or name.startswith('alpha/region/') for name in after_files),
            'No unexplained source save file additions')
    require(bounded(entry / 'alpha/session.lock', 8) == bounded(saved / 'alpha/session.lock', 8) and
            len(bounded(entry / 'alpha/session.lock', 8)) == 8, 'Actual full-save session token unchanged')
    require(bounded(saved / 'alpha/level.dat_old', decoder.BOUND) == bounded(entry / 'alpha/level.dat', decoder.BOUND),
            'Actual source metadata backup is the byte-identical selected entry')
    data, player = metadata_gate(decoder, saved / 'alpha/level.dat', saved=True)
    require(data['Time'] == [4, actions['world_time']] and data['SizeOnDisk'] == [4, actions['size_before']] and
            actions['first'] <= data['LastPlayed'][1] <= actions['last'] and
            process_interval[0] <= actions['first'] <= actions['last'] <= process_interval[1],
            'Source signed64 time/size/clock receipt fields agree with actual saved metadata')
    source_level, source_blocks, source_nibbles = selected_chunk(decoder, saved / 'alpha')
    require(source_level.get('LastUpdate', [None])[0] == 4 and
            0 <= source_level['LastUpdate'][1] <= actions['world_time'], 'Actual selected Chunk LastUpdate is a valid source prefix')
    stamp = struct.unpack_from('>i', bounded(saved / 'alpha/region/r.0.0.mcr', decoder.BOUND), 4096)[0]
    entry_level, _, _ = selected_chunk(decoder, entry / 'alpha')
    entry_stamp = struct.unpack_from('>i', bounded(entry / 'alpha/region/r.0.0.mcr', decoder.BOUND), 4096)[0]
    if (source_level, stamp) != (entry_level, entry_stamp):
        require(source_level['LastUpdate'] == [4, actions['world_time']] and
                actions['first'] // 1000 <= stamp <= actions['last'] // 1000, 'Actual changed QJ Chunk joins full-save time and clock')
    result = saved_semantics(player, source_level, source_blocks, source_nibbles)
    require(bounded(evidence / 'options-input.txt', 15) == b'viewDistance:3\n', 'Retained exact authored Options input')
    return {'metadata_time': data['Time'][1], 'size_before': data['SizeOnDisk'][1], 'last_played': data['LastPlayed'][1], 'semantic': result}

def saved_semantics(player, level, blocks, nibbles):
    require(typed(player.get('Dimension'), 3, 'Player Dimension int') == 0 and
            G.slot_rows(player.get('Inventory')) == [(8, 265, 1, 0)], 'Saved independently consumed placed Furnace and transferred ore/fuel')
    at = (2 << 7) | 64
    require(blocks[at] == 62 and ((nibbles[at // 2] >> ((at & 1) * 4)) & 15) == 2 and
            blocks[at - 1] == 3 and blocks[at + 1] == 0, 'Actual lit placed Furnace orientation/support/air')
    entities = typed(level.get('Entities'), 9, 'Chunk Entity List')
    require(type(entities) is list and len(entities) == 2 and type(entities[0]) is int and
            0 <= entities[0] <= 10 and entities[1] == [], 'No dropped Item or unexpected Entity')
    tiles = typed(level.get('TileEntities'), 9, 'Furnace Tile List')
    require(type(tiles) is list and len(tiles) == 2 and tiles[0] == 10 and type(tiles[1]) is list and
            len(tiles[1]) == 1, 'Single actual Furnace Compound')
    tile = tiles[1][0]
    require(type(tile) is dict and set(tile) == {'id', 'x', 'y', 'z', 'BurnTime', 'CookTime', 'Items'}, 'Exact actual Furnace NBT fields; no persisted fuel duration')
    require(typed(tile['id'], 8, 'Furnace id String') == 'Furnace' and
            [typed(tile[key], 3, 'Furnace coordinate int') for key in ('x', 'y', 'z')] == [0, 64, 2], 'Actual saved Furnace identity and coordinates')
    burn, cook = typed(tile['BurnTime'], 2, 'BurnTime short'), typed(tile['CookTime'], 2, 'CookTime short')
    require(type(burn) is int and type(cook) is int and 0 < burn < 1600 and 0 < cook < 200,
            'Saved while the remaining real fuel and second cook are active')
    require(G.slot_rows(tile['Items']) == [(0, 15, 1, 0), (1, 263, 1, 0)], 'Independent saved SK cells after first output pickup')
    return {'burn_remaining': burn, 'cook_progress': cook, 'player': [(8, 265, 1, 0)],
            'furnace': [(0, 15, 1, 0), (1, 263, 1, 0)], 'metadata': 2}


def saved_gate(decoder, directory):
    _, player = metadata_gate(decoder, directory / 'level.dat', saved=True)
    level, blocks, nibbles = selected_chunk(decoder, directory)
    return saved_semantics(player, level, blocks, nibbles)


def image_gate(evidence, first_millis):
    require({path.name for path in evidence.glob('furnace-*.png')} == IMAGES, 'Exact fresh Furnace image inventory')
    return {name: G.png_gate(evidence / name, first_millis) for name in sorted(IMAGES)}


def self_test():
    C = module('furnace_decoder_test_writers', ROOT / 'tests/tools/client/test_chest_controls.py')
    D = G.module('furnace_decoder_test_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    MONITOR = {'selected': {'work': [-1920, 0, 1920, 1040]}}
    class Controls(unittest.TestCase):
        @staticmethod
        def item(slot, ident, count):
            return {'Slot': [1, slot], 'id': [2, ident], 'Count': [1, count], 'Damage': [2, 0]}

        def graph(self):
            item = self.item
            player = {'Dimension': [3, 0], 'Inventory': [9, [10, [item(8, 265, 1)]]]}
            tile = {'id': [8, 'Furnace'], 'x': [3, 0], 'y': [3, 64], 'z': [3, 2],
                    'BurnTime': [2, 1371], 'CookTime': [2, 29],
                    'Items': [9, [10, [item(0, 15, 1), item(1, 263, 1)]]]}
            level = {'Entities': [9, [10, []]], 'TileEntities': [9, [10, [tile]]]}
            blocks, nibbles = bytearray(32768), bytearray(16384)
            at = (2 << 7) | 64
            blocks[at], blocks[at - 1], nibbles[at // 2] = 62, 3, 2
            return player, level, blocks, nibbles

        def test_independent_saved_furnace(self):
            self.assertEqual(saved_semantics(*self.graph())['burn_remaining'], 1371)

        def test_counter_width_consumption_tile_and_variant_corruptions(self):
            def tile(level): return level['TileEntities'][1][1][0]
            variants = (
                lambda p, l, b, n: p['Inventory'][1][1].append(self.item(0, 61, 1)),
                lambda p, l, b, n: p['Inventory'][1][1][0]['Count'].__setitem__(1, 2),
                lambda p, l, b, n: tile(l)['BurnTime'].__setitem__(0, 3),
                lambda p, l, b, n: tile(l)['BurnTime'].__setitem__(1, 0),
                lambda p, l, b, n: tile(l)['BurnTime'].__setitem__(1, -1),
                lambda p, l, b, n: tile(l)['CookTime'].__setitem__(1, 200),
                lambda p, l, b, n: tile(l).update(FuelDuration=[3, 1600]),
                lambda p, l, b, n: tile(l)['Items'][1][1][1]['Count'].__setitem__(1, 2),
                lambda p, l, b, n: l['TileEntities'][1][1].append(copy.deepcopy(tile(l))),
                lambda p, l, b, n: b.__setitem__((2 << 7) | 64, 61),
                lambda p, l, b, n: n.__setitem__(((2 << 7) | 64) // 2, 3),
            )
            for index, mutate in enumerate(variants):
                values = self.graph(); mutate(*values)
                with self.subTest(index=index), self.assertRaises(RuntimeError): saved_semantics(*values)

        def transport(self, entry_bytes=10, snapshots=None):
            snapshots = snapshots or {name: (2, 11) for name in ('furnace-saved', 'furnace-open-stopped', 'furnace-open-prepared')}
            def hosts(stage):
                return [f'WORLD_SELECTION_MONITOR stage={stage} window={window} x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040'
                        for window in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']
            def tap(y, placed):
                return f'FURNACE_TAP target=0,{y},2 placed={placed} attempts=1 clicks=1 block-use=1 admitted={placed} setters={placed} sounds={placed} count-stores={placed}'
            def click(index, shift, cursor, before):
                return f'FURNACE_CLICK index={index} right=0 shift={shift} cursor={cursor} calls-before={before} calls-after={before + 1}'
            def snapshot(name):
                count, total = snapshots[name]
                return f'WORLD_RELOAD_SNAPSHOT phase={name} files={count} bytes={total} actual-file-copy=1'
            lines = hosts('open') + [tap(63, 1), tap(64, 0)]
            lines += [click(index, 0, cursor, before) for index, cursor, before in ((3, 1, 0), (0, 0, 1), (4, 1, 2), (1, 0, 3))]
            lines += ['FURNACE_COOK epoch=1 start-ticks=10 start-cook=0 end-ticks=210 end-cook=0 burn=1400 output=1 shared-sk=1 draw-progress=1 draw-flame=1',
                      click(2, 1, 0, 4), 'FURNACE_AWARD parent-present=0 parent-value=0 iron-value=0 writer-same=1',
                      'WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=4 resumed=1',
                      'WORLD_SAVE_TITLE_ESCAPE keyboard-before=4 keyboard-after=6 fresh-pause=1 distinct-recovered=1',
                      f'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=5 time=500 size-before=3145728 size-after=3149824 entered=1 bytes={entry_bytes} copied-before-frame=1',
                      f'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=5 time=500 size-before=3145728 size-after=3149824 first=1500 last=2500 entry-frame=0 entry-bytes={entry_bytes}',
                      snapshot('furnace-saved'), tap(64, 0),
                      'FURNACE_COOK epoch=2 start-ticks=230 start-cook=10 end-ticks=420 end-cook=0 burn=1200 output=1 shared-sk=1 draw-progress=1 draw-flame=1', click(2, 1, 0, 0)]
            lines += hosts('after-furnace-reload') + [snapshot('furnace-open-stopped'), snapshot('furnace-open-prepared'), PREPARED, STOPPED, SHARED, SDK,
                                                    'FURNACE_FINISH profile=cycle cleanup=1 windows-absent=1 checks=7']
            return '\n'.join(lines) + '\n'

        def metadata(self, saved):
            player = {'Dimension': [3, 0], 'Health': [2, 20], 'Score': [3, 7],
                      'Inventory': [9, [10, [self.item(8, 265, 1)] if saved else
                                       [self.item(0, 61, 1), self.item(9, 15, 2), self.item(10, 263, 2)]]]}
            for name, kind, fmt, values in (('Pos', 6, 'd', (.5, 65.62, -.5)), ('Motion', 6, 'd', (0., 0., 0.)), ('Rotation', 5, 'f', (0., 30.))):
                player[name] = [9, [kind, [struct.unpack('>Q' if kind == 6 else '>I', struct.pack('>' + fmt, value))[0] for value in values]]]
            data = {'RandomSeed': [4, 123], 'SpawnX': [3, 0], 'SpawnY': [3, 64], 'SpawnZ': [3, 0],
                    'LevelName': [8, 'Furnace world'], 'version': [3, 19132], 'SizeOnDisk': [4, 3145728],
                    'LastPlayed': [4, 2000 if saved else 1304000000000], 'Player': [10, player]}
            if saved: data.update(Time=[4, 500], rainTime=[3, 0], raining=[1, 0], thunderTime=[3, 0], thundering=[1, 0])
            return {'Data': [10, data]}

        def chunk(self, saved):
            _, level, blocks, nibbles = self.graph()
            if not saved:
                blocks[(2 << 7) | 64] = 0; nibbles[(2 << 7) // 2 + 32] = 0
                level['TileEntities'] = [9, [10, []]]
            level.update(xPos=[3, 0], zPos=[3, 0], Blocks=[7, blocks], Data=[7, nibbles], SkyLight=[7, bytes(16384)],
                         BlockLight=[7, bytes(16384)], HeightMap=[7, bytes(256)], LastUpdate=[4, 500 if saved else 0])
            return {'Level': [10, level]}

        def write(self, directory, saved=True, metadata=None, chunk=None):
            C.write_saved(directory, metadata or self.metadata(saved), chunk or self.chunk(saved))
            if saved:
                path = directory / 'region/r.0.0.mcr'; raw = bytearray(path.read_bytes()); struct.pack_into('>i', raw, 4096, 2); path.write_bytes(raw)

        def files(self, evidence):
            before = evidence / 'before'; self.write(before / 'alpha', saved=False)
            (before / 'alpha/session.lock').write_bytes(b'12345678')
            (before / 'protected').write_bytes(b'protected actual bytes')
            entry = evidence / 'entries/frame-0'; entry.parent.mkdir(); import shutil; shutil.copytree(before, entry)
            saved = evidence / 'snapshots/furnace-saved'; saved.parent.mkdir(); shutil.copytree(entry, saved)
            self.write(saved / 'alpha'); (saved / 'alpha/level.dat_old').write_bytes((entry / 'alpha/level.dat').read_bytes())
            for name in ('furnace-open-stopped', 'furnace-open-prepared'): shutil.copytree(saved, saved.parent / name)
            (evidence / 'options-input.txt').write_bytes(b'viewDistance:3\n')
            snapshots = {name: (len(G.snapshot_files(evidence / 'snapshots' / name)),
                               sum(value['bytes'] for value in G.snapshot_files(evidence / 'snapshots' / name).values()))
                         for name in ('furnace-saved', 'furnace-open-stopped', 'furnace-open-prepared')}
            return self.transport(sum(value['bytes'] for value in G.snapshot_files(entry).values()), snapshots)

        def test_exact_native_protocol_and_corruptions(self):
            source, monitor = self.transport(), {'selected': {'work': [-1920, 0, 1920, 1040]}}
            self.assertEqual(native_receipts(source, monitor)['checks'], 7)
            variants = [source.replace(line + '\n', '') for line in (PREPARED, STOPPED, SHARED, SDK)]
            variants += [source.replace(SHARED, SHARED + '\n' + SHARED), source + 'late output\n',
                         source.replace('real-ticks=1', 'real-ticks=0'), source.replace('shared-sk=1', 'shared-sk=0'),
                         source.replace('checks=7', 'checks=0'), source.replace('checks=7', 'checks=4294967296'),
                         source.replace('width=854', 'width=853', 1), source.replace('frame-id=3', 'frame-id=0', 1),
                         source.replace(PREPARED + '\n', '').replace(SHARED, SHARED + '\n' + PREPARED)]
            for index, value in enumerate(variants):
                with self.subTest(index=index), self.assertRaises(RuntimeError): native_receipts(value, monitor)
            with self.assertRaises(RuntimeError): native_receipts(source, monitor, 'unexpected stderr')


        def test_each_intermediate_native_record_is_unique_and_ordered(self):
            lines = self.transport().splitlines()
            families = ('FURNACE_TAP', 'FURNACE_CLICK', 'FURNACE_COOK', 'FURNACE_AWARD', 'WORLD_SAVE_TITLE_RECOVERY',
                        'WORLD_SAVE_TITLE_ESCAPE', 'WORLD_SAVE_TITLE_ENTRY', 'WORLD_SAVE_TITLE ', 'WORLD_RELOAD_SNAPSHOT')
            for index, line in enumerate(lines):
                if not line.startswith(families): continue
                without = lines[:index] + lines[index + 1:]
                variants = (without, lines[:index] + [line] + lines[index:], [line] + without,
                            without[:-1] + [line] + without[-1:], lines[:index] + [line + ' forged=1'] + lines[index + 1:])
                for variant, value in enumerate(variants):
                    with self.subTest(index=index, variant=variant), self.assertRaises(RuntimeError):
                        native_receipts('\n'.join(value) + '\n', MONITOR)

        def test_three_taps_bind_target_attempts_and_exact_deltas(self):
            source = self.transport()
            for old, new in (('target=0,63,2', 'target=1,63,2'), ('attempts=1', 'attempts=13'), ('attempts=1', 'attempts=01'),
                             ('clicks=1', 'clicks=2'), ('block-use=1', 'block-use=0'), ('admitted=1', 'admitted=0'),
                             ('setters=1', 'setters=0'), ('sounds=1', 'sounds=0'), ('count-stores=1', 'count-stores=0')):
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(source.replace(old, new, 1), MONITOR)
            self.assertEqual(native_receipts(source.replace('attempts=1', 'attempts=12'), MONITOR)['checks'], 7)

        def test_six_clicks_bind_actual_slots_keys_cursor_and_source_calls(self):
            source = self.transport()
            for old, new in (('index=3 ', 'index=4 '), ('index=3 ', 'index=03 '), ('right=0', 'right=1'), ('shift=1', 'shift=0'),
                             ('cursor=1', 'cursor=0'), ('calls-after=1', 'calls-after=2'), ('calls-before=0', 'calls-before=1'),
                             ('calls-before=0', 'calls-before=18446744073709551616')):
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(source.replace(old, new, 1), MONITOR)

        def test_cook_epoch_tick_width_progress_burn_and_draw_corruptions(self):
            source = self.transport()
            for old, new in (('epoch=1 start', 'epoch=2 start'), ('start-ticks=10', 'start-ticks=-1'),
                             ('start-ticks=10', 'start-ticks=01'), ('end-ticks=210', 'end-ticks=209'),
                             ('end-ticks=210', 'end-ticks=18446744073709551616'), ('start-cook=10', 'start-cook=200'),
                             ('end-cook=0', 'end-cook=200'), ('burn=1400', 'burn=1600'), ('burn=1200', 'burn=1400'),
                             ('output=1', 'output=0'), ('draw-flame=1', 'draw-flame=0'), ('draw-progress=1', 'draw-progress=0')):
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(source.replace(old, new, 1), MONITOR)

        def test_award_receipt_binds_actual_missing_parent_and_shared_writer(self):
            source = self.transport()
            for old, new in (('parent-present=0', 'parent-present=1'), ('parent-value=0', 'parent-value=1'),
                             ('iron-value=0', 'iron-value=1'), ('iron-value=0', 'iron-value=2'),
                             ('writer-same=1', 'writer-same=0')):
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(source.replace(old, new), MONITOR)

        def test_forged_strafe_and_unexecuted_native_families_are_rejected(self):
            source = self.transport()
            for x in ('1.5', 'nan', 'inf', '-inf', '1e999'):
                for prefix in ('FURNACE_STRAFE', 'CHEST_STRAFE'):
                    value = source.replace(SHARED, f'{prefix} attempt=1 target=1,63,2 key=A x={x}\n' + SHARED)
                    with self.subTest(prefix=prefix, x=x), self.assertRaises(RuntimeError): native_receipts(value, MONITOR)
            with self.assertRaises(RuntimeError): native_receipts(source.replace(SHARED, 'FURNACE_REPAIRED forged=1\n' + SHARED), MONITOR)

        def test_recovery_escape_save_widths_and_shared_fields(self):
            source = self.transport()
            changes = (('focus-pauses=0', 'focus-pauses=5'), ('keyboard-before=4', 'keyboard-before=3'),
                       ('keyboard-after=6', 'keyboard-after=5'), ('frame=0 eligible', 'frame=64 eligible'),
                       ('eligible=5', 'eligible=0'), ('time=500', 'time=-1'), ('time=500', 'time=9223372036854775808'),
                       ('size-after=3149824', 'size-after=9223372036854775808'), ('first=1500', 'first=0'),
                       ('last=2500', 'last=1499'), ('entry-frame=0', 'entry-frame=1'), ('entry-bytes=10', 'entry-bytes=11'),
                       ('copied-before-frame=1', 'copied-before-frame=0'), ('leave-pc=366', 'leave-pc=64'))
            for old, new in changes:
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(source.replace(old, new, 1), MONITOR)

        def test_real_autopause_retry_is_bounded_and_contiguous(self):
            source = self.transport()
            recovery = 'WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=4 resumed=1\n'
            retry = recovery + ('WORLD_SAVE_TITLE_AUTOPAUSE attempt=1 focus-pauses=1 old-oz-keys=2 real-id4=1 resumed=1 frames=2\n'
                                'WORLD_SAVE_TITLE_RECOVERY focus-pauses=1 keyboard-before-escape=6 resumed=1\n')
            value = source.replace(recovery, retry).replace('keyboard-before=4 keyboard-after=6', 'keyboard-before=6 keyboard-after=8')
            self.assertEqual(native_receipts(value, MONITOR)['checks'], 7)
            for old, new in (('attempt=1 focus-pauses=1', 'attempt=2 focus-pauses=1'), ('frames=2', 'frames=64'),
                             ('old-oz-keys=2', 'old-oz-keys=1'), ('keyboard-before-escape=6', 'keyboard-before-escape=5')):
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(value.replace(old, new), MONITOR)

        def test_timestamps_are_inside_this_process(self):
            source = self.transport()
            self.assertEqual(native_receipts(source, MONITOR, process_interval=(1000, 3000))['checks'], 7)
            for interval in ((1501, 3000), (1000, 2499), (0, 3000), (3000, 1000), (1000,), ('1000', 3000)):
                with self.subTest(interval=interval), self.assertRaises(RuntimeError): native_receipts(source, MONITOR, process_interval=interval)

        def test_all_three_snapshots_and_selected_entry_bind_retained_bytes(self):
            with tempfile.TemporaryDirectory(prefix='furnace-accounting-') as tmp:
                evidence = Path(tmp); source = self.files(evidence)
                result = native_receipts(source, MONITOR, evidence=evidence, process_interval=(1000, 3000))
                self.assertEqual(result['checks'], 7)
                paths = [evidence / 'entries/frame-0/protected'] + [evidence / 'snapshots' / name / 'protected'
                          for name in ('furnace-saved', 'furnace-open-stopped', 'furnace-open-prepared')]
                for path in paths:
                    old = path.read_bytes(); path.write_bytes(old + b'x')
                    with self.subTest(path=path), self.assertRaises(RuntimeError): native_receipts(source, MONITOR, evidence=evidence)
                    path.write_bytes(old)
                (evidence / 'entries/frame-1').mkdir()
                with self.assertRaises(RuntimeError): native_receipts(source, MONITOR, evidence=evidence)

        def test_native_prepare_matching_sizes_still_requires_identical_bytes(self):
            with tempfile.TemporaryDirectory(prefix='furnace-prepare-bytes-') as tmp:
                evidence = Path(tmp); source = self.files(evidence)
                path = evidence / 'snapshots/furnace-open-prepared/protected'; raw = path.read_bytes(); path.write_bytes(b'X' + raw[1:])
                with self.assertRaises(RuntimeError): native_receipts(source, MONITOR, evidence=evidence)

        def test_monitor_window_host_geometry_and_uint32_corruptions(self):
            source = self.transport()
            for old, new in (('x=-1387', 'x=0'), ('target=-1920,0,1920,1040', 'target=0,0,1920,1040'),
                             ('frame-id=3', 'frame-id=4294967296'), ('canvas-id=4', 'canvas-id=3'),
                             ('borrowed-window=1', 'borrowed-window=0'), ('bound-app=1', 'bound-app=0'),
                             ('stage=after-furnace-reload frame-id=3', 'stage=after-furnace-reload frame-id=5')):
                with self.subTest(field=old), self.assertRaises(RuntimeError): native_receipts(source.replace(old, new, 1), MONITOR)

        def test_png_crc_framing_pixels_and_dimensions(self):
            with tempfile.TemporaryDirectory(prefix='furnace-png-') as tmp:
                root = Path(tmp)
                for name in IMAGES: (root / name).write_bytes(C.png())
                self.assertEqual(set(image_gate(root, 1)), IMAGES)
                path = root / 'furnace-placed.png'; damaged = bytearray(C.png()); damaged[-1] ^= 1
                for raw in (b'not PNG', C.png()[:-1], C.png() + b'trailing', C.png(853), C.png(first_filter=5), bytes(damaged)):
                    path.write_bytes(raw)
                    with self.subTest(length=len(raw)), self.assertRaises(RuntimeError): image_gate(root, 1)

        def test_png_freshness_and_exact_stage_inventory(self):
            with tempfile.TemporaryDirectory(prefix='furnace-png-inventory-') as tmp:
                root = Path(tmp)
                for name in IMAGES:
                    path = root / name; path.write_bytes(C.png()); os.utime(path, ns=(20000000, 20000000))
                self.assertEqual(set(image_gate(root, 10)), IMAGES)
                path = root / 'furnace-placed.png'; os.utime(path, ns=(1000000, 1000000))
                with self.assertRaises(RuntimeError): image_gate(root, 10)
                os.utime(path, ns=(20000000, 20000000)); raw = path.read_bytes(); path.unlink()
                with self.assertRaises(RuntimeError): image_gate(root, 10)
                path.write_bytes(raw); (root / 'furnace-unknown.png').write_bytes(C.png())
                with self.assertRaises(RuntimeError): image_gate(root, 10)

        def test_independent_full_saved_bytes_and_source_transport_join(self):
            with tempfile.TemporaryDirectory(prefix='furnace-full-save-') as tmp:
                root = Path(tmp); source = self.files(root)
                actions = native_receipts(source, MONITOR, evidence=root, process_interval=(1000, 3000))['actions']
                self.assertEqual(saved_gate(D, root / 'snapshots/furnace-saved/alpha')['burn_remaining'], 1371)
                self.assertEqual(retained_save_gate(D, root, actions, (1000, 3000))['metadata_time'], 500)

        def test_player_nbt_slot_width_duplicate_and_leftover_input(self):
            mutations = (lambda p: p['Inventory'][1][1].append(self.item(0, 61, 1)),
                         lambda p: p['Inventory'][1][1].append(copy.deepcopy(p['Inventory'][1][1][0])),
                         lambda p: p['Inventory'][1][1][0]['Count'].__setitem__(0, 2),
                         lambda p: p['Inventory'][1][1][0]['id'].__setitem__(1, 15),
                         lambda p: p['Inventory'][1][1][0]['Slot'].__setitem__(1, 9))
            with tempfile.TemporaryDirectory(prefix='furnace-player-nbt-') as tmp:
                for index, mutate in enumerate(mutations):
                    metadata = self.metadata(True); mutate(metadata['Data'][1]['Player'][1]); self.write(Path(tmp), metadata=metadata)
                    with self.subTest(index=index), self.assertRaises(RuntimeError): saved_gate(D, Path(tmp))

        def test_furnace_nbt_counter_widths_slot_aliases_and_type(self):
            mutations = (lambda t: t['BurnTime'].__setitem__(0, 3), lambda t: t['BurnTime'].__setitem__(1, -1),
                         lambda t: t['CookTime'].__setitem__(1, 200), lambda t: t['id'].__setitem__(1, 'Chest'),
                         lambda t: t['x'].__setitem__(0, 2), lambda t: t['Items'][1][1].append(copy.deepcopy(t['Items'][1][1][0])),
                         lambda t: t['Items'][1][1][0]['Count'].__setitem__(1, 2), lambda t: t.update(FuelDuration=[3, 1600]))
            with tempfile.TemporaryDirectory(prefix='furnace-tile-nbt-') as tmp:
                for index, mutate in enumerate(mutations):
                    chunk = self.chunk(True); mutate(chunk['Level'][1]['TileEntities'][1][1][0]); self.write(Path(tmp), chunk=chunk)
                    with self.subTest(index=index), self.assertRaises(RuntimeError): saved_gate(D, Path(tmp))

        def test_full_metadata_seed_types_and_finite_player_lists(self):
            mutations = (lambda d: d.pop('RandomSeed'), lambda d: d['RandomSeed'].__setitem__(1, 124),
                         lambda d: d['RandomSeed'].__setitem__(0, 3), lambda d: d.pop('rainTime'),
                         lambda d: d['Time'].__setitem__(0, 3), lambda d: d['Player'][1].pop('Motion'),
                         lambda d: d['Player'][1]['Pos'][1][1].pop(),
                         lambda d: d['Player'][1]['Motion'][1][1].__setitem__(0, struct.unpack('>Q', struct.pack('>d', float('nan')))[0]))
            with tempfile.TemporaryDirectory(prefix='furnace-metadata-nbt-') as tmp:
                for index, mutate in enumerate(mutations):
                    metadata = self.metadata(True); mutate(metadata['Data'][1]); self.write(Path(tmp), metadata=metadata)
                    with self.subTest(index=index), self.assertRaises(RuntimeError): saved_gate(D, Path(tmp))

        def test_partial_and_trailing_compressed_metadata_are_rejected(self):
            with tempfile.TemporaryDirectory(prefix='furnace-metadata-wire-') as tmp:
                root = Path(tmp); self.write(root); path = root / 'level.dat'; original = path.read_bytes()
                for raw in (original[:-1], original + b'garbage', original + gzip.compress(b'\x0a\0\0\0'), b'X' * (D.BOUND + 1)):
                    path.write_bytes(raw)
                    with self.subTest(length=len(raw)), self.assertRaises((RuntimeError, AssertionError)): saved_gate(D, root)

        def test_region_complete_disjoint_allocations_and_record_bounds(self):
            with tempfile.TemporaryDirectory(prefix='furnace-region-') as tmp:
                root = Path(tmp); self.write(root); path = root / 'region/r.0.0.mcr'; original = path.read_bytes()
                variants = []
                for value in (0, 1, 257, 512, 769):
                    raw = bytearray(original); struct.pack_into('>I', raw, 0, value); variants.append(bytes(raw))
                raw = bytearray(original); raw[4:8] = raw[:4]; variants.extend((bytes(raw), original[:-1], original + b'X'))
                for size in (0, 1, 4093, 1000000):
                    raw = bytearray(original); struct.pack_into('>I', raw, 8192, size); variants.append(bytes(raw))
                for index, raw in enumerate(variants):
                    path.write_bytes(raw)
                    with self.subTest(index=index), self.assertRaises((RuntimeError, AssertionError, zlib.error)): saved_gate(D, root)

        def test_region_arrays_and_chunk_coordinates_are_independent(self):
            mutations = (lambda l: l['xPos'].__setitem__(1, 1), lambda l: l['Data'].__setitem__(1, bytes(16383)),
                         lambda l: l['Blocks'].__setitem__(1, bytes(32767)), lambda l: l.pop('HeightMap'),
                         lambda l: l['TileEntities'][1][1].append(copy.deepcopy(l['TileEntities'][1][1][0])))
            with tempfile.TemporaryDirectory(prefix='furnace-chunk-nbt-') as tmp:
                for index, mutate in enumerate(mutations):
                    chunk = self.chunk(True); mutate(chunk['Level'][1]); self.write(Path(tmp), chunk=chunk)
                    with self.subTest(index=index), self.assertRaises(RuntimeError): saved_gate(D, Path(tmp))

        def test_source_save_backup_protected_file_and_session_bytes(self):
            with tempfile.TemporaryDirectory(prefix='furnace-save-bytes-') as tmp:
                root = Path(tmp); source = self.files(root); actions = native_receipts(source, MONITOR)['actions']
                for path in (root / 'snapshots/furnace-saved/protected', root / 'snapshots/furnace-saved/alpha/session.lock',
                             root / 'snapshots/furnace-saved/alpha/level.dat_old'):
                    raw = path.read_bytes(); path.write_bytes(bytes([raw[0] ^ 1]) + raw[1:])
                    with self.subTest(path=path), self.assertRaises((RuntimeError, AssertionError)):
                        retained_save_gate(D, root, actions, (1000, 3000))
                    path.write_bytes(raw)

        def test_saved_metadata_clock_time_size_and_chunk_stamp_join(self):
            with tempfile.TemporaryDirectory(prefix='furnace-save-clock-') as tmp:
                root = Path(tmp); source = self.files(root); actions = native_receipts(source, MONITOR)['actions']
                for key, value in (('Time', 501), ('SizeOnDisk', 3145729), ('LastPlayed', 2501)):
                    metadata = self.metadata(True); metadata['Data'][1][key][1] = value; self.write(root / 'snapshots/furnace-saved/alpha', metadata=metadata)
                    with self.subTest(key=key), self.assertRaises(RuntimeError): retained_save_gate(D, root, actions, (1000, 3000))
                self.write(root / 'snapshots/furnace-saved/alpha')
                path = root / 'snapshots/furnace-saved/alpha/region/r.0.0.mcr'; raw = bytearray(path.read_bytes()); struct.pack_into('>i', raw, 4096, 3); path.write_bytes(raw)
                with self.assertRaises(RuntimeError): retained_save_gate(D, root, actions, (1000, 3000))

        def test_initial_inventory_and_partial_authored_seed_are_not_accepted(self):
            with tempfile.TemporaryDirectory(prefix='furnace-initial-nbt-') as tmp:
                root = Path(tmp); source = self.files(root); actions = native_receipts(source, MONITOR)['actions']
                for missing in (False, True):
                    metadata = self.metadata(False)
                    if missing: metadata['Data'][1].pop('RandomSeed')
                    else: metadata['Data'][1]['Player'][1]['Inventory'][1][1][0]['Count'][1] = 2
                    self.write(root / 'before/alpha', saved=False, metadata=metadata)
                    with self.subTest(missing_seed=missing), self.assertRaises(RuntimeError): retained_save_gate(D, root, actions, (1000, 3000))

        def test_forged_partial_entry_cannot_be_laundered_through_backup(self):
            with tempfile.TemporaryDirectory(prefix='furnace-partial-entry-') as tmp:
                root = Path(tmp); source = self.files(root); actions = native_receipts(source, MONITOR)['actions']
                metadata = self.metadata(False); metadata['Data'][1].pop('RandomSeed')
                entry = root / 'entries/frame-0/alpha'; self.write(entry, saved=False, metadata=metadata)
                (root / 'snapshots/furnace-saved/alpha/level.dat_old').write_bytes((entry / 'level.dat').read_bytes())
                with self.assertRaises(RuntimeError): retained_save_gate(D, root, actions, (1000, 3000))

        def test_input_hash_inventory_additions_deletions_dlls_and_unreadable_values(self):
            good, assets = {'source.c': 'a' * 64, 'runtime.dll': 'b' * 64}, {'asset.png': 'c' * 64}
            self.assertTrue(continuity_gate(good, dict(good), assets, dict(assets)))
            for after in ({}, {'source.c': 'a' * 64}, dict(good, added='d' * 64), dict(good, **{'runtime.dll': 'd' * 64}),
                          dict(good, **{'source.c': {'error': 'OSError'}}), dict(good, **{'source.c': 'stale-not-hash'})):
                with self.subTest(after=after), self.assertRaises(RuntimeError): continuity_gate(good, after, assets, assets)
            with self.assertRaises(RuntimeError): continuity_gate(good, good, assets, {})

        def test_actual_input_enumeration_detects_new_source_and_changed_dll(self):
            with tempfile.TemporaryDirectory(prefix='furnace-source-inventory-') as tmp:
                root = Path(tmp)
                for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client', 'tests/tools/world'): (root / folder).mkdir(parents=True, exist_ok=True)
                for name in ('CMakeLists.txt', 'tests/tools/world/test_source_saving.py', 'client.exe', 'runtime.dll'): (root / name).write_bytes(b'actual decoder input')
                with C.patch.object(G, 'ROOT', root):
                    before = G.fingerprints(G.source_inputs(root / 'client.exe'))
                    (root / 'src/new.c').write_bytes(b'new input')
                    with self.assertRaises(RuntimeError): continuity_gate(before, G.fingerprints(G.source_inputs(root / 'client.exe')), {'asset': 'a' * 64}, {'asset': 'a' * 64})
                    (root / 'src/new.c').unlink(); (root / 'runtime.dll').write_bytes(b'changed DLL')
                    with self.assertRaises(RuntimeError): continuity_gate(before, G.fingerprints(G.source_inputs(root / 'client.exe')), {'asset': 'a' * 64}, {'asset': 'a' * 64})

    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(Controls))
    require(result.wasSuccessful(), 'Portable Furnace decoder controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.controls:
        self_test(); return
    require(os.name == 'nt' and args.executable is not None and args.assets is not None and args.artifacts is not None,
            'Native Windows executable/assets/artifacts required')
    world = module('furnace_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = module('furnace_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = module('furnace_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    decoder = module('furnace_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    monitor = module('furnace_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='furnace-', dir=args.artifacts.resolve()))
    for name in ('snapshots', 'entries'): (evidence / name).mkdir()
    before, assets_before = G.fingerprints(G.source_inputs(executable)), world.snapshot(assets)
    require(all(type(pin) is str for pin in before.values()), 'Readable current source/helpers/product/DLL inputs')
    report = {'status': 'FAIL', 'monitor': monitor, 'inputs_before': before,
              'scope': 'Authored placed Furnace CK/SK/BM/VQ/OV SDK, actual World ticks, save/reselection and open-OV Frame shutdown; no original pixels, arbitrary Worlds or FPS claim.',
              'achievement_input': 'Held Furnace is authored saved inventory with no crafting achievement history; missing EP4 presence makes actual DC35 skip EP5, expected values0/0.'}
    (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    try:
        with tempfile.TemporaryDirectory(prefix='furnace-appdata-', dir=evidence) as temporary:
            home = Path(temporary); game = home / '.minecraft'; game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            (evidence / 'options-input.txt').write_bytes((game / 'options.txt').read_bytes())
            saves = game / 'saves'
            for folder, name, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000), ('alpha', 'Furnace world', 1304000000000)):
                directory = saves / folder; directory.mkdir(parents=True)
                wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                if folder == 'alpha':
                    old = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
                    new = world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 3) +
                                    b''.join(G.stack(world, *row) for row in ((0, 61, 1), (9, 15, 2), (10, 263, 2))))
                    require(wire.count(old) == 1, 'One authored held Furnace/ore/fuel Inventory')
                    wire = wire.replace(old, new)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Furnace control')
            region = saves / 'alpha/region'; region.mkdir()
            G.regions(world, region, 'placement')  # Shared authored empty flat floor; no Chest or Furnace Tile.
            unrelated = region / 'r.9.9.mcr'; unrelated.write_bytes(bytes(8192))
            before_files = world.save_title_files(saves, evidence / 'before')
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds, MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds,
                               MC_WORLD_RELOAD_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'), MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first_millis = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence)], environment, evidence, saves, before_files,
                                                'furnace-cycle', first_millis, 600)
            print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native Furnace SDK loop and checked shutdown returned')
            interval = (first_millis, time.time_ns() // 1000000)
            report['native'] = native_receipts(result.stdout, monitor, result.stderr, process_interval=interval, evidence=evidence)
            report['source_save'] = retained_save_gate(decoder, evidence, report['native']['actions'], interval)
            report['images'] = image_gate(evidence, first_millis)
            report['saved'] = saved_gate(decoder, evidence / 'snapshots/furnace-saved/alpha')
            stopped, prepared = (evidence / 'snapshots' / name for name in ('furnace-open-stopped', 'furnace-open-prepared'))
            require(G.snapshot_files(stopped) == G.snapshot_files(prepared), 'Actual Frame-stop/native-prepare saved bytes unchanged')
            report['terminal_saved_bytes_unchanged'] = True
            require(unrelated.read_bytes() == bytes(8192), 'Unrelated region preserved')
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = G.fingerprints(G.source_inputs(executable))
        assets_after = world.snapshot(assets)
        report['assets_unchanged'] = assets_after == assets_before
        try:
            continuity_gate(before, report['inputs_after'], assets_before, assets_after)
        except RuntimeError as error:
            report['status'] = 'FAIL'
            report['continuity_error'] = str(error)
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('Furnace artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Furnace source/assets continuity throughout actual process')
    print('PASS: Furnace SDK/save/reload/open shutdown')


if __name__ == '__main__':
    main()
