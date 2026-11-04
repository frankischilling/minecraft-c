"""Windows SDK loaded-Crop bone meal, ordinary save/reselection and independent NBT."""
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
import zlib

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
    stack = (world.tag(1, 'Slot', b'\0') + world.tag(2, 'id', struct.pack('>h', 351)) +
             world.tag(1, 'Count', b'\2') + world.tag(2, 'Damage', struct.pack('>h', 15)) + b'\0')
    health = world.tag(2, 'Health', struct.pack('>h', 20))
    require(wire.count(empty) == wire.count(health) == 1,
            'One declared saved Player Inventory and Health')
    wire = wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + stack))
    return wire.replace(health, world.tag(2, 'Health', struct.pack('>h', 5)))


def authored_regions(world, parser, farming, directory):
    # All changes are declared saved-NBT inputs before bootstrap.
    world.saved_regions(directory, False, crops=True)
    region = directory / 'r.0.0.mcr'
    raw, at, sectors, wire, level = farming.selected_chunk(parser, region)
    index = (3 << 11) | (3 << 7) | 64
    data = bytearray(level['Data'][1])
    require(level['Blocks'][1][index:index + 2] == bytes([60, 59]) and
            data[index // 2] == 0x70, 'Exact reused mature Crop/Farmland saved producer')
    data[index // 2] = 0
    old = level['Data'][1]
    field = world.tag(7, 'Data', struct.pack('>i', len(old)) + old)
    require(wire.count(field) == 1, 'One declared metadata array')
    wire = wire.replace(field, world.tag(7, 'Data', struct.pack('>i', len(data)) + data))
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    require(len(record) <= sectors * 4096, 'Bounded pre-bootstrap saved allocation')
    image = bytearray(raw)
    image[at:at + sectors * 4096] = record + bytes(sectors * 4096 - len(record))
    region.write_bytes(image)
    actual = farming.selected_chunk(parser, region)[4]
    require(actual['Blocks'][1][index:index + 2] == bytes([60, 59]) and
            actual['Blocks'][1][(3 << 11) | 64] == 1 and
            actual['Data'][1][index // 2] == 0 and
            actual['SkyLight'][1][index // 2] == 0xf0 and
            actual['HeightMap'][1][3 * 16 + 3] == 65,
            'Independent saved stage0 Crop/Farmland, platform, light and height decode')


def inventory(player, count):
    require(player.get('Health') == [2, 5], 'Unchanged actual Player Short Health')
    require(player.get('Inventory', [None])[0] == 9, 'Actual Player Inventory List')
    kind, values = player['Inventory'][1]
    require(kind == 10 and len(values) == 1, 'Exactly one genuine saved Stack')
    item = values[0]
    require(item.get('Slot') == [1, 0] and item.get('id') == [2, 351] and
            item.get('Count') == [1, count] and item.get('Damage') == [2, 15],
            'Exact Dye Stack record and source widths')


def exact(row, expected, numeric=()):
    require(set(row) == set(expected) | set(numeric) and
            all(row[key] == value for key, value in expected.items()) and
            all(row[key].isdigit() for key in numeric), 'Exact typed Dye scene receipt')


def observations(farming, stdout):
    inputs, returned, saved, loaded, finish = [farming.rows(stdout, name) for name in
        ('DYE_INPUT', 'DYE_RETURN', 'DYE_SAVE', 'DYE_RELOAD', 'DYE_FINISH')]
    require(all(len(rows) == 1 for rows in (inputs, returned, saved, loaded, finish)),
            'Complete actual sole-tap input/save/reload/cleanup receipts')
    exact(inputs[0], {'action': 'bone-meal', 'taps': '1', 'right-clicks': '1',
        'block-uses': '1', 'air-uses': '0', 'count-stores': '1', 'used': '1', 'cleared': '0',
        'equip-reset': '1', 'placed': '0', 'sounds': '0', 'child-retired': '1', 'drained': '1'})
    exact(returned[0], {'ground': '60', 'crop': '59', 'stage-before': '0', 'stage-after': '7',
        'count-before': '2', 'count-after': '1', 'damage': '15', 'health': '5', 'same-stack': '1',
        'selected': '0', 'hud': '1', 'held': '1', 'icon': '191', 'used': '1',
        'target-lists-replaced': '1', 'exact-original-position-uv-before': '48',
        'exact-original-position-uv-after': '48', 'source-colors': '48'},
        ('source-color-matches-before', 'source-color-matches-after'))
    require(all(0 <= int(returned[0][key]) <= 48 for key in
                ('source-color-matches-before', 'source-color-matches-after')),
            'Observed source lighting match counts only')
    exact(saved[0], {'health': '5', 'ground': '60', 'crop': '59', 'stage': '7',
        'source-save-title': '1', 'pre-frame-owned-nbt': '1'}, ('sample', 'candidates', 'moisture'))
    require(0 <= int(saved[0]['sample']) < int(saved[0]['candidates']) and
            0 <= int(saved[0]['moisture']) <= 7, 'Actual selected paused save entry')
    exact(loaded[0], {'distinct-world': '1', 'distinct-player': '1', 'health': '5',
        'ground': '60', 'crop': '59', 'stage': '7', 'count': '1', 'damage': '15', 'selected': '0',
        'source-selected-slot-not-saved': '1', 'hud': '1', 'held': '1', 'used': '1'})
    exact(finish[0], {'cleanup': '1', 'windows-absent': '1'}, ('checks',))
    require(int(finish[0]['checks']) > 0, 'Checked terminal owner cleanup')
    return {'inputs': inputs, 'returned': returned[0], 'save': saved[0],
            'reloaded': loaded[0], 'finish': finish[0]}


def readback(parser, mining, farming, evidence, receipt, authored_player):
    def owned(name):
        return parser.Nbt(parser.read(evidence / name)).root()

    before = owned('dye-player-before.nbt')
    grown = owned('dye-player-grown.nbt')
    inventory(before, 2)
    inventory(grown, 1)
    mining.same_fields(authored_player, before,
                       ('Pos', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Independent authored input versus actual loaded Player')
    saved = evidence / 'snapshots/dye-saved/alpha'
    player = parser.inflate(parser.read(saved / 'level.dat'), 31)['Data'][1]['Player'][1]
    sample = int(receipt['save']['sample'])
    entry = owned(f'dye-player-entry-{sample:04d}.nbt')
    reopened = owned('dye-player-reloaded.nbt')
    mining.same_fields(player, entry,
                       ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Saved Player versus exact selected passive entry')
    mining.same_fields(player, reopened, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Independent reloaded Player stable fields')
    inventory(player, 1)
    inventory(entry, 1)
    inventory(reopened, 1)
    level = farming.selected_chunk(parser, saved / 'region/r.0.0.mcr')[4]
    index = (3 << 11) | (3 << 7) | 64
    require(level['Blocks'][1][index:index + 2] == bytes([60, 59]) and
            level['Blocks'][1][(3 << 11) | 64] == 1 and
            level['Data'][1][index // 2] == (0x70 | int(receipt['save']['moisture'])),
            'Independent saved Region mature Crop/Farmland and selected moisture')
    return {'health_before_after': [5, 5], 'saved_health': 5,
            'saved_inventory': [[0, 351, 1, 15]],
            'region_target': [3, 65, 3, 59, 7],
            'saved_level_sha256': digest(saved / 'level.dat')}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--executable', type=Path, required=True)
    args.add_argument('--assets', type=Path, required=True)
    args.add_argument('--artifacts', type=Path, required=True)
    options = args.parse_args()
    require(os.name == 'nt', 'Windows SDK display qualification required before live inputs')
    world = module('dye_world_fixture', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('dye_save_decoder', ROOT / 'tests/tools/client/test_mining_loop.py')
    farming = module('dye_saved_platform', ROOT / 'tests/tools/client/test_farming_loop.py')
    projection = module('dye_original_geometry', ROOT / 'tests/tools/client/test_chunk_compile.py')
    parser = module('dye_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = options.executable.resolve(strict=True), options.assets.resolve(strict=True)
    options.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='dye-', dir=options.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {Path(__file__).resolve(), executable, ROOT / 'CMakeLists.txt'}
    inputs.update(executable.parent.glob('*.dll'))
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client',
                   'tests/reference/client/crops_world'):
        inputs.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                      p.suffix in ('.c', '.h', '.inc', '.py', '.cmake', '.json', '.tsv', '.txt', '.java'))
    inputs.add(ROOT / 'tests/tools/world/test_source_saving.py')
    before_inputs = {str(p): digest(p) for p in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'evidence': str(evidence),
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'limits': ['One authored native saved Crop/Player scene and one ordinary SDK RMB tap.',
                         'No runtime camera, clock, timer, Random, World or input-queue repair.',
                         'Only the position/UV fields of 48 stage0/stage7 vertices use executed-original Crops geometry; colors retain actual source lighting.',
                         'Pre-bootstrap difficulty 2 prevents peaceful regeneration; live health remains 5.',
                         'Used statistic is the real current application writer; selected slot is source default 0 after reload.',
                         'No original gameplay scene, whole frame pixels, audio, unrestricted farming or FPS qualification.']}
    try:
        with tempfile.TemporaryDirectory(prefix='dye-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            saves.mkdir()
            (saves / 'preserve-me.bin').write_bytes(b'unrelated dye fixture input\0\xff')
            authored_player = None
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Dye world', 1304000000000)):
                directory = saves / folder
                directory.mkdir()
                wire = metadata(world, name, played) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder == 'alpha':
                    authored_player = parser.Nbt(wire).root()['Data'][1]['Player'][1]
                    inventory(authored_player, 2)
                    saved_y = 65.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0]
                    require(authored_player['Pos'] == [9, [6, [int.from_bytes(struct.pack('>d', x), 'big')
                                                              for x in (3.5, saved_y, .5)]]] and
                            authored_player['Rotation'] == [9, [5, [0, int.from_bytes(struct.pack('>f', 26.), 'big')]]],
                            'Independent authored standing pose decode')
                    report['authored_player'] = {'position': authored_player['Pos'],
                                                 'rotation': authored_player['Rotation'], 'health': 5,
                                                 'inventory': [[0, 351, 2, 15]]}
            regions = saves / 'alpha/region'
            regions.mkdir()
            authored_regions(world, parser, farming, regions)
            before = world.save_title_files(saves, evidence / 'before')
            references = {}
            for stage in (0, 7):
                cases = [case for case in projection.crops_reference_cases(2)
                         if case['metadata'] == stage]
                require(len(cases) == 1 and cases[0]['xyz'] == [3, 65, 3],
                        'One pinned original Crop geometry placement per stage')
                reference = evidence / f'crops-original-stage{stage}.txt'
                projection.write_block_reference(reference, cases, marker='DYE_CROPS_V1')
                references[stage] = reference
            report['original_geometry_reference_sha256'] = {
                str(stage): digest(path) for stage, path in references.items()}
            env = os.environ.copy()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                       ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'),
                       MC_DYE_CROPS_STAGE0_REFERENCE=str(references[0]),
                       MC_DYE_CROPS_STAGE7_REFERENCE=str(references[7]))
            first = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence)], env,
                                               evidence, saves, before, 'dye', first, 300)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Actual SDK loaded-Crop Dye process failed')
            report['receipts'] = observations(farming, result.stdout)
            report['readback'] = readback(parser, mining, farming, evidence,
                                          report['receipts'], authored_player)
            require(report['original_geometry_reference_sha256'] == {
                str(stage): digest(path) for stage, path in references.items()},
                'Executed-original geometry projection files unchanged')
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Unrelated authored save changed')
            require(all(name.startswith('alpha/') for name in set(after) - set(before)),
                    'No unrelated World creation')
            selected = world.snapshot(evidence / 'snapshots/dye-saved')
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
    require(report['status'] == 'PASS', 'Dye scene continuity')
    print(f'Actual SDK loaded-Crop Dye/save/readback PASS: {evidence}')


if __name__ == '__main__':
    main()
