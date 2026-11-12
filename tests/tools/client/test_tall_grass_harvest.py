"""Native supported TallGrass punches, Seed drawing/pickup and checked cleanup."""
import argparse
import gzip
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def require(value, message):
    if not value:
        raise RuntimeError(message)


def metadata(mining, selected, world, name):
    wire = mining.mining_metadata(selected, world, name)
    empty = world.tag(9, 'Inventory', b'\x0a\x00\x00\x00\x00')
    stack = (world.tag(1, 'Slot', b'\x01') +
             world.tag(2, 'id', struct.pack('>h', 359)) +
             world.tag(1, 'Count', b'\x01') +
             world.tag(2, 'Damage', struct.pack('>h', 238)) + b'\x00')
    require(wire.count(empty) == 1, 'One exact saved empty Inventory input')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + stack))


def regions(selected, world, directory):
    selected.saved_regions(world, directory, {'id': 31, 'metadata': 1})
    path = directory / 'r.0.0.mcr'
    image = bytearray(path.read_bytes())
    for chunk_z in range(5):
        location, = struct.unpack_from('>I', image, chunk_z * 32 * 4)
        sector, count = location >> 8, location & 255
        size, = struct.unpack_from('>I', image, sector * 4096)
        require(image[sector * 4096 + 4] == 2, 'Authored TallGrass Chunk uses zlib')
        wire = zlib.decompress(image[sector * 4096 + 5:sector * 4096 + 4 + size])
        for name, length in [('Blocks', 32768), ('Data', 16384)]:
            marker = world.tag(7, name, struct.pack('>i', length))
            require(wire.count(marker) == 1, 'One exact authored TallGrass ' + name)
            at = wire.index(marker) + len(marker)
            array = bytearray(wire[at:at + length])
            require(len(array) == length, 'Complete authored TallGrass array')
            for plant in range(64):
                z = 2 + plant
                if z // 16 != chunk_z:
                    continue
                offset = ((z & 15) << 7) | 64
                if name == 'Blocks':
                    array[offset] = 31
                else:
                    array[offset // 2] = (array[offset // 2] & 0xf0) | (1 + plant % 3)
            wire = wire[:at] + bytes(array) + wire[at + length:]
        compressed = zlib.compress(wire)
        record = struct.pack('>I', len(compressed) + 1) + b'\x02' + compressed
        require(len(record) <= count * 4096, 'TallGrass row fits existing authored extents')
        image[sector * 4096:(sector + count) * 4096] = record + bytes(count * 4096 - len(record))
    path.write_bytes(image)


def receipts(mining, stdout):
    harvested = mining.rows(stdout, 'TALL_SDK_HARVEST')
    finished = mining.rows(stdout, 'TALL_SDK_FINISH')
    inputs = mining.rows(stdout, 'TALL_SDK_INPUT')
    births = mining.rows(stdout, 'TALL_SDK_BIRTH')
    removals = mining.rows(stdout, 'TALL_SDK_REMOVAL')
    draws = mining.rows(stdout, 'TALL_SDK_DRAW')
    held = mining.rows(stdout, 'TALL_SDK_HELD')
    observers = mining.rows(stdout, 'TALL_SDK_OBSERVER')
    require(len(harvested) == len(finished) == 1 and inputs,
            'Complete native TallGrass input, harvest and cleanup receipts')
    row = harvested[0]
    fixed = {'empty-hand': '1', 'shears-damage': '238', 'used-stat': '0', 'broken-stat': '0',
             'live-items': '0', 'item-draw': '1', 'sdk-movement': '1', 'pickup': '1',
             'held-frames': '3', 'held-icon': '9', 'held-vertices': '264',
             'inventory-draws': '3', 'focus': '1'}
    require(set(row) == set(fixed) | {'plants', 'shears-punches', 'mined-stat', 'seeds',
                                    'event2001', 'destruction-particles'} and
            all(row[key] == value for key, value in fixed.items()),
            'Exact real TallGrass/Seed source role receipts')
    plants, seeds = int(row['plants']), int(row['seeds'])
    require(2 <= plants <= 64 and 1 <= seeds <= 2 and
            int(row['shears-punches']) == plants - 1 and
            int(row['mined-stat']) == int(row['event2001']) == plants and
            int(row['destruction-particles']) == plants * 64 and len(inputs) == plants,
            'Bounded actual multi-punch Seed search and complete destruction publication')
    for index, event in enumerate(inputs):
        require(set(event) == {'z', 'attempt', 'mined', 'actual-clicks', 'release-consumed', 'focus'} and
                int(event['z']) == index + 2 and int(event['mined']) == index + 1 and
                1 <= int(event['attempt']) <= 12 and int(event['actual-clicks']) >= index + 1 and
                event['release-consumed'] in ('0', '1') and event['focus'] == '1',
                'Actual contiguous picked targets and consumed source clicks')
    # Match the exact float-product/double-margin extrema used by the protected
    # original Block spawn. These observations are made at entity_added before
    # Item motion; no proximity assumption is applied to the live World List.
    f32 = lambda value: struct.unpack('>f', struct.pack('>f', value))[0]
    low = f32(1.0 - f32(.7)) * .5
    high = f32(f32(16777215.0 / 16777216.0) * f32(.7)) + low
    require(len(births) == len(removals) == seeds and len(draws) == len(observers) == 1,
            'Each scenario Seed has one actual publication and removal; one actual BB draw')
    identities, targets = set(), set()
    for birth in births:
        require(set(birth) == {'target-z', 'entity', 'entity-id', 'stack', 'item-id', 'count',
                               'damage', 'age', 'delay', 'x', 'y', 'z', 'world', 'chunk'} and
                birth['item-id'] == '295' and birth['count'] == '1' and
                birth['damage'] == birth['age'] == '0' and birth['delay'] == '10' and
                birth['world'] == birth['chunk'] == '1' and
                int(birth['entity']) > 0 and int(birth['stack']) > 0,
                'Source publication has a ready Seed Stack, delay10 and actual World/Chunk aliases')
        target = int(birth['target-z'])
        identity = (birth['entity'], birth['entity-id'])
        require(2 <= target < 2 + plants and target not in targets and identity not in identities and
                low <= float(birth['x']) <= high and
                64.0 + low <= float(birth['y']) <= 64.0 + high and
                target + low <= float(birth['z']) <= target + high,
                'Seed birth belongs to its current actual punched target and exact source span')
        targets.add(target)
        identities.add(identity)
    removed = set()
    for event in removals:
        require(set(event) == {'entity', 'entity-id', 'dead'} and event['dead'] == '1',
                'Scenario Item source removal is observed after actual pickup death')
        identity = (event['entity'], event['entity-id'])
        require(identity in identities and identity not in removed,
                'Only an actual scenario publication satisfies each unique removal')
        removed.add(identity)
    draw = draws[0]
    require(set(draw) == {'entity', 'entity-id', 'source-pc', 'valid', 'returned', 'retired',
                          'primary', 'post', 'renderer'} and
            (draw['entity'], draw['entity-id']) in identities and
            draw['source-pc'] == '796' and draw['renderer'] == 'bb' and
            all(draw[key] == '1' for key in ('valid', 'returned', 'retired', 'primary', 'post')),
            'The actual scenario Seed returns and retires its complete BB primary/post draw')
    require(set(observers[0]) == {'removed', 'freed', 'births', 'unrelated'} and
            observers[0]['removed'] == observers[0]['freed'] == '1' and
            int(observers[0]['births']) == seeds and int(observers[0]['unrelated']) >= 0,
            'Observer registration is removed and freed before native shutdown')
    held_flags = {'hand-returned', 'held-returned', 'draw-returned', 'stack-captured',
                  'child-stack-null', 'flat', 'icon-returned', 'geometry-returned',
                  'hud-returned', 'hotbar-returned'}
    require(len(held) == 3, 'Three complete actual held Seed frames')
    for index, event in enumerate(held, 1):
        require(set(event) == held_flags | {'frame', 'selected', 'current', 'cache', 'item-id',
                                            'count', 'damage', 'empty-hand', 'icon', 'faces',
                                            'vertices'} and
                int(event['frame']) == index and event['selected'] == '0' and
                int(event['current']) > 0 and event['current'] == event['cache'] and
                event['item-id'] == '295' and int(event['count']) == seeds and
                event['damage'] == event['empty-hand'] == '0' and
                event['icon'] == '9' and event['faces'] == '66' and event['vertices'] == '264' and
                all(event[key] == '1' for key in held_flags),
                'Live selected Seed and RA-owned Stack identity accompany the completed flat draw')
    require(set(finished[0]) == {'cleanup', 'windows-absent', 'checks'} and
            finished[0]['cleanup'] == finished[0]['windows-absent'] == '1' and
            int(finished[0]['checks']) > 0, 'Complete checked native graph/window retirement')
    return {'harvest': row, 'input': inputs, 'birth': births, 'removal': removals,
            'draw': draw, 'held': held, 'observer': observers[0], 'finish': finished[0]}


def semantic(mining, decoder, evidence, receipt):
    item = decoder.Nbt((evidence / 'tall-grass-item.nbt').read_bytes()).root()
    require('id' not in item and item.get('Item') == [10, {
        'id': [2, 295], 'Count': [1, 1], 'Damage': [2, 0]}],
        'Independent Item body NBT has actual Seeds, not a TallGrass Block')
    player = decoder.Nbt((evidence / 'tall-grass-player.nbt').read_bytes()).root()
    seeds = int(receipt['harvest']['seeds'])
    require(player.get('Inventory') == [9, [10, [
        {'Slot': [1, 0], 'id': [2, 295], 'Count': [1, seeds], 'Damage': [2, 0]},
        {'Slot': [1, 1], 'id': [2, 359], 'Count': [1, 1], 'Damage': [2, 238]}]]],
        'Independent Player NBT has exactly picked Seeds and unchanged high-damage Shears')
    require(player.get('Health') == [2, 20] and player.get('Score') == [3, 7],
            'Authored Player health and score remain intact')
    return {'item': mining.typed([10, item]), 'player': mining.typed([10, player])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'This finite native SDK qualification is Windows only')
    mining = module('tall_sdk_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('tall_sdk_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('tall_sdk_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder = selected.module('tall_sdk_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='tall-grass-sdk-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {Path(__file__).resolve(), executable, ROOT / 'CMakeLists.txt',
              ROOT / 'tests/support/java_random.h', ROOT / 'tests/tools/world/test_source_saving.py'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(executable.parent.glob('*.dll'))
    before_inputs = {str(path): mining.digest(path) for path in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before_inputs,
              'assets_before': before_assets, 'authored_inventory': [[1, 359, 1, 238]],
              'authored_targets': [[0, 64, 2 + i, 31, 1 + i % 3] for i in range(64)],
              'authored_pose': {'feet': [.5, 64., -.5], 'rotation': [0., 30.]},
              'limits': 'One native supported TallGrass row; up to64 actual rolls search for Seeds without live entropy repair. Passive WorldAccess publication observes exact scenario births; unrelated World Items are permitted. No original pixels/audio, full save roundtrip, unrestricted gameplay or FPS claim.'}
    try:
        with tempfile.TemporaryDirectory(prefix='tall-grass-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'TallGrass harvest', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                wire = metadata(mining, selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
                    gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            region_directory = saves / 'alpha/region'
            region_directory.mkdir()
            regions(selected, world, region_directory)
            unrelated = region_directory / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            before = world.save_title_files(saves, evidence / 'before')
            environment = os.environ.copy()
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), str(evidence)],
                environment, evidence, saves, before, 'tall-grass-sdk', time.time_ns() // 1000000, 300)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            report['native_returncode'] = result.returncode
            require(result.returncode == 0, 'Real SDK TallGrass/controller process must return successfully')
            report['receipts'] = receipts(mining, result.stdout)
            report['semantic'] = semantic(mining, decoder, evidence, report['receipts'])
            for name in ('tall-grass-before.png', 'tall-grass-dropped.png', 'tall-grass-inventory.png', 'tall-grass-after.png'):
                image = evidence / name
                require(image.is_file() and image.stat().st_size > 1000,
                        'Actual returned native source-window capture ' + name)
            stopped = evidence / 'snapshots/tall-grass-stopped'
            closed = evidence / 'snapshots/tall-grass-after-native-close'
            require(stopped.is_dir() and closed.is_dir(), 'Both real stopped native cleanup snapshots')
            report['stopped_save'] = world.snapshot(stopped)
            report['closed_save'] = world.snapshot(closed)
            require(report['stopped_save'] == report['closed_save'] == world.snapshot(saves),
                    'Stopped save tree remains unchanged through checked native cleanup')
            after = world.save_title_files(saves, evidence / 'after')
            require(all(after.get(name) == value for name, value in before.items()
                        if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat' or
                        name == 'alpha/region/r.9.9.mcr'),
                    'Unselected Worlds and authored unrelated selected-World files are preserved')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): mining.digest(path) for path in sorted(inputs)}
        report['assets_after'] = world.snapshot(assets)
        report['inputs_unchanged'] = report['inputs_after'] == before_inputs
        report['assets_unchanged'] = report['assets_after'] == before_assets
        if not report['inputs_unchanged'] or not report['assets_unchanged']:
            report['status'] = 'FAIL'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Native TallGrass harvest and selected input continuity')
    print('Actual SDK TallGrass harvest PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
