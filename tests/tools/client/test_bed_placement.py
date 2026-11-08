"""One Windows SDK Bed placement/daytime scene; direct source save, no reload/night claim."""
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
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
WORK_SECONDS = 210
CLEANUP_SECONDS = 20
NATIVE_SECONDS = 240
OUTER_SECONDS = 300


def require(value, message):
    if not value:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def modules():
    return (module('bed_placement_world', HERE / 'test_world_selection_loop.py'),
            module('bed_placement_nbt', HERE.parent / 'world/test_source_saving.py'),
            module('bed_placement_regions', HERE / 'test_bed_respawn.py'))


def metadata(world, display, played):
    raw = world.metadata(display, played, 19132, player_score=0)
    empty = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
    stack = (world.tag(1, 'Slot', b'\0') + world.tag(2, 'id', struct.pack('>h', 355)) +
             world.tag(1, 'Count', b'\2') + world.tag(2, 'Damage', bytes(2)) + b'\0')
    rotation = world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 0.))
    require(raw.count(empty) == raw.count(rotation) == 1 and raw.endswith(bytes(3)),
            'one complete inherited Player/Inventory/Rotation input')
    raw = raw.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + stack))
    raw = raw.replace(rotation, world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 41.)))
    player = (world.tag(1, 'OnGround', b'\1') + world.tag(1, 'Sleeping', b'\0') +
              world.tag(2, 'SleepTimer', bytes(2)))
    raw = raw[:-3] + player + raw[-3:]
    return raw[:-2] + world.tag(4, 'Time', struct.pack('>q', 0)) + raw[-2:]


def grass_arrays():
    blocks, sky = bytearray(32768), bytearray(16384)
    for x in range(16):
        for z in range(16):
            index = (x << 11) | (z << 7)
            blocks[index] = 7
            blocks[index + 1:index + 60] = bytes([1] * 59)
            blocks[index + 60:index + 63] = bytes([3] * 3)
            blocks[index + 63] = 2
            sky[(index + 64) // 2:(index + 128) // 2] = bytes([255] * 32)
    return dict(Blocks=bytes(blocks), Data=bytes(16384), SkyLight=bytes(sky),
                BlockLight=bytes(16384), HeightMap=bytes([64] * 256))


def author_regions(world, directory, arrays_for_chunk=None):
    arrays, regions = grass_arrays(), {}
    for x in range(-8, 9):
        for z in range(-8, 9):
            if arrays_for_chunk is not None:
                arrays = arrays_for_chunk(x, z)
            level = world.tag(3, 'xPos', struct.pack('>i', x)) + world.tag(3, 'zPos', struct.pack('>i', z))
            level += world.tag(4, 'LastUpdate', struct.pack('>q', 0))
            for name, data in arrays.items():
                level += world.tag(7, name, struct.pack('>i', len(data)) + data)
            level += world.tag(1, 'TerrainPopulated', b'\1')
            for name in ('Entities', 'TileEntities'):
                level += world.tag(9, name, b'\x0a' + bytes(4))
            raw = b'\x0a\0\0' + world.tag(10, 'Level', level + b'\0') + b'\0'
            compressed = zlib.compress(raw)
            body = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
            count = (len(body) + 4095) // 4096
            image = regions.setdefault((x >> 5, z >> 5), bytearray(8192))
            slot = (x & 31) + (z & 31) * 32
            struct.pack_into('>I', image, slot * 4, ((len(image) // 4096) << 8) | count)
            image.extend(body + bytes(count * 4096 - len(body)))
    for (x, z), image in regions.items():
        (directory / f'r.{x}.{z}.mcr').write_bytes(image)


def inventory(player, count):
    require(player['Inventory'][0] == 9 and player['Inventory'][1][0] == 10 and
            len(player['Inventory'][1][1]) == 1, 'one independently decoded actual Stack')
    stack = player['Inventory'][1][1][0]
    require(stack['Slot'] == [1, 0] and stack['id'] == [2, 355] and stack['Count'] == [1, count] and
            stack['Damage'] == [2, 0], 'actual Bed355 Stack, selected slot0, count and damage')


def player_fields(player, count):
    inventory(player, count)
    require(player['Health'] == [2, 20] and player['Dimension'] == [3, 0] and
            player['Sleeping'] == [1, 0] and player['SleepTimer'] == [2, 0] and
            all(name not in player for name in ('SpawnX', 'SpawnY', 'SpawnZ')),
            'healthy awake Player with no saved Bed coordinates')


def input_check(nbt, helper, level_path, directory):
    data = nbt.inflate(nbt.read(level_path), 31)['Data'][1]
    player = data['Player'][1]
    player_fields(player, 2)
    saved_y = 64. + struct.unpack('>f', struct.pack('>f', 1.62))[0]
    require(data['Time'] == [4, 0] and player['OnGround'] == [1, 1] and
            player['Pos'] == [9, [6, [int.from_bytes(struct.pack('>d', v), 'big') for v in (.5, saved_y, .5)]]] and
            player['Motion'] == [9, [6, [0, 0, 0]]] and
            player['Rotation'] == [9, [5, [0, int.from_bytes(struct.pack('>f', 41.), 'big')]]],
            'independent exact pre-bootstrap position, zero motion, yaw0/pitch41 and day')
    records = helper.region_records(directory)
    require(set(records) == {(x, z) for x in range(-8, 9) for z in range(-8, 9)}, '289 declared stored Chunks')
    expected = grass_arrays()
    for (x, z), record in records.items():
        level = nbt.Nbt(helper.record_raw(record)).root()['Level'][1]
        require(level['xPos'] == [3, x] and level['zPos'] == [3, z] and
                level['TerrainPopulated'] == [1, 1] and level['Entities'] == [9, [10, []]] and
                level['TileEntities'] == [9, [10, []]] and all(level[name] == [7, value] for name, value in expected.items()),
                '289 complete Grass63 inputs, no marker, no Bed, no authored Entities')
    return dict(chunks=289, grass_y=63, feet=64, position=[.5, saved_y, .5], yaw=0, pitch=41,
                item=355, count=2, health=20, saved_spawn=None,
                plane_sha256={name: hashlib.sha256(value).hexdigest() for name, value in expected.items()})


def imported_notice(assets, helper):
    path = assets / 'jar/lang/en_US.lang'
    selected = [line.split('=', 1)[1] for line in path.read_bytes().decode('iso-8859-1').splitlines()
                if line.startswith('tile.bed.noSleep=')]
    require(len(selected) == 1, 'one actual imported noSleep translation property')
    text = helper.property_unescape(selected[0])
    require(text and '\n' not in text and '\r' not in text, 'one imported warning line')
    return text.encode('utf-16-be', errors='surrogatepass').hex()


def receipts(stdout, notice):
    lines = stdout.splitlines()
    expected = [
        'BED_PLACEMENT_INPUTS chunks=289 grass-y=63 Bed=absent item=355 count=2 health=20 selected=0 saved-spawn=null feet=64 x=.5 z=.5 yaw=0 pitch=41 pre-bootstrap-only=1',
        'BED_PLACEMENT_INPUT action=place SDK-right-tap=1 source-returned=1 no-air-fallback=1 drained=1 count=1 used=1 pose-bounds-unchanged=0',
        'BED_PLACEMENT_INPUT action=daytime SDK-right-tap=1 source-returned=1 no-air-fallback=1 drained=1 count=1 used=1 pose-bounds-unchanged=1',
        'BED_PLACEMENT_RETURN foot=0,64,2:26:0 head=0,64,3:26:8 count=2,1,1 used=1 sleeping=0 timer=0 Q=null same-stack=1 pose-bounds-unchanged=1',
        'BED_PLACEMENT_SAVE source-full-save=1 live-player=1 SDK-save-menu=0 reload=0 crafting=0 night=0']
    require(all(lines.count(line) == 1 for line in expected) and
            [lines.index(line) for line in expected] == sorted(lines.index(line) for line in expected),
            'one ordered genuine input/place/daytime/save route')
    for phase, chat in (('before', 0), ('placed', 0), ('notice', 1)):
        require(lines.count(f'BED_PLACEMENT_FRAME phase={phase} terrain=1 HUD=1 health=20 selected=0 item-slots=1 chat={chat}') == 1,
                'returned terrain/HUD phase ' + phase)
    rows = [line for line in lines if line.startswith('BED_PLACEMENT_NOTICE ')]
    match = re.fullmatch(r'BED_PLACEMENT_NOTICE key=tile.bed.noSleep age=([0-9]+) utf16=([0-9a-f]+)', rows[0]) if len(rows) == 1 else None
    require(match is not None and int(match[1]) < 200 and match[2] == notice,
            'actual live UQ UTF16 equals imported original translation')
    footer = [line for line in lines if line.startswith('BED_PLACEMENT_FINISH ')]
    require(len(footer) == 1 and re.fullmatch(r'BED_PLACEMENT_FINISH cleanup=1 windows-absent=1 checks=[1-9][0-9]*', footer[0]),
            'checked terminal root cleanup and both windows absent')
    require(not any(line.startswith(('BED_PLACEMENT_FAILURE', 'WORLD_NATIVE_FOCUS')) for line in lines),
            'no failed focus/source receipt')
    return dict(input_actions=2, count=[2, 1, 1], used=1, notice_utf16=notice, notice_age=int(match[1]), cleanup=True)


def readback(nbt, helper, saves, evidence):
    player = nbt.inflate(nbt.read(saves / 'alpha/level.dat'), 31)['Data'][1]['Player'][1]
    player_fields(player, 1)
    before = nbt.Nbt(nbt.read(evidence / 'bed-player-placed.nbt')).root()
    after = nbt.Nbt(nbt.read(evidence / 'bed-player-notice.nbt')).root()
    player_fields(before, 1); player_fields(after, 1)
    for name in ('Pos', 'Rotation', 'Health', 'Sleeping', 'SleepTimer', 'Inventory', 'Dimension'):
        require(before[name] == after[name] == player[name], 'saved activation preservation ' + name)
    records = helper.region_records(saves / 'alpha/region')
    level = nbt.Nbt(helper.record_raw(records[0, 0])).root()['Level'][1]
    for z, metadata in ((2, 0), (3, 8)):
        index = (z << 7) | 64
        require(level['Blocks'][1][index] == 26 and level['Blocks'][1][index - 1] == 2 and
                ((level['Data'][1][index // 2] >> ((index & 1) * 4)) & 15) == metadata,
                'actual saved Bed pair and Grass support')
    require(sum(byte == 26 for byte in level['Blocks'][1]) == 2, 'exactly two saved Bed cells')
    return dict(foot=[0, 64, 2, 26, 0], head=[0, 64, 3, 26, 8], health=20, count=1,
                sleeping=False, timer=0, saved_spawn=None, selected_player_fields_preserved=True)


def input_paths(executable):
    result = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format'}
    result.update(executable.parent.glob('*.dll'))
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client', 'tests/tools/world'):
        result.update(path for path in (ROOT / folder).rglob('*') if path.is_file() and
                      path.suffix in ('.c', '.h', '.inc', '.py', '.cmake', '.json', '.txt'))
    return result


def fingerprints(paths):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(paths)}


def self_test():
    require(WORK_SECONDS + CLEANUP_SECONDS + 10 <= NATIVE_SECONDS < OUTER_SECONDS,
            'separate work/cleanup budgets leave process and outer timeout margin')
    world, nbt, helper = modules()
    with tempfile.TemporaryDirectory(prefix='bed-placement-input-') as name:
        root = Path(name); regions = root / 'region'; regions.mkdir()
        level = root / 'level.dat'; level.write_bytes(gzip.compress(metadata(world, 'Bed scene', 1304000000000), mtime=0))
        author_regions(world, regions)
        result = input_check(nbt, helper, level, regions)
        require(result['chunks'] == 289 and result['count'] == 2, 'finite authored input roundtrip')
        raw = metadata(world, 'Bed scene', 1304000000000)
        corrupted = raw.replace(world.tag(2, 'Health', struct.pack('>h', 20)), world.tag(2, 'Health', struct.pack('>h', 0)))
        level.write_bytes(gzip.compress(corrupted, mtime=0))
        try:
            input_check(nbt, helper, level, regions)
        except ValueError:
            pass
        else:
            raise ValueError('corrupted pre-bootstrap health accepted')
    print('Bed placement Python inputs: 289 decoded Chunks, Player/Stack/pose, corruption refusal; no native process')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    require(os.name == 'nt' and args.executable is not None and args.assets is not None and args.work_dir is not None,
            'Windows scene requires --executable --assets --work-dir')
    world, nbt, helper = modules()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='bed-placement-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'; game = appdata / '.minecraft'
    world.repeat_statistics(game, assets)
    (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
    saves = game / 'saves'; saves.mkdir()
    for folder, display, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000), ('alpha', 'Bed placement', 1304000000000)):
        directory = saves / folder; directory.mkdir()
        raw = metadata(world, display, played) if folder == 'alpha' else world.metadata(display, played, 19132)
        (directory / 'level.dat').write_bytes(gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Bed scene bytes\0\xff')
    regions = saves / 'alpha/region'; regions.mkdir(); author_regions(world, regions)
    inputs = input_check(nbt, helper, saves / 'alpha/level.dat', regions)
    before_files = world.save_title_files(saves, evidence / 'before')
    before_inputs, before_assets = fingerprints(input_paths(executable)), world.snapshot(assets)
    notice = imported_notice(assets, helper)
    env = os.environ.copy()
    env.update(APPDATA=str(appdata), LOCALAPPDATA=str(appdata), USERPROFILE=str(appdata), HOME=str(appdata), ALSOFT_DRIVERS='null')
    report = dict(status='FAIL', scope=__doc__, inputs=inputs, source_product_before=before_inputs,
                  assets_before=before_assets, work_timeout=WORK_SECONDS, cleanup_timeout=CLEANUP_SECONDS,
                  native_timeout=NATIVE_SECONDS, outer_timeout=OUTER_SECONDS)
    try:
        result = world.retained_native_run([str(executable), str(assets), 'place-daytime', str(evidence)],
            env, evidence, saves, before_files, 'bed-placement-daytime', time.time_ns() // 1000000, NATIVE_SECONDS)
        print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0 and result.stderr == '', 'single actual SDK scene failed; retain this attempt')
        report['native'] = receipts(result.stdout, notice)
        report['readback'] = readback(nbt, helper, saves, evidence)
        after = world.save_title_files(saves, evidence / 'after-verified')
        require(all(after.get(name) == value for name, value in before_files.items()
                    if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat')),
                'unrelated saved Worlds/files unchanged')
        report['screenshots'] = {}
        for phase in ('before', 'placed', 'notice'):
            path = evidence / f'bed-{phase}.png'; data = path.read_bytes()
            require(data.startswith(b'\x89PNG\r\n\x1a\n') and struct.unpack('>II', data[16:24]) == (854, 480),
                    'retained real854x480 returned frame ' + phase)
            report['screenshots'][path.name] = hashlib.sha256(data).hexdigest()
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = dict(type=type(error).__name__, message=str(error)); raise
    finally:
        report['source_product_after'] = fingerprints(input_paths(executable)); report['assets_after'] = world.snapshot(assets)
        unchanged = report['source_product_after'] == before_inputs and report['assets_after'] == before_assets
        if not unchanged:
            report['status'] = 'FAIL'
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        require(unchanged, 'current source/product/imported-resource hashes changed')
    print('Bed placement/daytime scene: PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
