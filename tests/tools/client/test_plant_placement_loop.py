"""Windows SDK Plant placement, actual terrain draw and source save/reselection."""
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

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(value, message):
    if not value:
        raise AssertionError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def metadata(world, name, played, id):
    wire = world.metadata(name, played, 19132, crops_player=True)
    health = world.tag(2, 'Health', struct.pack('>h', 20))
    require(wire.count(health) == 1, 'One declared inherited Player Health tag')
    wire = wire.replace(health, world.tag(2, 'Health', struct.pack('>h', 5)))
    empty = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
    stack = (world.tag(1, 'Slot', b'\0') + world.tag(2, 'id', struct.pack('>h', id)) +
             world.tag(1, 'Count', b'\2') + world.tag(2, 'Damage', bytes(2)) + b'\0')
    require(wire.count(empty) == 1, 'One declared saved Inventory')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + stack))


def inventory(player, id, count):
    require(player.get('Health') == [2, 5], 'Actual unchanged Short Health5')
    require(player.get('Inventory', [None])[0] == 9, 'Actual Inventory List')
    kind, values = player['Inventory'][1]
    require(kind == 10 and len(values) == 1, 'One genuine saved Stack')
    item = values[0]
    require(item.get('Slot') == [1, 0] and item.get('id') == [2, id] and
            item.get('Count') == [1, count] and item.get('Damage') == [2, 0],
            'Exact Plant Stack values and NBT widths')


def exact(row, expected, numeric=()):
    require(set(row) == set(expected) | set(numeric) and
            all(row[key] == value for key, value in expected.items()) and
            all(row[key].isdigit() for key in numeric), 'Exact Plant receipt fields')


def observations(farming, stdout, kind, id):
    names = ('PLANT_INPUT', 'PLANT_RETURN', 'PLANT_SAVE', 'PLANT_RELOAD', 'PLANT_FINISH')
    found = [farming.rows(stdout, name) for name in names]
    require(all(len(rows) == 1 for rows in found), 'One complete input/return/save/reload/cleanup')
    input_row, returned, saved, loaded, finish = [rows[0] for rows in found]
    exact(input_row, {'kind': kind, 'taps': '1', 'right-clicks': '1', 'block-uses': '1',
        'air-uses': '0', 'admitted': '1', 'placed': '1', 'sounds': '1', 'count-stores': '1',
        'used': '1', 'cleared': '0', 'equip-reset': '1', 'drained': '1',
        'placed-hooks': 'source-control-flow', 'metadata': '0', 'target': '3,65,3', 'face': '1'},
        ('direct-suffix',))
    require(input_row['direct-suffix'] in ('0', '1'), 'Actual final-tick copied suffix availability')
    exact(returned, {'kind': kind, 'ground': '3', 'id': str(id), 'count-before': '2',
        'count-after': '1', 'damage': '0', 'health': '5', 'same-stack': '1', 'selected': '0',
        'hud': '1', 'held': '1', 'continuing': '3', 'used': '1', 'list-replaced': '1'}, ('stage',))
    exact(saved, {'health': '5', 'ground': '3', 'id': str(id), 'source-save-title': '1',
                  'pre-frame-owned-nbt': '1'}, ('sample', 'candidates', 'stage'))
    require(0 <= int(saved['sample']) < int(saved['candidates']), 'Selected actual paused save entry')
    for row in (returned, saved):
        require(int(row['stage']) in ((0, 8) if id == 6 else (0,)),
                'Natural Sapling stage bit only; no growth or metadata repair')
    exact(loaded, {'distinct-world': '1', 'distinct-player': '1', 'health': '5', 'ground': '3',
                   'count': '1', 'damage': '0', 'selected': '0', 'hud': '1', 'held': '1',
                   'used': '1'}, ('stage',))
    require(int(loaded['stage']) == int(saved['stage']) or
            (id == 6 and saved['stage'] == '0' and loaded['stage'] == '8'),
            'Saved metadata retained; only a natural new Sapling stage bit may follow reload')
    exact(finish, {'cleanup': '1', 'windows-absent': '1'}, ('checks',))
    require(int(finish['checks']) > 0, 'Checked terminal cleanup')
    geometry = farming.rows(stdout, 'PLANT_GEOMETRY')
    require(len(geometry) == 3, 'Before/placed/reloaded list observations')
    for row, phase, vertices in zip(geometry, ('before', 'planted', 'reloaded'), (60, 84, 84)):
        exact(row, {'phase': phase, 'vertices': str(vertices),
                    'plant-vertices': '0' if phase == 'before' else '24', 'prefix': '60',
                    'arrays': '7', 'mode': '4', 'source-list-returned': '1', 'terrain-returned': '1'})
    return {'input': input_row, 'returned': returned, 'save': saved, 'loaded': loaded,
            'finish': finish, 'geometry': geometry}


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def geometry(evidence, id, digest):
    # Independent selected XYZ/UV construction, without invoking CV or staging NW.
    # Colors/normals/reserved words remain the actual native compiled bytes.
    texture = 15 if id == 6 else 13
    u0, v0 = f32((texture & 15) * 16 / 256), f32((texture & 240) / 256)
    u1 = f32(f32((texture & 15) * 16 + f32(15.99)) / 256)
    v1 = f32(f32((texture & 240) + f32(15.99)) / 256)
    lo, hi = 3.5 - 0.44999998807907104, 3.5 + 0.44999998807907104
    quads = [((lo, 2, lo), (lo, 1, lo), (hi, 1, hi), (hi, 2, hi)),
             ((hi, 2, hi), (hi, 1, hi), (lo, 1, lo), (lo, 2, lo)),
             ((lo, 2, hi), (lo, 1, hi), (hi, 1, lo), (hi, 2, lo)),
             ((hi, 2, lo), (hi, 1, lo), (lo, 1, hi), (lo, 2, hi))]
    uv = ((u0, v0), (u0, v1), (u1, v1), (u1, v0))
    expected = []
    for quad in quads:
        for index in (0, 1, 2, 0, 2, 3):
            expected.append(struct.pack('<5f', *quad[index], *uv[index]))
    result = {}
    for phase, count in (('before', 60), ('planted', 84), ('reloaded', 84)):
        path = evidence / f'plant-geometry-{phase}.bin'
        raw = path.read_bytes()
        require(len(raw) == count * 32, 'Exact actual compiled list byte count')
        if phase != 'before':
            for index, words in enumerate(expected):
                require(raw[(60 + index) * 32:(60 + index) * 32 + 20] == words,
                        f'Actual crossed Plant position/UV differs: {phase} vertex{index}')
        result[phase] = {'sha256': digest(path), 'vertices': count,
                         'exact_position_uv_vertices': 0 if phase == 'before' else 24}
    return result


def readback(parser, mining, farming, evidence, receipt, authored, id):
    def owned(name):
        return parser.Nbt(parser.read(evidence / name)).root()
    before, planted = owned('plant-player-before.nbt'), owned('plant-player-planted.nbt')
    inventory(before, id, 2)
    inventory(planted, id, 1)
    mining.same_fields(authored, before, ('Pos', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Declared saved input versus genuine loaded Player')
    saved = evidence / 'snapshots/plant-saved/alpha'
    player = parser.inflate(parser.read(saved / 'level.dat'), 31)['Data'][1]['Player'][1]
    entry = owned(f"plant-player-entry-{int(receipt['save']['sample']):04}.nbt")
    reloaded = owned('plant-player-reloaded.nbt')
    for value in (player, entry, reloaded):
        inventory(value, id, 1)
    mining.same_fields(player, entry, ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual saved Player versus exact passive save-entry snapshot')
    mining.same_fields(player, reloaded, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual reloaded Player stable fields')
    level = farming.selected_chunk(parser, saved / 'region/r.0.0.mcr')[4]
    index = (3 << 11) | (3 << 7) | 64
    stage = int(receipt['save']['stage'])
    require(level['Blocks'][1][index:index + 2] == bytes((3, id)) and
            level['Blocks'][1][(3 << 11) | 64] == 1 and
            level['Data'][1][index // 2] == stage << 4,
            'Independent saved support/Plant/metadata/platform Region decode')
    return {'inventory_before': [0, id, 2, 0], 'inventory_saved': [0, id, 1, 0],
            'target': [3, 65, 3, id, stage], 'saved_level_sha256': mining.digest(saved / 'level.dat')}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--executable', required=True, type=Path)
    args.add_argument('--assets', required=True, type=Path)
    args.add_argument('--artifacts', required=True, type=Path)
    args.add_argument('--kind', required=True, choices=('sapling', 'flower'))
    options = args.parse_args()
    require(os.name == 'nt', 'Windows SDK qualification; no implicit Linux home redirection')
    world = module('plant_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('plant_readback', ROOT / 'tests/tools/client/test_mining_loop.py')
    farming = module('plant_platform', ROOT / 'tests/tools/client/test_farming_loop.py')
    parser = module('plant_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = options.executable.resolve(strict=True), options.assets.resolve(strict=True)
    id = 6 if options.kind == 'sapling' else 37
    options.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=f'plant-{options.kind}-', dir=options.artifacts.resolve()))
    for name in ('snapshots', 'entries'):
        (evidence / name).mkdir()
    inputs = {Path(__file__).resolve(), executable, ROOT / 'CMakeLists.txt',
              ROOT / 'tests/tools/world/test_source_saving.py'}
    inputs.update(executable.parent.glob('*.dll'))
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*') if path.is_file() and
                      path.suffix in ('.c', '.h', '.inc', '.py', '.cmake'))
    before_inputs = {str(path): mining.digest(path) for path in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'kind': options.kind,
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'operands': {'support': [3, 64, 3, 3, 0], 'target': [3, 65, 3, 0, 0],
                           'standing_support': [3, 64, 0, 1], 'source_ray': [3, 64, 3, 1],
                           'slot': [0, id, 2, 0], 'player_xz': [3.5, .5],
                           'player_y': 65.0 + f32(1.62), 'yaw_pitch': [0, 26], 'player_health': 5},
              'limits': ['One fresh process, saved input and ordinary SDK RMB tap per profile.',
                         'Existing focus requests/Source pause UI and full timer/tick/frame work remain unchanged.',
                         'Placed hooks follow from the returned CK sound/count/statistic suffix; no direct hook observer.',
                         'Sound completion is observed; no audible output or callback-argument equivalence is claimed.',
                         'Exact native crossed-plant XYZ/UV only; source lighting colors retained without normalization.',
                         'Natural Sapling stage8 is observed; no live World, pose, input, clock, Timer or RNG repair.',
                         'No original loaded scene, whole-frame pixel equality, general plant use or FPS claim.']}
    (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    try:
        with tempfile.TemporaryDirectory(prefix='plant-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            saves.mkdir()
            (saves / 'preserve-me.bin').write_bytes(b'unrelated plant fixture input\0\xff')
            authored = None
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Plant world', 1304000000000)):
                directory = saves / folder
                directory.mkdir()
                wire = metadata(world, name, played, id) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder == 'alpha':
                    authored = parser.Nbt(wire).root()['Data'][1]['Player'][1]
                    inventory(authored, id, 2)
            regions = saves / 'alpha/region'
            regions.mkdir()
            farming.authored_regions(world, parser, regions)
            before = world.save_title_files(saves, evidence / 'before')
            env = os.environ.copy()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home), HOME=str(home),
                       ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), options.kind, str(evidence)],
                                               env, evidence, saves, before, f'plant-{options.kind}',
                                               time.time_ns() // 1000000, 1200)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Actual SDK Plant placement process failed')
            report['receipts'] = observations(farming, result.stdout, options.kind, id)
            report['geometry'] = geometry(evidence, id, mining.digest)
            report['readback'] = readback(parser, mining, farming, evidence,
                                          report['receipts'], authored, id)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Unrelated authored save changed')
            require(all(name.startswith('alpha/') for name in set(after) - set(before)),
                    'No unrelated World creation')
            selected, final = world.snapshot(evidence / 'snapshots/plant-saved'), world.snapshot(saves)
            session_key = str(Path('alpha') / 'session.lock')
            report['reselection_disk'] = {
                'same_paths': set(final) == set(selected),
                'changed': sorted(key for key in set(final) | set(selected)
                                  if final.get(key) != selected.get(key)),
                'source_session_key': session_key,
                'session_bytes': (saves / session_key).stat().st_size}
            require(set(final) == set(selected) and
                    all(final[key] == value for key, value in selected.items()
                        if key != session_key) and
                    (saves / session_key).stat().st_size == 8,
                    'Reselection disk-byte invariance except actual session token')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): mining.digest(path) for path in sorted(inputs)}
        report['assets_after'] = world.snapshot(assets)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/imported-resource bytes changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Plant fixture continuity')
    print(f'Actual SDK {options.kind} placement/save/readback PASS: {evidence}')


if __name__ == '__main__':
    main()
