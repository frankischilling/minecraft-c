"""Loaded unpowered Dispenser SDK activation/slot transfer/close/save/reselection.

Run --self-test for offline reader controls. The Windows SDK mode needs the
root-owned mc_test_dispenser product, supplied assets and physical monitor 2.
It supplies authored SDL records, not physical mouse/keyboard or original pixels.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time
import types
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/tools/client'))
import test_chest_loop as G

require = G.require
U = r'(0|[1-9][0-9]{0,19})'
S = r'(-?(?:0|[1-9][0-9]{0,19}))'
IMAGES = frozenset(('dispenser-open.png', 'dispenser-transferred.png',
                    'dispenser-title.png', 'dispenser-reloaded.png'))


def write(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True) + '\n', encoding='utf8', newline='\n')


def inputs(executable):
    result = {ROOT / 'CMakeLists.txt', Path(__file__).resolve(), executable, Path(sys.executable).resolve()}
    for name in ('src', 'include', 'tests', 'scripts', 'cmake'):
        result.update(path for path in (ROOT / name).rglob('*') if path.is_file())
    result.update(executable.parent.glob('*.dll'))
    return result


def fingerprints(paths):
    result = {}
    for path in sorted(paths):
        require(path.is_file() and not path.is_symlink() and
                not (getattr(path.lstat(), 'st_file_attributes', 0) & 0x400), 'Ordinary declared input')
        result[str(path)] = {'bytes': path.stat().st_size, 'sha256': G.digest(path)}
    return result


def planes(loaded):
    blocks, light, metadata = bytearray(32768), bytearray(16384), bytearray(16384)
    for bx in range(16):
        for bz in range(16):
            at = (bx << 11) | (bz << 7)
            blocks[at] = 7
            blocks[at + 1:at + 64] = bytes([3] * 63)
            light[(at + 64) // 2:(at + 128) // 2] = bytes([255] * 32)
    if loaded:
        at = (2 << 7) | 64
        blocks[at], metadata[at // 2] = 23, 3
    return bytes(blocks), bytes(light), bytes(metadata)


def regions(world, directory):
    """Authored saved blocks/AZ only; no runtime setter, placement or neighbor callback."""
    output = {}
    for x in range(-8, 9):
        for z in range(-8, 9):
            blocks, light, metadata = planes((x, z) == (0, 0))
            tiles = []
            if (x, z) == (0, 0):
                tile = world.tag(8, 'id', world.string('Trap'))
                for key, value in (('x', 0), ('y', 64), ('z', 2)):
                    tile += world.tag(3, key, struct.pack('>i', value))
                tile += world.tag(9, 'Items', b'\x0a' + struct.pack('>i', 2) +
                                  G.stack(world, 0, 264, 5) + G.stack(world, 8, 266, 3))
                tiles.append(tile + b'\0')
            level = world.tag(3, 'xPos', struct.pack('>i', x)) + world.tag(3, 'zPos', struct.pack('>i', z))
            level += world.tag(4, 'LastUpdate', bytes(8))
            for key, value in (('Blocks', blocks), ('Data', metadata), ('SkyLight', light),
                               ('BlockLight', bytes(16384)), ('HeightMap', bytes([64] * 256))):
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


def saved_semantics(level, player, moved):
    require(type(moved) is bool and type(level) is dict and type(player) is dict,
            'Actual decoded selected Chunk and Player compounds')
    coordinates = [G.typed(level.get(key), 3, 'Chunk coordinate Int width') for key in ('xPos', 'zPos')]
    require(all(type(value) is int for value in coordinates) and coordinates == [0, 0], 'Selected Chunk coordinates')
    blocks = G.typed(level.get('Blocks'), 7, 'Complete block plane')
    metadata = G.typed(level.get('Data'), 7, 'Complete metadata plane')
    require(type(blocks) is bytes and len(blocks) == 32768 and
            type(metadata) is bytes and len(metadata) == 16384, 'Complete saved planes')
    expected_blocks, _, expected_metadata = planes(True)
    require(blocks == expected_blocks and metadata == expected_metadata,
            'Complete selected Blocks/Data exactly retain authored flat prestate and block23; no placement or power source')
    value = G.typed(level.get('TileEntities'), 9, 'Actual Tile list')
    require(type(value) is list and len(value) == 2 and value[0] == 10 and
            type(value[0]) is int and type(value[1]) is list and len(value[1]) == 1, 'One actual AZ compound')
    tile = value[1][0]
    require(type(tile) is dict and set(tile) == {'id', 'x', 'y', 'z', 'Items'}, 'Closed actual AZ fields')
    coordinates = [G.typed(tile[key], 3, 'AZ coordinate Int width') for key in ('x', 'y', 'z')]
    require(G.typed(tile['id'], 8, 'AZ id String width') == 'Trap' and
            all(type(value) is int for value in coordinates) and coordinates == [0, 64, 2],
            'Exact actual AZ id/coordinates/widths')
    slots = G.slot_rows(tile['Items'])
    expected = [(8, 266, 3, 0)] if moved else [(0, 264, 5, 0), (8, 266, 3, 0)]
    require(slots == expected, 'Exact AZ nine-cell persistence after actual slot clicks')
    inventory = G.slot_rows(player['Inventory'])
    dimension = G.typed(player['Dimension'], 3, 'Player Dimension Int width')
    require(inventory == ([(9, 264, 5, 0)] if moved else []) and type(dimension) is int and dimension == 0,
            'Exact separate Player Inventory persistence')
    entities = G.typed(level.get('Entities'), 9, 'Actual saved Entity list')
    require(type(entities) is list and len(entities) == 2 and type(entities[0]) is int and
            type(entities[1]) is list and not entities[1], 'No unexpected Entity in this finite selected Chunk')
    return {'tile_slots': slots, 'player_slots': inventory, 'block': 23, 'metadata': 3,
            'scope': 'Loaded authored metadata retained; no orientation, power delivery or firing body credited.'}


def saved_gate(nbt, mining, directory, moved):
    level = mining.selected_chunk(nbt, directory)
    metadata = nbt.inflate(G.bounded(directory / 'level.dat', nbt.BOUND), 31)
    player = G.compound(G.compound(metadata['Data'], 'Saved Data')['Player'], 'Saved Player')
    return {**saved_semantics(level, player, moved),
            'region_sha256': G.digest(directory / 'region/r.0.0.mcr'),
            'metadata_sha256': G.digest(directory / 'level.dat')}


def save_protocol(rows, evidence, interval):
    at = 0

    def take(pattern):
        nonlocal at
        row = re.fullmatch(pattern, rows[at]) if at < len(rows) else None
        require(row is not None, 'Complete ordered actual save/copy protocol')
        at += 1
        values = tuple(map(int, row.groups()))
        require(all(-(1 << 63) <= value < 1 << 64 for value in values), 'Bounded save integers')
        return values

    recovery = take(r'WORLD_SAVE_TITLE_RECOVERY focus-pauses=' + U + r' keyboard-before-escape=' + U + r' resumed=1')
    require(recovery[0] <= 4, 'Bounded source focus recovery')
    retries = 0
    while at < len(rows) and rows[at].startswith('WORLD_SAVE_TITLE_AUTOPAUSE'):
        pause = take(r'WORLD_SAVE_TITLE_AUTOPAUSE attempt=' + U + r' focus-pauses=' + U +
                     r' old-oz-keys=2 real-id4=1 resumed=1 frames=' + U)
        retries += 1
        require(pause[0] == retries <= 3 and pause[1] == recovery[0] + 1 <= 4 and 1 <= pause[2] < 64,
                'Contiguous bounded actual pause recovery')
        following = take(r'WORLD_SAVE_TITLE_RECOVERY focus-pauses=' + U + r' keyboard-before-escape=' + U + r' resumed=1')
        require(following == (pause[1], recovery[1] + 2), 'Recovery keyboard continuity')
        recovery = following
    escape = take(r'WORLD_SAVE_TITLE_ESCAPE keyboard-before=' + U + r' keyboard-after=' + U + r' fresh-pause=1 distinct-recovered=1')
    require(escape[0] == recovery[1] and escape[1] >= escape[0] + 2, 'SDK Escape after recovery')
    entry = take(r'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=' + U + r' eligible=' + U + r' time=' + S +
                 r' size-before=' + S + r' size-after=' + S + r' entered=1 bytes=' + U + r' copied-before-frame=1')
    saved = take(r'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 '
                 r'old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=' + U + r' time=' + S +
                 r' size-before=' + S + r' size-after=' + S + r' first=' + S + r' last=' + S +
                 r' entry-frame=' + U + r' entry-bytes=' + U)
    frame, eligible, world_time, before, after, entry_bytes = entry
    require(0 <= frame < 64 and 0 < eligible <= 1024 and 0 <= world_time < 1 << 63 and
            0 <= before <= after < 1 << 63 and 0 < entry_bytes <= 16 * 1048576 and
            saved[:4] == entry[1:5] and saved[6:] == (frame, entry_bytes) and
            0 < saved[4] <= saved[5] < 1 << 63, 'Actual entry/continuation fields agree')
    if interval is not None:
        require(interval[0] <= saved[4] <= saved[5] <= interval[1], 'Actual process/save clock join')
    snapshot = take(r'WORLD_RELOAD_SNAPSHOT phase=dispenser-saved files=' + U + r' bytes=' + U + r' actual-file-copy=1')
    require(0 < snapshot[0] <= 1024 and 0 < snapshot[1] <= 16 * 1048576 and at == len(rows),
            'Unique complete actual save/copy protocol')
    if evidence is not None:
        entries = evidence / 'entries'
        require(entries.is_dir() and not entries.is_symlink() and
                {p.name for p in entries.iterdir()} == {f'frame-{i}' for i in range(frame + 1)},
                'Exact retained pre-save candidate inventory')
        for i in range(frame + 1):
            inventory = G.snapshot_files(entries / f'frame-{i}')
            if i == frame:
                require(sum(v['bytes'] for v in inventory.values()) == entry_bytes, 'Actual selected entry byte count')
        inventory = G.snapshot_files(evidence / 'snapshots/dispenser-saved')
        require((len(inventory), sum(v['bytes'] for v in inventory.values())) == snapshot,
                'Independent actual saved copy count/bytes')
    return {'entry_frame': frame, 'snapshot': snapshot, 'world_time': world_time}


def receipts(stdout, stderr, monitor, *, evidence=None, interval=None):
    require(type(stdout) is str and stdout.endswith('\n') and '\r' not in stdout and stderr == '',
            'Complete clean decoded process streams; physical raw bytes retained separately')
    rows, at = stdout.splitlines(), 2
    require(rows[:2] == ['16 achievements', '151 recipes'], 'One exact startup banner pair')

    def exact(value):
        nonlocal at
        require(at < len(rows) and rows[at] == value, 'Ordered exact Dispenser receipt: ' + value)
        at += 1

    def match(pattern):
        nonlocal at
        row = re.fullmatch(pattern, rows[at]) if at < len(rows) else None
        require(row is not None, 'Complete typed Dispenser receipt')
        at += 1
        return row

    target = monitor['selected']['work']
    require(type(target) in (list, tuple) and len(target) == 4 and all(type(v) is int for v in target) and
            target[2] >= 854 and target[3] >= 480, 'Actual physical monitor2 work rectangle')

    def host(stage):
        for window in ('frame', 'canvas'):
            row = match(r'WORLD_SELECTION_MONITOR stage=' + stage + ' window=' + window +
                        r' x=(-?[0-9]+) y=(-?[0-9]+) width=854 height=480 target=(\S+)')
            x, y = map(int, row.group(1, 2))
            require(row[3] == ','.join(map(str, target)) and target[0] <= x and target[1] <= y and
                    x + 854 <= target[0] + target[2] and y + 480 <= target[1] + target[3],
                    'Both actual windows contained on selected physical monitor2')
        row = match(r'WORLD_SAVE_TITLE_HOST stage=' + stage + r' frame-id=([1-9][0-9]{0,9}) '
                    r'canvas-id=([1-9][0-9]{0,9}) canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1')
        ids = tuple(map(int, row.groups()))
        require(ids[0] != ids[1] and max(ids) <= 4294967295, 'Distinct uint32 actual host window IDs')
        return ids

    first = host('open')
    exact('DISPENSER_READY epoch=0 distinct-world-player=0 ll-frame-returned=1 picked=0,64,2')
    exact('DISPENSER_OPEN epoch=0 sdk-right=1 real-gq-sh=1 slots=45 placed=0 cursor=0')
    exact('DISPENSER_CLICK slot=0 sdk-left=1 source-call-delta=1 cursor=1 az0=0 az8=3 player9=0')
    exact('DISPENSER_CLICK slot=9 sdk-left=1 source-call-delta=1 cursor=0 az0=0 az8=3 player9=5')
    exact('DISPENSER_CLOSE epoch=0 sdk-key=E inherited-close=1 default-container=1 cursor=0 player9=5')
    start = at
    while at < len(rows) and rows[at].startswith(('WORLD_SAVE_TITLE_RECOVERY ', 'WORLD_SAVE_TITLE_AUTOPAUSE ',
            'WORLD_SAVE_TITLE_ESCAPE ', 'WORLD_SAVE_TITLE_ENTRY ', 'WORLD_SAVE_TITLE mode=', 'WORLD_RELOAD_SNAPSHOT ')):
        at += 1
    saved = save_protocol(rows[start:at], evidence, interval)
    exact('DISPENSER_READY epoch=1 distinct-world-player=1 ll-frame-returned=1 picked=0,64,2')
    exact('DISPENSER_OPEN epoch=1 sdk-right=1 real-gq-sh=1 slots=45 placed=0 cursor=0')
    exact('DISPENSER_CLOSE epoch=1 sdk-key=E inherited-close=1 default-container=1 cursor=0 player9=5')
    require(host('after-dispenser') == first, 'Same actual Frame/Canvas through save/reselection')
    row = match(r'DISPENSER_BOUNDED opens=2 clicks=2 closes=2 epochs=2 frames=' + U + r' gui-frames=' + U +
                r' loaded-unpowered=1 field-repair=0 physical-input=0 placement-firing=0 fps-claim=0')
    frames, gui_frames = map(int, row.groups())
    require(4 <= gui_frames <= frames <= 65536, 'Bounded returning World/GUI frames, no FPS claim')
    exact('DISPENSER_STOP actual-frame-close=1 source-frame-retry=0')
    row = match(r'DISPENSER_FINISH cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9}) source-field-repair=0')
    require(int(row[1]) <= 4294967295 and at == len(rows), 'Checked final closure and no trailing receipts')
    return {'window_ids': first, 'frames': frames, 'gui_frames': gui_frames, 'checks': int(row[1]), 'save': saved,
            'route_scope': 'Actual runtime-selected World/DC/GQ/SH path inferred from pinned production and real owner/public outcomes; no direct callback-PC observer.'}


def capture_owned(transport, command, evidence, environment, execution, spawn_process=None):
    """Compose the unchanged bounded helper with genuine actual-handle custody.

    spawn_process is supplied only by offline synthetic transport controls. The
    actual SDK path always creates subprocess.Popen and never a callback host.
    """
    handles = []
    custody = {'pid': None, 'handles': 0, 'reaped': False, 'returncode': None,
               'cleanup_kill_attempted': False, 'cleanup_errors': []}
    execution['process_custody'] = custody
    selected_spawn = subprocess.Popen if spawn_process is None else spawn_process

    def spawn(*positional, **named):
        process = selected_spawn(*positional, **named)
        handles.append(process)
        custody['pid'], custody['handles'] = process.pid, len(handles)
        return process

    previous = transport.subprocess
    transport.subprocess = types.SimpleNamespace(Popen=spawn, DEVNULL=subprocess.DEVNULL,
                                                 PIPE=subprocess.PIPE, TimeoutExpired=subprocess.TimeoutExpired)
    result = None
    try:
        result = transport.capture(command, evidence, 'native', 600, environment, str(Path.cwd()))
        execution['terminal'] = result
        return result
    finally:
        transport.subprocess = previous
        for process in handles:
            try:
                if process.poll() is None:
                    custody['cleanup_kill_attempted'] = True
                    process.kill()
                custody['returncode'] = process.wait(timeout=5)
                custody['reaped'] = True
            except BaseException as error:
                custody['cleanup_errors'].append({'stage': 'owned-process', 'type': type(error).__name__, 'message': str(error)})
            if result is None:
                for stream in (process.stdout, process.stderr):
                    if stream is None:
                        continue
                    try:
                        stream.close()
                    except BaseException as error:
                        custody['cleanup_errors'].append({'stage': 'interrupted-pipe-close', 'type': type(error).__name__, 'message': str(error)})


def natural_custody(execution):
    c = execution['process_custody']
    require(c['handles'] == 1 and type(c['pid']) is int and c['pid'] > 0 and c['reaped'] is True and
            type(c['returncode']) is int and c['returncode'] == 0 and
            c['cleanup_kill_attempted'] is False and c['cleanup_errors'] == [], 'Natural actual owned-child closure')


def self_test():
    monitor = {'selected': {'work': [100, 200, 1000, 800]}}

    def host(stage):
        return [f'WORLD_SELECTION_MONITOR stage={stage} window={w} x=100 y=200 width=854 height=480 target=100,200,1000,800'
                for w in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']

    rows = ['16 achievements', '151 recipes'] + host('open') + [
        'DISPENSER_READY epoch=0 distinct-world-player=0 ll-frame-returned=1 picked=0,64,2',
        'DISPENSER_OPEN epoch=0 sdk-right=1 real-gq-sh=1 slots=45 placed=0 cursor=0',
        'DISPENSER_CLICK slot=0 sdk-left=1 source-call-delta=1 cursor=1 az0=0 az8=3 player9=0',
        'DISPENSER_CLICK slot=9 sdk-left=1 source-call-delta=1 cursor=0 az0=0 az8=3 player9=5',
        'DISPENSER_CLOSE epoch=0 sdk-key=E inherited-close=1 default-container=1 cursor=0 player9=5',
        'WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=2 resumed=1',
        'WORLD_SAVE_TITLE_ESCAPE keyboard-before=2 keyboard-after=4 fresh-pause=1 distinct-recovered=1',
        'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=84 time=5 size-before=100 size-after=200 entered=1 bytes=10 copied-before-frame=1',
        'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=84 time=5 size-before=100 size-after=200 first=1000 last=1001 entry-frame=0 entry-bytes=10',
        'WORLD_RELOAD_SNAPSHOT phase=dispenser-saved files=1 bytes=10 actual-file-copy=1',
        'DISPENSER_READY epoch=1 distinct-world-player=1 ll-frame-returned=1 picked=0,64,2',
        'DISPENSER_OPEN epoch=1 sdk-right=1 real-gq-sh=1 slots=45 placed=0 cursor=0',
        'DISPENSER_CLOSE epoch=1 sdk-key=E inherited-close=1 default-container=1 cursor=0 player9=5'] + host('after-dispenser') + [
        'DISPENSER_BOUNDED opens=2 clicks=2 closes=2 epochs=2 frames=12 gui-frames=4 loaded-unpowered=1 field-repair=0 physical-input=0 placement-firing=0 fps-claim=0',
        'DISPENSER_STOP actual-frame-close=1 source-frame-retry=0',
        'DISPENSER_FINISH cleanup=1 windows-absent=1 checks=100 source-field-repair=0']
    valid = '\n'.join(rows) + '\n'
    receipts(valid, '', monitor, interval=(999, 1002))
    negatives = []
    # Every missing or duplicate lifecycle/action row must fail, including save
    # clocks/ownership/closure, rather than accepting just summary markers.
    for i in range(2, len(rows)):
        negatives.append('\n'.join(rows[:i] + rows[i + 1:]) + '\n')
        negatives.append('\n'.join(rows[:i] + [rows[i]] + rows[i:]) + '\n')
    negatives += [valid + 'extra\n', valid.replace('slots=45', 'slots=44'),
                  valid.replace('source-call-delta=1', 'source-call-delta=0'),
                  valid.replace('player9=5', 'player9=4'), valid.replace('width=854', 'width=855'),
                  valid.replace('x=100 ', 'x=99 '), valid.replace('frame-id=3', 'frame-id=4'),
                  valid.replace('first=1000 last=1001', 'first=1002 last=1000'),
                  valid.replace('entry-frame=0 entry-bytes=10', 'entry-frame=1 entry-bytes=10'),
                  valid.replace('windows-absent=1', 'windows-absent=0'), valid[:-1]]
    for value in negatives:
        try:
            receipts(value, '', monitor, interval=(999, 1002))
        except (RuntimeError, ValueError, KeyError, TypeError):
            pass
        else:
            raise RuntimeError('Corrupted authored receipt was admitted')
    try:
        receipts(valid, 'error', monitor)
    except RuntimeError:
        pass
    else:
        raise RuntimeError('Actual stderr was admitted')
    import copy
    blocks, _, metadata = planes(True)
    stack = {'Slot': [1, 8], 'id': [2, 266], 'Count': [1, 3], 'Damage': [2, 0]}
    tile = {'id': [8, 'Trap'], 'x': [3, 0], 'y': [3, 64], 'z': [3, 2], 'Items': [9, [10, [stack]]]}
    level = {'xPos': [3, 0], 'zPos': [3, 0], 'Blocks': [7, blocks], 'Data': [7, metadata],
             'TileEntities': [9, [10, [tile]]], 'Entities': [9, [10, []]]}
    player = {'Dimension': [3, 0], 'Inventory': [9, [10, [
        {'Slot': [1, 9], 'id': [2, 264], 'Count': [1, 5], 'Damage': [2, 0]}]]]}
    saved_semantics(level, player, True)
    bad_saves = []
    for key, value in (('id', [8, 'Chest']), ('x', [3, False]), ('y', [2, 64]),
                       ('Items', [9, [10, [dict(stack, Slot=[1, 9])]]])):
        changed = copy.deepcopy(level)
        changed['TileEntities'][1][1][0][key] = value
        bad_saves.append((changed, player))
    changed = copy.deepcopy(level)
    changed['TileEntities'][1][1][0]['extra'] = [3, 0]
    bad_saves.append((changed, player))
    changed = copy.deepcopy(level)
    changed['TileEntities'][1][1].append(copy.deepcopy(tile))
    bad_saves.append((changed, player))
    for key, value in (('Slot', [1, 8]), ('Count', [1, 4]), ('Damage', [2, 1])):
        changed = copy.deepcopy(player)
        changed['Inventory'][1][1][0][key] = value
        bad_saves.append((level, changed))
    changed = copy.deepcopy(player)
    changed['Dimension'] = [3, False]
    bad_saves.append((level, changed))
    for key, offset, changed_byte in (('Blocks', 10000, 0), ('Data', 11000, 1)):
        changed = copy.deepcopy(level)
        raw = bytearray(changed[key][1])
        raw[offset] = changed_byte
        changed[key][1] = bytes(raw)
        bad_saves.append((changed, player))
    for value, inventory in bad_saves:
        try:
            saved_semantics(value, inventory, True)
        except (RuntimeError, ValueError, KeyError, TypeError):
            pass
        else:
            raise RuntimeError('Corrupted saved Tile/Player semantics were admitted')
    class SyntheticPipe:
        closed = False

        def close(self):
            self.closed = True

    class SyntheticProcess:
        pid = 7

        def __init__(self, returned=False):
            self.stdout, self.stderr = SyntheticPipe(), SyntheticPipe()
            self.returned, self.killed, self.waited = returned, False, False

        def poll(self):
            return 0 if self.returned else None

        def kill(self):
            self.killed, self.returned = True, True

        def wait(self, timeout):
            require(timeout == 5, 'Bounded synthetic custody wait')
            self.waited = True
            return -9 if self.killed else 0

    process = SyntheticProcess(True)
    transport = types.SimpleNamespace(subprocess=object(), capture=lambda *args: {'exit': 0})

    def returned(*args):
        transport.subprocess.Popen()
        return {'exit': 0}

    transport.capture = returned
    execution = {}
    original = transport.subprocess
    capture_owned(transport, [], Path('.'), {}, execution, lambda: process)
    natural_custody(execution)
    require(transport.subprocess is original and process.waited and not process.killed,
            'Synthetic natural custody and module restoration')
    for error in (KeyboardInterrupt(), RuntimeError('wait-error'), OSError('thread-start-error')):
        process = SyntheticProcess()
        execution = {}

        def interrupted(*args):
            transport.subprocess.Popen()
            raise error

        transport.capture = interrupted
        try:
            capture_owned(transport, [], Path('.'), {}, execution, lambda: process)
        except BaseException as actual:
            require(actual is error and process.killed and process.waited and process.stdout.closed and
                    process.stderr.closed and transport.subprocess is original, 'Interrupted actual-handle cleanup control')
        else:
            raise RuntimeError('Interrupted transport became a returned capture')
        try:
            natural_custody(execution)
        except RuntimeError:
            pass
        else:
            raise RuntimeError('Cleanup-killed capture admitted as natural success')
    print(f'PASS: Dispenser offline reader controls 3 positive {len(negatives) + len(bad_saves) + 4} negative; no native execution')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.self_test:
        require(args.executable is None and args.assets is None and args.artifacts is None,
                'Offline controls do not admit SDK launch operands')
        self_test()
        return
    require(os.name == 'nt' and all(v is not None for v in (args.executable, args.assets, args.artifacts)),
            'Windows SDK run requires explicit native product/assets/artifacts')
    world = G.module('dispenser_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = G.module('dispenser_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = G.module('dispenser_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    nbt = G.module('dispenser_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    transport = G.module('dispenser_transport', ROOT / 'tests/reference/client/controller/capture.py')
    product = G.module('dispenser_products', ROOT / 'tests/tools/client/native_product_capture.py')
    monitor = G.module('dispenser_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='loaded-unpowered-', dir=args.artifacts.resolve()))
    for name in ('snapshots', 'entries'):
        (evidence / name).mkdir()
    before, assets_before = fingerprints(inputs(executable)), world.snapshot(assets)
    report = {'status': 'FAIL', 'monitor': monitor, 'inputs_before': before, 'assets_before': assets_before,
              'scope': 'Finite loaded/unpowered authored block23/AZ; actual SDK/World/DC/GQ/SH slots, close and save/reselect. '
                       'No physical input, acquisition, placement, orientation, power/firing, sustained play, FPS or original pixel/native parity claim.',
              'capture_limit': 'Bounded existing single-child stdout/stderr transport; no independent descendant or loaded-library census.',
              'python': {'executable': sys.executable, 'version': sys.version}}
    write(evidence / 'run.json', report)
    try:
        with tempfile.TemporaryDirectory(prefix='dispenser-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000), ('alpha', 'Dispenser gameplay', 1304000000000)):
                directory = saves / folder
                directory.mkdir(parents=True)
                wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Dispenser control')
            region = saves / 'alpha/region'
            region.mkdir()
            regions(world, region)
            unrelated = region / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            before_files = world.save_title_files(saves, evidence / 'before')
            report['authored'] = saved_gate(nbt, mining, saves / 'alpha', False)
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
            require('after' in execution and 'after_snapshot_error' not in execution, 'Complete retained actual saves after child')
            raw_out, raw_err = (evidence / 'native.stdout').read_bytes(), (evidence / 'native.stderr').read_bytes()
            stdout, stderr = raw_out.decode('utf8').replace('\r\n', '\n'), raw_err.decode('utf8')
            print(stdout, end='')
            print(stderr, end='', file=sys.stderr)
            interval = (execution['process_first_millis'], execution['process_last_millis'])
            report['native'] = receipts(stdout, stderr, monitor, evidence=evidence, interval=interval)
            require({path.name for path in evidence.glob('dispenser-*.png')} == IMAGES, 'Exact fresh image inventory')
            report['images'] = {name: G.png_gate(evidence / name, interval[0]) for name in sorted(IMAGES)}
            report['save'] = saved_gate(nbt, mining, evidence / 'snapshots/dispenser-saved/alpha', True)
            report['after_save'] = saved_gate(nbt, mining, evidence / 'after/alpha', True)
            unaffected_before = {name: value for name, value in before_files.items()
                                 if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat') or name == 'alpha/region/r.9.9.mcr'}
            unaffected_after = {name: value for name, value in execution['after'].items()
                                if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat') or name == 'alpha/region/r.9.9.mcr'}
            require(unaffected_after == unaffected_before, 'Exact unrelated saves and control files preserved')
            require(unrelated.read_bytes() == bytes(8192), 'Authored unrelated region preserved')
        report['status'] = 'PASS'
    except BaseException as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        try:
            report['inputs_after'] = fingerprints(inputs(executable))
            report['assets_after'] = world.snapshot(assets)
            report['assets_unchanged'] = report['assets_after'] == assets_before
            if report['inputs_after'] != before or not report['assets_unchanged']:
                report['status'] = 'FAIL'
        except BaseException as error:
            report['status'] = 'FAIL'
            report['continuity_failure'] = {'type': type(error).__name__, 'message': str(error)}
        write(evidence / 'run.json', report)
        print('Dispenser artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Dispenser sources/assets/products or returned protocol changed')
    print('PASS: loaded unpowered Dispenser SDK save/reselection')


if __name__ == '__main__':
    main()
