"""One real Windows Sand mining, dropped Item, pickup, held and Inventory scene."""
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
    return mining.mining_metadata(selected, world, name)


def regions(selected, world, directory):
    selected.saved_regions(world, directory, {'id': 12, 'metadata': 0})
    path = directory / 'r.0.0.mcr'
    image = bytearray(path.read_bytes())
    location, = struct.unpack_from('>I', image, 0)
    sector, count = location >> 8, location & 255
    size, = struct.unpack_from('>I', image, sector * 4096)
    require(image[sector * 4096 + 4] == 2, 'Authored Sand Chunk uses zlib')
    wire = zlib.decompress(image[sector * 4096 + 5:sector * 4096 + 4 + size])
    for name, length, index, value in [('HeightMap', 256, 32, 65),
                                      ('SkyLight', 16384, 160, 0xf0)]:
        marker = world.tag(7, name, struct.pack('>i', length))
        require(wire.count(marker) == 1, 'One exact authored ' + name + ' byte array')
        at = wire.index(marker) + len(marker)
        array = bytearray(wire[at:at + length])
        require(len(array) == length, 'Complete authored ' + name + ' span')
        array[index] = value
        wire = wire[:at] + bytes(array) + wire[at + length:]
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\x02' + compressed
    require(len(record) <= count * 4096, 'Consistent Sand height/light fits the authored extent')
    image[sector * 4096:(sector + count) * 4096] = record + bytes(count * 4096 - len(record))
    path.write_bytes(image)


def receipts(mining, stdout):
    harvested = mining.rows(stdout, 'SAND_SDK_HARVEST')
    finished = mining.rows(stdout, 'SAND_SDK_FINISH')
    inventory = mining.rows(stdout, 'SAND_SDK_INVENTORY')
    require(len(harvested) == len(finished) == 1,
            'One complete actual Sand harvest/pickup/rendering and cleanup receipt')
    expected = {'target-air': '1', 'source-input': '1', 'mined': '1', 'block': '12',
                'drop-count': '1', 'damage': '0', 'same-world-chunk': '1', 'item-draw': '1',
                'particles': '64', 'statistic': '1', 'actual-movement': '1', 'pickup': '1',
                'live-items': '0', 'held-frames': '3', 'inventory-draw': '1', 'inventory-count': '1'}
    require(harvested[0] == expected, 'Exact real source-input, mining, Item draw and pickup roles')
    require(inventory == [{'slots': '45', 'bb-calls': '45', 'cursor': '0', 'same-container': '1'}],
            'Actual ID/BB call count includes NULL Stacks and the same real Player Container')
    require(set(finished[0]) == {'cleanup', 'windows-absent', 'checks'} and
            finished[0]['cleanup'] == finished[0]['windows-absent'] == '1' and
            int(finished[0]['checks']) > 0, 'Checked native ownership and both window retirement')
    return {'harvest': harvested[0], 'inventory': inventory[0], 'finish': finished[0]}


def semantic(mining, decoder, evidence):
    item = decoder.Nbt((evidence / 'sand-item.nbt').read_bytes()).root()
    require('id' not in item and item.get('Item') == [10, {
        'id': [2, 12], 'Count': [1, 1], 'Damage': [2, 0]}],
        'Independent Item body NBT contains exactly the naturally harvested Sand Stack')
    player = decoder.Nbt((evidence / 'sand-player.nbt').read_bytes()).root()
    require(player.get('Inventory') == [9, [10, [{'Slot': [1, 0], 'id': [2, 12],
                                              'Count': [1, 1], 'Damage': [2, 0]}]]],
            'Independent Player NBT contains only the picked-up Sand in the selected slot')
    require(player.get('Health') == [2, 20] and player.get('Score') == [3, 7],
            'Authored Player health and score survive actual mining and pickup')
    return {'item': mining.typed([10, item]), 'player': mining.typed([10, player])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'This finite native SDK qualification is Windows only')
    mining = module('sand_sdk_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('sand_sdk_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('sand_sdk_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder = selected.module('sand_sdk_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='sand-sdk-', dir=args.artifacts.resolve()))
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
              'assets_before': before_assets, 'authored_inventory': [],
              'authored_target': [0, 64, 2, 12, 0],
              'authored_pose': {'feet': [.5, 64., -.5], 'rotation': [0., 30.]},
              'limits': 'One native single-World input/controller/publication scene; no original pixel/audio, long-session, full save roundtrip or FPS claim.'}
    try:
        with tempfile.TemporaryDirectory(prefix='sand-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Sand harvest', 1304000000000)]:
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
                environment, evidence, saves, before, 'sand-sdk', time.time_ns() // 1000000, 300)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            report['native_returncode'] = result.returncode
            require(result.returncode == 0, 'Real SDK Sand/controller process must return successfully')
            report['receipts'] = receipts(mining, result.stdout)
            report['semantic'] = semantic(mining, decoder, evidence)
            for name in ('sand-before.png', 'sand-drop.png', 'sand-held.png', 'sand-inventory.png'):
                image = evidence / name
                require(image.is_file() and image.stat().st_size > 1000,
                        'Actual returned native source-window capture ' + name)
            stopped = evidence / 'snapshots/sand-stopped'
            closed = evidence / 'snapshots/sand-after-native-close'
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
    require(report['status'] == 'PASS', 'Native Sand harvest and selected input continuity')
    print('Actual SDK Sand harvest PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
