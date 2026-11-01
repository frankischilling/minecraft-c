"""Actual mining/pickup followed by SDK Dirt placement and ordinary save/reselection."""
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


def placement_receipts(mining, stdout):
    admitted = [row for row in mining.rows(stdout, 'PLACEMENT_INPUT')
                if int(row.get('clicks', '0')) > 0]
    returned = mining.rows(stdout, 'PLACEMENT_RETURN')
    saved = mining.rows(stdout, 'PLACEMENT_SAVE_ENTRY')
    reloaded = mining.rows(stdout, 'PLACEMENT_RELOAD')
    finish = mining.rows(stdout, 'PLACEMENT_FINISH')
    mining.require(len(admitted) == len(returned) == len(saved) == len(reloaded) == len(finish) == 1,
                   'Complete actual placement lifecycle receipts')
    mining.require(int(admitted[0]['clicks']) >= 1 and int(admitted[0]['block-uses']) >= 1,
                   'Actual right caller and Block-use calls may include a held-source invocation')
    for key in ('drained', 'admitted', 'placed', 'sounds', 'statistics',
                'count-stores', 'clears', 'swings'):
        mining.require(admitted[0].get(key) == '1', 'Actual source placement receipt: ' + key)
    mining.require(admitted[0].get('equips') == admitted[0].get('air-uses') == '0',
                   'Count1 clear bypasses RA reset and successful placement bypasses fallback')
    mining.require(returned[0].get('block') == '3' and returned[0].get('metadata') == '0' and
                   int(returned[0]['empty-frames']) >= 3 and int(returned[0]['clean-frames']) >= 3 and
                   int(returned[0]['completed-compiles']) > 0 and
                   returned[0].get('source-used-statistic') == '1',
                   'Actual returned empty HUD/hand, clean target DK and completed compile observations')
    mining.require(int(saved[0]['samples']) > 0 and 0 <= int(saved[0]['all-items']) <= 128 and
                   saved[0].get('paused') == saved[0].get('owned-nbt') ==
                   saved[0].get('selected-before-leave-frame') == '1',
                   'Owned live semantic snapshot before actual placement save/leave frame')
    mining.require(reloaded[0] == {'epochs': '4', 'block': '3', 'metadata': '0',
                   'inventory-empty': '1', 'dirt-items': '0', 'ordinary-save-title': '1'} and
                   finish[0].get('cleanup') == finish[0].get('windows-absent') == '1',
                   'Actual fourth World epoch and checked cleanup')
    return {'input': admitted[0], 'returned': returned[0], 'entry': saved[0], 'reload': reloaded[0]}


def placement_semantics(mining, parser, evidence):
    saved = evidence / 'snapshots/saved-3/alpha'
    root = parser.inflate(parser.read(saved / 'level.dat'), 31)
    mining.require(root.get('Data', [None])[0] == 10 and
                   root['Data'][1].get('Player', [None])[0] == 10, 'Actual saved placement Player')
    player = root['Data'][1]['Player'][1]
    entry = parser.Nbt(parser.read(evidence / 'placement-player-entry.nbt')).root()
    reopened = parser.Nbt(parser.read(evidence / 'placement-player-reloaded.nbt')).root()
    mining.same_fields(player, entry,
                       ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual placement save versus selected live entry Player')
    mining.same_fields(player, reopened, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual fourth-epoch Player stable semantic fields')
    mining.inventory(player, 0)
    mining.inventory(reopened, 0)
    mining.require(player.get('Score') == [3, 7], 'Authored Score7 remains intact')
    level = mining.selected_chunk(parser, saved)
    mining.require(level.get('xPos') == [3, 0] and level.get('zPos') == [3, 0] and
                   level.get('Blocks', [None])[0] == 7 and len(level['Blocks'][1]) == 32768 and
                   level['Blocks'][1][(2 << 7) | 64] == 3 and
                   level.get('Data', [None])[0] == 7 and len(level['Data'][1]) == 16384 and
                   level['Data'][1][((2 << 7) | 64) // 2] & 15 == 0,
                   'Independent actual saved placed coordinate is Dirt3/metadata0')
    items, chunks = mining.saved_items(parser, saved)
    count = int(parser.read(evidence / 'placement-items.count').decode('ascii').strip())
    mining.require(0 <= count <= 128, 'Bounded placement entry Item inventory')
    entry_items = [parser.Nbt(parser.read(evidence / f'placement-items-{i:04d}.nbt')).root()
                   for i in range(count)]
    mining.require(Counter(mining.item_key(row['entity']) for row in items) ==
                   Counter(mining.item_key(entity) for entity in entry_items),
                   'Every incidental saved Item matches actual live entry NBT')
    mining.require(not any(row['entity']['Item'][1]['id'] == [2, 3] for row in items),
                   'Consumed collected Dirt has no saved dropped Item')
    return {'block': 3, 'metadata': 0, 'coordinate': [0, 64, 2], 'inventory_slots': 0,
            'all_items': len(items), 'saved_chunks': chunks,
            'saved_level_sha256': mining.digest(saved / 'level.dat')}


def main():
    arguments = argparse.ArgumentParser(description=__doc__)
    arguments.add_argument('--executable', required=True, type=Path)
    arguments.add_argument('--assets', required=True, type=Path)
    arguments.add_argument('--artifacts', required=True, type=Path)
    args = arguments.parse_args()
    mining = module('placement_mining_evidence', ROOT / 'tests/tools/client/test_mining_loop.py')
    mining.require(os.name == 'nt', 'Disposable Linux account/mount qualification required before live World inputs')
    selected = mining.module('placement_saved_inputs', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('placement_world_evidence', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    parser = selected.module('placement_nbt_decoder', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='collected-dirt-placement-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = [Path(__file__).resolve(), executable, ROOT / 'tests/client/gameplay/test_placement.c',
              ROOT / 'tests/client/gameplay/test_mining.c', ROOT / 'tests/tools/client/test_mining_loop.py',
              ROOT / 'tests/client/startup/test_world_selection_loop.c',
              ROOT / 'tests/client/startup/test_world_reload.inc', ROOT / 'tests/support/java_random.h',
              ROOT / 'tests/tools/client/test_block_selection.py',
              ROOT / 'tests/tools/client/test_world_selection_loop.py',
              ROOT / 'tests/tools/world/test_source_saving.py']
    inputs += sorted(path for folder in ('src', 'include', 'tests/reference/client/placement')
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
        with tempfile.TemporaryDirectory(prefix='placement-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Placement world', 1304000000000)]:
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
                     environment, evidence, saves, before, 'collected-dirt-placement', first, 1200)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            mining.require(result.returncode == 0, 'Native placement executable failed')
            report['mining'] = mining.receipts(result.stdout)
            report['mining_saved_semantics'] = mining.semantic_evidence(parser, evidence)
            report['placement'] = placement_receipts(mining, result.stdout)
            report['placed_saved_semantics'] = placement_semantics(mining, parser, evidence)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    mining.require(after.get(name) == value, 'Placement changed unrelated authored input')
            mining.require(not any(not name.startswith('alpha/') for name in set(after) - set(before)),
                           'Placement created an unrelated World file')
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
    mining.require(report['status'] == 'PASS', 'Placement fixture input continuity failed')
    print(f'Actual SDK collected Dirt placement PASS; {evidence}')


if __name__ == '__main__':
    main()
