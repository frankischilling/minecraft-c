"""Monitor-2 saved Arrow/Pig encounter through ordinary Timer ticks and SDK save.

Pre-bootstrap NBT proves Pig10 and one Arrow; passive live Pig6/Arrow absence,
returned terrain/HUD and independent committed NBT provide causal scene evidence.
No direct SL741 receipt, damage/tick helper, state repair or original-frame claim.
"""
import argparse
import copy
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
import test_zombie_damage as zombie

require = bed.require
metadata = zombie.metadata


def base_entity(world, name, position, motion, grounded):
    result = world.tag(8, 'id', world.string(name))
    for key, values in (('Pos', position), ('Motion', motion)):
        result += world.tag(9, key, b'\6' + struct.pack('>i3d', 3, *values))
    result += world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 0.))
    result += world.tag(5, 'FallDistance', bytes(4))
    result += world.tag(2, 'Fire', bytes(2)) + world.tag(2, 'Air', struct.pack('>h', 300))
    return result + world.tag(1, 'OnGround', bytes([grounded]))


def pig_payload(world):
    result = base_entity(world, 'Pig', (.5, 64., 3.5), (0., 0., 0.), 1)
    for key, value in (('Health', 10), ('HurtTime', 0), ('DeathTime', 0), ('AttackTime', 0)):
        result += world.tag(2, key, struct.pack('>h', value))
    return result + world.tag(1, 'Saddle', b'\0') + b'\0'


def arrow_payload(world):
    result = base_entity(world, 'Arrow', (.5, 64.45, 2.4), (0., 0., .6), 0)
    for key, value in (('xTile', 0), ('yTile', 64), ('zTile', 2)):
        result += world.tag(2, key, struct.pack('>h', value))
    for key in ('inTile', 'inData', 'shake', 'inGround', 'player'):
        result += world.tag(1, key, b'\0')
    return result + b'\0'


def author_regions(world, helper, directory):
    arrays = night.glowstone_arrays()
    bed.author_regions(world, directory, lambda x, z: arrays)
    path = directory / 'r.0.0.mcr'
    original, image = path.read_bytes(), bytearray(8192)
    for slot, location in enumerate(struct.unpack_from('>1024I', original)):
        if location == 0:
            continue
        offset = (location >> 8) * 4096
        length = struct.unpack_from('>I', original, offset)[0]
        body = original[offset:offset + length + 4]
        if slot == 0:
            raw = helper.record_raw({'body': body})
            empty = world.tag(9, 'Entities', b'\x0a' + bytes(4))
            require(raw.count(empty) == 1, 'One complete pre-bootstrap Entity list')
            actors = world.tag(9, 'Entities', b'\x0a' + struct.pack('>i', 2) +
                               pig_payload(world) + arrow_payload(world))
            raw = raw.replace(empty, actors, 1)
            compressed = b'\2' + zlib.compress(raw)
            body = struct.pack('>I', len(compressed)) + compressed
        sectors = (len(body) + 4095) // 4096
        struct.pack_into('>I', image, slot * 4, ((len(image) // 4096) << 8) | sectors)
        image.extend(body + bytes(sectors * 4096 - len(body)))
    path.write_bytes(image)


def expected_entities():
    def base(name, pos, motion, ground):
        return {'id': [8, name], 'Pos': [9, [6, zombie.bits(pos, 'd')]],
                'Motion': [9, [6, zombie.bits(motion, 'd')]], 'Rotation': [9, [5, [0, 0]]],
                'FallDistance': [5, 0], 'Fire': [2, 0], 'Air': [2, 300], 'OnGround': [1, ground]}
    pig = base('Pig', (.5, 64., 3.5), (0., 0., 0.), 1)
    pig.update(Health=[2, 10], HurtTime=[2, 0], DeathTime=[2, 0], AttackTime=[2, 0], Saddle=[1, 0])
    arrow = base('Arrow', (.5, 64.45, 2.4), (0., 0., .6), 0)
    arrow.update(xTile=[2, 0], yTile=[2, 64], zTile=[2, 2], inTile=[1, 0], inData=[1, 0],
                 shake=[1, 0], inGround=[1, 0], player=[1, 0])
    return [pig, arrow]


def entity_inputs(entities):
    require(zombie.same_tagged_value(entities, expected_entities()),
            'Exactly one canonical Pig10 then one in-flight Arrow; no target, shooter or entropy input')


def input_check(nbt, helper, level_path, directory):
    data = nbt.inflate(nbt.read(level_path), 31)['Data'][1]
    player = data['Player'][1]
    zombie.player_fields(player, 20, inventory_type=10)
    saved_y = 64. + struct.unpack('>f', struct.pack('>f', 1.62))[0]
    require(data['Time'] == [4, 13000] and player['OnGround'] == [1, 1] and
            player['Pos'] == [9, [6, zombie.bits((.5, saved_y, .5), 'd')]] and
            player['Motion'] == [9, [6, [0, 0, 0]]] and player['Rotation'] == [9, [5, [0, 0]]],
            'Awake unarmored Player20, night and exact pre-bootstrap pose')
    records = helper.region_records(directory)
    require(set(records) == {(x, z) for x in range(-8, 9) for z in range(-8, 9)},
            '289 pre-bootstrap normal Chunks')
    expected, entities = night.glowstone_arrays(), []
    for (x, z), record in records.items():
        level = nbt.Nbt(helper.record_raw(record)).root()['Level'][1]
        require(level['xPos'] == [3, x] and level['zPos'] == [3, z] and
                level['TerrainPopulated'] == [1, 1] and level['TileEntities'] == [9, [10, []]] and
                all(level[name] == [7, value] for name, value in expected.items()) and
                level['Entities'][0] == 9 and level['Entities'][1][0] == 10,
                'Complete Glowstone/light planes and canonical compound Entity lists')
        if (x, z) == (0, 0):
            entities = level['Entities'][1][1]
        else:
            require(level['Entities'][1][1] == [], 'No other saved NPC or projectile')
    entity_inputs(entities)
    return dict(chunks=289, time=13000, player_health=20, pig_health=10, pigs=1, arrows=1,
                pig_position=[.5, 64., 3.5], arrow_position=[.5, 64.45, 2.4],
                arrow_motion=[0., 0., .6], shooter=None, armor=0, inventory=[],
                plane_sha256={name: hashlib.sha256(value).hexdigest() for name, value in expected.items()})


def receipts(stdout, monitor):
    lines = stdout.splitlines()
    bounds = ','.join(map(str, monitor['selected']['work']))
    for name in ('frame', 'canvas'):
        rows = [line for line in lines if line.startswith('ARROW_PIG_MONITOR window=' + name + ' ')]
        require(len(rows) == 1 and rows[0].endswith(' target=' + bounds),
                'Both source windows on physical monitor2')
    ordered = (
        'ARROW_PIG_INPUTS chunks=289 glowstone-y=63 saved-Pig=1 saved-Arrow=1 time=13000 player-health=20 pig-health=10 pre-bootstrap-only=1',
        'ARROW_PIG_CAUSAL saved-pig-health=10 observed-health=6 arrow-members=0 ordinary-world-ticks=1 direct741-receipt=0',
        'ARROW_PIG_FRAME phase=hit terrain=1 HUD=1 player-health=20 pig-health=6 empty-slots=9 source-px=1013 source-uq=2101',
        'ARROW_PIG_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 player-health=20 pig-health=6 arrow-members=0',
        'ARROW_PIG_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Pig=1 new-title=1')
    require(all(lines.count(line) == 1 for line in ordered) and
            [lines.index(line) for line in ordered] == sorted(lines.index(line) for line in ordered),
            'Ordered causal Pig6/Arrow absence, returned terrain/HUD and SDK pause/save')
    observed = [line for line in lines if line.startswith('ARROW_PIG_OBSERVE ')]
    match = re.fullmatch(r'ARROW_PIG_OBSERVE initial10-seen=([01]) arrow-seen=([01]) world-ticks=([1-9][0-9]*)',
                         observed[0]) if len(observed) == 1 else None
    require(match is not None, 'Honest post-frame loading observations and real World ticks')
    footer = [line for line in lines if line.startswith('ARROW_PIG_FINISH ')]
    require(len(footer) == 1 and re.fullmatch(
        r'ARROW_PIG_FINISH cleanup=1 windows-absent=1 checks=[1-9][0-9]*', footer[0]),
        'Checked terminal cleanup and absent source windows')
    require(not any(line.startswith(('ARROW_PIG_FAILURE', 'WORLD_NATIVE_FOCUS',
                                    'WORLD_REPEAT_FOCUS_FAILURE', 'BED_PLACEMENT_FAILURE')) for line in lines),
            'Failed source/focus opportunities never qualify')
    return dict(observation='causal', direct_741_receipt=False, input_pig_health=10, pig_health=6,
                player_health=20, arrows_remaining=0, initial10_seen=match[1] == '1',
                arrow_seen=match[2] == '1', world_ticks=int(match[3]), SDK_pause_save=True, cleanup=True)


def saved_pig(entities, paused):
    require(type(paused) is dict and zombie.same_tagged_value(paused.get('id'), [8, 'Pig']) and
            zombie.same_tagged_value(paused.get('Health'), [2, 6]), 'Canonical actual paused Pig6')
    require(len(entities) == 1 and zombie.same_tagged_value(entities[0], paused),
            'Complete committed Entity inventory is the unique surviving paused Pig; Arrow absent')
    return entities[0]


def readback(nbt, helper, saves, evidence):
    player = nbt.Nbt(nbt.read(evidence / 'arrow-pig-player-paused.nbt')).root()
    pig = nbt.Nbt(nbt.read(evidence / 'arrow-pig-npc-paused.nbt')).root()
    zombie.player_fields(player, 20)
    data = nbt.inflate(nbt.read(saves / 'alpha/level.dat'), 31)['Data'][1]
    require(13000 <= data['Time'][1] < 24000, 'Natural elapsed nighttime ticks')
    saved_player = data['Player'][1]
    zombie.player_fields(saved_player, 20)
    for name in ('Pos', 'Motion', 'Rotation', 'Health', 'Inventory', 'Dimension', 'Score',
                 'Sleeping', 'SleepTimer', 'HurtTime', 'DeathTime', 'AttackTime'):
        require(zombie.same_tagged_value(player[name], saved_player[name]), 'Paused Player matches save: ' + name)
    entities, inventory = [], []
    for coordinate, record in helper.region_records(saves / 'alpha/region').items():
        raw = helper.record_raw(record)
        level = nbt.Nbt(raw).root()['Level'][1]
        rows = level['Entities'][1][1]
        entities.extend(rows)
        inventory.append(dict(coordinate=list(coordinate), bytes=len(raw),
                              sha256=hashlib.sha256(raw).hexdigest(), entities=rows))
    saved_pig(entities, pig)
    require(pig['DeathTime'] == [2, 0], 'Surviving Pig, no fatal drop or replacement subject')
    return dict(time=data['Time'][1], player_health=20, pig_health=6, pigs=1, arrows=0,
                snapshots_match_save=True, complete_entity_inventory=entities, chunks=inventory)


def self_test():
    world, nbt, helper = bed.modules()
    with tempfile.TemporaryDirectory(prefix='arrow-pig-input-') as name:
        root = Path(name)
        region = root / 'region'; region.mkdir()
        level = root / 'level.dat'
        raw = metadata(world, 'Arrow Pig encounter', 1304000000000)
        level.write_bytes(gzip.compress(raw, mtime=0))
        author_regions(world, helper, region)
        require(input_check(nbt, helper, level, region)['arrows'] == 1, 'Independent actual input roundtrip')
        bad = raw.replace(world.tag(2, 'Health', struct.pack('>h', 20)),
                          world.tag(2, 'Health', struct.pack('>h', 19)), 1)
        level.write_bytes(gzip.compress(bad, mtime=0))
        try:
            input_check(nbt, helper, level, region)
        except ValueError:
            pass
        else:
            raise ValueError('Wrong initial Player health accepted')
    refused = 0
    for key, value in (('Health', [2, 6]), ('Motion', [9, [6, [0, 0, 0]]]),
                        ('inGround', [1, 1]), ('Shooter', [4, 1])):
        actors = copy.deepcopy(expected_entities())
        actors[0 if key == 'Health' else 1][key] = value
        try:
            entity_inputs(actors)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupt saved Entity input accepted: ' + key)
    paused = {'id': [8, 'Pig'], 'Health': [2, 6], 'Pos': [9, [6, [0, 1, 2]]]}
    require(saved_pig([paused], paused) is paused, 'Unique complete paused subject match')
    for actors in ([], [paused, dict(paused)], [paused, {'id': [8, 'Arrow']}],
                    [{**paused, 'Health': [2, 10]}], [{**paused, 'extra': [1, 0]}]):
        try:
            saved_pig(actors, paused)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Missing/duplicate/corrupt Pig or surviving Arrow accepted')
    monitor = dict(selected=dict(work=[100, 200, 1920, 1080]))
    lines = [f'ARROW_PIG_MONITOR window={name} x=100 y=200 width=854 height=480 target=100,200,1920,1080'
             for name in ('frame', 'canvas')]
    lines += [
        'ARROW_PIG_INPUTS chunks=289 glowstone-y=63 saved-Pig=1 saved-Arrow=1 time=13000 player-health=20 pig-health=10 pre-bootstrap-only=1',
        'ARROW_PIG_CAUSAL saved-pig-health=10 observed-health=6 arrow-members=0 ordinary-world-ticks=1 direct741-receipt=0',
        'ARROW_PIG_OBSERVE initial10-seen=0 arrow-seen=0 world-ticks=1',
        'ARROW_PIG_FRAME phase=hit terrain=1 HUD=1 player-health=20 pig-health=6 empty-slots=9 source-px=1013 source-uq=2101',
        'ARROW_PIG_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 player-health=20 pig-health=6 arrow-members=0',
        'ARROW_PIG_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Pig=1 new-title=1',
        'ARROW_PIG_FINISH cleanup=1 windows-absent=1 checks=1']
    good = '\n'.join(lines)
    require(receipts(good, monitor)['direct_741_receipt'] is False, 'Causal scope is not direct741 proof')
    for bad in (good.replace('direct741-receipt=0', 'direct741-receipt=1'),
                good.replace('observed-health=6', 'observed-health=7'),
                good.replace('arrow-members=0', 'arrow-members=1'),
                good.replace('source-leave=366', 'source-leave=0'),
                '\n'.join(lines[:-1]), good + '\nWORLD_NATIVE_FOCUS failed'):
        try:
            receipts(bad, monitor)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupted causal/focus/save/cleanup receipt accepted')
    print(f'PASS: Arrow/Pig289-chunk input/readback/causal receipt controls; {refused} corruption refusals; no native process')


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
            'Windows scene needs executable/assets/work-dir')
    monitor = night.second_monitor()
    world, nbt, helper = bed.modules()
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='arrow-pig-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'; game = appdata / '.minecraft'
    world.repeat_statistics(game, assets)
    (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
    saves = game / 'saves'; saves.mkdir()
    for folder, display, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000),
                                    ('alpha', 'Arrow Pig encounter', 1304000000000)):
        directory = saves / folder; directory.mkdir()
        raw = metadata(world, display, played) if folder == 'alpha' else world.metadata(display, played, 19132)
        (directory / 'level.dat').write_bytes(gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Arrow Pig scene bytes\0\xff')
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
        result = world.retained_native_run([str(executable), str(assets), 'loaded-arrow-pig', str(evidence)],
            environment, evidence, saves, before_files, 'arrow-pig', time.time_ns() // 1000000,
            bed.NATIVE_SECONDS)
        print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0 and result.stderr == '', 'Native Arrow/Pig scene failed; retain this opportunity')
        report['native'] = receipts(result.stdout, monitor)
        report['readback'] = readback(nbt, helper, saves, evidence)
        after = world.save_title_files(saves, evidence / 'after-verified')
        require(all(after.get(name) == value for name, value in before_files.items()
                    if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat')),
                'Unrelated saved Worlds/files preserved')
        report['screenshots'] = {}
        for phase in ('hit', 'paused', 'title'):
            path = evidence / ('arrow-pig-' + phase + '.png'); data = path.read_bytes()
            require(data.startswith(b'\x89PNG\r\n\x1a\n') and struct.unpack('>II', data[16:24]) == (854, 480),
                    'Actual returned854x480 scene frame: ' + phase)
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
        require(unchanged, 'Source/product/imported assets changed during native opportunity')
    print('Saved Arrow/Pig causal SDK scene: PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
