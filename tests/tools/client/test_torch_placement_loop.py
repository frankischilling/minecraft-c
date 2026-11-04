"""Windows SDK Torch placement, actual terrain draw and source save/reselection."""
import argparse
from collections import Counter
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


def region_records(parser, directory):
    records, images = {}, {}
    expected_names = {f'r.{x}.{z}.mcr' for x in (-1, 0) for z in (-1, 0)}
    require({path.name for path in directory.iterdir()} == expected_names,
            'Exactly four declared prelaunch Region files')
    for path in sorted(directory.iterdir()):
        _, rx, rz, _ = path.name.split('.')
        rx, rz = int(rx), int(rz)
        raw = path.read_bytes()
        require(len(raw) >= 8192 and len(raw) % 4096 == 0, 'Complete Region sector allocation')
        images[path.name] = bytearray(raw)
        allocated = {0, 1}
        for slot in range(1024):
            location, = struct.unpack_from('>I', raw, slot * 4)
            if location == 0:
                continue
            sector, count = location >> 8, location & 255
            extent = set(range(sector, sector + count))
            require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(raw) and
                    not allocated.intersection(extent), 'Disjoint complete Chunk sector extent')
            allocated.update(extent)
            at = sector * 4096
            size, = struct.unpack_from('>I', raw, at)
            require(1 < size <= count * 4096 - 4 and raw[at + 4] == 2,
                    'Declared zlib Chunk payload extent')
            payload = raw[at + 5:at + 4 + size]
            inflate = zlib.decompressobj()
            wire = inflate.decompress(payload) + inflate.flush()
            require(inflate.eof and not inflate.unused_data and not inflate.unconsumed_tail,
                    'Exactly one complete compressed Chunk')
            level = parser.Nbt(wire).root()['Level'][1]
            coordinate = (rx * 32 + slot % 32, rz * 32 + slot // 32)
            require(coordinate not in records and level['xPos'] == [3, coordinate[0]] and
                    level['zPos'] == [3, coordinate[1]], 'Unique NBT coordinates match Region slot')
            require(level['TerrainPopulated'] == [1, 1] and level['LastUpdate'] == [4, 0] and
                    all(level[name][0] == 7 and len(level[name][1]) == length for name, length in
                        (('Blocks', 32768), ('Data', 16384), ('SkyLight', 16384),
                         ('BlockLight', 16384), ('HeightMap', 256))) and
                    level['BlockLight'][1] == bytes(16384) and
                    level['Entities'] == [9, [10, []]] and level['TileEntities'] == [9, [10, []]],
                    'Complete populated saved arrays, zero BlockLight and empty entity lists')
            records[coordinate] = (path.name, location, raw[4096 + slot * 4:4100 + slot * 4],
                                   raw[at:at + count * 4096], payload, wire, level)
        require(allocated == set(range(len(raw) // 4096)), 'No undeclared Region sectors')
    return records, images


def authored_regions(world, parser, farming, directory):
    # PRELAUNCH441: preserve the original local operands and append only the
    # missing radius9 random-tick footprint plus Grass's one-Chunk read halo.
    farming.authored_regions(world, parser, directory)
    old, images = region_records(parser, directory)
    original = {(x, z) for x in range(-8, 9) for z in range(-8, 9)}
    required = {(x, z) for x in range(-10, 11) for z in range(-10, 11)}
    require(set(old) == original, 'Exact unchanged original 289-Chunk coordinate inventory')
    for x, z in sorted(required - original):
        name = f'r.{x >> 5}.{z >> 5}.mcr'
        image = images[name]
        slot = (x & 31) + (z & 31) * 32
        require(image[slot * 4:slot * 4 + 4] == bytes(4), 'Appended Chunk uses an empty slot')
        body, sectors = world.chunk_record(x, z, crops=True)
        require(0 < sectors <= 255 and len(body) == sectors * 4096,
                'Complete bounded new flat Chunk allocation')
        struct.pack_into('>I', image, slot * 4, ((len(image) // 4096) << 8) | sectors)
        image.extend(body)
    for name, image in images.items():
        (directory / name).write_bytes(image)
    actual, _ = region_records(parser, directory)
    require(set(actual) == required and len(actual) == 441, 'Exactly 441 declared Chunk slots, no extras')
    for coordinate in original:
        require(actual[coordinate][:-1] == old[coordinate][:-1],
                'All 289 original slots, timestamps, allocation bytes, payloads and NBT wires preserved')
    flat_blocks = (b'\x07' + b'\x01' * 59 + b'\x03' * 3 + b'\x02' + bytes(64)) * 256
    flat_sky = (bytes(32) + b'\xff' * 32) * 256
    for coordinate in required - original:
        level = actual[coordinate][-1]
        require(level['Blocks'][1] == flat_blocks and level['Data'][1] == bytes(16384) and
                level['SkyLight'][1] == flat_sky and level['HeightMap'][1] == bytes([64]) * 256,
                'Independent appended flat Bedrock/Stone/Dirt/Grass/Air, metadata, Sky and height decode')
    return {'revision': 'PRELAUNCH441', 'bounds': [-10, 10, -10, 10], 'chunks': len(actual),
            'original_chunks_preserved': len(old), 'flat_chunks_appended': len(required - original),
            'preserved': ['slots', 'timestamps', 'allocation_bytes', 'compressed_payloads', 'nbt_wires'],
            'terrain_populated': True, 'block_light': 0, 'independent_coordinate_audit': True}


def inventory(player, id, count):
    require(player.get('Health') == [2, 5], 'Actual unchanged Short Health5')
    require(player.get('Inventory', [None])[0] == 9, 'Actual Inventory List')
    kind, values = player['Inventory'][1]
    require(kind == 10 and len(values) == 1, 'One genuine saved Stack')
    item = values[0]
    require(item.get('Slot') == [1, 0] and item.get('id') == [2, id] and
            item.get('Count') == [1, count] and item.get('Damage') == [2, 0],
            'Exact Torch Stack values and NBT widths')


def exact(row, expected, numeric=()):
    require(set(row) == set(expected) | set(numeric) and
            all(row[key] == value for key, value in expected.items()) and
            all(row[key].isdigit() for key in numeric), 'Exact Torch receipt fields')


def observations(farming, stdout, kind, id, removal):
    names = ('TORCH_INPUT', 'TORCH_RETURN', 'TORCH_SAVE', 'TORCH_RELOAD', 'TORCH_FINISH')
    found = [farming.rows(stdout, name) for name in names]
    require(all(len(rows) == 1 for rows in found), 'One complete input/return/save/reload/cleanup')
    input_row, returned, saved, loaded, finish = [rows[0] for rows in found]
    exact(input_row, {'kind': kind, 'taps': '1', 'right-clicks': '1', 'block-uses': '1',
        'air-uses': '0', 'admitted': '1', 'placed': '1', 'sounds': '1', 'count-stores': '1',
        'used': '1', 'cleared': '0', 'equip-reset': '1', 'drained': '1',
        'placed-hooks': 'source-control-flow', 'onAdded-before-face': 'source-control-flow',
        'metadata': '0', 'target': '3,65,3', 'face': '1'},
        ('direct-suffix',))
    require(input_row['direct-suffix'] in ('0', '1'), 'Actual final-tick copied suffix availability')
    exact(returned, {'kind': kind, 'ground': '3', 'id': str(id), 'count-before': '2',
        'count-after': '1', 'damage': '0', 'health': '5', 'same-stack': '1', 'selected': '0',
        'hud': '1', 'held': '1', 'continuing': '3', 'used': '1', 'list-replaced': '1'}, ('stage',))
    exact(saved, {'health': '5', 'ground': '0' if removal else '3',
                  'id': '0' if removal else str(id), 'source-save-title': '1',
                  'blocklight': '0' if removal else '14', 'items': '2' if removal else '0',
                  'pre-frame-owned-nbt': '1'}, ('sample', 'candidates', 'stage'))
    require(0 <= int(saved['sample']) < int(saved['candidates']), 'Selected actual paused save entry')
    require(returned['stage'] == '5', 'Actual upright Torch attachment5 before removal')
    exact(loaded, {'distinct-world': '1', 'distinct-player': '1', 'health': '5',
                   'ground': '0' if removal else '3', 'items': '2' if removal else '0',
                   'count': '1', 'damage': '0', 'selected': '0', 'hud': '1', 'held': '1',
                   'used': '1'}, ('stage',))
    require(loaded['stage'] == saved['stage'] == ('0' if removal else '5'),
            'Saved attached Torch or removed Air metadata retained')
    exact(finish, {'cleanup': '1', 'windows-absent': '1'}, ('checks',))
    require(int(finish['checks']) > 0, 'Checked terminal cleanup')
    geometry = farming.rows(stdout, 'TORCH_GEOMETRY')
    phases = (('before', 60), ('placed', 90)) + ((('removed', 30), ('reloaded', 30))
              if removal else (('reloaded', 90),))
    require(len(geometry) == len(phases), 'Complete ordered geometry observations')
    for row, (phase, vertices) in zip(geometry, phases):
        exact(row, {'phase': phase, 'vertices': str(vertices),
                    'torch-vertices': '30' if vertices == 90 else '0',
                    'prefix': '30' if vertices == 30 else '60',
                    'arrays': '7', 'mode': '4', 'source-list-returned': '1', 'terrain-returned': '1'})
    lights = farming.rows(stdout, 'TORCH_LIGHT')
    light_phases = ('placed', 'removed', 'reloaded') if removal else ('placed', 'reloaded')
    require(len(lights) == len(light_phases), 'Complete ordered ordinary propagation receipts')
    for row, phase in zip(lights, light_phases):
        present = not removal or phase == 'placed'
        exact(row, {'phase': phase, 'queue': '0', 'target': '14' if present else '0',
                    'east': '13' if present else '0', 'west': '13' if present else '0',
                    'south': '13' if present else '0', 'north': '13' if present else '0',
                    'up': '13' if present else '0', 'down': '0', 'up-two': '12' if present else '0',
                    'diagonal': '12' if present else '0', 'normal-frames': '1'})
    removed = farming.rows(stdout, 'TORCH_REMOVAL')
    require(len(removed) == int(removal), 'Separate declared removal profile only')
    if removal:
        exact(removed[0], {'motion': '1', 'xrel': '24', 'yrel': '0', 'sensitivity': '0.5',
              'ray': '3,64,3', 'face': '1', 'mined': '1', 'support': '0', 'torch': '0',
              'dirt-drops': '1', 'torch-drops': '1', 'count': '1', 'used': '1',
              'light': '0', 'queue': '0', 'mined-statistic': '1', 'aim-repair': '0'})
    return {'input': input_row, 'returned': returned, 'save': saved, 'loaded': loaded,
            'finish': finish, 'geometry': geometry, 'lighting': lights, 'removal': removed}


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def geometry(evidence, id, digest, removal):
    require(id == 50, 'Genuine registered Torch50')
    # Native type2 geometry, independent of the producer. No Original scene claim.
    shape = [(3.4375, 1.625, 3.4375), (3.4375, 1.625, 3.5625),
             (3.5625, 1.625, 3.5625), (3.5625, 1.625, 3.4375),
             (3.4375, 2, 3), (3.4375, 1, 3), (3.4375, 1, 4), (3.4375, 2, 4),
             (3.5625, 2, 4), (3.5625, 1, 4), (3.5625, 1, 3), (3.5625, 2, 3),
             (3, 2, 3.5625), (3, 1, 3.5625), (4, 1, 3.5625), (4, 2, 3.5625),
             (4, 2, 3.4375), (4, 1, 3.4375), (3, 1, 3.4375), (3, 2, 3.4375)]
    expected = []
    for quad in range(5):
        u0, u1 = (7 / 256, 9 / 256) if quad == 0 else (0, f32(f32(15.99) / 256))
        v0, v1 = ((86 / 256, 88 / 256) if quad == 0 else
                  (80 / 256, f32(f32(80 + f32(15.99)) / 256)))
        uv = ((u0, v0), (u0, v1), (u1, v1), (u1, v0))
        for corner in (0, 1, 2, 0, 2, 3):
            expected.append(struct.pack('<5fI', *shape[quad * 4 + corner], *uv[corner], 0xffffffff))
    result = {}
    phases = (('before', 60), ('placed', 90)) + ((('removed', 30), ('reloaded', 30))
              if removal else (('reloaded', 90),))
    for phase, count in phases:
        path = evidence / f'torch-geometry-{phase}.bin'
        raw = path.read_bytes()
        require(len(raw) == count * 32, 'Exact actual compiled list byte count')
        if count == 90:
            for index, words in enumerate(expected):
                require(raw[(60 + index) * 32:(60 + index) * 32 + 24] == words,
                        f'Actual type2 Torch position/UV/color differs: {phase} vertex{index}')
        result[phase] = {'sha256': digest(path), 'vertices': count,
                         'exact_position_uv_color_vertices': 30 if count == 90 else 0}
    return result


def readback(parser, mining, farming, evidence, receipt, authored, id, removal):
    def owned(name):
        return parser.Nbt(parser.read(evidence / name)).root()
    before, placed = owned('torch-player-before.nbt'), owned('torch-player-placed.nbt')
    inventory(before, id, 2)
    inventory(placed, id, 1)
    mining.same_fields(authored, before, ('Pos', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Declared saved input versus genuine loaded Player')
    saved = evidence / 'snapshots/torch-saved/alpha'
    player = parser.inflate(parser.read(saved / 'level.dat'), 31)['Data'][1]['Player'][1]
    entry = owned(f"torch-player-entry-{int(receipt['save']['sample']):04}.nbt")
    reloaded = owned('torch-player-reloaded.nbt')
    for value in (player, entry, reloaded):
        inventory(value, id, 1)
    mining.same_fields(player, entry, ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual saved Player versus exact passive save-entry snapshot')
    mining.same_fields(player, reloaded, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Actual reloaded Player stable fields')
    level = farming.selected_chunk(parser, saved / 'region/r.0.0.mcr')[4]
    index = (3 << 11) | (3 << 7) | 64
    stage = int(receipt['save']['stage'])
    require(level['Blocks'][1][index:index + 2] == (bytes(2) if removal else bytes((3, id))) and
            level['Blocks'][1][(3 << 11) | 64] == 1 and
            level['Data'][1][index // 2] == stage << 4,
            'Independent saved support/Torch/metadata/platform Region decode')
    cells = ((3, 65, 3), (4, 65, 3), (2, 65, 3), (3, 65, 4), (3, 65, 2),
             (3, 66, 3), (3, 64, 3), (3, 67, 3), (4, 65, 4))
    plane = level['BlockLight'][1]
    require(level['BlockLight'][0] == 7 and len(plane) == 16384, 'Saved BlockLight byte plane')
    values = []
    for x, y, z in cells:
        cell = (x << 11) | (z << 7) | y
        values.append((plane[cell // 2] >> ((cell & 1) * 4)) & 15)
    require(values == ([0] * 9 if removal else [14, 13, 13, 13, 13, 13, 0, 12, 12]),
            'Independent saved Torch emission and ordinary propagated neighborhood')
    items, chunks = mining.saved_items(parser, saved)
    require(len(items) == (2 if removal else 0), 'All actual saved Item entities retained')
    if removal:
        sample = int(receipt['save']['sample'])
        entries = [owned(f'torch-item-entry-{sample:04}-{index:04}.nbt') for index in range(2)]
        require(Counter(mining.item_key(row['entity']) for row in items) ==
                Counter(mining.item_key(entity) for entity in entries),
                'Every saved drop field equals its selected native pre-save NBT body')
        stacks = [row['entity']['Item'][1] for row in items]
        require(sorted(stack['id'][1] for stack in stacks) == [3, 50] and
                all(stack['Count'] == [1, 1] and stack['Damage'] == [2, 0] for stack in stacks),
                'One genuine Dirt drop and one detached Torch drop')
    return {'inventory_before': [0, id, 2, 0], 'inventory_saved': [0, id, 1, 0],
            'target': [3, 65, 3, 0 if removal else id, stage],
            'saved_items': len(items), 'saved_chunks': chunks,
            'block_light_cells': list(zip(cells, values)),
            'saved_level_sha256': mining.digest(saved / 'level.dat')}


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--executable', required=True, type=Path)
    args.add_argument('--assets', required=True, type=Path)
    args.add_argument('--artifacts', required=True, type=Path)
    args.add_argument('--mode', choices=('placement', 'support-removal'), default='placement')
    options = args.parse_args()
    removal = options.mode == 'support-removal'
    require(os.name == 'nt', 'Windows SDK qualification; no implicit Linux home redirection')
    world = module('torch_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('torch_readback', ROOT / 'tests/tools/client/test_mining_loop.py')
    farming = module('torch_platform', ROOT / 'tests/tools/client/test_farming_loop.py')
    parser = module('torch_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = options.executable.resolve(strict=True), options.assets.resolve(strict=True)
    id = 50
    options.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix=f'torch-{options.mode}-', dir=options.artifacts.resolve()))
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
    report = {'status': 'FAIL', 'scope': __doc__, 'kind': 'torch', 'mode': options.mode,
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'operands': {'support': [3, 64, 3, 3, 0], 'target': [3, 65, 3, 0, 0],
                           'standing_support': [3, 64, 0, 1], 'source_ray': [3, 64, 3, 1],
                           'slot': [0, id, 2, 0], 'player_xz': [3.5, .5],
                           'player_y': 65.0 + f32(1.62), 'yaw_pitch': [0, 26], 'player_health': 5},
              'limits': ['One fresh process, saved input and ordinary SDK RMB tap per profile.',
                         'Existing focus requests/Source pause UI and full timer/tick/frame work remain unchanged.',
                         'Placed hooks follow from the returned CK sound/count/statistic suffix; no direct hook observer.',
                         'onAdded-before-CK-face ordering is inferred from the returned setter/placed suffix, not a new direct observer.',
                         'Sound completion is observed; no audible output or callback-argument equivalence is claimed.',
                         'Exact native type2 Torch XYZ/UV and emission-forced white; surrounding lighting retained.',
                         'No live World, pose, input, clock, Timer, lighting queue or RNG repair.',
                         'No original loaded scene, whole-frame pixel equality, general torch use or FPS claim.']}
    if removal:
        report['removal_input'] = {'mouse_sensitivity': .5, 'relative_motion': [24, 0],
            'motion_attempts': 1, 'required_source_ray': [3, 64, 3, 1],
            'left_button': 'one press, natural mining, one release', 'aim_repair': False,
            'saved_drops': [[3, 1, 0], [50, 1, 0]]}
    (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    try:
        with tempfile.TemporaryDirectory(prefix='torch-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            if removal:
                with (game / 'options.txt').open('ab') as settings:
                    settings.write(b'mouseSensitivity:0.5\ninvertYMouse:false\n')
            saves = game / 'saves'
            saves.mkdir()
            (saves / 'preserve-me.bin').write_bytes(b'unrelated torch fixture input\0\xff')
            authored = None
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Torch world', 1304000000000)):
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
            report['saved_input'] = authored_regions(world, parser, farming, regions)
            before = world.save_title_files(saves, evidence / 'before')
            env = os.environ.copy()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home), HOME=str(home),
                       ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), options.mode, str(evidence)],
                                               env, evidence, saves, before, f'torch-{options.mode}',
                                               time.time_ns() // 1000000, 1200)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Actual SDK Torch placement process failed')
            report['receipts'] = observations(farming, result.stdout, 'torch', id, removal)
            report['geometry'] = geometry(evidence, id, mining.digest, removal)
            report['readback'] = readback(parser, mining, farming, evidence,
                                          report['receipts'], authored, id, removal)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Unrelated authored save changed')
            require(all(name.startswith('alpha/') for name in set(after) - set(before)),
                    'No unrelated World creation')
            selected, final = world.snapshot(evidence / 'snapshots/torch-saved'), world.snapshot(saves)
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
    require(report['status'] == 'PASS', 'Torch fixture continuity')
    print(f'Actual SDK Torch {options.mode}/save/readback PASS: {evidence}')


if __name__ == '__main__':
    main()
