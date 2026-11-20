"""Monitor-2 saved Arrow/Slime encounter through ordinary Timer ticks and SDK save.

Pre-bootstrap NBT and passive live observations prove size2/Health4 plus Arrow,
then Health0, delayed removal and four size1/Health1 children. SDK pause/save and
independent committed NBT retain that finite scene. No direct hurt/K/tick helper,
state repair, owned Mob environment or original-frame claim.
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
ROOT = Path(__file__).resolve().parents[3]


def input_paths(executable):
    paths = bed.input_paths(executable)
    paths.update((Path(__file__).resolve(), ROOT / 'tests/client/gameplay/test_arrow_slime.c',
                  ROOT / 'cmake/tests/arrow_slime.cmake'))
    return paths


def base_entity(world, name, position, motion, grounded):
    result = world.tag(8, 'id', world.string(name))
    for key, values in (('Pos', position), ('Motion', motion)):
        result += world.tag(9, key, b'\6' + struct.pack('>i3d', 3, *values))
    result += world.tag(9, 'Rotation', b'\5' + struct.pack('>i2f', 2, 0., 0.))
    result += world.tag(5, 'FallDistance', bytes(4))
    result += world.tag(2, 'Fire', bytes(2)) + world.tag(2, 'Air', struct.pack('>h', 300))
    return result + world.tag(1, 'OnGround', bytes([grounded]))


def slime_payload(world):
    result = base_entity(world, 'Slime', (.5, 64., 5.5), (0., 0., 0.), 1)
    for key, value in (('Health', 4), ('HurtTime', 0), ('DeathTime', 0), ('AttackTime', 0)):
        result += world.tag(2, key, struct.pack('>h', value))
    return result + world.tag(3, 'Size', struct.pack('>i', 1)) + b'\0'


def arrow_payload(world):
    result = base_entity(world, 'Arrow', (.5, 64.45, 2.4), (0., 0., .6), 0)
    for key, value in (('xTile', 0), ('yTile', 64), ('zTile', 2)):
        result += world.tag(2, key, struct.pack('>h', value))
    for key in ('inTile', 'inData', 'shake', 'inGround', 'player'):
        result += world.tag(1, key, b'\0')
    return result + b'\0'


def author_regions(world, _helper, directory):
    # Scene-local halo: ordinary border GETs stay on authored, populated
    # terrain. No runtime generation/spawning flag or old helper is changed.
    arrays, regions = night.glowstone_arrays(), {}
    for x in range(-12, 13):
        for z in range(-12, 13):
            level = world.tag(3, 'xPos', struct.pack('>i', x))
            level += world.tag(3, 'zPos', struct.pack('>i', z))
            level += world.tag(4, 'LastUpdate', struct.pack('>q', 0))
            for name, data in arrays.items():
                level += world.tag(7, name, struct.pack('>i', len(data)) + data)
            level += world.tag(1, 'TerrainPopulated', b'\1')
            actors = (b'\x0a' + struct.pack('>i', 2) + slime_payload(world) + arrow_payload(world)
                      if (x, z) == (0, 0) else b'\x0a' + bytes(4))
            level += world.tag(9, 'Entities', actors)
            level += world.tag(9, 'TileEntities', b'\x0a' + bytes(4))
            raw = b'\x0a\0\0' + world.tag(10, 'Level', level + b'\0') + b'\0'
            compressed = b'\2' + zlib.compress(raw)
            body = struct.pack('>I', len(compressed)) + compressed
            sectors = (len(body) + 4095) // 4096
            image = regions.setdefault((x >> 5, z >> 5), bytearray(8192))
            slot = (x & 31) + (z & 31) * 32
            struct.pack_into('>I', image, slot * 4, ((len(image) // 4096) << 8) | sectors)
            image.extend(body + bytes(sectors * 4096 - len(body)))
    for (x, z), image in regions.items():
        (directory / f'r.{x}.{z}.mcr').write_bytes(image)


def expected_entities():
    def base(name, pos, motion, ground):
        return {'id': [8, name], 'Pos': [9, [6, zombie.bits(pos, 'd')]],
                'Motion': [9, [6, zombie.bits(motion, 'd')]], 'Rotation': [9, [5, [0, 0]]],
                'FallDistance': [5, 0], 'Fire': [2, 0], 'Air': [2, 300], 'OnGround': [1, ground]}
    slime = base('Slime', (.5, 64., 5.5), (0., 0., 0.), 1)
    slime.update(Health=[2, 4], HurtTime=[2, 0], DeathTime=[2, 0], AttackTime=[2, 0], Size=[3, 1])
    arrow = base('Arrow', (.5, 64.45, 2.4), (0., 0., .6), 0)
    arrow.update(xTile=[2, 0], yTile=[2, 64], zTile=[2, 2], inTile=[1, 0], inData=[1, 0],
                 shake=[1, 0], inGround=[1, 0], player=[1, 0])
    return [slime, arrow]


def entity_inputs(entities):
    require(zombie.same_tagged_value(entities, expected_entities()),
            'Exactly one canonical Slime Health4/Size1 then one in-flight Arrow; no shooter or entropy input')


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
    require(set(records) == {(x, z) for x in range(-12, 13) for z in range(-12, 13)},
            'Exactly625 pre-bootstrap normal Chunks covering [-12,12] on both axes')
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
    return dict(chunks=625, chunk_bounds=[-12, 12], time=13000, player_health=20,
                parent_health=4, parent_size=2,
                parent_nbt_size=1, slimes=1, arrows=1,
                slime_position=[.5, 64., 5.5], arrow_position=[.5, 64.45, 2.4],
                arrow_motion=[0., 0., .6], shooter=None, armor=0, inventory=[],
                plane_sha256={name: hashlib.sha256(value).hexdigest() for name, value in expected.items()})


def receipts(stdout, monitor):
    require(type(stdout) is str and stdout.endswith('\n') and '\0' not in stdout,
            'Complete textual native protocol ending in a newline')
    if '\r' in stdout:
        require(stdout.count('\r\n') == stdout.count('\r') == stdout.count('\n'),
                'Uniform complete CRLF native output')
    lines = stdout.splitlines()
    bounds = ','.join(map(str, monitor['selected']['work']))
    reserved = [line for line in lines if line.startswith('ARROW_SLIME_')]
    require(len(reserved) == 12, 'Exactly twelve complete scene receipts')
    for name in ('frame', 'canvas'):
        rows = [line for line in lines if line.startswith('ARROW_SLIME_MONITOR window=' + name + ' ')]
        match = re.fullmatch(r'ARROW_SLIME_MONITOR window=' + name +
                             r' x=(-?[0-9]+) y=(-?[0-9]+) width=([1-9][0-9]*) height=([1-9][0-9]*) target=' +
                             re.escape(bounds), rows[0]) if len(rows) == 1 else None
        require(match is not None, 'One typed monitor receipt per source window')
        x, y, width, height = map(int, match.groups())
        left, top, available_width, available_height = monitor['selected']['work']
        require(x >= left and y >= top and width == 854 and height == 480 and
                x + width <= left + available_width and y + height <= top + available_height,
                'Both source windows on physical monitor2')
    ordered = (
        'ARROW_SLIME_INPUTS chunks=625 glowstone-y=63 saved-Slime=1 saved-Arrow=1 time=13000 player-health=20 parent-health=4 parent-size=2 parent-NBT-Size=1 pre-bootstrap-only=1',
        'ARROW_SLIME_INITIAL live-parent-health=4 size=2 live-Arrow=1 receipt-only=0',
        'ARROW_SLIME_CAUSAL saved-parent-health=4 observed-health=0 parent-absent=1 children=4 arrow-members=0 ordinary-world-ticks=1 direct741-receipt=0 direct-K-receipt=0 owned-Mob-environment=0',
        'ARROW_SLIME_FRAME phase=split terrain=1 HUD=1 player-health=20 children=4 child-health=1 child-size=1 empty-slots=9 source-px=1013 source-uq=2101',
        'ARROW_SLIME_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 player-health=20 children=4 child-health=1 child-size=1 arrow-members=0 parent-absent=1',
        'ARROW_SLIME_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Slime-children=4 saved-parent=0 saved-Arrow=0 new-title=1')
    require(all(lines.count(line) == 1 for line in ordered) and
            [lines.index(line) for line in ordered] == sorted(lines.index(line) for line in ordered),
            'Ordered live causal stages, returned terrain/HUD and SDK pause/save')
    zero = [line for line in lines if line.startswith('ARROW_SLIME_ZERO ')]
    zero_match = re.fullmatch(r'ARROW_SLIME_ZERO live-parent-health=0 dying=1 death-age=([0-9]+) arrow-members=0 world-ticks=([1-9][0-9]*)',
                              zero[0]) if len(zero) == 1 else None
    split = [line for line in lines if line.startswith('ARROW_SLIME_SPLIT ')]
    split_match = re.fullmatch(r'ARROW_SLIME_SPLIT parent-absent=1 children=4 child-size=1 child-health=1 arrow-members=0 world-ticks=([1-9][0-9]*)',
                               split[0]) if len(split) == 1 else None
    observed = [line for line in lines if line.startswith('ARROW_SLIME_OBSERVE ')]
    match = re.fullmatch(r'ARROW_SLIME_OBSERVE initial-tick=([0-9]+) zero-tick=([1-9][0-9]*) split-tick=([1-9][0-9]*) first-death-age=([0-9]+) greatest-death-age=([0-9]+)',
                         observed[0]) if len(observed) == 1 else None
    require(match is not None and zero_match is not None and split_match is not None,
            'Typed live health0, split and ordinary World tick observations')
    initial_tick, zero_tick, split_tick, first_age, greatest_age = map(int, match.groups())
    require(initial_tick <= zero_tick < split_tick and 0 <= first_age <= greatest_age <= 20 and
            split_tick - zero_tick >= 21 - first_age and
            int(zero_match[1]) == first_age and int(zero_match[2]) == zero_tick and
            int(split_match[1]) == split_tick,
            'Observed death-age prefix and remaining twenty-one-tick wait')
    footer = [line for line in lines if line.startswith('ARROW_SLIME_FINISH ')]
    require(len(footer) == 1 and re.fullmatch(
        r'ARROW_SLIME_FINISH cleanup=1 windows-absent=1 checks=[1-9][0-9]*', footer[0]),
        'Checked terminal cleanup and absent source windows')
    required_order = [*ordered[:2], zero[0], split[0], ordered[2], observed[0], *ordered[3:], footer[0]]
    require([lines.index(line) for line in required_order] ==
            sorted(lines.index(line) for line in required_order) and lines[-1] == footer[0],
            'Complete ordered causal protocol ends only after checked cleanup')
    require(not any(line.startswith(('ARROW_SLIME_FAILURE', 'WORLD_NATIVE_FOCUS',
                                    'WORLD_REPEAT_FOCUS_FAILURE', 'BED_PLACEMENT_FAILURE')) for line in lines),
            'Failed source/focus opportunities never qualify')
    return dict(observation='causal', direct_741_receipt=False, direct_K_receipt=False,
                owned_Mob_environment=False, input_parent_health=4, observed_parent_health=0,
                parent_removed=True, children=4, child_health=1, child_size=1, player_health=20,
                arrows_remaining=0, live_initial_pair=True, initial_tick=initial_tick,
                zero_tick=zero_tick, split_tick=split_tick, first_death_age=first_age,
                greatest_death_age=greatest_age, SDK_pause_save=True, cleanup=True)


def saved_children(entities, paused):
    require(type(entities) is list and type(paused) is list and len(entities) == len(paused) == 4,
            'Exactly four paused and four committed child occurrences')
    for child in paused + entities:
        require(type(child) is dict and zombie.same_tagged_value(child.get('id'), [8, 'Slime']) and
                zombie.same_tagged_value(child.get('Health'), [2, 1]) and
                zombie.same_tagged_value(child.get('Size'), [3, 0]) and
                zombie.same_tagged_value(child.get('DeathTime'), [2, 0]),
                'Only actual living size1/Health1 Slimes; parent and Arrow absent')
    for index, child in enumerate(paused):
        require(not any(zombie.same_tagged_value(child, earlier) for earlier in paused[:index]),
                'Four distinct complete paused child records')
    unmatched = list(entities)
    for child in paused:
        matches = [index for index, saved in enumerate(unmatched)
                   if zombie.same_tagged_value(child, saved)]
        require(len(matches) == 1, 'One complete committed match per paused child')
        unmatched.pop(matches[0])
    require(unmatched == [], 'Complete committed Entity inventory consumed once')
    return entities


def readback(nbt, helper, saves, evidence):
    player = nbt.Nbt(nbt.read(evidence / 'arrow-slime-player-paused.nbt')).root()
    children = [nbt.Nbt(nbt.read(evidence / f'arrow-slime-child-{index}-paused.nbt')).root()
                for index in range(4)]
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
    saved_children(entities, children)
    return dict(time=data['Time'][1], player_health=20, child_health=1, child_size=1,
                slimes=4, parent=0, arrows=0,
                snapshots_match_save=True, complete_entity_inventory=entities, chunks=inventory)


def self_test():
    world, nbt, helper = bed.modules()
    refused = 0
    with tempfile.TemporaryDirectory(prefix='arrow-slime-input-') as name:
        root = Path(name)
        region = root / 'region'; region.mkdir()
        level = root / 'level.dat'
        raw = metadata(world, 'Arrow Slime encounter', 1304000000000)
        level.write_bytes(gzip.compress(raw, mtime=0))
        author_regions(world, helper, region)
        inputs = input_check(nbt, helper, level, region)
        require(inputs['arrows'] == 1 and inputs['chunks'] == 625 and
                inputs['chunk_bounds'] == [-12, 12], 'Independent complete625-Chunk input roundtrip')
        outer = region / 'r.-1.-1.mcr'
        complete = outer.read_bytes()
        missing = bytearray(complete)
        struct.pack_into('>I', missing, ((-12 & 31) + (-12 & 31) * 32) * 4, 0)
        outer.write_bytes(missing)
        try:
            input_check(nbt, helper, level, region)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Missing outer prepared Chunk accepted')
        outer.write_bytes(complete)  # Pure authored-input comparator control, before any native run.
        bad = raw.replace(world.tag(2, 'Health', struct.pack('>h', 20)),
                          world.tag(2, 'Health', struct.pack('>h', 19)), 1)
        level.write_bytes(gzip.compress(bad, mtime=0))
        try:
            input_check(nbt, helper, level, region)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Wrong initial Player health accepted')
    for key, value in (('Health', [2, 0]), ('Size', [3, 2]), ('Size', [2, 1]),
                        ('Motion', [9, [6, [0, 0, 0]]]), ('inGround', [1, 1]), ('Shooter', [4, 1])):
        actors = copy.deepcopy(expected_entities())
        actors[0 if key in ('Health', 'Size') else 1][key] = value
        try:
            entity_inputs(actors)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupt saved Entity input accepted: ' + key)
    paused = [{'id': [8, 'Slime'], 'Health': [2, 1], 'Size': [3, 0], 'DeathTime': [2, 0],
               'Pos': [9, [6, zombie.bits((.25 + index, 64., 5.5), 'd')]]}
              for index in range(4)]
    committed = list(reversed(copy.deepcopy(paused)))
    require(saved_children(committed, paused) is committed,
            'Complete independent child records match across Chunk list ordering')
    for actors in ([], paused[:3], paused + [{'id': [8, 'Arrow']}],
                    [paused[0], paused[0], paused[2], paused[3]],
                    [{**paused[0], 'Health': [2, 4]}, *paused[1:]],
                    [{**paused[0], 'Size': [3, 1]}, *paused[1:]],
                    [{**paused[0], 'DeathTime': [2, 1]}, *paused[1:]],
                    [{**paused[0], 'extra': [1, 0]}, *paused[1:]]):
        try:
            saved_children(actors, paused)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Missing/duplicate/corrupt child, parent or surviving Arrow accepted')
    monitor = dict(selected=dict(work=[100, 200, 1920, 1080]))
    lines = [f'ARROW_SLIME_MONITOR window={name} x=100 y=200 width=854 height=480 target=100,200,1920,1080'
             for name in ('frame', 'canvas')]
    lines += [
        'ARROW_SLIME_INPUTS chunks=625 glowstone-y=63 saved-Slime=1 saved-Arrow=1 time=13000 player-health=20 parent-health=4 parent-size=2 parent-NBT-Size=1 pre-bootstrap-only=1',
        'ARROW_SLIME_INITIAL live-parent-health=4 size=2 live-Arrow=1 receipt-only=0',
        'ARROW_SLIME_ZERO live-parent-health=0 dying=1 death-age=0 arrow-members=0 world-ticks=4',
        'ARROW_SLIME_SPLIT parent-absent=1 children=4 child-size=1 child-health=1 arrow-members=0 world-ticks=25',
        'ARROW_SLIME_CAUSAL saved-parent-health=4 observed-health=0 parent-absent=1 children=4 arrow-members=0 ordinary-world-ticks=1 direct741-receipt=0 direct-K-receipt=0 owned-Mob-environment=0',
        'ARROW_SLIME_OBSERVE initial-tick=0 zero-tick=4 split-tick=25 first-death-age=0 greatest-death-age=20',
        'ARROW_SLIME_FRAME phase=split terrain=1 HUD=1 player-health=20 children=4 child-health=1 child-size=1 empty-slots=9 source-px=1013 source-uq=2101',
        'ARROW_SLIME_PAUSE escape-sdk=1 source-pause=1 keyboard-drained=1 simulation-paused=1 player-health=20 children=4 child-health=1 child-size=1 arrow-members=0 parent-absent=1',
        'ARROW_SLIME_SAVE button-id=1 source-leave=366 saved-Player=1 saved-Slime-children=4 saved-parent=0 saved-Arrow=0 new-title=1',
        'ARROW_SLIME_FINISH cleanup=1 windows-absent=1 checks=1']
    good = '\n'.join(lines) + '\n'
    require(receipts(good, monitor)['direct_741_receipt'] is False and
            receipts(good.replace('\n', '\r\n'), monitor)['children'] == 4,
            'Causal scope and uniform LF/CRLF protocols')
    for bad in (good.replace('direct741-receipt=0', 'direct741-receipt=1'),
                good.replace('chunks=625', 'chunks=289'),
                good.replace('direct-K-receipt=0', 'direct-K-receipt=1'),
                good.replace('owned-Mob-environment=0', 'owned-Mob-environment=1'),
                good.replace('live-parent-health=4', 'live-parent-health=3'),
                good.replace('receipt-only=0', 'receipt-only=1'),
                good.replace('observed-health=0', 'observed-health=1'),
                good.replace('arrow-members=0', 'arrow-members=1'),
                good.replace('source-leave=366', 'source-leave=0'),
                good.replace('split-tick=25', 'split-tick=24').replace('world-ticks=25', 'world-ticks=24'),
                good.replace('greatest-death-age=20', 'greatest-death-age=21'),
                good.replace('window=canvas x=100', 'window=canvas x=99'),
                good.replace('checks=1', 'checks=0'),
                good[:-1], good.replace('\n', '\r\n', 1),
                '\n'.join(lines[:-1]) + '\n', good + 'WORLD_NATIVE_FOCUS failed\n',
                good + lines[3] + '\n'):
        try:
            receipts(bad, monitor)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupted causal/focus/save/cleanup receipt accepted')
    print(f'PASS: Arrow/Slime625-chunk input/readback/causal receipt controls; {refused} corruption refusals; no native process')


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
    evidence = Path(tempfile.mkdtemp(prefix='arrow-slime-', dir=args.work_dir.resolve()))
    appdata = evidence / 'appdata'; game = appdata / '.minecraft'
    world.repeat_statistics(game, assets)
    (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
    saves = game / 'saves'; saves.mkdir()
    for folder, display, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000),
                                    ('alpha', 'Arrow Slime encounter', 1304000000000)):
        directory = saves / folder; directory.mkdir()
        raw = metadata(world, display, played) if folder == 'alpha' else world.metadata(display, played, 19132)
        (directory / 'level.dat').write_bytes(gzip.compress(raw, mtime=0))
        (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated Arrow Slime scene bytes\0\xff')
    region = saves / 'alpha/region'; region.mkdir()
    author_regions(world, helper, region)
    inputs = input_check(nbt, helper, saves / 'alpha/level.dat', region)
    before_files = world.save_title_files(saves, evidence / 'before')
    source, resources = bed.fingerprints(input_paths(executable)), world.snapshot(assets)
    environment = os.environ.copy()
    environment.update(APPDATA=str(appdata), LOCALAPPDATA=str(appdata), USERPROFILE=str(appdata),
                       HOME=str(appdata), ALSOFT_DRIVERS='null',
                       SDL_DISPLAY_USABLE_BOUNDS=','.join(map(str, monitor['selected']['work'])))
    report = dict(status='FAIL', scope=__doc__, inputs=inputs, monitor=monitor,
                  source_product_before=source, assets_before=resources)
    try:
        result = world.retained_native_run([str(executable), str(assets), 'loaded-arrow-slime', str(evidence)],
            environment, evidence, saves, before_files, 'arrow-slime', time.time_ns() // 1000000,
            bed.NATIVE_SECONDS)
        print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr)
        require(result.returncode == 0 and result.stderr == '', 'Native Arrow/Slime scene failed; retain this opportunity')
        report['native'] = receipts(result.stdout, monitor)
        report['readback'] = readback(nbt, helper, saves, evidence)
        after = world.save_title_files(saves, evidence / 'after-verified')
        require(all(after.get(name) == value for name, value in before_files.items()
                    if not name.startswith('alpha/') or name.endswith('/preserve-chunk.dat')),
                'Unrelated saved Worlds/files preserved')
        report['screenshots'] = {}
        for phase in ('split', 'paused', 'title'):
            path = evidence / ('arrow-slime-' + phase + '.png'); data = path.read_bytes()
            require(data.startswith(b'\x89PNG\r\n\x1a\n') and struct.unpack('>II', data[16:24]) == (854, 480),
                    'Actual returned854x480 scene frame: ' + phase)
            report['screenshots'][path.name] = hashlib.sha256(data).hexdigest()
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = dict(type=type(error).__name__, message=str(error)); raise
    finally:
        report['source_product_after'] = bed.fingerprints(input_paths(executable))
        report['assets_after'] = world.snapshot(assets)
        unchanged = source == report['source_product_after'] and resources == report['assets_after']
        if not unchanged:
            report['status'] = 'FAIL'
        (evidence / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        require(unchanged, 'Source/product/imported assets changed during native opportunity')
    print('Saved Arrow/Slime causal SDK scene: PASS; ' + str(evidence / 'report.json'))


if __name__ == '__main__':
    main()
