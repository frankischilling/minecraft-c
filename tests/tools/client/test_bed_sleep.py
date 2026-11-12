"""One retained Windows SDK nighttime Bed scene, natural sleep, CQfalse and dawn."""
import argparse
import ctypes
from ctypes import wintypes
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_bed_placement as bed

require = bed.require


def metadata(world, display, played):
    raw = bed.metadata(world, display, played)
    old = world.tag(4, 'Time', struct.pack('>q', 0))
    require(raw.count(old) == 1, 'one authored initial World Time')
    return raw.replace(old, world.tag(4, 'Time', struct.pack('>q', 13000)))


def glowstone_arrays(supports=False):
    arrays = bed.grass_arrays()
    blocks, light = bytearray(arrays['Blocks']), bytearray(16384)
    for x in range(16):
        for z in range(16):
            base = (x << 11) | (z << 7)
            blocks[base + 63] = 89
            for y in range(63, 78):
                index = base + y
                light[index >> 1] |= (78 - y) << ((index & 1) * 4)
    if supports:
        for z in (2, 3):
            blocks[(z << 7) | 63] = 2
            for y in range(63, 78):
                index = (z << 7) | y
                shift = (index & 1) * 4
                value = 0 if y == 63 else max(0, 77 - y)
                light[index >> 1] = (light[index >> 1] & ~(15 << shift)) | (value << shift)
    arrays.update(Blocks=bytes(blocks), BlockLight=bytes(light))
    return arrays


def input_check(nbt, helper, level_path, directory):
    data = nbt.inflate(nbt.read(level_path), 31)['Data'][1]
    bed.player_fields(data['Player'][1], 2)
    require(data['Time'] == [4, 13000], 'night authored only before bootstrap')
    records = helper.region_records(directory)
    require(set(records) == {(x, z) for x in range(-8, 9) for z in range(-8, 9)},
            '289 pre-bootstrap stored Chunks')
    normal, supports = glowstone_arrays(), glowstone_arrays(True)
    for (x, z), record in records.items():
        level = nbt.Nbt(helper.record_raw(record)).root()['Level'][1]
        expected = supports if (x, z) == (0, 0) else normal
        require(level['xPos'] == [3, x] and level['zPos'] == [3, z] and
                level['TerrainPopulated'] == [1, 1] and level['Entities'] == [9, [10, []]] and
                level['TileEntities'] == [9, [10, []]] and
                all(level[name] == [7, value] for name, value in expected.items()),
                'exact supported Glowstone floor and complete naturally sustained light planes')
    return dict(chunks=289, floor_y=63, floor_block=89, bed_supports=[[0, 63, 2, 2], [0, 63, 3, 2]],
                time=13000, difficulty=2, sleeping=False, timer=0, count=2,
                normal_plane_sha256={name: hashlib.sha256(value).hexdigest() for name, value in normal.items()},
                support_plane_sha256={name: hashlib.sha256(value).hexdigest() for name, value in supports.items()})


def second_monitor():
    """Read physical topology; an SDL startup hint places this process's windows."""
    require(os.name == 'nt', 'native Windows monitor enumeration')

    class MonitorInfo(ctypes.Structure):
        _fields_ = [('size', wintypes.DWORD), ('monitor', wintypes.RECT),
                    ('work', wintypes.RECT), ('flags', wintypes.DWORD),
                    ('device', wintypes.WCHAR * 32)]

    records = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HANDLE, wintypes.HDC,
                                      ctypes.POINTER(wintypes.RECT), wintypes.LPARAM)
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.GetMonitorInfoW.argtypes = [wintypes.HANDLE, ctypes.POINTER(MonitorInfo)]
    user32.GetMonitorInfoW.restype = wintypes.BOOL

    @callback_type
    def collect(handle, dc, rect, data):
        info = MonitorInfo(); info.size = ctypes.sizeof(info)
        if not user32.GetMonitorInfoW(handle, ctypes.byref(info)):
            return False
        records.append(dict(device=info.device, primary=bool(info.flags & 1),
                            work=[info.work.left, info.work.top,
                                  info.work.right - info.work.left, info.work.bottom - info.work.top]))
        return True

    user32.EnumDisplayMonitors.argtypes = [wintypes.HDC, ctypes.c_void_p, callback_type, wintypes.LPARAM]
    user32.EnumDisplayMonitors.restype = wintypes.BOOL
    require(user32.EnumDisplayMonitors(None, None, collect, 0), 'read monitor topology')
    selected = [record for record in records if record['device'].endswith('DISPLAY2')]
    require(len(selected) == 1, 'physical monitor2 must be connected before opening any game window')
    require(selected[0]['work'][2] >= 900 and selected[0]['work'][3] >= 550,
            'monitor2 can contain both source windows')
    return dict(all=records, selected=selected[0])


def receipts(stdout, monitor):
    lines = stdout.splitlines()
    require(lines.count('BED_SLEEP_INPUTS chunks=289 glowstone-y=63 Bed=absent item=355 count=2 time=13000 pre-bootstrap-only=1') == 1,
            'one declared loaded nighttime input')
    bounds = ','.join(map(str, monitor['selected']['work']))
    for name in ('frame', 'canvas'):
        rows = [line for line in lines if line.startswith('BED_SLEEP_MONITOR window=' + name + ' ')]
        require(len(rows) == 1 and rows[0].endswith(' target=' + bounds),
                'actual native window geometry on monitor2: ' + name)
    entry = [line for line in lines if line.startswith('BED_SLEEP_ENTRY ')]
    require(len(entry) == 1 and re.fullmatch(
        r'BED_SLEEP_ENTRY SDK-right-tap=1 source-returned=1 sleeping=1 timer=([0-9]+) bed=0,64,3 saved-spawn=null difficulty=2 hostiles=1', entry[0]),
        'genuine nighttime SDK Bed activation')
    dawn = [line for line in lines if line.startswith('BED_SLEEP_DAWN ')]
    match = re.fullmatch(r'BED_SLEEP_DAWN input-to-dawn-world-ticks=([0-9]+) highest-observed=([0-9]+) timer-transitions=([0-9]+) overlays=([0-9]+) CQ-calls=1 CQ-returns=1 CQ-result=0 wake-calls=1 wake-returns=1 copied-spawn=0,64,3 health=20', dawn[0]) if len(dawn) == 1 else None
    require(match is not None and int(match[1]) >= 90 and int(match[2]) >= 90 and
            int(match[3]) >= 10 and int(match[4]) > 0,
            'completed input-to-dawn World ticks, natural timer progression and complete source dawn')
    for phase in ('before', 'sleeping', 'dawn'):
        require(any(line.startswith('BED_PLACEMENT_FRAME phase=' + phase + ' ') for line in lines),
                'returned actual World/HUD frame ' + phase)
    require(lines.count('BED_SLEEP_SAVE source-full-save=1 live-player=1 SDK-save-menu=0 timer=0 sleeping=0') == 1 and
            sum(re.fullmatch(r'BED_SLEEP_FINISH cleanup=1 windows-absent=1 checks=[1-9][0-9]*', line) is not None for line in lines) == 1,
            'source full save and checked terminal cleanup')
    require(not any(line.startswith(('BED_PLACEMENT_FAILURE', 'WORLD_NATIVE_FOCUS', 'WORLD_REPEAT_FOCUS_FAILURE')) for line in lines),
            'no repaired/failed focus or source opportunities')
    return dict(input_to_dawn_world_ticks=int(match[1]), highest_observed_timer=int(match[2]),
                timer_transitions=int(match[3]), overlays=int(match[4]), nightmare_result=False,
                source_wake_returns=1, monitor2=True, cleanup=True)


def readback(nbt, helper, saves, evidence):
    snapshots = {phase: nbt.Nbt(nbt.read(evidence / ('bed-player-' + phase + '.nbt'))).root()
                 for phase in ('sleeping', 'dawn', 'awake')}
    require(snapshots['sleeping']['Sleeping'] == [1, 1] and
            0 <= snapshots['sleeping']['SleepTimer'][1] < 100 and
            all(name not in snapshots['sleeping'] for name in ('SpawnX', 'SpawnY', 'SpawnZ')),
            'independent NBT actual sleeping prefix with no saved spawn')
    require(snapshots['dawn']['Sleeping'] == [1, 0] and 100 <= snapshots['dawn']['SleepTimer'][1] < 110,
            'independent NBT actual source wake fade')
    data = nbt.inflate(nbt.read(saves / 'alpha/level.dat'), 31)['Data'][1]
    player = data['Player'][1]
    require(24000 <= data['Time'][1] < 24500 and player['Sleeping'] == [1, 0] and
            player['SleepTimer'] == [2, 0] and player['Health'] == [2, 20],
            'saved natural dawn, complete fade, awake and healthy')
    for phase in ('dawn', 'awake'):
        for axis, value in zip('XYZ', (0, 64, 3)):
            require(snapshots[phase]['Spawn' + axis] == player['Spawn' + axis] == [3, value],
                    'saved copied Bed spawn ' + axis)
    bed.inventory(player, 1)
    bed.inventory(snapshots['awake'], 1)
    records = helper.region_records(saves / 'alpha/region')
    level = nbt.Nbt(helper.record_raw(records[0, 0])).root()['Level'][1]
    for z, metadata in ((2, 0), (3, 8)):
        index = (z << 7) | 64
        require(level['Blocks'][1][index] == 26 and level['Blocks'][1][index - 1] == 2 and
                ((level['Data'][1][index >> 1] >> ((index & 1) * 4)) & 15) == metadata,
                'saved actual Bed pair has occupancy cleared by source wake')
    return dict(time=data['Time'][1], sleeping=False, timer=0, health=20,
                saved_spawn=[0, 64, 3], bed_occupancy=False, count=1)


def self_test():
    world, nbt, helper = bed.modules()
    with tempfile.TemporaryDirectory(prefix='bed-sleep-input-') as name:
        root = Path(name); region = root / 'region'; region.mkdir()
        level = root / 'level.dat'
        level.write_bytes(gzip.compress(metadata(world, 'Bed sleep', 1304000000000), mtime=0))
        arrays = (glowstone_arrays(), glowstone_arrays(True))
        bed.author_regions(world, region, lambda x, z: arrays[int((x, z) == (0, 0))])
        require(input_check(nbt, helper, level, region)['chunks'] == 289, 'full authored input roundtrip')
        raw = metadata(world, 'Bed sleep', 1304000000000)
        raw = raw.replace(world.tag(4, 'Time', struct.pack('>q', 13000)), world.tag(4, 'Time', bytes(8)))
        level.write_bytes(gzip.compress(raw, mtime=0))
        try:
            input_check(nbt, helper, level, region)
        except ValueError:
            pass
        else:
            raise ValueError('daytime corruption accepted')
    monitor = dict(selected=dict(work=[100, 200, 1920, 1080]))
    lines = [f'BED_SLEEP_MONITOR window={name} x=100 y=200 width=854 height=480 target=100,200,1920,1080'
             for name in ('frame', 'canvas')]
    dawn = ('BED_SLEEP_DAWN input-to-dawn-world-ticks=100 highest-observed=99 timer-transitions=72 '
            'overlays=158 CQ-calls=1 CQ-returns=1 CQ-result=0 wake-calls=1 wake-returns=1 '
            'copied-spawn=0,64,3 health=20')
    lines += [
        'BED_SLEEP_INPUTS chunks=289 glowstone-y=63 Bed=absent item=355 count=2 time=13000 pre-bootstrap-only=1',
        'BED_SLEEP_ENTRY SDK-right-tap=1 source-returned=1 sleeping=1 timer=1 bed=0,64,3 saved-spawn=null difficulty=2 hostiles=1',
        *(f'BED_PLACEMENT_FRAME phase={phase} terrain=1 HUD=1 health=20 selected=0 item-slots=1 chat=0'
          for phase in ('before', 'sleeping', 'dawn')), dawn,
        'BED_SLEEP_SAVE source-full-save=1 live-player=1 SDK-save-menu=0 timer=0 sleeping=0',
        'BED_SLEEP_FINISH cleanup=1 windows-absent=1 checks=1']
    good = '\n'.join(lines)
    result = receipts(good, monitor)
    require(type(result['input_to_dawn_world_ticks']) is int and
            result['input_to_dawn_world_ticks'] == 100 and 'natural_ticks' not in result,
            'Precise typed input-to-dawn completed World-tick JSON field')
    require(receipts(good.replace('input-to-dawn-world-ticks=100',
                                  'input-to-dawn-world-ticks=90'), monitor)['input_to_dawn_world_ticks'] == 90,
            'Original90 completed World-tick threshold retained')
    refused = 0
    for bad in (
            good.replace('input-to-dawn-world-ticks=100', 'input-to-dawn-world-ticks=89'),
            good.replace('input-to-dawn-world-ticks=', 'natural-player-ticks='),
            good.replace('input-to-dawn-world-ticks=100', 'input-to-dawn-world-ticks=True'),
            good.replace('input-to-dawn-world-ticks=100', 'input-to-dawn-world-ticks=100.0'),
            good.replace('input-to-dawn-world-ticks=100', 'input-to-dawn-world-ticks="100"'),
            good.replace('highest-observed=99', 'highest-observed=89'),
            good.replace('timer-transitions=72', 'timer-transitions=9'),
            good.replace('overlays=158', 'overlays=0'),
            good.replace('CQ-result=0', 'CQ-result=1'),
            good.replace('wake-calls=1', 'wake-calls=0'),
            good.replace('copied-spawn=0,64,3', 'copied-spawn=0,64,2'),
            good.replace('health=20', 'health=19'),
            '\n'.join(lines[:-1]), good + '\nWORLD_NATIVE_FOCUS failed', good + '\n' + dawn):
        try:
            receipts(bad, monitor)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupt input-boundary/progression/dawn/cleanup receipt accepted')
    require(refused == 15, 'Every malformed synthetic dawn receipt refused')
    print('Bed sleep inputs:289 complete Glowstone Chunks, source night/Player/Stack and daytime refusal; '
          '15 input-to-dawn/progression/typed receipt controls refused; no native process')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    require(os.name == 'nt' and args.executable and args.assets and args.work_dir,
            'Windows scene requires executable/assets/work-dir')
    monitor = second_monitor()
    world, nbt, helper = bed.modules()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='bed-sleep-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'; game = appdata / '.minecraft'
    world.repeat_statistics(game, assets)
    (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
    saves = game / 'saves'; saves.mkdir()
    for folder, display, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000), ('alpha', 'Bed sleep', 1304000000000)):
        directory = saves / folder; directory.mkdir()
        raw = metadata(world, display, played) if folder == 'alpha' else world.metadata(display, played, 19132)
        (directory / 'level.dat').write_bytes(gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Bed sleep bytes\0\xff')
    region = saves / 'alpha/region'; region.mkdir()
    arrays = (glowstone_arrays(), glowstone_arrays(True))
    bed.author_regions(world, region, lambda x, z: arrays[int((x, z) == (0, 0))])
    inputs = input_check(nbt, helper, saves / 'alpha/level.dat', region)
    before_files = world.save_title_files(saves, evidence / 'before')
    source, resources = bed.fingerprints(bed.input_paths(executable)), world.snapshot(assets)
    env = os.environ.copy()
    env.update(APPDATA=str(appdata), LOCALAPPDATA=str(appdata), USERPROFILE=str(appdata), HOME=str(appdata),
               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=','.join(map(str, monitor['selected']['work'])))
    report = dict(status='FAIL', scope=__doc__, inputs=inputs, monitor=monitor,
                  source_product_before=source, assets_before=resources)
    try:
        result = world.retained_native_run([str(executable), str(assets), 'natural-night', str(evidence)],
            env, evidence, saves, before_files, 'bed-sleep-night', time.time_ns() // 1000000, bed.NATIVE_SECONDS)
        print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0 and result.stderr == '', 'actual native night scene failed; retain this opportunity')
        report['native'] = receipts(result.stdout, monitor)
        report['readback'] = readback(nbt, helper, saves, evidence)
        after = world.save_title_files(saves, evidence / 'after-verified')
        require(all(after.get(name) == value for name, value in before_files.items()
                    if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat')),
                'unrelated Worlds and files preserved')
        report['screenshots'] = {}
        for phase in ('before', 'sleeping', 'dawn'):
            path = evidence / ('bed-' + phase + '.png'); data = path.read_bytes()
            require(data.startswith(b'\x89PNG\r\n\x1a\n') and struct.unpack('>II', data[16:24]) == (854, 480),
                    'actual returned854x480 World/HUD frame ' + phase)
            report['screenshots'][path.name] = hashlib.sha256(data).hexdigest()
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = dict(type=type(error).__name__, message=str(error)); raise
    finally:
        report['source_product_after'] = bed.fingerprints(bed.input_paths(executable))
        report['assets_after'] = world.snapshot(assets)
        unchanged = source == report['source_product_after'] and resources == report['assets_after']
        if not unchanged:
            report['status'] = 'FAIL'
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        require(unchanged, 'source/product/imported resource hashes changed during native opportunity')
    print('Bed sleep nighttime scene:PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
