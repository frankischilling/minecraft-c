"""SDK setup, authored Workbench invalidation, actual client World tick and save."""
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


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


B = module('workbench_auto_source_support', ROOT / 'tests/tools/client/test_workbench_loop.py')
require = B.require


def metadata(mining, selected, world, name):
    wire = mining.mining_metadata(selected, world, name)
    empty = world.tag(9, 'Inventory', b'\x0a\x00\x00\x00\x00')
    records = []
    for slot, item, count in ((0, 270, 1), (9, 5, 7), (11, 280, 2)):
        records.append(world.tag(1, 'Slot', bytes([slot])) +
                       world.tag(2, 'id', struct.pack('>h', item)) +
                       world.tag(1, 'Count', bytes([count])) +
                       world.tag(2, 'Damage', b'\x00\x00') + b'\x00')
    require(wire.count(empty) == 1, 'One declared pre-load Inventory')
    return wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' +
                        struct.pack('>i', len(records)) + b''.join(records)))


def saved_regions(selected, world, directory):
    # Keep the existing solid target's consistent authored height and light.
    selected.saved_regions(world, directory, {'id': 3, 'metadata': 0})
    path = directory / 'r.0.0.mcr'
    region = bytearray(path.read_bytes())
    location = struct.unpack_from('>I', region, 0)[0]
    sector, count = location >> 8, location & 255
    size = struct.unpack_from('>I', region, sector * 4096)[0]
    require(region[sector * 4096 + 4] == 2, 'Authored target zlib record')
    wire = zlib.decompress(region[sector * 4096 + 5:sector * 4096 + 4 + size])
    marker = b'\x07\x00\x06Blocks' + struct.pack('>i', 32768)
    require(wire.count(marker) == 1, 'One exact authored Blocks array')
    start = wire.index(marker) + len(marker)
    blocks = bytearray(wire[start:start + 32768])
    require(len(blocks) == 32768 and blocks[320] == 3, 'Declared solid target0,64,2')
    blocks[320] = 58
    wire = wire[:start] + bytes(blocks) + wire[start + 32768:]
    compressed = zlib.compress(wire)
    record = struct.pack('>I', len(compressed) + 1) + b'\x02' + compressed
    require(len(record) <= count * 4096, 'Authored Table fits its allocated sector range')
    region[sector * 4096:(sector + count) * 4096] = record + bytes(count * 4096 - len(record))
    path.write_bytes(region)


def save_gate(mining, parser, evidence, saves, mode):
    raw = json.loads((evidence / 'workbench-raw-entry.json').read_text(encoding='utf-8'),
                     object_pairs_hook=B.unique)
    require(type(raw) is dict and set(raw) == {'inventory', 'items'} and
            type(raw['items']) is list and len(raw['items']) <= 128, 'Exact selected raw save entry')
    inventory = B.raw_inventory(raw['inventory'])
    entry = parser.Nbt((evidence / 'workbench-player-entry.nbt').read_bytes()).root()
    saved = parser.Nbt(gzip.decompress((saves / 'alpha/level.dat').read_bytes())).root()['Data'][1]['Player'][1]
    require(entry.get('Inventory') == saved.get('Inventory') == inventory,
            'All40 raw source slots match independent entry and saved Inventory NBT')
    mining.same_fields(saved, entry, ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Selected actual paused entry versus ordinary save')
    require(saved.get('Score') == [3, 7] and saved.get('Health') == [2, 20],
            'Authored Player score and health preserved')
    count = int((evidence / 'workbench-items.count').read_text(encoding='ascii'))
    require(0 <= count <= 128 and count == len(raw['items']), 'Every live Item owner selected')
    live = [parser.Nbt((evidence / f'workbench-items-{index:04d}.nbt').read_bytes()).root()
            for index in range(count)]
    expected = [B.raw_item(row) for row in raw['items']]
    require([mining.item_key(row) for row in live] == [mining.item_key(row) for row in expected],
            'Every Item source field independently matches semantic NBT')
    items, chunks = mining.saved_items(parser, saves / 'alpha')
    require(Counter(mining.item_key(row['entity']) for row in items) ==
            Counter(mining.item_key(row) for row in expected),
            'All World Items saved exactly once, including unrelated natural drops')
    totals = Counter()
    for _, item, amount, damage in raw['inventory']:
        if item in (5, 280, 270):
            require(damage == 0, 'Unhurt declared Inventory material/tool')
            totals[item] += amount
    for row in raw['items']:
        item, amount, damage = row['stack']
        if item in (5, 280, 270):
            require(damage == 0, 'Unhurt declared World material/tool')
            totals[item] += amount
    require(totals == Counter({5: 7, 280: 2, 270: 1}),
            'Exact declared material conservation across later actual pickup')
    level = mining.selected_chunk(parser, saves / 'alpha')
    before = mining.selected_chunk(parser, evidence / 'before/alpha')
    blocks, height = bytearray(before['Blocks'][1]), bytearray(before['HeightMap'][1])
    require(blocks[320] == 58 and height[32] == 65, 'Consistent authored solid Table')
    if mode == 'removed':
        blocks[320], height[32] = 0, 64
    require(level['Blocks'] == [7, bytes(blocks)] and level['Data'] == before['Data'] and
            level['HeightMap'] == [7, bytes(height)], 'Exact selected terrain after declared trigger')
    return {'inventory': inventory, 'items': len(items), 'chunks': chunks,
            'known_material_totals': dict(totals), 'raw_entry': raw}


def receipts(mining, stdout, mode):
    setup = mining.rows(stdout, 'WORKBENCH_AUTO_SETUP')
    require(len(setup) == 1 and set(setup[0]) == {'mode', 'sdk-open', 'sdk-cursor-grid',
            'ui-frames', 'world-ticks'} and setup[0]['mode'] == mode and
            setup[0]['sdk-open'] == setup[0]['sdk-cursor-grid'] == '1' and
            int(setup[0]['ui-frames']) >= 3 and int(setup[0]['world-ticks']) >= 3,
            'Genuine SDK open/cursor/grid and continuing nonpausing World')
    close = mining.rows(stdout, 'WORKBENCH_AUTO_CLOSE')
    require(len(close) == 1, 'One standalone actual client World close call')
    row = close[0]
    require(set(row) == {'mode', 'trigger', 'client-world-ticks', 'same-call-r-null',
            'default-restored', 'cursor-drop', 'grid-drop', 'publication-order', 'same-stacks',
            'grid-retained', 'world-time-delta', 'before-distance', 'after-distance'} and
            row['mode'] == mode and row['trigger'] ==
            ('authored-world-set' if mode == 'removed' else 'authored-entity-position') and
            row['publication-order'] == 'cursor-grid' and all(row[key] == '1' for key in
            ('client-world-ticks', 'same-call-r-null', 'default-restored', 'cursor-drop',
             'grid-drop', 'same-stacks', 'grid-retained', 'world-time-delta')),
            'Exact authored trigger and actual source close/drop receipts')
    before, after = [B.number(row[key], 8) for key in ('before-distance', 'after-distance')]
    require(before <= 64 and (after <= 64 if mode == 'removed' else after > 64),
            'Actual finite distance distinguishes the two invalidation reasons')
    returned = mining.rows(stdout, 'WORKBENCH_AUTO_SDK')
    require(len(returned) == 1 and set(returned[0]) == {'mode', 'held-pickaxe-frames',
            'save-samples', 'ordinary-save'} and returned[0]['mode'] == mode and
            returned[0]['held-pickaxe-frames'] == '3' and int(returned[0]['save-samples']) > 0 and
            returned[0]['ordinary-save'] == '1', 'Continued World/HUD/hand and ordinary save')
    finish = mining.rows(stdout, 'WORKBENCH_AUTO_FINISH')
    require(len(finish) == 1 and set(finish[0]) == {'mode', 'cleanup', 'windows-absent', 'checks'} and
            finish[0]['mode'] == mode and finish[0]['cleanup'] == finish[0]['windows-absent'] == '1' and
            int(finish[0]['checks']) > 0, 'Checked cleanup and absent source windows')
    return {'setup': setup[0], 'close': row, 'continued': returned[0], 'finish': finish[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--mode', choices=('removed', 'distance'), required=True)
    args = parser.parse_args()
    require(os.name == 'nt', 'Windows SDK/composed integration qualification')
    mining = module('workbench_auto_mining_support', ROOT / 'tests/tools/client/test_mining_loop.py')
    selected = mining.module('workbench_auto_saved_inputs', ROOT / 'tests/tools/client/test_block_selection.py')
    world = selected.module('workbench_auto_world_evidence', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    decoder = selected.module('workbench_auto_independent_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='workbench-auto-' + args.mode + '-', dir=args.artifacts.resolve()))
    (evidence / 'snapshots').mkdir()
    (evidence / 'entries').mkdir()
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
              ROOT / 'tests/tools/world/test_source_saving.py'}
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client'):
        inputs.update(path for path in (ROOT / folder).rglob('*')
                      if path.is_file() and path.suffix in ('.c', '.h', '.inc', '.cmake', '.py'))
    inputs.update(executable.parent.glob('*.dll'))
    packet_directory = ROOT / 'tests/reference/inventory/workbench_auto_close'
    inputs.update(path for path in packet_directory.iterdir()
                  if path.is_file() and path.suffix in ('.py', '.json'))
    before_inputs = {str(path): mining.digest(path) for path in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'mode': args.mode, 'inputs_before': before_inputs,
              'assets_before': before_assets, 'authored_inventory': [[0, 270, 1, 0], [9, 5, 7, 0], [11, 280, 2, 0]],
              'authored_table': [0, 64, 2, 58, 0],
              'invalidation_scope': 'Declared native World setter or Entity position method; not SDK mining, movement or knockback.',
              'input_scope': 'Selected source/harness/CMake/executable/siblingDLLs and imported resources; excludes toolchain and Desktop state'}
    try:
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            game = Path(temporary) / '.minecraft'
            game.mkdir()
            report['statistics_input'] = B.statistics_input(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\n')
            saves = game / 'saves'
            saves.mkdir()
            for folder, name, played in [('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Automatic close world', 1304000000000)]:
                directory = saves / folder
                directory.mkdir()
                wire = metadata(mining, selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
            region = saves / 'alpha/region'
            region.mkdir()
            saved_regions(selected, world, region)
            unrelated = region / 'r.9.9.mcr'
            unrelated.write_bytes(bytes(8192))
            unrelated_before = unrelated.read_bytes()
            before = world.save_title_files(saves, evidence / 'before')
            home = game.parent
            environment = os.environ.copy()
            environment.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                               ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                               MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                               MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), str(evidence), args.mode],
                environment, evidence, saves, before, 'workbench-auto', time.time_ns() // 1000000, 600)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Native automatic-close integration returned')
            report['receipt'] = receipts(mining, result.stdout, args.mode)
            original = module('workbench_auto_original_packet', packet_directory / 'packet.py')
            structural = module('workbench_auto_structure', packet_directory / 'sdk_compare.py')
            original.audit_public()
            actual = structural.rows(result.stdout)
            require(len(actual) == 1 and actual[0]['profile'] == 'tick-' + args.mode,
                    'Exactly this native primary structural profile')
            report['original_structure'] = structural.compare(actual, original.load('finite.json')['rows'])
            report['save'] = save_gate(mining, decoder, evidence, saves, args.mode)
            require(unrelated.read_bytes() == unrelated_before and
                    (evidence / 'snapshots/workbench-saved/alpha/region/r.9.9.mcr').read_bytes() == unrelated_before,
                    'Unrelated actual region bytes preserved through ordinary Save and quit')
            for name in ('workbench-auto-open.png', 'workbench-auto-before-close.png',
                         'workbench-auto-held.png', 'workbench-save-title.png'):
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
    require(report['status'] == 'PASS', 'Complete integration/save and selected input continuity')
    print('Workbench automatic-close integration PASS: ' + str(evidence))


if __name__ == '__main__':
    main()
