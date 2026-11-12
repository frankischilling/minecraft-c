"""Real SDK crafting-table progression with independent paused-entry/save checks."""
import argparse
from collections import Counter
import gzip
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(value, message):
    if not value:
        raise RuntimeError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def unique(pairs):
    result = dict(pairs)
    require(len(result) == len(pairs), 'Unique raw source observation keys')
    return result


def statistics_input(game, assets):
    mapping = {}
    for line in (assets / 'jar/achievement/map.txt').read_text(encoding='ascii').splitlines():
        key, _, value = line.partition(',')
        if key in ('5242880', '5242881'):
            require(key not in mapping and re.fullmatch('[0-9a-f]{1,32}', value),
                    'Unique imported initial achievement GUID')
            mapping[key] = value
    require(set(mapping) == {'5242880', '5242881'}, 'Both declared initial achievement GUIDs')
    declarations = [{'5242880': 1}, {'5242881': 1}]
    payload = ('local' + ''.join(mapping[key] + ',1,' for key in ('5242880', '5242881'))).encode('ascii')
    checksum = format(int.from_bytes(hashlib.md5(payload).digest(), 'big'), 'x')
    wire = (json.dumps({'stats-change': declarations, 'checksum': checksum}) + '\n').encode('ascii')
    directory = game / 'stats'
    directory.mkdir()
    (directory / 'stats_selection control_unsent.dat').write_bytes(wire)
    return {'declared': declarations, 'buildWorkbench_initially_present': False,
            'buildPickaxe_initially_present': False, 'sha256': hashlib.sha256(wire).hexdigest(),
            'wire': wire.decode('ascii')}


def metadata(mining, selected, world, name):
    wire = mining.mining_metadata(selected, world, name)
    empty = world.tag(9, 'Inventory', b'\x0a\x00\x00\x00\x00')
    stack = (world.tag(1, 'Slot', b'\x09') + world.tag(2, 'id', struct.pack('>h', 17)) +
             world.tag(1, 'Count', b'\x04') + world.tag(2, 'Damage', b'\x00\x00') + b'\x00')
    require(wire.count(empty) == 1, 'One declared pre-load Inventory')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 1) + stack))


def integer(value):
    require(type(value) is int and -(1 << 31) <= value < (1 << 31), 'Raw source signed int32')
    return value


def signed(value, bits):
    value = integer(value) & ((1 << bits) - 1)
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def number(value, width):
    require(type(value) is str and re.fullmatch('[0-9a-f]{' + str(width * 2) + '}', value),
            'Exact raw source floating word')
    result, = struct.unpack('>f' if width == 4 else '>d', bytes.fromhex(value))
    require(math.isfinite(result), 'Finite actual saved Item operand')
    return result


def floating_word(value, width):
    number(value, width)
    return int(value, 16)


def raw_inventory(values):
    require(type(values) is list and len(values) <= 40, 'Bounded raw source Inventory')
    result, previous = [], -1
    for row in values:
        require(type(row) is list and len(row) == 4, 'Raw source Slot/id/count/damage')
        slot, item_id, count, damage = map(integer, row)
        require(slot > previous and slot in (*range(36), *range(100, 104)) and
                item_id > 0 and count > 0 and damage >= 0, 'Ordered actual nonempty source slots')
        previous = slot
        result.append({'Slot': [1, signed(slot, 8)], 'id': [2, signed(item_id, 16)],
                       'Count': [1, signed(count, 8)], 'Damage': [2, signed(damage, 16)]})
    require(values and values[0] == [0, 270, 1, 0], 'Crafted unhurt Pickaxe remains selected slot0')
    return [9, [10, result]]


def raw_item(row):
    require(type(row) is dict and set(row) ==
            {'stack', 'pos', 'ysize', 'motion', 'rotation', 'fall', 'fire', 'air', 'ground', 'health', 'age'},
            'Exact passive World Item source schema')
    require(type(row['stack']) is list and len(row['stack']) == 3 and
            type(row['pos']) is list and len(row['pos']) == 3 and
            type(row['motion']) is list and len(row['motion']) == 3 and
            type(row['rotation']) is list and len(row['rotation']) == 2 and
            type(row['ground']) is bool, 'Typed actual Item source operands')
    item_id, count, damage = map(integer, row['stack'])
    require(item_id > 0 and count > 0 and damage >= 0, 'Actual nonempty World Item Stack')
    pos = [number(value, 8) for value in row['pos']]
    pos[1] += number(row['ysize'], 4)
    pos_bits = [struct.unpack('>Q', struct.pack('>d', value))[0] for value in pos]
    motion_bits = [floating_word(value, 8) for value in row['motion']]
    rotation_bits = [floating_word(value, 4) for value in row['rotation']]
    return {'Pos': [9, [6, pos_bits]],
            'Motion': [9, [6, motion_bits]], 'Rotation': [9, [5, rotation_bits]],
            'FallDistance': [5, floating_word(row['fall'], 4)],
            'Fire': [2, signed(row['fire'], 16)], 'Air': [2, signed(row['air'], 16)],
            'OnGround': [1, int(row['ground'])], 'Health': [2, signed(row['health'], 8)],
            'Age': [2, signed(row['age'], 16)],
            'Item': [10, {'id': [2, signed(item_id, 16)], 'Count': [1, signed(count, 8)],
                          'Damage': [2, signed(damage, 16)]}]}


def save_gate(mining, parser, evidence, saves):
    raw = json.loads((evidence / 'workbench-raw-entry.json').read_text(encoding='utf-8'),
                     object_pairs_hook=unique)
    require(type(raw) is dict and set(raw) == {'inventory', 'items'} and
            type(raw['items']) is list and len(raw['items']) <= 128, 'Exact selected raw save entry')
    expected_inventory = raw_inventory(raw['inventory'])
    entry = parser.Nbt((evidence / 'workbench-player-entry.nbt').read_bytes()).root()
    saved = parser.Nbt(gzip.decompress((saves / 'alpha/level.dat').read_bytes())).root()['Data'][1]['Player'][1]
    require(entry.get('Inventory') == saved.get('Inventory') == expected_inventory,
            'Every actual source Inventory field matches independent entry and saved NBT')
    mining.same_fields(saved, entry, ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Selected actual paused entry versus source save')
    require(saved.get('Score') == [3, 7] and saved.get('Health') == [2, 20],
            'Authored Player score/health preserved')
    count = int((evidence / 'workbench-items.count').read_text(encoding='ascii'))
    require(0 <= count <= 128 and count == len(raw['items']), 'All live Item source owners selected')
    live = [parser.Nbt((evidence / f'workbench-items-{index:04d}.nbt').read_bytes()).root()
            for index in range(count)]
    expected_items = [raw_item(row) for row in raw['items']]
    require([mining.item_key(row) for row in live] == [mining.item_key(row) for row in expected_items],
            'Every Item NBT body independently matches its actual raw source fields')
    items, chunks = mining.saved_items(parser, saves / 'alpha')
    require(Counter(mining.item_key(row['entity']) for row in items) ==
            Counter(mining.item_key(row) for row in expected_items),
            'All actual World Items persisted exactly once, including unrelated natural drops')
    totals = Counter()
    for _, item_id, amount, damage in raw['inventory']:
        if item_id in (5, 280, 270):
            require(damage == 0, 'Unhurt crafted material/tool in Inventory')
            totals[item_id] += amount
    for row in raw['items']:
        item_id, amount, damage = row['stack']
        if item_id in (5, 280, 270):
            require(damage == 0, 'Unhurt crafted material/tool in World')
            totals[item_id] += amount
    require(totals == Counter({5: 7, 280: 2, 270: 1}),
            'Exact recipe material conservation across the actual save-time pickup boundary')
    level = mining.selected_chunk(parser, saves / 'alpha')
    before = mining.selected_chunk(parser, evidence / 'before/alpha')
    expected_blocks = bytearray(before['Blocks'][1])
    require(expected_blocks[192] == 0 and expected_blocks[320] == 3, 'Declared placement neighborhood')
    expected_blocks[192] = 58
    require(level['Blocks'] == [7, bytes(expected_blocks)] and level['Data'][1][96] & 15 == 0,
            'Only the crafted Table occupies actual picked adjacent cell0,64,1; original Dirt retained')
    return {'inventory': expected_inventory, 'items': len(items), 'chunks': chunks,
            'known_material_totals': dict(totals), 'raw_entry': raw}


def receipts(mining, stdout):
    expected = {
        'WORKBENCH_WORLD_READY': {'source-N-TH-frame': '1', 'engine-camera-settings': '1',
                                  'no-field-repair': '1'},
        'WORKBENCH_UE': {'logs': '4', 'planks-crafted': '16', 'table-crafted': '1',
                         'sticks-crafted': '4', 'ingredients': '1', 'notices': '1'},
        'WORKBENCH_OO': {'slots': '46', 'shift-main-hotbar': '1', 'shift-hotbar-main': '1',
                         'pickaxe-crafted': '1', 'ingredients': '1', 'notices': '1'},
        'WORKBENCH_CLOSE': {'parent-default': '1', 'direct-it': '1', 'cursor-planks': '7',
                            'grid-sticks': '2', 'same-stacks': '1', 'publications': '2',
                            'grid-retained': '1', 'default-restored': '1'}}
    for marker, value in expected.items():
        require(mining.rows(stdout, marker) == [value], 'Exact actual source receipt ' + marker)
    rows = mining.rows(stdout, 'WORKBENCH_SDK')
    require(len(rows) == 1, 'One completed SDK progression')
    row = rows[0]
    require(set(row) == {'ui-frames', 'world-ticks', 'held-pickaxe-frames', 'save-samples',
                         'real-place', 'real-activate', 'craft2x2', 'craft3x3', 'shift',
                         'cursor-grid-close', 'ordinary-save'} and
            int(row['ui-frames']) >= 3 and int(row['world-ticks']) >= 3 and
            row['held-pickaxe-frames'] == '3' and int(row['save-samples']) > 0 and
            all(row[key] == '1' for key in ('real-place', 'real-activate', 'craft2x2', 'craft3x3',
                                          'shift', 'cursor-grid-close', 'ordinary-save')),
            'Actual continuing World, genuine recipes/inputs and selected save')
    finish = mining.rows(stdout, 'WORKBENCH_FINISH')
    require(len(finish) == 1 and set(finish[0]) == {'cleanup', 'windows-absent', 'checks'} and
            finish[0]['cleanup'] == finish[0]['windows-absent'] == '1' and int(finish[0]['checks']) > 0,
            'Checked terminal retirement and absent source windows')
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'Windows SDK fixture qualification')
    mining = module('workbench_mining_support', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('workbench_saved_inputs', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('workbench_world_evidence', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder = selected.module('workbench_independent_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='workbench-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
              ROOT / 'tests/tools/world/test_source_saving.py'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(executable.parent.glob('*.dll'))
    before_inputs = {str(path): mining.digest(path) for path in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before_inputs,
              'assets_before': before_assets, 'authored_inventory': [[9, 17, 4, 0]],
              'input_scope': 'Selected source/harness/CMake/executable/siblingDLLs and imported resources; excludes toolchain and Desktop state'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            game.mkdir()
            report['statistics_input'] = statistics_input(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Workbench world', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                wire = metadata(mining, selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
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
            result = world.retained_native_run([str(executable), str(assets), str(evidence)],
                environment, evidence, saves, before, 'workbench', time.time_ns() // 1000000, 600)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native Workbench SDK source loop returned')
            report['receipt'] = receipts(mining, result.stdout)
            report['save'] = save_gate(mining, decoder, evidence, saves)
            require(unrelated.read_bytes() == unrelated_before and
                    (evidence / 'snapshots/workbench-saved/alpha/region/r.9.9.mcr').read_bytes() == unrelated_before,
                    'Unrelated actual region bytes preserved through ordinary Save and quit')
            for name in ('workbench-crafted-2x2.png', 'workbench-placed.png', 'workbench-open-3x3.png',
                         'workbench-pickaxe-recipe.png', 'workbench-cursor-grid-before-close.png',
                         'workbench-held-pickaxe.png', 'workbench-save-title.png'):
                image = evidence / name
                require(image.is_file() and image.stat().st_size > 1000, 'Actual source-window capture ' + name)
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
    require(report['status'] == 'PASS', 'Complete SDK/save and selected input continuity')
    print('Workbench SDK crafting/save PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
