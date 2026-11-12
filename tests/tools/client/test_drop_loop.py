"""Real Q drop of collected Dirt, empty-hand Q, Item drawing and save/reselection."""
import argparse
from collections import Counter
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
sys.dont_write_bytecode = True


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def drop_receipts(mining, stdout):
    inputs = mining.rows(stdout, 'DROP_INPUT')
    returned = mining.rows(stdout, 'DROP_RETURN')
    saved = mining.rows(stdout, 'DROP_SAVE_ENTRY')
    reloaded = mining.rows(stdout, 'DROP_RELOAD')
    finish = mining.rows(stdout, 'DROP_FINISH')
    mining.require(len(inputs) == 2 and all(len(rows) == 1 for rows in
                   (returned, saved, reloaded, finish)), 'Complete actual drop lifecycle receipts')
    for row, nonempty in zip(inputs, ('1', '0')):
        mining.require(row.get('nonempty') == row.get('inserted') == nonempty and
                       row.get('calls') == row.get('inventory-empty') == row.get('live-dirt') == '1',
                       'One real Q call per pressed key; empty hand creates no second Item')
    mining.require(int(returned[0]['empty-frames']) >= 3 and int(returned[0]['item-frames']) >= 3 and
                   returned[0].get('source-drop-statistic') == '1',
                   'Returned empty HUD/hand and real dropped-Item draw frames')
    mining.require(int(saved[0]['samples']) > 0 and 1 <= int(saved[0]['all-items']) <= 128 and
                   saved[0].get('paused') == saved[0].get('owned-nbt') == '1',
                   'Actual paused save-entry semantic snapshot')
    mining.require(reloaded[0] == {'epochs': '4', 'inventory-empty': '1', 'dirt-items': '1',
                                  'ordinary-save-title': '1'} and
                   finish[0].get('cleanup') == finish[0].get('windows-absent') == '1',
                   'Fourth ordinary World load and checked cleanup')
    return {'inputs': inputs, 'returned': returned[0], 'entry': saved[0], 'reload': reloaded[0]}


def compare_saved_items(mining, items, entry_items, loaded, previous):
    active = [row for row in items if tuple(row['chunk']) in loaded]
    preserved = [row for row in items if tuple(row['chunk']) not in loaded]
    old_preserved = [row for row in previous if tuple(row['chunk']) not in loaded]
    mining.require(Counter((tuple(row['chunk']), mining.item_key(row['entity'])) for row in preserved) ==
                   Counter((tuple(row['chunk']), mining.item_key(row['entity'])) for row in old_preserved),
                   'Every unloaded saved Item preserves its exact pre-save Chunk and typed body')
    mining.require(Counter(mining.item_key(row['entity']) for row in active) ==
                   Counter(mining.item_key(entity) for entity in entry_items),
                   'Every loaded saved Item matches actual live entry NBT')
    return active, preserved


def drop_semantics(mining, parser, evidence):
    saved = evidence / 'snapshots/saved-3/alpha'
    root = parser.inflate(parser.read(saved / 'level.dat'), 31)
    mining.require(root.get('Data', [None])[0] == 10 and
                   root['Data'][1].get('Player', [None])[0] == 10, 'Actual saved drop Player')
    player = root['Data'][1]['Player'][1]
    entry = parser.Nbt(parser.read(evidence / 'drop-player-entry.nbt')).root()
    reopened = parser.Nbt(parser.read(evidence / 'drop-player-reloaded.nbt')).root()
    mining.same_fields(player, entry,
                       ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual dropped Player save versus selected live entry')
    mining.same_fields(player, reopened, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual fourth-epoch stable Player fields')
    mining.inventory(player, 0)
    mining.inventory(reopened, 0)
    mining.require(player.get('Score') == [3, 7], 'Authored Score7 remains intact')
    level = mining.selected_chunk(parser, saved)
    mining.require(level.get('Blocks', [None])[0] == 7 and len(level['Blocks'][1]) == 32768 and
                   level['Blocks'][1][(2 << 7) | 64] == 0 and
                   level.get('Data', [None])[0] == 7 and len(level['Data'][1]) == 16384 and
                   level['Data'][1][((2 << 7) | 64) // 2] & 15 == 0,
                   'Mined coordinate remains air/metadata0')
    items, chunks = mining.saved_items(parser, saved)
    count = int(parser.read(evidence / 'drop-items.count').decode('ascii').strip())
    mining.require(1 <= count <= 128, 'Bounded drop entry Item inventory')
    entry_items = [parser.Nbt(parser.read(evidence / f'drop-items-{i:04d}.nbt')).root()
                   for i in range(count)]
    coordinates = [tuple(map(int, line.split())) for line in
                   parser.read(evidence / 'drop-loaded-chunks.txt').decode('ascii').splitlines()]
    loaded = set(coordinates)
    mining.require(0 < len(loaded) == len(coordinates) <= 4096 and
                   all(len(row) == 2 for row in coordinates), 'Actual loaded Chunk inventory')
    receipts = mining.rows((evidence / 'stdout.txt').read_text(encoding='utf-8'),
                           'WORLD_SAVE_TITLE_ENTRY')
    mining.require(len(receipts) == 3, 'Three selected save-entry frames')
    entry_disk = evidence / 'entries/epoch-3' / f"frame-{int(receipts[2]['frame'])}" / 'alpha'
    previous, _ = mining.saved_items(parser, entry_disk)
    active, preserved = compare_saved_items(mining, items, entry_items, loaded, previous)
    dirt = [row['entity'] for row in items if row['entity']['Item'][1]['id'] == [2, 3]]
    mining.require(len(dirt) == 1, 'One actual Q-dropped Dirt Item persisted')
    live_item = parser.Nbt(parser.read(evidence / 'drop-item-entry.nbt')).root()
    reopened_item = parser.Nbt(parser.read(evidence / 'drop-item-reloaded.nbt')).root()
    mining.same_fields(dirt[0], live_item, ('Pos', 'Motion', 'Rotation', 'Health', 'Age', 'Item'),
                       'Q-dropped Item save versus selected live entry')
    mining.same_fields(dirt[0], reopened_item, ('Health', 'Item'),
                       'Fourth-epoch Q-dropped Item stable fields')
    mining.require(dirt[0]['Item'][1].get('Count') == [1, 1] and
                   dirt[0]['Item'][1].get('Damage') == [2, 0] and
                   reopened_item['Age'][1] >= dirt[0]['Age'][1],
                   'Actual dropped Stack and ordinary reloaded Item age')
    return {'block': 0, 'metadata': 0, 'inventory_slots': 0, 'dirt_items': 1,
            'all_items': len(items), 'live_items': len(active), 'preserved_items': len(preserved),
            'loaded_chunks': len(loaded), 'saved_chunks': chunks,
            'saved_level_sha256': mining.digest(saved / 'level.dat')}


def main():
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument('--executable', required=True, type=Path)
    arguments.add_argument('--assets', required=True, type=Path)
    arguments.add_argument('--artifacts', required=True, type=Path)
    args = arguments.parse_args()
    mining = module('drop_mining_evidence', ROOT / 'tests/tools/client/test_mining_loop.py')
    mining.require(os.name == 'nt', 'Disposable Linux account/mount qualification required before live World inputs')
    selected = mining.module('drop_saved_inputs', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('drop_world_evidence', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    parser = selected.module('drop_nbt_decoder', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='collected-dirt-drop-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = [Path(__file__).resolve(), executable, ROOT / 'tests/client/gameplay/test_drop.c',
              ROOT / 'tests/client/gameplay/test_mining.c', ROOT / 'tests/tools/client/test_mining_loop.py',
              ROOT / 'tests/client/startup/test_world_selection_loop.c',
              ROOT / 'tests/client/startup/test_world_reload.inc', ROOT / 'tests/support/java_random.h',
              ROOT / 'tests/tools/client/test_block_selection.py',
              ROOT / 'tests/tools/client/test_world_selection_loop.py',
              ROOT / 'tests/tools/world/test_source_saving.py']
    inputs += sorted(path for folder in ('src', 'include', 'tests/reference/entity/player_drop')
                     for path in (ROOT / folder).rglob('*') if path.is_file() and
                     path.suffix in ('.c', '.h', '.inc', '.py', '.json', '.md'))
    before_inputs = {str(path): mining.digest(path) for path in inputs}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'evidence': str(evidence),
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'authored_player_input': {'feet': [.5, 64., -.5], 'saved_y': '64 + binary32(1.62)',
                  'rotation': [0., 30.], 'motion': [0., 0., 0.], 'target': [0, 64, 2],
                  'source': 'Saved Player NBT before bootstrap; subsequent movement uses real SDK keys'}}
    try:
        with tempfile.TemporaryDirectory(prefix='drop-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Drop world', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                metadata = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                if folder == 'alpha':
                    actual = parser.Nbt(metadata).root()['Data'][1]['Player'][1]
                    report['authored_pose_from_nbt'] = {
                        'position_double_bits': [f'{v:016x}' for v in actual['Pos'][1][1]],
                        'rotation_float_bits': [f'{v:08x}' for v in actual['Rotation'][1][1]],
                        'metadata_sha256': hashlib.sha256(metadata).hexdigest()}
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(
                    gzip.compress(metadata, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            region = saves / 'alpha/region'
            region.mkdir()
            selected.saved_regions(world, region, {'id': 3, 'metadata': 0})
            before = world.save_title_files(saves, evidence / 'before')
            environment = os.environ.copy()
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence)],
                     environment, evidence, saves, before, 'collected-dirt-drop', first, 1200)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            mining.require(result.returncode == 0, 'Native drop executable failed')
            report['mining'] = mining.receipts(result.stdout)
            report['mining_saved_semantics'] = mining.semantic_evidence(parser, evidence)
            report['drop'] = drop_receipts(mining, result.stdout)
            report['dropped_saved_semantics'] = drop_semantics(mining, parser, evidence)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    mining.require(after.get(name) == value, 'Drop changed unrelated authored input')
            mining.require(not any(not name.startswith('alpha/') for name in set(after) - set(before)),
                           'Drop created an unrelated World file')
            last_save = world.snapshot(evidence / 'snapshots/saved-3')
            final_disk = world.snapshot(saves)
            mining.require(set(last_save) == set(final_disk) and
                           all(final_disk[key] == value for key, value in last_save.items()
                               if Path(key).as_posix() != 'alpha/session.lock') and
                           (saves / 'alpha/session.lock').stat().st_size == 8,
                           'Fourth reselection changes only its actual eight-byte session token')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): mining.digest(path) for path in inputs}
        report['assets_after'] = world.snapshot(assets)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/imported resources changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    mining.require(report['status'] == 'PASS', 'Drop fixture input continuity failed')
    print(f'Actual SDK collected Dirt drop PASS; {evidence}')


if __name__ == '__main__':
    main()
