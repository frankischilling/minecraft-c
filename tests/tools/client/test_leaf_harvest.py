"""One real Windows SDK Leaf harvest with saved Shears and checked native cleanup."""
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
    stack = (world.tag(1, 'Slot', b'\x00') +
             world.tag(2, 'id', struct.pack('>h', 359)) +
             world.tag(1, 'Count', b'\x01') +
             world.tag(2, 'Damage', b'\x00\x00') + b'\x00')
    require(wire.count(empty) == 1, 'One exact saved empty Inventory input')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + stack))


def regions(selected, world, directory):
    selected.saved_regions(world, directory, {'id': 18, 'metadata': 2})
    path = directory / 'r.0.0.mcr'
    image = bytearray(path.read_bytes())
    location, = struct.unpack_from('>I', image, 0)
    sector, count = location >> 8, location & 255
    size, = struct.unpack_from('>I', image, sector * 4096)
    require(image[sector * 4096 + 4] == 2, 'Authored Leaf Chunk uses zlib')
    wire = zlib.decompress(image[sector * 4096 + 5:sector * 4096 + 4 + size])
    for name, length, index, value in [('HeightMap', 256, 32, 65),
                                      ('SkyLight', 16384, 160, 0xfe)]:
        marker = world.tag(7, name, struct.pack('>i', length))
        require(wire.count(marker) == 1, 'One exact authored ' + name + ' byte array')
        at = wire.index(marker) + len(marker)
        array = bytearray(wire[at:at + length])
        require(len(array) == length, 'Complete authored ' + name + ' span')
        array[index] = value
        wire = wire[:at] + bytes(array) + wire[at + length:]
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\x02' + compressed
    require(len(record) <= count * 4096, 'Consistent Leaf height/light fits the authored extent')
    image[sector * 4096:(sector + count) * 4096] = record + bytes(count * 4096 - len(record))
    path.write_bytes(image)


def receipts(mining, stdout):
    harvested = mining.rows(stdout, 'LEAF_SDK_HARVEST')
    finished = mining.rows(stdout, 'LEAF_SDK_FINISH')
    inputs = mining.rows(stdout, 'LEAF_SDK_INPUT')
    require(len(harvested) == len(finished) == 1 and inputs,
            'Complete native Leaf input, harvest and cleanup receipts')
    fixed = {'target-air': '1', 'mined': '1', 'shears-id': '359', 'count': '1', 'damage': '1',
             'used-stat': '0', 'broken-stat': '0', 'leaf-stat-null': '1', 'drop-id': '18',
             'drop-count': '1', 'drop-damage': '2', 'world-occurrences': '1',
             'chunk-occurrences': '1', 'focus': '1', 'item-draw': '1', 'event2001': '1',
             'destruction-particles': '64'}
    require(set(harvested[0]) == set(fixed) | {'actual-clicks', 'mining-starts'} and
            all(harvested[0][key] == value for key, value in fixed.items()) and
            int(harvested[0]['actual-clicks']) > 0 and int(harvested[0]['mining-starts']) > 0,
            'Actual SDK/controller harvest, exact surviving Shears and same published Item roles')
    require(finished[0].get('cleanup') == finished[0].get('windows-absent') == '1' and
            set(finished[0]) == {'cleanup', 'windows-absent', 'checks'} and
            int(finished[0]['checks']) > 0, 'Complete checked native graph/window retirement')
    return {'harvest': harvested[0], 'input': inputs, 'finish': finished[0]}


def semantic(mining, decoder, evidence):
    item = decoder.Nbt((evidence / 'leaf-item.nbt').read_bytes()).root()
    # This fixture invokes mc_entity_build_nbt, the actual Entity body writer.
    # The separate source Chunk wrapper supplies the String id on a save.
    require('id' not in item and item.get('Item') == [10, {
        'id': [2, 18], 'Count': [1, 1], 'Damage': [2, 2]}],
        'Independent Item NBT has one Leaf block with exact subtype metadata')
    player = decoder.Nbt((evidence / 'leaf-player.nbt').read_bytes()).root()
    inventory = player.get('Inventory')
    require(inventory == [9, [10, [{'Slot': [1, 0], 'id': [2, 359],
                                   'Count': [1, 1], 'Damage': [2, 1]}]]],
            'Independent Player NBT has only the surviving selected Shears with wear1')
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
    mining = module('leaf_sdk_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('leaf_sdk_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('leaf_sdk_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder = selected.module('leaf_sdk_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='leaf-sdk-', dir=args.artifacts.resolve()))
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
              'assets_before': before_assets, 'authored_inventory': [[0, 359, 1, 0]],
              'authored_target': [0, 64, 2, 18, 2],
              'authored_pose': {'feet': [.5, 64., -.5], 'rotation': [0., 30.]},
              'limits': 'One native single-World input/controller/publication scene; no original pixel/audio, long-session, full save roundtrip or FPS claim.'}
    try:
        with tempfile.TemporaryDirectory(prefix='leaf-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Leaf harvest', 1304000000000)]:
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
                environment, evidence, saves, before, 'leaf-sdk', time.time_ns() // 1000000, 300)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            report['native_returncode'] = result.returncode
            require(result.returncode == 0, 'Real SDK Leaf/controller process must return successfully')
            report['receipts'] = receipts(mining, result.stdout)
            report['semantic'] = semantic(mining, decoder, evidence)
            for name in ('leaf-before.png', 'leaf-after.png'):
                image = evidence / name
                require(image.is_file() and image.stat().st_size > 1000,
                        'Actual returned native source-window capture ' + name)
            stopped = evidence / 'snapshots/leaf-stopped'
            closed = evidence / 'snapshots/leaf-after-native-close'
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
    require(report['status'] == 'PASS', 'Native Leaf harvest and selected input continuity')
    print('Actual SDK Leaf harvest PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
