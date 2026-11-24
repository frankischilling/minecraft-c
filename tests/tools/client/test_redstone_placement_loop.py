"""One SDL SDK unpowered Item331 placement, loaded terrain draw, save and reselection."""
import argparse
import gzip
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
HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'src/client/runtime/menu_loop.c').is_file() and
            (p / 'CMakeLists.txt').is_file())
FIXTURE = HERE.parents[1] / 'client/gameplay/redstone/test_placement.c'


def require(value, label):
    if not value:
        raise AssertionError(label)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'Duplicate operand key')
        result[key] = value
    return result


def exact(row, expected, numeric=()):
    require(set(row) == set(expected) | set(numeric) and
            all(row[key] == value for key, value in expected.items()) and
            all(re.fullmatch(r'0|[1-9][0-9]*', row[key]) for key in numeric),
            'Exact Redstone Item receipt fields')


def observations(farming, stdout):
    names = ('INPUT', 'RETURN', 'SAVE', 'RELOAD', 'FINISH', 'CLEANUP')
    groups = [farming.rows(stdout, 'REDSTONE_ITEM_' + name) for name in names]
    require(all(len(rows) == 1 for rows in groups), 'Complete input/return/save/reload/cleanup')
    inp, returned, saved, loaded, finish, cleanup = [rows[0] for rows in groups]
    exact(inp, {'kind': 'redstone', 'taps': '1', 'right-clicks': '1', 'block-uses': '1',
                'air-uses': '0', 'admitted': '1', 'placed': '1', 'sounds': '0', 'count-stores': '1',
                'used': '1', 'cleared': '0', 'equip-reset': '1', 'drained': '1', 'item': '331',
                'block': '55', 'sdk-events': '2', 'metadata': '0', 'target': '3,65,3', 'face': '1'},
          ('direct-suffix',))
    require(inp['direct-suffix'] in ('0', '1'), 'Copied last-tick suffix availability')
    exact(returned, {'kind': 'redstone', 'ground': '3', 'item': '331', 'block': '55', 'stage': '0',
                    'count-before': '2', 'count-after': '1', 'damage': '0', 'health': '5',
                    'same-stack': '1', 'selected': '0', 'hud': '1', 'held': '1',
                    'continuing': '3', 'used': '1', 'list-replaced': '1'})
    exact(saved, {'health': '5', 'ground': '3', 'block': '55', 'stage': '0',
                  'source-save-title': '1', 'pre-frame-owned-nbt': '1', 'blocklight': '0',
                  'items': '0'}, ('sample', 'candidates'))
    require(0 <= int(saved['sample']) < int(saved['candidates']), 'Actual paused save entry')
    exact(loaded, {'distinct-world': '1', 'distinct-player': '1', 'health': '5', 'ground': '3',
                   'block': '55', 'item': '331', 'count': '1', 'damage': '0', 'selected': '0',
                   'hud': '1', 'held': '1', 'used': '1', 'stage': '0', 'items': '0'})
    exact(cleanup, {'status': '0', 'owners-null': '1', 'windows-absent': '1'})
    exact(finish, {'cleanup': '1', 'windows-absent': '1'}, ('checks',))
    require(int(finish['checks']) > 0, 'Terminal native checks')
    geometry = farming.rows(stdout, 'REDSTONE_ITEM_GEOMETRY')
    require(len(geometry) == 3, 'Three ordered actual display-list captures')
    for row, phase, vertices, dust in zip(geometry, ('before', 'placed', 'reloaded'), (60, 72, 72), (0, 12, 12)):
        exact(row, {'phase': phase, 'vertices': str(vertices), 'redstone-vertices': str(dust),
                    'prefix': '60', 'arrays': '7', 'mode': '4', 'source-list-returned': '1',
                    'terrain-returned': '1'}, ('initial-compile-paired',))
        require(row['initial-compile-paired'] in ('0', '1') and
                (phase != 'placed' or row['initial-compile-paired'] == '1'),
                'Placement compile must have a stable actual frame bracket')
    lights = farming.rows(stdout, 'REDSTONE_ITEM_LIGHT')
    require(len(lights) == 2, 'Placed/reloaded ordinary light completion')
    for row, phase in zip(lights, ('placed', 'reloaded')):
        exact(row, {'phase': phase, 'queue': '0', 'target': '0', 'east': '0', 'west': '0',
                    'south': '0', 'north': '0', 'up': '0', 'down': '0', 'up-two': '0',
                    'diagonal': '0', 'normal-frames': '1'})
    return dict(zip(('input', 'returned', 'save', 'loaded', 'finish', 'cleanup'),
                    (inp, returned, saved, loaded, finish, cleanup)), geometry=geometry, lighting=lights)


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def float_bits(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{8}', value), 'Exact float bits')
    return struct.unpack('<f', struct.pack('<I', int(value, 16)))[0]


def operands(value, target):
    keys = {'identities', 'texture', 'emission', 'sky', 'block_light', 'sky_subtraction',
            'light_level', 'brightness_bits', 'table_bits', 'voxels', 'world_random'}
    require(type(value) is dict and set(value) == keys, 'Complete passive operand snapshot')
    require(len(value['identities']) == 8 and all(type(x) is int and 0 < x < 2**64
            for x in value['identities']), 'Actual live native operand identities')
    for name in ('texture', 'emission', 'sky', 'block_light', 'sky_subtraction', 'light_level'):
        require(type(value[name]) is int and -2**31 <= value[name] < 2**31, 'Actual Java int operand')
    require(value['texture'] == 164 and value['emission'] == value['block_light'] == 0 and
            0 <= value['sky'] <= 15 and 0 <= value['sky_subtraction'] <= 15 and
            0 <= value['light_level'] <= 15,
            'Admitted isolated metadata0 Dust texture and non-emitting lighting')
    require(value['light_level'] == max(value['sky'] - value['sky_subtraction'],
                                       value['block_light'], value['emission']),
            'Independent captured light-operand reduction')
    require(len(value['table_bits']) == 16, 'Actual provider brightness table16')
    table = [float_bits(x) for x in value['table_bits']]
    require(all(math.isfinite(x) and 0 <= x <= 1 for x in table) and
            value['brightness_bits'] == value['table_bits'][value['light_level']],
            'Actual provider table supplies captured brightness; no time constant')
    require(type(value['voxels']) is list and len(value['voxels']) == 27 and
            all(type(row) is list and len(row) == 2 and
                all(type(cell) is int for cell in row) for row in value['voxels']),
            'Complete passive 3x3x3 operand cube')
    expected = [[0, 0] for _ in range(27)]
    expected[10] = [3, 0]
    expected[13] = [target, 0]
    require(value['voxels'] == expected, 'Exact declared isolated support/target/neighbor operands')
    random = value['world_random']
    require(set(random) == {'state', 'gaussian', 'ready'} and
            re.fullmatch('[0-9a-f]{12}', random['state']) and
            re.fullmatch('[0-9a-f]{16}', random['gaussian']) and type(random['ready']) is bool,
            'Passive actual World Random fields; no equality across ordinary frames')
    return table[value['light_level']]


def same_operands(before, after, allow_placement=False):
    for name in set(before) - {'world_random', 'voxels'}:
        require(before[name] == after[name], 'Relevant passive operand changed: ' + name)
    expected = [list(v) for v in before['voxels']]
    if allow_placement:
        require(expected[13] in ([0, 0], [55, 0]), 'Actual pre-compile target')
        expected[13] = [55, 0]
    require(expected == after['voxels'], 'Unexpected voxel change across observation bracket')


def original_floor(packet):
    packet.read_packet()
    source = ROOT / 'tests/reference/client/redstone_world/flat/selected.txt'
    rows = source.read_text(encoding='ascii').splitlines()
    fields = rows[2].split('\t')
    require(fields[:6] == ['REDSTONE', '0', 'flat-mask-00-power-00', '0', 'true', 'NONE'] and
            fields[6:8] == ['0', '96'], 'Independent original isolated metadata0 floor row')
    words = [int(word, 16) for word in fields[-1].split(',')[:96]]
    require(len(words) == 96, 'Twelve actual original packed vertices')
    return words


def expected_floor(words, brightness):
    output = []
    for index in range(12):
        original = words[index * 8:index * 8 + 8]
        xyz = [struct.unpack('<f', struct.pack('<I', word))[0] for word in original[:3]]
        # Original CPU arguments: xyz8,64,8 and NW translation(-.25,.5,-.75).
        # Loaded native arguments: xyz3,65,3 and DK translation(0,-64,0).
        xyz = [f32(value + delta) for value, delta in zip(xyz, (-4.75, -63.5, -4.25))]
        uv = [struct.unpack('<f', struct.pack('<I', word))[0] for word in original[3:5]]
        channel = int(f32((f32(brightness * f32(.3)) if index < 6 else brightness) * 255))
        require(0 <= channel <= 255, 'Finite admitted NW color channel')
        color = 0xff000000 | (channel if index < 6 else channel * 0x010101)
        output.append(struct.pack('<5fI', *xyz, *uv, color))
    return output


def compare_floor(raw, words, brightness, phase):
    require(len(raw) == 72 * 32, 'Exact actual display-list extent')
    for index, vertex in enumerate(expected_floor(words, brightness)):
        require(raw[(60 + index) * 32:(60 + index) * 32 + 24] == vertex,
                f'Actual type5 XYZ/UV/color differs: {phase} vertex{index}')


def geometry(evidence, packet, receipts, digest):
    words = original_floor(packet)
    result, captures = {}, {}
    for phase, count, target in (('before', 60, 0), ('placed', 72, 55), ('reloaded', 72, 55)):
        raw = (evidence / f'redstone-geometry-{phase}.bin').read_bytes()
        require(len(raw) == count * 32, 'Exact committed display-list batch extent')
        captures[phase] = raw
        state = json.loads((evidence / f'redstone-operands-{phase}.json').read_text(), object_pairs_hook=unique)
        require(set(state) == {'phase', 'batch_identity', 'initial_compile_paired', 'current_before',
                              'current_after', 'compile_before', 'compile_after'} and state['phase'] == phase and
                type(state['batch_identity']) is int and state['batch_identity'] > 0 and
                type(state['initial_compile_paired']) is bool, 'Typed actual list/operand identity')
        brightness = operands(state['current_before'], target)
        operands(state['current_after'], target)
        same_operands(state['current_before'], state['current_after'])
        require(int(state['initial_compile_paired']) == int(receipts[phase]['initial-compile-paired']),
                'Native compile-pair receipt agrees with sidecar')
        if state['initial_compile_paired']:
            require(target == 55 and state['compile_before'] is not None and state['compile_after'] is not None,
                    'Actual paired native list compile snapshots')
            operands(state['compile_before'], state['compile_before']['voxels'][13][0])
            operands(state['compile_after'], 55)
            same_operands(state['compile_before'], state['compile_after'], allow_placement=True)
            same_operands(state['compile_after'], state['current_after'])
        else:
            require(phase != 'placed' and state['compile_before'] is None,
                    'No invented initial compile operands')
            if state['compile_after'] is not None:
                operands(state['compile_after'], 55)
        if target == 55:
            compare_floor(raw, words, brightness, phase)
        result[phase] = {'sha256': digest(evidence / f'redstone-geometry-{phase}.bin'), 'vertices': count,
                         'exact_xyz_uv_color_vertices': 12 if target == 55 else 0,
                         'initial_compile_paired': state['initial_compile_paired'],
                         'operands': state}
    for index in range(60):
        require(captures['before'][index * 32:index * 32 + 24] ==
                captures['placed'][index * 32:index * 32 + 24], 'Unchanged actual terrain prefix XYZ/UV/color')
    for index in range(12):
        require(captures['placed'][(60 + index) * 32:(60 + index) * 32 + 24] ==
                captures['reloaded'][(60 + index) * 32:(60 + index) * 32 + 24],
                'Reloaded actual native Redstone suffix equals qualified placed suffix')
    return result


def corruption_controls(farming, stdout, evidence, packet):
    """Reject changes to actual observed evidence; never rewrite retained files."""
    rejected = 0

    def reject(operation):
        nonlocal rejected
        try:
            operation()
        except (AssertionError, ValueError, KeyError):
            rejected += 1
            return
        raise AssertionError('Redstone validator accepted corrupted evidence')

    for old, new in (('taps=1', 'taps=2'), ('sounds=0', 'sounds=1'),
                     ('count-stores=1', 'count-stores=0'), ('used=1', 'used=0'),
                     ('same-stack=1', 'same-stack=0'), ('block=55', 'block=54'),
                     ('count-after=1', 'count-after=2'), ('metadata=0', 'metadata=1'),
                     ('owners-null=1', 'owners-null=0'),
                     ('windows-absent=1', 'windows-absent=0')):
        require(old in stdout, 'Mutation requires an actual receipt operand')
        reject(lambda old=old, new=new: observations(farming, stdout.replace(old, new, 1)))
    row = next(line for line in stdout.splitlines() if line.startswith('REDSTONE_ITEM_INPUT '))
    reject(lambda: observations(farming, stdout + '\n' + row + '\n'))
    words = original_floor(packet)
    state = json.loads((evidence / 'redstone-operands-placed.json').read_text(), object_pairs_hook=unique)
    actual = state['current_after']
    brightness = operands(actual, 55)
    raw = (evidence / 'redstone-geometry-placed.bin').read_bytes()
    for offset in (0, 4, 8, 12, 16, 20):
        changed = bytearray(raw)
        changed[60 * 32 + offset] ^= 1
        reject(lambda changed=changed: compare_floor(changed, words, brightness, 'corruption'))
    reject(lambda: compare_floor(raw[:-1], words, brightness, 'truncated'))
    for name in ('texture', 'emission', 'sky', 'sky_subtraction', 'light_level'):
        changed = json.loads(json.dumps(actual))
        changed[name] += 1
        reject(lambda changed=changed: same_operands(actual, changed))
    changed = json.loads(json.dumps(actual))
    changed['identities'][0] += 1
    reject(lambda: same_operands(actual, changed))
    changed = json.loads(json.dumps(actual))
    changed['voxels'][13][0] = 0
    reject(lambda: operands(changed, 55))
    for non_integer in (False, 0.0):
        changed = json.loads(json.dumps(actual))
        changed['voxels'][0][0] = non_integer
        reject(lambda changed=changed: operands(changed, 55))
    return rejected


def readback(parser, mining, farming, helper, evidence, receipt, authored):
    def owned(name):
        return parser.Nbt(parser.read(evidence / name)).root()
    before, placed = owned('redstone-player-before.nbt'), owned('redstone-player-placed.nbt')
    helper.inventory(before, 331, 2)
    helper.inventory(placed, 331, 1)
    mining.same_fields(authored, before, ('Pos', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Declared saved input versus actual loaded Player')
    saved = evidence / 'snapshots/redstone-saved/alpha'
    player = parser.inflate(parser.read(saved / 'level.dat'), 31)['Data'][1]['Player'][1]
    entry = owned(f"redstone-player-entry-{int(receipt['save']['sample']):04}.nbt")
    reloaded = owned('redstone-player-reloaded.nbt')
    for value in (player, entry, reloaded):
        helper.inventory(value, 331, 1)
    mining.same_fields(player, entry, ('Pos', 'Motion', 'Rotation', 'Health', 'Score', 'Dimension', 'Inventory'),
                       'Saved Player versus actual paused save-entry NBT')
    mining.same_fields(player, reloaded, ('Health', 'Score', 'Dimension', 'Inventory'),
                       'Reloaded Player persistent fields')
    level = farming.selected_chunk(parser, saved / 'region/r.0.0.mcr')[4]
    index = (3 << 11) | (3 << 7) | 64
    require(level['Blocks'][1][index:index + 2] == bytes((3, 55)) and
            level['Blocks'][1][(3 << 11) | 64] == 1 and level['Data'][1][index // 2] == 0,
            'Independent saved Dirt/Dust0/platform Region decode')
    cells = ((3, 65, 3), (4, 65, 3), (2, 65, 3), (3, 65, 4), (3, 65, 2),
             (3, 66, 3), (3, 64, 3), (3, 67, 3), (4, 65, 4))
    plane = level['BlockLight'][1]
    require(level['BlockLight'][0] == 7 and len(plane) == 16384 and
            all((plane[((x << 11) | (z << 7) | y) // 2] >> ((y & 1) * 4)) & 15 == 0
                for x, y, z in cells), 'Independent saved non-emitting neighborhood')
    items, chunks = mining.saved_items(parser, saved)
    require(not items, 'No authored or silently discarded Item entity')
    return {'inventory_before': [0, 331, 2, 0], 'inventory_saved': [0, 331, 1, 0],
            'target': [3, 65, 3, 55, 0], 'saved_items': 0, 'saved_chunks': chunks,
            'saved_level_sha256': mining.digest(saved / 'level.dat')}


def self_test():
    """Parser controls use declared examples, not a native gameplay capture."""
    farming = module('redstone_controls_rows', ROOT / 'tests/tools/client/test_farming_loop.py')
    packet = module('redstone_controls_reference', ROOT / 'tests/reference/client/redstone_world/packet.py')
    stdout = '\n'.join((
        'REDSTONE_ITEM_INPUT kind=redstone taps=1 right-clicks=1 block-uses=1 air-uses=0 admitted=1 '
        'placed=1 sounds=0 count-stores=1 used=1 cleared=0 equip-reset=1 drained=1 item=331 block=55 '
        'sdk-events=2 direct-suffix=1 metadata=0 target=3,65,3 face=1',
        'REDSTONE_ITEM_RETURN kind=redstone ground=3 item=331 block=55 stage=0 count-before=2 '
        'count-after=1 damage=0 health=5 same-stack=1 selected=0 hud=1 held=1 continuing=3 used=1 list-replaced=1',
        'REDSTONE_ITEM_SAVE sample=0 candidates=1 health=5 ground=3 block=55 stage=0 '
        'source-save-title=1 pre-frame-owned-nbt=1 blocklight=0 items=0',
        'REDSTONE_ITEM_RELOAD distinct-world=1 distinct-player=1 health=5 ground=3 block=55 '
        'item=331 count=1 damage=0 selected=0 hud=1 held=1 used=1 stage=0 items=0',
        'REDSTONE_ITEM_CLEANUP status=0 owners-null=1 windows-absent=1',
        'REDSTONE_ITEM_FINISH cleanup=1 windows-absent=1 checks=1',
        *('REDSTONE_ITEM_GEOMETRY phase=' + phase + ' vertices=' + str(count) +
          ' redstone-vertices=' + str(count - 60) + ' prefix=60 arrays=7 mode=4 '
          'source-list-returned=1 terrain-returned=1 initial-compile-paired=' + str(paired)
          for phase, count, paired in (('before', 60, 0), ('placed', 72, 1), ('reloaded', 72, 0))),
        *('REDSTONE_ITEM_LIGHT phase=' + phase + ' queue=0 target=0 east=0 west=0 south=0 '
          'north=0 up=0 down=0 up-two=0 diagonal=0 normal-frames=1' for phase in ('placed', 'reloaded')),
    )) + '\n'
    receipt = observations(farming, stdout)
    table = [f'{struct.unpack("<I", struct.pack("<f", i / 15))[0]:08x}' for i in range(16)]
    value = {'identities': list(range(1, 9)), 'texture': 164, 'emission': 0, 'sky': 15,
             'block_light': 0, 'sky_subtraction': 0, 'light_level': 15,
             'brightness_bits': table[15], 'table_bits': table,
             'voxels': [[0, 0] for _ in range(27)],
             'world_random': {'state': '000000000001', 'gaussian': '0000000000000000', 'ready': False}}
    value['voxels'][10] = [3, 0]
    value['voxels'][13] = [55, 0]
    empty = json.loads(json.dumps(value))
    empty['voxels'][13] = [0, 0]
    words = original_floor(packet)
    packed = bytes(60 * 32) + b''.join(row + bytes(8) for row in expected_floor(words, 1.0))
    with tempfile.TemporaryDirectory(prefix='redstone-placement-parser-') as temporary:
        evidence = Path(temporary)
        for phase, present in (('before', False), ('placed', True), ('reloaded', True)):
            current = value if present else empty
            state = {'phase': phase, 'batch_identity': 1, 'initial_compile_paired': phase == 'placed',
                     'current_before': current, 'current_after': current,
                     'compile_before': empty if phase == 'placed' else None,
                     'compile_after': value if present else None}
            (evidence / f'redstone-operands-{phase}.json').write_text(json.dumps(state))
            (evidence / f'redstone-geometry-{phase}.bin').write_bytes(packed if present else bytes(60 * 32))
        geometry(evidence, packet, {r['phase']: r for r in receipt['geometry']}, farming.digest)
        rejected = corruption_controls(farming, stdout, evidence, packet)
        ordinary_random = json.loads(json.dumps(value))
        ordinary_random['world_random']['state'] = '000000000002'
        same_operands(value, ordinary_random)
    print(f'PASS: Redstone placement parser/geometry controls; {rejected} corruptions rejected; '
          'declared examples and original CPU rows only, no native gameplay run')


def main():
    parser_args = argparse.ArgumentParser(description=__doc__)
    parser_args.add_argument('--executable', type=Path)
    parser_args.add_argument('--assets', type=Path)
    parser_args.add_argument('--artifacts', type=Path)
    parser_args.add_argument('--self-test', action='store_true')
    options = parser_args.parse_args()
    if options.self_test:
        if any(value is not None for value in (options.executable, options.assets, options.artifacts)):
            parser_args.error('--self-test takes no executable, assets or artifacts')
        self_test()
        return
    if any(value is None for value in (options.executable, options.assets, options.artifacts)):
        parser_args.error('native run requires --executable, --assets and --artifacts')
    require(os.name == 'nt', 'Explicit Windows SDK profile; no implicit home redirection')
    world = module('redstone_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    mining = module('redstone_readback', ROOT / 'tests/tools/client/test_mining_loop.py')
    farming = module('redstone_platform', ROOT / 'tests/tools/client/test_farming_loop.py')
    helper = module('redstone_saved_inputs', ROOT / 'tests/tools/client/test_torch_placement_loop.py')
    parser = module('redstone_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    packet = module('redstone_cpu_reference', ROOT / 'tests/reference/client/redstone_world/packet.py')
    executable, assets = options.executable.resolve(strict=True), options.assets.resolve(strict=True)
    options.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='redstone-placement-', dir=options.artifacts.resolve()))
    for folder in ('snapshots', 'entries'):
        (evidence / folder).mkdir()
    inputs = {Path(__file__).resolve(), FIXTURE, executable, ROOT / 'CMakeLists.txt',
              ROOT / 'tests/tools/world/test_source_saving.py'}
    inputs.update(executable.parent.glob('*.dll'))
    for folder in ('src', 'include', 'cmake', 'tests/client', 'tests/tools/client',
                   'tests/reference/client/redstone_world'):
        inputs.update(p for p in (ROOT / folder).rglob('*') if p.is_file())
    before_inputs = {str(p): mining.digest(p) for p in sorted(inputs)}
    before_assets = world.snapshot(assets)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before_inputs,
              'assets_before': before_assets,
              'operands': {'support': [3, 64, 3, 3, 0], 'target': [3, 65, 3, 0, 0],
                           'standing_support': [3, 64, 0, 1], 'source_ray': [3, 64, 3, 1],
                           'slot': [0, 331, 2, 0], 'player_xz': [3.5, .5],
                           'player_y': 65.0 + f32(1.62), 'yaw_pitch': [0, 26], 'health': 5},
              'limits': ['One fresh process and one existing SDL SDK right-button down/up pair; no physical hardware-input claim.',
                         'Inherited focus/Pause GUI, frame/tick/clock/RNG/work budgets and queued work remain ordinary.',
                         'No live pose/World/queue/Timer/Random/input-state repair, no support-removal or powered devices.',
                         'Placement compile uses passive stable pre/post-frame operands; this is not a direct CV child receipt.',
                         'Reload initial compile pairing may be unavailable; retain actual bytes/current operands and that limit.',
                         'Independent CPU reference supplies floor topology/UV; actual provider operands supply native colors.',
                         'Exact XYZ/UV/color only: all32-byte vertex words are retained, unused packed tail is not normalized.',
                         'Used331 is actual stats-writer observation; Player NBT is not a statistic oracle.',
                         'No loaded-original scene, whole-frame pixel, general circuit, audible output or FPS claim.']}
    (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n')
    try:
        with tempfile.TemporaryDirectory(prefix='redstone-appdata-', dir=evidence) as temporary:
            home = Path(temporary)
            game = home / '.minecraft'
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            saves.mkdir()
            (saves / 'preserve-me.bin').write_bytes(b'unrelated redstone fixture input\0\xff')
            authored = None
            for folder, name, played in (('older', 'Old world', 1303000000000),
                                         ('bravo', '', 1304000000000),
                                         ('alpha', 'Redstone world', 1304000000000)):
                directory = saves / folder
                directory.mkdir()
                wire = helper.metadata(world, name, played, 331) if folder == 'alpha' else world.metadata(name, played, 19132)
                (directory / 'level.dat').write_bytes(gzip.compress(wire, mtime=0))
                (directory / 'preserve-chunk.dat').write_bytes(b'authored unrelated bytes')
                if folder == 'alpha':
                    authored = parser.Nbt(wire).root()['Data'][1]['Player'][1]
                    helper.inventory(authored, 331, 2)
            regions = saves / 'alpha/region'
            regions.mkdir()
            report['saved_input'] = helper.authored_regions(world, parser, farming, regions)
            before = world.save_title_files(saves, evidence / 'before')
            env = os.environ.copy()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home), HOME=str(home),
                       ALSOFT_DRIVERS='null', MC_WORLD_RELOAD_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                       MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'),
                       MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            result = world.retained_native_run([str(executable), str(assets), 'placement', str(evidence)],
                                               env, evidence, saves, before, 'redstone-placement',
                                               time.time_ns() // 1000000, 1200)
            print(result.stdout, end='')
            print(result.stderr, end='', file=sys.stderr)
            require(result.returncode == 0, 'Actual SDK Redstone placement process failed')
            report['receipts'] = observations(farming, result.stdout)
            report['geometry'] = geometry(evidence, packet,
                {r['phase']: r for r in report['receipts']['geometry']}, mining.digest)
            report['corruption_controls'] = corruption_controls(farming, result.stdout, evidence, packet)
            report['readback'] = readback(parser, mining, farming, helper, evidence,
                                          report['receipts'], authored)
            after = world.save_title_files(saves, evidence / 'after-verified')
            for name, value in before.items():
                if not name.startswith('alpha/') or name == 'alpha/preserve-chunk.dat':
                    require(after.get(name) == value, 'Unrelated authored save changed')
            require(all(name.startswith('alpha/') for name in set(after) - set(before)),
                    'No unrelated World creation')
            selected, final = world.snapshot(evidence / 'snapshots/redstone-saved'), world.snapshot(saves)
            session = str(Path('alpha') / 'session.lock')
            report['reselection_disk'] = {'same_paths': set(final) == set(selected),
                'changed': sorted(k for k in set(final) | set(selected) if final.get(k) != selected.get(k)),
                'source_session_key': session, 'session_bytes': (saves / session).stat().st_size}
            require(set(final) == set(selected) and
                    all(final[key] == value for key, value in selected.items() if key != session) and
                    (saves / session).stat().st_size == 8, 'Strict reselection bytes except session token')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'] = {str(p): mining.digest(p) for p in sorted(inputs)}
        report['assets_after'] = world.snapshot(assets)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Source/product/reference/imported-resource bytes changed'
        (evidence / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'Native Redstone fixture continuity')
    print(f'Actual SDK Redstone placement/save/readback PASS: {evidence}')


if __name__ == '__main__':
    main()
