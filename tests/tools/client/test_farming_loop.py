"""Actual SDK till/plant input, source save/reselection and independent NBT readback."""
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
    stacks = []
    for slot, item, count in ((0, 290, 1), (1, 295, 2)):
        stacks.append(world.tag(1, 'Slot', bytes([slot])) +
                      world.tag(2, 'id', struct.pack('>h', item)) +
                      world.tag(1, 'Count', bytes([count])) +
                      world.tag(2, 'Damage', bytes(2)) + b'\0')
    require(wire.count(empty) == 1, 'One declared saved Inventory')
    return wire.replace(empty, world.tag(9, 'Inventory',
                                        b'\x0a' + struct.pack('>i', 2) + b''.join(stacks)))


def selected_chunk(parser, region):
    raw = region.read_bytes()
    location, = struct.unpack_from('>I', raw, 0)
    sector, count = location >> 8, location & 255
    at = sector * 4096
    require(sector >= 2 and count >= 1 and at + count * 4096 <= len(raw),
            'Real Chunk0,0 region allocation')
    size, = struct.unpack_from('>I', raw, at)
    require(1 < size <= count * 4096 - 4 and raw[at + 4] == 2,
            'Actual compressed selected Chunk extent')
    wire = zlib.decompress(raw[at + 5:at + size + 4])
    level = parser.Nbt(wire).root()['Level'][1]
    require(level['xPos'] == [3, 0] and level['zPos'] == [3, 0], 'Decoded selected coordinates')
    return raw, at, count, wire, level


def authored_regions(world, parser, directory):
    # Reuse the unchanged mature-Crop input producer, then make explicit new
    # saved inputs before bootstrap. This never modifies that scene or a live World.
    world.saved_regions(directory, False, crops=True)
    region = directory / 'r.0.0.mcr'
    raw, at, sectors, wire, level = selected_chunk(parser, region)
    index = (3 << 11) | (3 << 7) | 64
    blocks, data = bytearray(level['Blocks'][1]), bytearray(level['Data'][1])
    require(blocks[index:index + 2] == bytes([60, 59]) and data[index // 2] == 0x70,
            'Exact reused saved producer input')
    blocks[index:index + 2] = bytes([3, 0])
    data[index // 2] = 0
    for name, old, new in (('Blocks', level['Blocks'][1], blocks),
                           ('Data', level['Data'][1], data)):
        field = world.tag(7, name, struct.pack('>i', len(old)) + old)
        require(wire.count(field) == 1, 'One declared saved array')
        wire = wire.replace(field, world.tag(7, name, struct.pack('>i', len(new)) + new))
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\2' + compressed
    require(len(record) <= sectors * 4096, 'Bounded authored allocation')
    image = bytearray(raw)
    image[at:at + sectors * 4096] = record + bytes(sectors * 4096 - len(record))
    region.write_bytes(image)
    actual = selected_chunk(parser, region)[4]
    require(actual['Blocks'][1][index:index + 2] == bytes([3, 0]) and
            actual['Blocks'][1][(3 << 11) | 64] == 1 and
            actual['Data'][1][index // 2] == 0 and
            actual['SkyLight'][1][index // 2] == 0xf0 and
            actual['HeightMap'][1][3 * 16 + 3] == 65,
            'Independent Dirt/Air, platform, light and height input decode')


def inventory(player, wear, seeds):
    require(player.get('Inventory', [None])[0] == 9, 'Actual Inventory List')
    kind, values = player['Inventory'][1]
    require(kind == 10 and len(values) == 2, 'Exactly two genuine saved Stacks')
    expected = [(0, 290, 1, wear), (1, 295, seeds, 0)]
    for item, (slot, item_id, count, damage) in zip(values, expected):
        require(item.get('Slot') == [1, slot] and item.get('id') == [2, item_id] and
                item.get('Count') == [1, count] and item.get('Damage') == [2, damage],
                'Exact saved Hoe/Seeds record and source widths')


def rows(stdout, prefix):
    result = []
    for line in stdout.splitlines():
        if line.startswith(prefix + ' '):
            pairs = [value.split('=', 1) for value in line[len(prefix) + 1:].split()]
            require(all(len(pair) == 2 for pair in pairs), 'Typed receipt fields')
            row = dict(pairs)
            require(len(row) == len(pairs), 'Unique receipt fields')
            result.append(row)
    return result


def observations(stdout):
    inputs = rows(stdout, 'FARMING_INPUT')
    returned, saved, loaded, finish = [rows(stdout, name) for name in
                                      ('FARMING_RETURN', 'FARMING_SAVE', 'FARMING_RELOAD',
                                       'FARMING_FINISH')]
    require(len(inputs) == 2 and [x['action'] for x in inputs] == ['till', 'plant'] and
            len(returned) == len(saved) == len(loaded) == len(finish) == 1,
            'Complete actual input/save/reload/cleanup receipts')
    for row, sound, count in zip(inputs, ('1', '0'), ('0', '1')):
        require(row['placed'] == row['used'] == row['drained'] == '1' and
                row['sounds'] == sound and row['count-stores'] == count and
                row['ck-predicate'] == '0' and 1 <= int(row['attempts']) <= 12 and
                int(row['right-clicks']) >= 1, 'Actual Hoe/Seeds caller effects')
    require(returned[0]['ground'] == '60' and returned[0]['crop'] == '59' and
            returned[0]['stage'] == '0' and returned[0]['hoe-count'] == '1' and
            returned[0]['hoe-damage'] == '1' and returned[0]['seeds-count'] == '1' and
            returned[0]['hotbar'] == '1' and returned[0]['target-lists-replaced'] == '2' and
            returned[0]['exact-original-position-uv'] == '48', 'Actual planted and compiled result')
    require(saved[0]['source-save-title'] == saved[0]['pre-frame-owned-nbt'] == '1' and
            saved[0]['ground'] == '60' and saved[0]['crop'] == '59' and
            0 <= int(saved[0]['stage']) <= 7 and 0 <= int(saved[0]['moisture']) <= 7 and
            0 <= int(saved[0]['sample']) < int(saved[0]['candidates']),
            'Actual selected paused save entry')
    require(loaded[0]['distinct-world'] == loaded[0]['distinct-player'] == '1' and
            loaded[0]['ground'] == '60' and loaded[0]['crop'] == '59' and
            loaded[0]['hoe-count'] == loaded[0]['hoe-damage'] == loaded[0]['seeds-count'] == '1' and
            loaded[0]['selected'] == '0' and
            loaded[0]['source-selected-slot-not-saved'] == '1', 'Actual independent World reload')
    require(finish[0]['cleanup'] == finish[0]['windows-absent'] == '1' and
            int(finish[0]['checks']) > 0, 'Checked terminal owner cleanup')
    return {'inputs': inputs, 'returned': returned[0], 'save': saved[0],
            'reloaded': loaded[0], 'finish': finish[0]}


def readback(parser, mining, evidence, receipt):
    saved = evidence / 'snapshots/farming-saved/alpha'
    player = parser.inflate(parser.read(saved / 'level.dat'), 31)['Data'][1]['Player'][1]
    sample = int(receipt['save']['sample'])
    entry = parser.Nbt(parser.read(evidence / f'farming-player-entry-{sample:04d}.nbt')).root()
    reopened = parser.Nbt(parser.read(evidence / 'farming-player-reloaded.nbt')).root()
    mining.same_fields(player, entry,
                       ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Saved Player versus exact selected passive entry')
    mining.same_fields(player, reopened, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Independent reloaded Player stable fields')
    inventory(player, 1, 1)
    inventory(reopened, 1, 1)
    level = selected_chunk(parser, saved / 'region/r.0.0.mcr')[4]
    index = (3 << 11) | (3 << 7) | 64
    require(level['Blocks'][1][index:index + 2] == bytes([60, 59]) and
            level['Data'][1][index // 2] & 15 == int(receipt['save']['moisture']) and
            level['Data'][1][index // 2] >> 4 == int(receipt['save']['stage']),
            'Independent actual Region Farmland/Crop and selected entry metadata')
    return {'coordinate': [3, 64, 3], 'ground': 60, 'above': 59,
            'moisture': int(receipt['save']['moisture']), 'stage': int(receipt['save']['stage']),
            'inventory': [[0, 290, 1, 1], [1, 295, 1, 0]],
            'saved_level_sha256': digest(saved / 'level.dat')}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--executable', type=Path, required=True)
    args.add_argument('--assets', type=Path, required=True)
    args.add_argument('--artifacts', type=Path, required=True)
    options = args.parse_args()
    require(os.name == 'nt', 'Disposable Linux account/mount qualification required before live inputs')
    world = module('farming_world_fixture', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('farming_save_decoder', ROOT / 'tests/tools/client/test_mining_loop.py')
    parser = module('farming_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    projection = module('farming_crop_projection', ROOT / 'tests/tools/client/test_chunk_compile.py')
    executable, assets = options.executable.resolve(strict=True), options.assets.resolve(strict=True)
    options.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='farming-', dir=options.artifacts.resolve()))
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
              'limits': ['Authored native saved scene and ordinary SDK input, no original gameplay scene execution.',
                         'Only the position/UV fields of 48 stage0 vertices use executed-original geometry; colors retain source lighting.',
                         'Later paused-entry Crop growth and moisture are observed, not frozen or repaired.',
                         'No unrestricted Desktop/focus, Linux SDK, pixels, audio or performance qualification.']}
    try:
        with tempfile.TemporaryDirectory(prefix='farming-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            (saves / 'preserve-me.bin').write_bytes(b'unrelated farming fixture input\0\xff')
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Farming world', 1304000000000)):
                directory = saves / folder
                directory.mkdir()
                wire = metadata(world, name, played) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder == 'alpha':
                    player = parser.Nbt(wire).root()['Data'][1]['Player'][1]
                    inventory(player, 0, 2)
                    require(player['Pos'] == [9, [6, [int.from_bytes(struct.pack('>d', x), 'big')
                                                    for x in (3.5, 65.0 + struct.unpack('>f', struct.pack('>f', 1.62))[0], .5)]]] and
                            player['Rotation'] == [9, [5, [0, int.from_bytes(struct.pack('>f', 26.), 'big')]]],
                            'Independent authored standing pose decode')
                    report['authored_player'] = {'position': player['Pos'], 'rotation': player['Rotation'],
                                                 'inventory': [[0, 290, 1, 0], [1, 295, 2, 0]]}
            regions = saves / 'alpha/region'
            regions.mkdir()
            authored_regions(world, parser, regions)
            before = world.save_title_files(saves, evidence / 'before')
            cases = [case for case in projection.crops_reference_cases(2) if case['metadata'] == 0]
            require(len(cases) == 1 and cases[0]['xyz'] == [3, 65, 3], 'One pinned original stage0 placement')
            reference = evidence / 'crops-original-stage0.txt'
            projection.write_block_reference(reference, cases, marker='FARMING_CROPS_V1')
            report['original_geometry_reference_sha256'] = digest(reference)
            env = os.environ.copy()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                       ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'),
                       MC_FARMING_CROPS_REFERENCE=str(reference))
            first = time.time_ns() // 1000000
            result = world.retained_native_run([str(executable), str(assets), str(evidence)], env,
                                               evidence, saves, before, 'farming', first, 1200)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Actual SDK farming process failed')
            report['receipts'] = observations(result.stdout)
            report['readback'] = readback(parser, mining, evidence, report['receipts'])
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Unrelated authored save changed')
            require(all(name.startswith('alpha/') for name in set(after) - set(before)),
                    'No unrelated World creation')
            selected = world.snapshot(evidence / 'snapshots/farming-saved')
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
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    require(report['status'] == 'PASS', 'Farming scene continuity')
    print(f'Actual SDK till/plant/save/readback PASS: {evidence}')


if __name__ == '__main__':
    main()
