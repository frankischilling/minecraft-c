"""Actual SDK Apple/Soup input, ordinary save/reselection and independent Player/Region NBT."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(value, message):
    if not value:
        raise AssertionError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def metadata(world, name, played):
    wire = world.metadata(name, played, 19132, crops_player=True)
    empty = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
    stacks = []
    for slot, item, count in ((0, 260, 2), (1, 282, 3)):
        stacks.append(world.tag(1, 'Slot', bytes([slot])) +
                      world.tag(2, 'id', struct.pack('>h', item)) +
                      world.tag(1, 'Count', bytes([count])) +
                      world.tag(2, 'Damage', bytes(2)) + b'\0')
    health = world.tag(2, 'Health', struct.pack('>h', 20))
    require(wire.count(empty) == wire.count(health) == 1,
            'One declared saved Player Inventory and Health')
    wire = wire.replace(empty, world.tag(9, 'Inventory',
                                        b'\x0a' + struct.pack('>i', 2) + b''.join(stacks)))
    return wire.replace(health, world.tag(2, 'Health', struct.pack('>h', 5)))


def inventory(player, health, apples, second_id, second_count):
    require(player.get('Health') == [2, health], 'Exact actual Player Short Health')
    require(player.get('Inventory', [None])[0] == 9, 'Actual Player Inventory List')
    kind, values = player['Inventory'][1]
    require(kind == 10 and len(values) == 2, 'Exactly two genuine saved Stacks')
    for item, (slot, item_id, count) in zip(values, ((0, 260, apples),
                                                   (1, second_id, second_count))):
        require(item.get('Slot') == [1, slot] and item.get('id') == [2, item_id] and
                item.get('Count') == [1, count] and item.get('Damage') == [2, 0],
                'Exact food/bowl Stack record and source widths')


def exact(row, expected, numeric=()):
    require(set(row) == set(expected) | set(numeric) and
            all(row[key] == value for key, value in expected.items()) and
            all(row[key].isdigit() for key in numeric), 'Exact typed Food scene receipt')


def observations(farming, stdout):
    inputs = farming.rows(stdout, 'FOOD_INPUT')
    returned, saved, loaded, finish = [farming.rows(stdout, name) for name in
                                      ('FOOD_RETURN', 'FOOD_SAVE', 'FOOD_RELOAD', 'FOOD_FINISH')]
    require(len(inputs) == 2 and len(returned) == len(saved) == len(loaded) == len(finish) == 1,
            'Complete actual one-tap input/save/reload/cleanup receipts')
    for row, action in zip(inputs, ('apple', 'soup')):
        exact(row, {'action': action, 'taps': '1', 'right-clicks': '1', 'air-uses': '1',
                    'count-stores': '1', 'used': '0', 'cleared': '0', 'equip-reset': '1',
                    'child-retired': '1', 'drained': '1'})
    exact(returned[0], {'health-before': '5', 'health-apple': '9', 'health-soup': '19',
                        'apple-count': '1', 'displaced-soup-count': '2', 'bowl-id': '281',
                        'bowl-count': '1', 'same-apple': '1', 'fresh-bowl': '1', 'selected': '1',
                        'hud': '1', 'held': '1', 'used': '0', 'displaced-hold-released': '1'})
    exact(saved[0], {'health': '19', 'ground': '3', 'above': '0', 'source-save-title': '1',
                     'pre-frame-owned-nbt': '1'}, ('sample', 'candidates'))
    require(0 <= int(saved[0]['sample']) < int(saved[0]['candidates']),
            'Actual selected paused save entry')
    exact(loaded[0], {'distinct-world': '1', 'distinct-player': '1', 'health': '19',
                      'apple-count': '1', 'bowl-id': '281', 'bowl-count': '1', 'selected': '0',
                      'source-selected-slot-not-saved': '1', 'hud': '1', 'held': '1', 'used': '0'})
    exact(finish[0], {'cleanup': '1', 'windows-absent': '1'}, ('checks',))
    require(int(finish[0]['checks']) > 0, 'Checked terminal owner cleanup')
    return {'inputs': inputs, 'returned': returned[0], 'save': saved[0],
            'reloaded': loaded[0], 'finish': finish[0]}


def readback(parser, mining, farming, evidence, receipt, authored_player):
    def owned(name):
        return parser.Nbt(parser.read(evidence / name)).root()

    before = owned('food-player-before.nbt')
    apple = owned('food-player-apple.nbt')
    soup = owned('food-player-soup.nbt')
    inventory(before, 5, 2, 282, 3)
    inventory(apple, 9, 1, 282, 3)
    inventory(soup, 19, 1, 281, 1)
    mining.same_fields(authored_player, before,
                       ('Pos', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Independent authored input versus actual loaded Player')
    saved = evidence / 'snapshots/food-saved/alpha'
    player = parser.inflate(parser.read(saved / 'level.dat'), 31)['Data'][1]['Player'][1]
    sample = int(receipt['save']['sample'])
    entry = owned(f'food-player-entry-{sample:04d}.nbt')
    reopened = owned('food-player-reloaded.nbt')
    mining.same_fields(player, entry,
                       ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Saved Player versus exact selected passive entry')
    mining.same_fields(player, reopened, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Independent reloaded Player stable fields')
    inventory(player, 19, 1, 281, 1)
    inventory(entry, 19, 1, 281, 1)
    inventory(reopened, 19, 1, 281, 1)
    level = farming.selected_chunk(parser, saved / 'region/r.0.0.mcr')[4]
    index = (3 << 11) | (3 << 7) | 64
    require(level['Blocks'][1][index:index + 2] == bytes([3, 0]) and
            level['Blocks'][1][(3 << 11) | 64] == 1 and
            level['Data'][1][index // 2] == 0,
            'Independent saved Region Dirt/Air and standing platform')
    return {'health': [5, 9, 19], 'saved_health': 19,
            'saved_inventory': [[0, 260, 1, 0], [1, 281, 1, 0]],
            'region_target': [3, 64, 3, 3, 0],
            'saved_level_sha256': digest(saved / 'level.dat')}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--executable', type=Path, required=True)
    args.add_argument('--assets', type=Path, required=True)
    args.add_argument('--artifacts', type=Path, required=True)
    options = args.parse_args()
    require(os.name == 'nt', 'Windows SDK display qualification required before live inputs')
    world = module('food_world_fixture', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('food_save_decoder', ROOT / 'tests/tools/client/test_mining_loop.py')
    farming = module('food_saved_platform', ROOT / 'tests/tools/client/test_farming_loop.py')
    parser = module('food_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = options.executable.resolve(strict=True), options.assets.resolve(strict=True)
    options.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='food-', dir=options.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {Path(__file__).resolve(), executable, ROOT / 'CMakeLists.txt'}
    inputs.update(executable.parent.glob('*.dll'))
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                      p.suffix in ('.c', '.h', '.inc', '.py', '.cmake', '.json', '.tsv', '.txt'))
    inputs.add(ROOT / 'tests/tools/world/test_source_saving.py')
    before_inputs = {str(p): digest(p) for p in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'evidence': str(evidence),
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'limits': ['Authored native saved scene and ordinary SDK input; no original gameplay scene execution.',
                         'One Apple tap, ordinary hotbar2 and one Soup tap; no input, camera, timer or source-field repair.',
                         'Pre-bootstrap difficulty2 prevents peaceful regeneration; live health is checked exactly.',
                         'A temporary native hold observes displaced Soup count2 and releases before ordinary Save/Quit.',
                         'No original pixels, audio, FPS or complete gameplay qualification.']}
    try:
        with tempfile.TemporaryDirectory(prefix='food-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            saves.mkdir()
            (saves / 'preserve-me.bin').write_bytes(b'unrelated food fixture input\0\xff')
            authored_player = None
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Food world', 1304000000000)):
                directory = saves / folder
                directory.mkdir()
                wire = metadata(world, name, played) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder == 'alpha':
                    authored_player = parser.Nbt(wire).root()['Data'][1]['Player'][1]
                    inventory(authored_player, 5, 2, 282, 3)
                    saved_y = 65.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0]
                    require(authored_player['Pos'] == [9, [6, [int.from_bytes(struct.pack('>d', x), 'big')
                                                              for x in (3.5, saved_y, .5)]]] and
                            authored_player['Rotation'] == [9, [5, [0, int.from_bytes(struct.pack('>f', 26.), 'big')]]],
                            'Independent authored standing pose decode')
                    report['authored_player'] = {'position': authored_player['Pos'],
                                                 'rotation': authored_player['Rotation'], 'health': 5,
                                                 'inventory': [[0, 260, 2, 0], [1, 282, 3, 0]]}
            regions = saves / 'alpha/region'
            regions.mkdir()
            farming.authored_regions(world, parser, regions)
            before = world.save_title_files(saves, evidence / 'before')
            env = os.environ.copy()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                       ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence)], env,
                                               evidence, saves, before, 'food', first, 300)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Actual SDK Food/Soup process failed')
            report['receipts'] = observations(farming, result.stdout)
            report['readback'] = readback(parser, mining, farming, evidence,
                                          report['receipts'], authored_player)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Unrelated authored save changed')
            require(all(name.startswith('alpha/') for name in set(after) - set(before)),
                    'No unrelated World creation')
            selected = world.snapshot(evidence / 'snapshots/food-saved')
            final = world.snapshot(saves)
            require(set(final) == set(selected) and
                    all(final[k] == v for k, v in selected.items() if Path(k).as_posix() != 'alpha/session.lock') and
                    (saves / 'alpha/session.lock').stat().st_size == 8,
                    'Reselection changes only the actual session token')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(p): digest(p) for p in inputs}
        report['assets_after'] = world.snapshot(assets)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/imported-resource bytes changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Food scene continuity')
    print(f'Actual SDK Food/Soup/save/readback PASS: {evidence}')


if __name__ == '__main__':
    main()
