"""Actual SDK Dirt mining, Item save/reload, pickup and held-item continuation."""
import argparse
from collections import Counter
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
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= 256 * 1048576,
            'Bounded actual source/product input')
    result = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1048576), b''):
            result.update(block)
    return result.hexdigest()


def rows(stdout, prefix):
    result = []
    for line in stdout.splitlines():
        if line.startswith(prefix + ' '):
            pairs = [word.split('=', 1) for word in line.split()[1:]]
            require(all(len(pair) == 2 for pair in pairs), 'Exact receipt word pairs')
            value = dict(pairs)
            require(len(value) == len(pairs), 'Unique receipt fields')
            result.append(value)
    return result


def typed(tag):
    kind, value = tag
    if kind == 10:
        return [kind, {name: typed(child) for name, child in value.items()}]
    if kind == 9:
        child, items = value
        # Only the unused child byte of an empty List is unspecified by the
        # existing generic builder; preserve every nonempty type and payload.
        return [kind, [child if items else 0, [typed([child, item])[1] for item in items]]]
    return [kind, value]


def same_fields(left, right, names, label):
    for name in names:
        require(name in left and name in right and typed(left[name]) == typed(right[name]),
                label + ': actual typed ' + name)


def selected_chunk(parser, directory):
    data = parser.read(directory / 'region/r.0.0.mcr')
    require(len(data) >= 8192 and len(data) % 4096 == 0, 'Complete actual region')
    location, = struct.unpack_from('>I', data, 0)
    offset, sectors = (location >> 8) * 4096, location & 255
    require(offset >= 8192 and sectors > 0 and offset + sectors * 4096 <= len(data),
            'Actual selected Chunk allocation')
    size, = struct.unpack_from('>I', data, offset)
    require(1 < size <= sectors * 4096 - 4 and data[offset + 4] == 2,
            'Actual selected compressed Chunk')
    tree = parser.inflate(data[offset + 5:offset + 4 + size], 15)
    require(tree.get('Level', [None])[0] == 10, 'Actual Level Compound')
    return tree['Level'][1]


def saved_items(parser, directory):
    regions = sorted((directory / 'region').glob('r.*.*.mcr'))
    require(0 < len(regions) <= 16, 'Bounded actual saved region inventory')
    items, chunks, total_bytes = [], 0, 0
    for path in regions:
        name = re.fullmatch(r'r\.(-?\d+)\.(-?\d+)\.mcr', path.name)
        require(name is not None and path.is_file() and not path.is_symlink() and
                8192 <= path.stat().st_size <= 16 * 1048576,
                'Bounded actual saved region file')
        data = path.read_bytes()
        total_bytes += len(data)
        require(len(data) % 4096 == 0 and total_bytes <= 64 * 1048576,
                'Complete bounded actual saved regions')
        rx, rz = map(int, name.groups())
        sectors_used = {0, 1}
        for slot in range(1024):
            location, = struct.unpack_from('>I', data, slot * 4)
            if location == 0:
                continue
            sector, sectors = location >> 8, location & 255
            offset = sector * 4096
            require(sector >= 2 and sectors > 0 and offset + sectors * 4096 <= len(data) and
                    not sectors_used.intersection(range(sector, sector + sectors)),
                    'Actual allocated nonoverlapping saved Chunk sectors')
            sectors_used.update(range(sector, sector + sectors))
            size, = struct.unpack_from('>I', data, offset)
            require(1 < size <= sectors * 4096 - 4 and data[offset + 4] == 2,
                    'Actual saved compressed Chunk record')
            tree = parser.inflate(data[offset + 5:offset + 4 + size], 15)
            require(tree.get('Level', [None])[0] == 10, 'Actual saved Level Compound')
            level = tree['Level'][1]
            coordinate = [rx * 32 + slot % 32, rz * 32 + slot // 32]
            require(level.get('xPos') == [3, coordinate[0]] and
                    level.get('zPos') == [3, coordinate[1]] and
                    level.get('Entities', [None])[0] == 9,
                    'Actual saved Chunk coordinate and Entity List')
            kind, entities = level['Entities'][1]
            require(not entities or kind == 10, 'Actual saved Entity Compound List type')
            chunks += 1
            require(chunks <= 4096, 'Bounded actual saved Chunk count')
            for entity in entities:
                if entity.get('id') != [8, 'Item']:
                    continue
                require(entity.get('Item', [None])[0] == 10, 'Actual Item Stack Compound')
                stack = entity['Item'][1]
                require(stack.get('id', [None])[0] == 2 and stack['id'][1] > 0 and
                        stack.get('Count', [None])[0] == 1 and stack['Count'][1] > 0 and
                        stack.get('Damage', [None])[0] == 2 and stack['Damage'][1] >= 0,
                        'Actual saved Item Stack fields')
                items.append({'chunk': coordinate, 'entity': entity})
                require(len(items) <= 128, 'Bounded actual saved Item count')
    return items, chunks


def item_key(entity):
    # The source Chunk wrapper adds id; mc_entity_build_nbt writes the Entity
    # body. Compare every body field, retaining all types and floating bits.
    require('id' not in entity or entity['id'] == [8, 'Item'], 'Actual Item wrapper identity')
    body = {name: value for name, value in entity.items() if name != 'id'}
    return json.dumps(typed([10, body]), sort_keys=True, separators=(',', ':'))


def mining_metadata(selected, world, name):
    """Authored saved position/look: target remains within ordinary four-block reach.

    Starting three blocks from the drop lets its genuine BB/save/reload phase
    precede the later SDK forward movement and actual pickup. Runtime pose and
    targeting always come from normal load/input/picking.
    """
    wire = selected.metadata(world, name)
    saved_y = 64 + struct.unpack('>f', struct.pack('>f', 1.62))[0]
    old_position = world.tag(9, 'Pos', b'\x06' + struct.pack('>i3d', 3, .5, saved_y, .5))
    new_position = world.tag(9, 'Pos', b'\x06' + struct.pack('>i3d', 3, .5, saved_y, -.5))
    old_rotation = world.tag(9, 'Rotation', b'\x05' + struct.pack('>i2f', 2, 0., 45.))
    new_rotation = world.tag(9, 'Rotation', b'\x05' + struct.pack('>i2f', 2, 0., 30.))
    require(wire.count(old_position) == wire.count(old_rotation) == 1,
            'One explicit authored saved Player position and rotation')
    return wire.replace(old_position, new_position).replace(old_rotation, new_rotation)


def inventory(player, expected):
    require(player.get('Inventory', [None])[0] == 9, 'Actual Player Inventory List')
    child, items = player['Inventory'][1]
    require(len(items) == expected and (not items or child == 10), 'Actual inventory count/type')
    if expected:
        item = items[0]
        require(item.get('id') == [2, 3] and item.get('Count') == [1, 1] and
                item.get('Damage') == [2, 0] and item.get('Slot') == [1, 0],
                'One actual Dirt stack in source-selected hotbar slot0')


def semantic_evidence(parser, evidence):
    reports = []
    for epoch, expected_items, expected_inventory in ((1, 1, 0), (2, 0, 1)):
        saved = evidence / f'snapshots/saved-{epoch}/alpha'
        root = parser.inflate(parser.read(saved / 'level.dat'), 31)
        require(root.get('Data', [None])[0] == 10 and
                root['Data'][1].get('Player', [None])[0] == 10,
                'Actual saved Data/Player compounds')
        player = root['Data'][1]['Player'][1]
        observed = parser.Nbt(parser.read(evidence / f'saved-player-{epoch}.nbt')).root()
        reopened = parser.Nbt(parser.read(evidence / f'reloaded-player-{epoch}.nbt')).root()
        same_fields(player, observed,
                    ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                    'Live Player at selected paused leave-entry frame versus decoded disk')
        same_fields(player, reopened, ('Health', 'Score', 'Dimension', 'Inventory'),
                    'Actual reselected Player stable semantic fields')
        require(player.get('Score') == [3, 7], 'Authored Score7 remains intact')
        inventory(player, expected_inventory)
        inventory(reopened, expected_inventory)
        level = selected_chunk(parser, saved)
        require(level.get('xPos') == [3, 0] and level.get('zPos') == [3, 0] and
                level.get('Blocks', [None])[0] == 7 and len(level['Blocks'][1]) == 32768 and
                level['Blocks'][1][(2 << 7) | 64] == 0 and level.get('Data', [None])[0] == 7 and
                len(level['Data'][1]) == 16384 and level['Data'][1][((2 << 7) | 64) // 2] & 15 == 0,
                'Actual saved mined coordinate is air with metadata0')
        all_items, chunk_count = saved_items(parser, saved)
        count = int(parser.read(evidence / f'saved-items-{epoch}.count').decode('ascii').strip())
        require(0 <= count <= 128, 'Bounded live entry Item semantic inventory')
        observed_items = [parser.Nbt(parser.read(
            evidence / f'saved-items-{epoch}-{index:04d}.nbt')).root() for index in range(count)]
        require(Counter(item_key(row['entity']) for row in all_items) ==
                Counter(item_key(entity) for entity in observed_items),
                'Every saved Item, including incidental Items, matches owned live entry NBT')
        dirt = [row for row in all_items if row['entity']['Item'][1]['id'] == [2, 3]]
        items = [row['entity'] for row in dirt]
        require(len(items) == expected_items, 'Unique actual saved Dirt Item after mining/pickup')
        if expected_items:
            item = items[0]
            observed_item = parser.Nbt(parser.read(evidence / 'saved-item-1.nbt')).root()
            reopened_item = parser.Nbt(parser.read(evidence / 'reloaded-item-1.nbt')).root()
            same_fields(item, observed_item, ('Pos', 'Motion', 'Rotation', 'Health', 'Age', 'Item'),
                        'Live dropped Item at selected paused leave-entry frame versus disk')
            same_fields(item, reopened_item, ('Health', 'Item'), 'Actual reopened Item stable fields')
            require(item.get('Item', [None])[0] == 10 and
                    item['Item'][1].get('id') == [2, 3] and
                    item['Item'][1].get('Count') == [1, 1] and
                    item['Item'][1].get('Damage') == [2, 0] and
                    reopened_item.get('Age', [None])[0] == 2 and
                    reopened_item['Age'][1] >= item['Age'][1],
                    'Real Dirt Item stack retained and ordinary reloaded ticks advance age')
        reports.append({'epoch': epoch, 'target': [0, 64, 2], 'block_id': 0, 'metadata': 0,
                        'items': len(items), 'inventory_slots': expected_inventory,
                        'all_items': len(all_items), 'incidental_items': len(all_items) - len(items),
                        'saved_chunks': chunk_count, 'dirt_item_chunks': [row['chunk'] for row in dirt],
                        'saved_level_sha256': digest(saved / 'level.dat')})
    return reports


def receipts(stdout):
    broken = rows(stdout, 'MINING_BREAK')
    destruction = rows(stdout, 'MINING_DESTRUCTION')
    pickup = rows(stdout, 'MINING_PICKUP')
    saved = rows(stdout, 'MINING_SAVE')
    entries = rows(stdout, 'MINING_SAVE_ENTRY')
    reloaded = rows(stdout, 'MINING_RELOAD')
    finish = rows(stdout, 'MINING_FINISH')
    require(len(broken) == len(destruction) == len(pickup) == len(reloaded) == len(finish) == 1 and
            len(saved) == len(entries) == 2, 'Complete actual native lifecycle receipts')
    for epoch, entry in enumerate(entries, 1):
        require(entry.get('epoch') == str(epoch) and int(entry.get('samples', '0')) > 0 and
                entry.get('items') == ('1' if epoch == 1 else '0') and
                int(entry.get('items', '0')) <= int(entry.get('all-items', '-1')) <= 128 and
                entry.get('paused') == entry.get('owned-nbt') ==
                entry.get('selected-before-leave-frame') == '1',
                'Owned semantic NBT captured from the live paused frame that entered leave')
    values = {key: int(value) for key, value in broken[0].items()}
    require(0 < values['partial'] <= values['frames'] and values['qm-frames'] > 0 and
            values['qm-ticks'] > 0 and values['item-frames'] >= 3 and 0 <= values['max-ticks'] <= 10 and
            values['clicks'] > 0 and values['swings'] > 0 and values['click-stores'] > 0 and
            values['starts'] > 0 and values['progress'] > 0 and values['mined'] == 1 and
            values['hit-constructed'] > 0 and values['hit-appended'] > 0,
            'Actual partial crack/QM and fresh click/swing/progress/full-break receipts')
    require(destruction[0] == {'event2001': '1', 'sound': '1', 'qm-constructed': '64',
            'qm-appended': '64', 'sound-before-particles': '1', 'before-air': '1'},
            'Real returned sound-before-QM64 auxiliary event')
    require(pickup[0].get('sdk-forward') == pickup[0].get('actual-movement') ==
            pickup[0].get('inventory-dirt') == pickup[0].get('count') == '1' and
            pickup[0].get('live-items') == '0' and int(pickup[0]['held-frames']) >= 3,
            'Actual SDK movement/pickup and returned nonempty HUD/held frames')
    require(saved == [{'epoch': '1', 'air': '1', 'items': '1', 'ordinary-save-title': '1'},
                      {'epoch': '2', 'air': '1', 'items': '0', 'ordinary-save-title': '1'}],
            'Real Item-present and Item-consumed ordinary saves')
    require(reloaded[0] == {'epochs': '3', 'saved-item': '1', 'reloaded-item': '1',
            'picked-item-absent': '1', 'air': '1', 'inventory-dirt': '1'} and
            finish[0].get('cleanup') == finish[0].get('windows-absent') == '1' and
            int(finish[0]['checks']) > 0, 'Full actual reload and checked native cleanup')
    return {'mining': values, 'pickup': pickup[0], 'cleanup_checks': int(finish[0]['checks'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'Disposable Linux account/mount qualification required before live World inputs')
    selected = module('mining_selection_inputs', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('mining_source_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder = selected.module('mining_independent_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='complete-dirt-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = [Path(__file__).resolve(), executable,
              ROOT / 'tests/client/gameplay/test_mining.c',
              ROOT / 'tests/client/startup/test_world_selection_loop.c',
              ROOT / 'tests/client/startup/test_world_reload.inc',
              ROOT / 'tests/tools/client/test_block_selection.py',
              ROOT / 'tests/tools/client/test_world_selection_loop.py',
              ROOT / 'tests/tools/world/test_source_saving.py', ROOT / 'tests/support/java_random.h']
    inputs += sorted(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                     if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    before_inputs = {str(path): digest(path) for path in inputs}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before_inputs,
              'assets_before': before_assets, 'evidence': str(evidence),
              'authored_player_input': {'feet': [.5, 64., -.5],
                  'saved_y': '64 + binary32(1.62)', 'rotation': [0., 30.],
                  'motion': [0., 0., 0.], 'target': [0, 64, 2],
                  'pose_source': 'saved Player NBT before bootstrap; no runtime pose/Hit writes'}}
    (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    try:
        with tempfile.TemporaryDirectory(prefix='mining-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Mining world', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                metadata = mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                if folder == 'alpha':
                    actual = decoder.Nbt(metadata).root()['Data'][1]['Player'][1]
                    require(actual['Pos'][0] == actual['Rotation'][0] == 9 and
                            actual['Pos'][1][0] == 6 and actual['Rotation'][1][0] == 5,
                            'Actual authored saved pose has Double/Float List types')
                    report['authored_player_pose_from_nbt'] = {
                        'position_double_bits': [f'{value:016x}' for value in actual['Pos'][1][1]],
                        'rotation_float_bits': [f'{value:08x}' for value in actual['Rotation'][1][1]],
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
                    environment, evidence, saves, before, 'complete-dirt', first, 900)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native mining executable failed')
            report['actual'] = receipts(result.stdout)
            report['saved_semantics'] = semantic_evidence(decoder, evidence)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Mining changed unrelated authored input')
            require(not any(not name.startswith('alpha/') for name in set(after) - set(before)),
                    'Mining created an unrelated World file')
            saved_after = {Path(key).as_posix(): value for key, value in
                           world.snapshot(evidence / 'snapshots/saved-2').items()}
            final_disk = {Path(key).as_posix(): value for key, value in world.snapshot(saves).items()}
            require(set(saved_after) == set(final_disk) and
                    all(final_disk[key] == value for key, value in saved_after.items()
                        if key != 'alpha/session.lock') and
                    (saves / 'alpha/session.lock').stat().st_size == 8,
                    'Reselection changes only its actual eight-byte session token')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['assets_after'] = world.snapshot(assets)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/imported resources changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Mining fixture input continuity failed')
    print(f'Actual SDK complete Dirt mining PASS; {evidence}')


if __name__ == '__main__':
    main()
