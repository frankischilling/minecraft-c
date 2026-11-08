"""One retained monitor-2 loaded night: saved Zombie AI hits Player, then SDK save.

All scene inputs precede bootstrap. Real Timer/entity AI/path/attack, returned
World/HUD, source pause/Save and Quit, independent NBT/statistics and cleanup.
No direct damage, source-state repair, scheduler skip or original-frame claim.
"""
import argparse
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
import zlib

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_bed_placement as bed
import test_bed_sleep as night

require = bed.require


def metadata(world, display, played):
    raw = world.metadata(display, played, 19132, player_score=0)
    require(raw.endswith(bytes(3)), 'Player/Data/root compound ends')
    player = (world.tag(1, 'OnGround', b'\1') + world.tag(1, 'Sleeping', b'\0') +
              world.tag(2, 'SleepTimer', bytes(2)) + world.tag(2, 'HurtTime', bytes(2)) +
              world.tag(2, 'DeathTime', bytes(2)) + world.tag(2, 'AttackTime', bytes(2)) +
              world.tag(2, 'Fire', bytes(2)) + world.tag(2, 'Air', struct.pack('>h', 300)))
    raw = raw[:-3] + player + raw[-3:]
    return raw[:-2] + world.tag(4, 'Time', struct.pack('>q', 13000)) + raw[-2:]


def zombie_payload(world):
    result = world.tag(8, 'id', world.string('Zombie'))
    for name, values in (('Pos', (.5, 64., 1.5)), ('Motion', (0., 0., 0.))):
        result += world.tag(9, name, b'\6' + struct.pack('>i3d', 3, *values))
    result += world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 0.))
    result += world.tag(5, 'FallDistance', bytes(4))
    for name, value in (('Fire', 0), ('Air', 300), ('Health', 20),
                        ('HurtTime', 0), ('DeathTime', 0), ('AttackTime', 0)):
        result += world.tag(2, name, struct.pack('>h', value))
    return result + world.tag(1, 'OnGround', b'\1') + b'\0'


def author_regions(world, helper, directory):
    arrays = night.glowstone_arrays()
    bed.author_regions(world, directory, lambda x, z: arrays)
    # Replace only the pre-bootstrap saved Entities list, preserving all other
    # normal records in its real region. No native object is constructed here.
    path = directory / 'r.0.0.mcr'
    original = path.read_bytes()
    image = bytearray(8192)
    for slot, location in enumerate(struct.unpack_from('>1024I', original)):
        if location == 0:
            continue
        offset = (location >> 8) * 4096
        length = struct.unpack_from('>I', original, offset)[0]
        body = original[offset:offset + length + 4]
        if slot == 0:
            raw = helper.record_raw({'body': body})
            empty = world.tag(9, 'Entities', b'\x0a' + bytes(4))
            require(raw.count(empty) == 1, 'One genuine saved Entity-list input')
            raw = raw.replace(empty, world.tag(9, 'Entities', b'\x0a' + struct.pack('>i', 1) +
                                               zombie_payload(world)))
            compressed = b'\2' + zlib.compress(raw)
            body = struct.pack('>I', len(compressed)) + compressed
        count = (len(body) + 4095) // 4096
        struct.pack_into('>I', image, slot * 4, ((len(image) // 4096) << 8) | count)
        image.extend(body + bytes(count * 4096 - len(body)))
    path.write_bytes(image)


def bits(values, width):
    return [int.from_bytes(struct.pack('>' + width, value), 'big') for value in values]


def player_fields(player, health):
    require(player['Health'] == [2, health] and player['Dimension'] == [3, 0] and
            player['Sleeping'] == [1, 0] and player['SleepTimer'] == [2, 0] and
            player['Inventory'] == [9, [10, []]] and player['Score'] == [3, 0] and
            all(name not in player for name in ('SpawnX', 'SpawnY', 'SpawnZ')),
            'Actual awake unarmored Player, empty Inventory and unchanged spawn/score')


def input_check(nbt, helper, level_path, directory):
    data = nbt.inflate(nbt.read(level_path), 31)['Data'][1]
    player = data['Player'][1]
    player_fields(player, 20)
    saved_y = 64. + struct.unpack('>f', struct.pack('>f', 1.62))[0]
    require(data['Time'] == [4, 13000] and player['OnGround'] == [1, 1] and
            player['Pos'] == [9, [6, bits((.5, saved_y, .5), 'd')]] and
            player['Motion'] == [9, [6, [0, 0, 0]]] and
            player['Rotation'] == [9, [5, [0, 0]]], 'Exact pre-bootstrap night/Player geometry')
    expected = night.glowstone_arrays()
    records = helper.region_records(directory)
    require(set(records) == {(x, z) for x in range(-8, 9) for z in range(-8, 9)},
            '289 pre-bootstrap stored normal Chunks')
    zombies = []
    for (x, z), record in records.items():
        level = nbt.Nbt(helper.record_raw(record)).root()['Level'][1]
        require(level['xPos'] == [3, x] and level['zPos'] == [3, z] and
                level['TerrainPopulated'] == [1, 1] and level['TileEntities'] == [9, [10, []]] and
                all(level[name] == [7, value] for name, value in expected.items()),
                'Complete stored Glowstone/light planes and no authored TileEntities')
        require(level['Entities'][0] == 9 and level['Entities'][1][0] == 10,
                'Actual compound Entity list input')
        for entity in level['Entities'][1][1]:
            zombies.append((x, z, entity))
    require(len(zombies) == 1 and zombies[0][:2] == (0, 0), 'Exactly one saved NPC in Chunk0,0')
    zombie = zombies[0][2]
    require(zombie['id'] == [8, 'Zombie'] and zombie['Health'] == [2, 20] and
            zombie['Pos'] == [9, [6, bits((.5, 64., 1.5), 'd')]] and
            zombie['Motion'] == [9, [6, [0, 0, 0]]] and zombie['Rotation'] == [9, [5, [0, 0]]] and
            zombie['OnGround'] == [1, 1] and zombie['Fire'] == [2, 0] and
            all(zombie[name] == [2, 0] for name in ('HurtTime', 'DeathTime', 'AttackTime')) and
            not any('target' in name.lower() or 'random' in name.lower() for name in zombie),
            'Canonical saved Zombie, healthy/grounded/zero timers and no authored target/entropy')
    return dict(chunks=289, time=13000, difficulty=2, hostiles=True, player_health=20,
                zombie_health=20, zombie_count=1, zombie_position=[.5, 64., 1.5],
                player_position=[.5, saved_y, .5], armor=0, inventory=[],
                plane_sha256={name: hashlib.sha256(value).hexdigest() for name, value in expected.items()})


def receipts(stdout, monitor):
    lines = stdout.splitlines()
    bounds = ','.join(map(str, monitor['selected']['work']))
    for name in ('frame', 'canvas'):
        selected = [line for line in lines if line.startswith('ZOMBIE_DAMAGE_MONITOR window=' + name + ' ')]
        require(len(selected) == 1 and selected[0].endswith(' target=' + bounds),
                'Both actual source windows on physical monitor2')
    exact = (
        'ZOMBIE_DAMAGE_INPUTS chunks=289 glowstone-y=63 saved-Zombie=1 time=13000 difficulty=2 hostiles=1 player-health=20 zombie-health=20 armor=0 pre-bootstrap-only=1',
        'ZOMBIE_DAMAGE_LOAD saved-NBT=1 canonical=uz monster-damage=5 same-world-and-classes=1',
        'ZOMBIE_DAMAGE_HIT caller=gz.a:61 returned=1 result=1 damage=5 health=20,15 statistic=2021 amount=5 calls=1 returns=1 true=1',
        'ZOMBIE_DAMAGE_FRAME phase=hit terrain=1 HUD=1 health=15 empty-slots=9 source-px=1013 source-uq=2101',
        'ZOMBIE_DAMAGE_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 health=15 statistic2021=5',
        'ZOMBIE_DAMAGE_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Zombie=1 new-title=1')
    require(all(lines.count(line) == 1 for line in exact) and
            [lines.index(line) for line in exact] == sorted(lines.index(line) for line in exact),
            'Complete ordered loaded NPC/real AI hit/HUD/SDK pause/save route')
    footer = [line for line in lines if line.startswith('ZOMBIE_DAMAGE_FINISH ')]
    require(len(footer) == 1 and re.fullmatch(
        r'ZOMBIE_DAMAGE_FINISH cleanup=1 windows-absent=1 checks=[1-9][0-9]*', footer[0]),
        'Checked native terminal cleanup')
    require(not any(line.startswith(('ZOMBIE_DAMAGE_FAILURE', 'WORLD_NATIVE_FOCUS',
                                    'WORLD_REPEAT_FOCUS_FAILURE', 'BED_PLACEMENT_FAILURE'))
                    for line in lines), 'Failed source/focus opportunities never qualify')
    return dict(hit_calls=1, hit_returns=1, hit_true_returns=1, damage=5, health=[20, 15],
                statistic=2021, statistic_amount=5, natural_AI=True, SDK_pause_save=True, cleanup=True)


def statistics_readback(game, assets):
    mapping = {}
    for line in (assets / 'jar/achievement/map.txt').read_text(encoding='ascii').splitlines():
        key, separator, guid = line.partition(',')
        if separator:
            mapping[key] = guid
    result = []
    for path in sorted((game / 'stats').glob('*.dat')):
        require(path.stat().st_size <= 1048576, 'Bounded actual statistics file')
        value = json.loads(path.read_text(encoding='ascii'))
        require(type(value) is dict and type(value.get('stats-change')) is list,
                'Actual statistics JSON shape')
        entries, payload = {}, ''
        for row in value['stats-change']:
            require(type(row) is dict and len(row) == 1, 'One statistic per actual entry')
            key, count = next(iter(row.items()))
            require(key not in entries and key in mapping and type(count) is int,
                    'Unique known statistic identity')
            entries[key] = count
            payload += mapping[key] + ',' + str(count) + ','
        salt = value.get('user', {}).get('sessionid', 'local')
        checksum = format(int.from_bytes(hashlib.md5((salt + payload).encode('ascii')).digest(), 'big'), 'x')
        require(value.get('checksum') == checksum, 'Independent source statistics checksum')
        if '2021' in entries:
            require(entries['2021'] == 5, 'Saved actual damage-taken total5')
            result.append(dict(file=path.name, value=5, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    require(result, 'Ordinary source leave saved statistic2021=5')
    return result


def readback(nbt, helper, saves, evidence):
    player = nbt.Nbt(nbt.read(evidence / 'zombie-player-paused.nbt')).root()
    zombie = nbt.Nbt(nbt.read(evidence / 'zombie-npc-paused.nbt')).root()
    player_fields(player, 15)
    data = nbt.inflate(nbt.read(saves / 'alpha/level.dat'), 31)['Data'][1]
    require(13000 <= data['Time'][1] < 24000, 'Natural elapsed nighttime World ticks')
    saved_player = data['Player'][1]
    player_fields(saved_player, 15)
    for name in ('Pos', 'Motion', 'Rotation', 'Health', 'Inventory', 'Dimension', 'Score',
                 'Sleeping', 'SleepTimer', 'HurtTime', 'DeathTime', 'AttackTime'):
        require(player[name] == saved_player[name], 'Paused Player matches ordinary save: ' + name)
    require(zombie['id'] == [8, 'Zombie'] and zombie['Health'] == [2, 20] and
            zombie['DeathTime'] == [2, 0], 'Actual canonical surviving Zombie snapshot')
    entities = []
    for record in helper.region_records(saves / 'alpha/region').values():
        level = nbt.Nbt(helper.record_raw(record)).root()['Level'][1]
        entities.extend(level['Entities'][1][1])
    require(len(entities) == 1 and entities[0]['id'] == [8, 'Zombie'], 'Exactly one committed saved Zombie')
    for name in ('id', 'Pos', 'Motion', 'Rotation', 'Health', 'HurtTime', 'DeathTime', 'AttackTime'):
        require(zombie[name] == entities[0][name], 'Paused Zombie matches ordinary save: ' + name)
    return dict(time=data['Time'][1], player_health=15, zombie_health=20,
                saved_zombies=1, sleeping=False, inventory=[], snapshots_match_save=True)


def self_test():
    world, nbt, helper = bed.modules()
    with tempfile.TemporaryDirectory(prefix='zombie-damage-input-') as name:
        root = Path(name)
        region = root / 'region'; region.mkdir()
        level = root / 'level.dat'
        level.write_bytes(gzip.compress(metadata(world, 'Zombie encounter', 1304000000000), mtime=0))
        author_regions(world, helper, region)
        require(input_check(nbt, helper, level, region)['zombie_count'] == 1, 'Full independent input roundtrip')
        raw = metadata(world, 'Zombie encounter', 1304000000000)
        for old, new in ((world.tag(4, 'Time', struct.pack('>q', 13000)), world.tag(4, 'Time', bytes(8))),
                         (world.tag(2, 'Health', struct.pack('>h', 20)), world.tag(2, 'Health', struct.pack('>h', 15)))):
            level.write_bytes(gzip.compress(raw.replace(old, new), mtime=0))
            try:
                input_check(nbt, helper, level, region)
            except ValueError:
                pass
            else:
                raise ValueError('Corrupted initial night/health accepted')
    monitor = dict(selected=dict(work=[100, 200, 1920, 1080]))
    good = '\n'.join((
        'ZOMBIE_DAMAGE_MONITOR window=frame x=110 y=210 width=854 height=480 target=100,200,1920,1080',
        'ZOMBIE_DAMAGE_MONITOR window=canvas x=110 y=210 width=854 height=480 target=100,200,1920,1080',
        'ZOMBIE_DAMAGE_INPUTS chunks=289 glowstone-y=63 saved-Zombie=1 time=13000 difficulty=2 hostiles=1 player-health=20 zombie-health=20 armor=0 pre-bootstrap-only=1',
        'ZOMBIE_DAMAGE_LOAD saved-NBT=1 canonical=uz monster-damage=5 same-world-and-classes=1',
        'ZOMBIE_DAMAGE_HIT caller=gz.a:61 returned=1 result=1 damage=5 health=20,15 statistic=2021 amount=5 calls=1 returns=1 true=1',
        'ZOMBIE_DAMAGE_FRAME phase=hit terrain=1 HUD=1 health=15 empty-slots=9 source-px=1013 source-uq=2101',
        'ZOMBIE_DAMAGE_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 health=15 statistic2021=5',
        'ZOMBIE_DAMAGE_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Zombie=1 new-title=1',
        'ZOMBIE_DAMAGE_FINISH cleanup=1 windows-absent=1 checks=100')) + '\n'
    require(receipts(good, monitor)['damage'] == 5, 'Parser-only complete receipt control')
    for bad in (good.replace('damage=5', 'damage=4'),
                good.replace('ZOMBIE_DAMAGE_FINISH', 'INCOMPLETE'),
                good.replace('target=100,200,1920,1080', 'target=0,0,1920,1080'),
                good + next(line for line in good.splitlines() if line.startswith('ZOMBIE_DAMAGE_HIT')) + '\n',
                good + 'ZOMBIE_DAMAGE_FAILURE phase=source operation=unimplemented\n'):
        try:
            receipts(bad, monitor)
        except ValueError:
            pass
        else:
            raise ValueError('Corrupted native outcome accepted by parser-only control')
    print('Zombie encounter inputs:289 complete lit Chunks, exactly1 canonical saved Zombie, awake unarmored Player; input/receipt corruption refusal; no native process')


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
    monitor = night.second_monitor()
    world, nbt, helper = bed.modules()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='zombie-damage-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'; game = appdata / '.minecraft'
    world.repeat_statistics(game, assets)
    (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
    saves = game / 'saves'; saves.mkdir()
    for folder, display, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000),
                                    ('alpha', 'Zombie encounter', 1304000000000)):
        directory = saves / folder; directory.mkdir()
        raw = metadata(world, display, played) if folder == 'alpha' else world.metadata(display, played, 19132)
        (directory / 'level.dat').write_bytes(gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Zombie scene bytes\0\xff')
    region = saves / 'alpha/region'; region.mkdir()
    author_regions(world, helper, region)
    inputs = input_check(nbt, helper, saves / 'alpha/level.dat', region)
    before_files = world.save_title_files(saves, evidence / 'before')
    source, resources = bed.fingerprints(bed.input_paths(executable)), world.snapshot(assets)
    environment = os.environ.copy()
    environment.update(APPDATA=str(appdata), LOCALAPPDATA=str(appdata), USERPROFILE=str(appdata),
                       HOME=str(appdata), ALSOFT_DRIVERS='null',
                       SDL_DISPLAY_USABLE_BOUNDS=','.join(map(str, monitor['selected']['work'])))
    report = dict(status='FAIL', scope=__doc__, inputs=inputs, monitor=monitor,
                  source_product_before=source, assets_before=resources)
    try:
        result = world.retained_native_run([str(executable), str(assets), 'loaded-zombie', str(evidence)],
            environment, evidence, saves, before_files, 'zombie-damage', time.time_ns() // 1000000,
            bed.NATIVE_SECONDS)
        print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0 and result.stderr == '', 'Actual native Zombie scene failed; retain this opportunity')
        report['native'] = receipts(result.stdout, monitor)
        report['readback'] = readback(nbt, helper, saves, evidence)
        report['statistics'] = statistics_readback(game, assets)
        after = world.save_title_files(saves, evidence / 'after-verified')
        require(all(after.get(name) == value for name, value in before_files.items()
                    if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat')),
                'Unrelated Worlds and files preserved')
        report['screenshots'] = {}
        for phase in ('hit', 'paused', 'title'):
            path = evidence / ('zombie-' + phase + '.png'); data = path.read_bytes()
            require(data.startswith(b'\x89PNG\r\n\x1a\n') and struct.unpack('>II', data[16:24]) == (854, 480),
                    'Actual854x480 source scene frame: ' + phase)
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
        require(unchanged, 'Source/product/imported resources changed during native opportunity')
    print('Loaded Zombie encounter:PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
