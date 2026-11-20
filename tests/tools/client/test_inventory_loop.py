"""Genuine SDK Inventory drawing, slot movement, cursor close and saved NBT."""
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

ROOT = Path(__file__).resolve().parents[3]
sys.dont_write_bytecode = True


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def authored_metadata(mining, selected, world, name, helmet):
    wire = mining.mining_metadata(selected, world, name)
    empty = world.tag(9, 'Inventory', b'\x0a\x00\x00\x00\x00')
    items = []
    declared = [(9, 3, 7), (10, 262, 10)] + ([(103, 86, 1)] if helmet else [])
    for slot, item_id, count in declared:
        items.append(world.tag(1, 'Slot', struct.pack('>b', slot)) +
                     world.tag(2, 'id', struct.pack('>h', item_id)) +
                     world.tag(1, 'Count', struct.pack('>b', count)) +
                     world.tag(2, 'Damage', b'\x00\x00') + b'\x00')
    require(wire.count(empty) == 1, 'One authored saved Inventory before bootstrap')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', len(items)) + b''.join(items)))


def inventory(player, helmet):
    value = player.get('Inventory')
    expected = [
        {'Slot': [1, 0], 'id': [2, 3], 'Count': [1, 1], 'Damage': [2, 0]},
        {'Slot': [1, 10], 'id': [2, 262], 'Count': [1, 10], 'Damage': [2, 0]},
    ]
    if helmet:
        expected.append({'Slot': [1, 103], 'id': [2, 86], 'Count': [1, 1], 'Damage': [2, 0]})
    require(value == [9, [10, expected]], 'Exact independently decoded final Inventory slots/fields')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--helmet', action='store_true')
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='inventory-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    selected = module('inventory_selection', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('inventory_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('inventory_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    decoder = selected.module('inventory_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    inputs = [Path(__file__).resolve(), executable, ROOT / 'tests/client/gameplay/test_inventory.c',
              ROOT / 'tests/client/gameplay/test_mining.c',
              ROOT / 'tests/client/startup/test_world_selection_loop.c']
    inputs += sorted(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                     if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    before_inputs = {str(path): digest(path) for path in inputs}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'evidence': str(evidence),
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'authored_inventory': [[9, 3, 7, 0], [10, 262, 10, 0]], 'helmet': args.helmet}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Inventory world', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                metadata = authored_metadata(mining, selected, world, name, args.helmet) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
                    gzip.compress(metadata, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            region = saves / 'alpha/region'
            region.mkdir()
            selected.saved_regions(world, region, {'id': 3, 'metadata': 0})
            unrelated = region / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            unrelated_before = unrelated.read_bytes()
            before = world.save_title_files(saves, evidence / 'before')
            environment = os.environ.copy()
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            command = [str(executable), str(assets), str(evidence)] + (['--helmet'] if args.helmet else [])
            result = world.retained_native_run(command,
                environment, evidence, saves, before, 'inventory', time.time_ns() // 1000000, 420)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native Inventory SDK executable failed')
            receipt = re.search(r'^INVENTORY_SDK draws=(\d+) world-ticks=(\d+) same-player=1 same-container=1 slots=45 left-right=1 tooltip=1 held-dirt=1 cursor-close-drop=1 dirt-count=6 ordinary-save=1 helmet=([01])$', result.stdout, re.M)
            require(receipt is not None and int(receipt[1]) >= 3 and int(receipt[2]) >= 3 and
                    bool(int(receipt[3])) == args.helmet,
                    'Returned actual Inventory frames, continuing World ticks and source interaction receipt')
            require(re.search(r'^INVENTORY_FINISH cleanup=1 windows-absent=1 checks=\d+$', result.stdout, re.M),
                    'Checked native retirement and actual absent windows')
            semantic = decoder.Nbt((evidence / 'inventory-player.nbt').read_bytes()).root()
            saved = decoder.Nbt(gzip.decompress((saves / 'alpha/level.dat').read_bytes())).root()['Data'][1]['Player'][1]
            report['semantic_inventory'] = inventory(semantic, args.helmet)
            report['saved_inventory'] = inventory(saved, args.helmet)
            saved_items, saved_chunks = mining.saved_items(decoder, saves / 'alpha')
            dirt = [row for row in saved_items if row['entity']['Item'][1].get('id') == [2, 3]]
            require(len(dirt) == 1 and dirt[0]['entity']['Item'][1] ==
                    {'id': [2, 3], 'Count': [1, 6], 'Damage': [2, 0]},
                    'Exact independently decoded cursor-close Dirt6 World Item saved once')
            report['saved_cursor_drop'] = dirt[0]
            report['saved_chunks'] = saved_chunks
            require(unrelated.read_bytes() == unrelated_before, 'Unrelated actual region bytes unchanged')
            snapshot = evidence / 'snapshots/inventory-saved/alpha'
            require((snapshot / 'region/r.9.9.mcr').read_bytes() == unrelated_before,
                    'Ordinary Save snapshot preserves unrelated actual region bytes')
            for image in ('inventory-open.png', 'inventory-moved.png', 'inventory-held-dirt.png'):
                require((evidence / image).is_file() and (evidence / image).stat().st_size > 1000,
                        'Actual native frame capture ' + image)
            report['receipt'] = receipt.group(0)
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['assets_after'] = world.snapshot(assets)
        report['inputs_unchanged'] = report['inputs_after'] == before_inputs
        report['assets_unchanged'] = report['assets_after'] == before_assets
        if not report['inputs_unchanged'] or not report['assets_unchanged']:
            report['status'] = 'FAIL'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Inventory evidence and source/assets continuity')
    print('Inventory SDK/save acceptance PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
