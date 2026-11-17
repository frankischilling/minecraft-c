"""Finite native SDK Sign placement/editor/Done/save/reselection; no FPS or original pixel claim."""
import argparse
import copy
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tests/tools/client'))
import test_chest_loop as G

require = G.require
ROWS = (('ABCDEFGHIJKLMNZ', '\u00e9 B', 'row2', 'last'), ('two', '2', 'done', ''))
SELECTED = (0x0041, 0x0042, 0x005a, 0x0020, 0x00e9, 0xd83d, 0xde00)
IMAGES = frozenset(('sign-editor-first.png', 'sign-editor-second.png',
                    'sign-gameplay-before-save.png', 'sign-gameplay-title.png',
                    'sign-gameplay-reloaded.png'))
U = r'(0|[1-9][0-9]{0,19})'


def write(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True) + '\n', encoding='utf8', newline='\n')


def units(text):
    return text.encode('utf-16be', 'surrogatepass').hex() or '-'


def allowed_indices(path):
    raw = G.bounded(path, 1048576)
    # FP uses UTF8 Reader.readLine: only CR/LF delimit lines, no Python
    # splitlines treatment of other Unicode separators, and # at column zero.
    text = raw.decode('utf8', 'strict')
    lines = re.split(r'\r\n|\r|\n', text)
    joined = ''.join(line for line in lines if not line.startswith('#'))
    wire = joined.encode('utf-16be')
    actual = [struct.unpack_from('>H', wire, offset)[0] for offset in range(0, len(wire), 2)]
    require(len(actual) <= 65536, 'Actual FP bounded complete UTF16 mapping')
    result = {f'{unit:04x}': actual.index(unit) if unit in actual else -1 for unit in SELECTED}
    require(all(result[f'{unit:04x}'] >= 0 for unit in SELECTED[:5]) and
            all(result[f'{unit:04x}'] == -1 for unit in SELECTED[5:]),
            'Actual full private font mapping accepts selected BMP/ASCII and rejects surrogate units')
    return {'file': {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()},
            'mapping_units': len(actual), 'allowed_per_index': result,
            'scope': 'Only seven selected first indexes; complete alphabet and font image are not copied.'}


def receipts(stdout, stderr, monitor, allowed, *, evidence=None, interval=None):
    require(type(stdout) is str and stdout.endswith('\n') and '\r' not in stdout and stderr == '',
            'Complete clean semantic process streams')
    rows = stdout.splitlines()
    require(rows[:2] == ['16 achievements', '151 recipes'], 'One exact Registry startup banner pair')
    at = 2

    def exact(text):
        nonlocal at
        require(at < len(rows) and rows[at] == text, 'Ordered exact Sign gameplay receipt: ' + text)
        at += 1

    def match(pattern, label):
        nonlocal at
        row = re.fullmatch(pattern, rows[at]) if at < len(rows) else None
        require(row is not None, label)
        at += 1
        return row

    ids = []

    def host(stage):
        target = monitor['selected']['work']
        require(type(target) is list and len(target) == 4 and all(type(v) is int for v in target) and
                target[2] >= 854 and target[3] >= 480, 'Actual physical monitor2 work rectangle')
        for window in ('frame', 'canvas'):
            row = match(r'WORLD_SELECTION_MONITOR stage=' + stage + ' window=' + window +
                        r' x=(-?[0-9]+) y=(-?[0-9]+) width=854 height=480 target=(\S+)',
                        'Actual Frame/Canvas monitor2 receipt')
            x, y = map(int, row.group(1, 2))
            require(row[3] == ','.join(map(str, target)) and target[0] <= x and target[1] <= y and
                    x + 854 <= target[0] + target[2] and y + 480 <= target[1] + target[3],
                    'Contained actual Frame/Canvas geometry')
        row = match(r'WORLD_SAVE_TITLE_HOST stage=' + stage +
                    r' frame-id=([1-9][0-9]{0,9}) canvas-id=([1-9][0-9]{0,9}) '
                    r'canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1',
                    'Same real Frame/App/Canvas/MouseHelper graph')
        pair = tuple(map(int, row.groups()))
        require(pair[0] != pair[1] and max(pair) <= 4294967295, 'Distinct actual native window IDs')
        ids.append(pair)

    def text_rows(phase, sign):
        for row, text in enumerate(ROWS[sign]):
            exact(f'SIGN_GAMEPLAY_ROW phase={phase} sign={sign} row={row} units={units(text)}')

    def place(sign):
        row = match(r'SIGN_GAMEPLAY_PLACE sign=' + str(sign) + ' attempts=' + U +
                    r' vc-dc-yc=1 count-store=1 iz-statistic=1 slot-cleared=1 placement-sound=0 rows=4',
                    'Actual SDK VC/IZ return, DC/YC editor and consumed one-Sign slot')
        require(1 <= int(row[1]) <= 12, 'Bounded source-stale/focus-only tap retries')
        return int(row[1])

    def edited_done(sign):
        text_rows('edited', sign)
        exact(f'SIGN_GAMEPLAY_EDIT sign={sign} rows=4 sdk=1 row=3 dirty-before-done=0')
        text_rows('done', sign)
        row = match(r'SIGN_GAMEPLAY_DONE sign=' + str(sign) +
                    r' dirty=1 display-returned=1 sp-close-returned=1 repeat=0 focused=1 gui-frames=' + U,
                    'Real Done dirty/display/SP-close and repeat-disable return')
        require(2 <= int(row[1]) <= 16384, 'Multiple bounded returning editor frames, not an FPS claim')
        return int(row[1])

    host('open')
    exact('SIGN_GAMEPLAY_READY epoch=0 distinct-world-player=0')
    attempts = [place(0)]
    for unit in SELECTED:
        row = match(r'SIGN_GAMEPLAY_ALLOWED unit=' + f'{unit:04x}' + r' first=(-1|0|[1-9][0-9]{0,4})',
                    'Selected actual YC.m first index')
        require(int(row[1]) == allowed['allowed_per_index'][f'{unit:04x}'],
                'Runtime captured YC.m joins independently decoded private FP asset')
    exact('SIGN_GAMEPLAY_UTF16 cap=15 excess-same-line=1 substring-empty-alias=1 '
          'sdk-supplementary-units=d83dde00 rejected=2')
    exact('SIGN_GAMEPLAY_DIRECT raw-high=d83d raw-low=de00 rejected=2 same-line=1 '
          'sdk-delivery=0 tick-marker-dirty-unchanged=1')
    gui_frames = [edited_done(0)]
    strafes = 0
    while at < len(rows) and rows[at].startswith('SIGN_GAMEPLAY_STRAFE '):
        row = match(r'SIGN_GAMEPLAY_STRAFE attempt=' + U + r' key=(A|D) x=(-?[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)',
                    'Actual bounded SDK strafe receipt')
        strafes += 1
        require(int(row[1]) == strafes <= 20 and math.isfinite(float(row[3])) and -16 < float(row[3]) < 16,
                'Contiguous finite SDK movement attempts')
    require(strafes > 0, 'Actual movement to second placement cell')
    exact('SIGN_GAMEPLAY_SLOT selected=1 sdk-key=2 stack=323 count=1')
    attempts.append(place(1))
    gui_frames.append(edited_done(1))
    row = match(r'SIGN_GAMEPLAY_BOUNDED placements=2 editors=2 done=2 frames=' + U +
                r' field-repair=0 physical-input=0 fps-claim=0', 'Finite returned frame cohort')
    require(4 <= int(row[1]) <= 65536, 'Bounded actual returning frame count')
    frames = int(row[1])
    start = at
    while at < len(rows) and rows[at].startswith(('WORLD_SAVE_TITLE_RECOVERY ',
            'WORLD_SAVE_TITLE_AUTOPAUSE ', 'WORLD_SAVE_TITLE_ESCAPE ', 'WORLD_SAVE_TITLE_ENTRY ',
            'WORLD_SAVE_TITLE mode=', 'WORLD_RELOAD_SNAPSHOT ')):
        at += 1
    sign = G.module('sign_gameplay_saved_protocol', ROOT / 'tests/client/test_sign_sdk.py')
    saved = sign.save_protocol(rows[start:at], evidence=evidence, interval=interval)
    exact('SIGN_GAMEPLAY_READY epoch=1 distinct-world-player=1')
    seen = set()
    for _ in range(2):
        row = re.fullmatch(r'SIGN_GAMEPLAY_ROW phase=reloaded sign=([01]) row=0 units=(?:-|[0-9a-f]+)',
                           rows[at]) if at < len(rows) else None
        require(row is not None and int(row[1]) not in seen, 'Two unique reloaded actual Sign groups')
        which = int(row[1]); seen.add(which)
        text_rows('reloaded', which)
    exact('SIGN_GAMEPLAY_RELOAD signs=2 rows=8 exact-utf16=1 consumed-slots=2 statistic=2 '
          'distinct-world-player=1 actual-world-draw=1')
    host('after-sign-gameplay')
    require(ids[0] == ids[1], 'Same actual host across ordinary save/reselection')
    exact('SIGN_GAMEPLAY_STOP actual-frame-close=1 source-frame-retry=0')
    row = match(r'SIGN_GAMEPLAY_FINISH cleanup=1 windows-absent=1 checks=([1-9][0-9]{0,9}) '
                r'source-field-repair=0', 'Checked complete native closure')
    require(int(row[1]) <= 4294967295 and at == len(rows), 'No extra/duplicate/trailing receipts')
    return {'attempts': attempts, 'gui_frames': gui_frames, 'strafe_attempts': strafes,
            'frames': frames, 'save': saved, 'checks': int(row[1]), 'window_ids': list(ids[0]),
            'rows_utf16': [[units(text) for text in group] for group in ROWS],
            'callback_scope': 'Actual runtime-selected callback and real DC/router/YC, inferred from the returned fixed production path and public outcomes; no direct callback-PC observer.'}


def saved_semantics(level, player):
    require(type(level) is dict and type(player) is dict, 'Decoded actual Chunk/Player compounds')
    blocks, data = G.typed(level.get('Blocks'), 7, 'Saved complete block plane'), G.typed(level.get('Data'), 7, 'Saved metadata plane')
    require(type(blocks) is bytes and type(data) is bytes and len(blocks) == 32768 and len(data) == 16384,
            'Complete saved selected planes')
    value = G.typed(level.get('TileEntities'), 9, 'Saved TileEntities List')
    require(type(value) is list and len(value) == 2 and type(value[0]) is int and value[0] == 10 and
            type(value[1]) is list and len(value[1]) == 2, 'Exactly two actual saved Sign compounds')
    tiles = value[1]
    actual = {}
    for tile in tiles:
        require(type(tile) is dict and set(tile) == {'id', 'x', 'y', 'z', 'Text1', 'Text2', 'Text3', 'Text4'},
                'Closed actual Sign NBT fields and widths')
        coordinate = [G.typed(tile[key], 3, 'Saved Sign coordinate Int width') for key in ('x', 'y', 'z')]
        require(tile['id'] == [8, 'Sign'] and all(type(v) is int for v in coordinate) and
                coordinate[0] in (0, 1) and coordinate[1:] == [64, 2],
                'Exact actual Sign id and coordinates')
        which = coordinate[0]
        require(which not in actual, 'Unique saved Sign coordinates')
        texts = [G.typed(tile[f'Text{row+1}'], 8, 'Saved Text String width') for row in range(4)]
        require(all(type(text) is str for text in texts) and tuple(texts) == ROWS[which],
                'Independently decoded exact saved four-row UTF16 text')
        at = (which << 11) | (2 << 7) | 64
        require(blocks[at] == 63 and (data[at // 2] >> ((at & 1) * 4)) & 15 == 8 and
                blocks[at - 1] == 3, 'Source-created standing Sign and genuine unchanged support')
        actual[which] = [units(text) for text in texts]
    require(set(actual) == {0, 1}, 'Complete two-Sign coordinate set')
    require(G.slot_rows(player['Inventory']) == [], 'Both ordinary one-item Sign slots consumed')
    return {'tiles': actual, 'inventory': []}


def saved_gate(nbt, directory):
    require(directory.is_dir() and not directory.is_symlink(), 'Retained actual saved world')
    level = G.module('sign_gameplay_selected_chunk', ROOT / 'tests/tools/client/test_mining_loop.py').selected_chunk(nbt, directory)
    metadata = nbt.inflate(G.bounded(directory / 'level.dat', nbt.BOUND), 31)
    player = G.compound(G.compound(metadata['Data'], 'Saved Data')['Player'], 'Saved Player')
    return {**saved_semantics(level, player), 'chunk': G.digest(directory / 'region/r.0.0.mcr'),
            'metadata': G.digest(directory / 'level.dat')}


def authored(world, selected, mining, nbt, saves):
    for folder, name, played in (('older', 'Old world', 1303000000000), ('bravo', '', 1304000000000),
                                  ('alpha', 'Sign gameplay', 1304000000000)):
        directory = saves / folder; directory.mkdir(parents=True)
        wire = mining.mining_metadata(selected, world, name) if folder == 'alpha' else world.metadata(name, played, 19132)
        if folder == 'alpha':
            empty = world.tag(9, 'Inventory', b'\x0a' + bytes(4))
            require(wire.count(empty) == 1, 'One prelaunch saved Inventory')
            wire = wire.replace(empty, world.tag(9, 'Inventory', b'\x0a' + struct.pack('>i', 2) +
                                G.stack(world, 0, 323, 1) + G.stack(world, 1, 323, 1)))
            player = nbt.Nbt(wire).root()['Data'][1]['Player'][1]
            require(G.slot_rows(player['Inventory']) == [(0, 323, 1, 0), (1, 323, 1, 0)],
                    'Two normal count-one Sign slots before any App/World exists')
        (directory / ('level.dat_old' if folder == 'bravo' else 'level.dat')).write_bytes(gzip.compress(wire, mtime=0))
        (directory / 'preserve.bin').write_bytes(b'independent Sign gameplay input\0\xff')
    region = saves / 'alpha/region'; region.mkdir()
    G.regions(world, region, 'placement')
    helper = G.module('sign_gameplay_authored_regions', ROOT / 'tests/tools/client/test_torch_placement_loop.py')
    records, _ = helper.region_records(nbt, region)
    require(set(records) == {(x, z) for x in range(-8, 9) for z in range(-8, 9)} and
            all(record[-1]['TileEntities'] == [9, [10, []]] and record[-1]['Entities'] == [9, [10, []]]
                for record in records.values()), '289 authored flat chunks, no initial Sign/Tile or Entity')
    return {'sign_slots': [[0, 323, 1, 0], [1, 323, 1, 0]], 'initial_tiles': 0,
            'chunks': len(records), 'saved_pose': [.5, 64 + struct.unpack('>f', struct.pack('>f', 1.62))[0], -.5],
            'saved_yaw_pitch': [0, 30], 'placement_targets': [[0, 63, 2, 1], [1, 63, 2, 1]]}


def native_run(command, environment, evidence, saves, before, first):
    products = G.module('sign_gameplay_product_custody', ROOT / 'tests/tools/client/native_product_capture.py')
    result, primary = None, None
    execution = {'command': command, 'before': before, 'process_first_millis': first, 'event': 'preparing'}
    execution['environment'] = {key: environment[key] for key in ('APPDATA', 'LOCALAPPDATA', 'USERPROFILE',
        'ALSOFT_DRIVERS', 'SDL_DISPLAY_USABLE_BOUNDS', 'MC_WORLD_SELECTION_MONITOR_BOUNDS',
        'MC_WORLD_RELOAD_SAVES', 'MC_WORLD_SAVE_TITLE_SAVES', 'MC_WORLD_SAVE_TITLE_SNAPSHOTS',
        'MC_WORLD_SAVE_TITLE_ENTRIES')}
    try:
        execution['native_products'] = products.capture_native_products(command, evidence)
        result = subprocess.run(command, env=environment, timeout=660, capture_output=True, check=False)
        execution.update(event='result', exit_code=result.returncode)
    except BaseException as error:
        primary = error
        execution.update(event='timeout' if isinstance(error, subprocess.TimeoutExpired) else 'launch-or-product-error',
                         error_type=type(error).__name__, error=str(error))
        if 'native_products' not in execution:
            execution['native_products'] = getattr(error, 'receipt', None)
    finally:
        for name in ('stdout', 'stderr'):
            data = getattr(result if result is not None else primary, name, b'') or b''
            require(type(data) is bytes, 'Raw physical native stream byte transport')
            (evidence / (name + '.bin')).write_bytes(data)
            execution[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        write(evidence / 'execution.json', execution)
        try:
            execution['after'] = G.module('sign_gameplay_failure_copy', ROOT / 'tests/tools/client/test_world_selection_loop.py').save_title_files(saves, evidence / 'after')
            require(bool(execution['after']), 'Actual postprocess disk copy retained')
        except BaseException as error:
            execution['after_snapshot_error'] = {'type': type(error).__name__, 'message': str(error)}
            if primary is None and result is not None and result.returncode == 0:
                primary = error
        write(evidence / 'execution.json', execution)
    if primary is not None:
        raise primary
    return result


def controls():
    """Authored offline protocol/semantic admission, never native evidence."""
    monitor = {'selected': {'work': [-1920, 0, 1920, 1040]}}
    mapping = {'allowed_per_index': {f'{unit:04x}': i if i < 5 else -1 for i, unit in enumerate(SELECTED)}}

    def host(stage):
        return [f'WORLD_SELECTION_MONITOR stage={stage} window={window} x=-1387 y=280 width=854 height=480 target=-1920,0,1920,1040'
                for window in ('frame', 'canvas')] + [f'WORLD_SAVE_TITLE_HOST stage={stage} frame-id=3 canvas-id=4 canvas=1 display-parent=1 helper-parent=1 bound-app=1 borrowed-window=1']

    def row_group(phase, sign):
        return [f'SIGN_GAMEPLAY_ROW phase={phase} sign={sign} row={row} units={units(text)}'
                for row, text in enumerate(ROWS[sign])]

    def place(sign):
        return f'SIGN_GAMEPLAY_PLACE sign={sign} attempts=1 vc-dc-yc=1 count-store=1 iz-statistic=1 slot-cleared=1 placement-sound=0 rows=4'

    def done(sign):
        return row_group('edited', sign) + [f'SIGN_GAMEPLAY_EDIT sign={sign} rows=4 sdk=1 row=3 dirty-before-done=0'] + row_group('done', sign) + [f'SIGN_GAMEPLAY_DONE sign={sign} dirty=1 display-returned=1 sp-close-returned=1 repeat=0 focused=1 gui-frames=20']

    save = ['WORLD_SAVE_TITLE_RECOVERY focus-pauses=0 keyboard-before-escape=2 resumed=1',
            'WORLD_SAVE_TITLE_ESCAPE keyboard-before=2 keyboard-after=4 fresh-pause=1 distinct-recovered=1',
            'WORLD_SAVE_TITLE_ENTRY mode=save-title frame=0 eligible=84 time=5 size-before=100 size-after=200 entered=1 bytes=10 copied-before-frame=1',
            'WORLD_SAVE_TITLE mode=save-title escape-sdk=1 button-id=1 leave-pc=366 i=1 f=1 h=1 new-title=1 old-renderer-world=1 old-effects-world=1 original-load-returned=1 eligible=84 time=5 size-before=100 size-after=200 first=1000 last=1001 entry-frame=0 entry-bytes=10',
            'WORLD_RELOAD_SNAPSHOT phase=sign-saved files=1 bytes=10 actual-file-copy=1']
    rows = ['16 achievements', '151 recipes'] + host('open') + ['SIGN_GAMEPLAY_READY epoch=0 distinct-world-player=0', place(0)]
    rows += [f'SIGN_GAMEPLAY_ALLOWED unit={unit:04x} first={mapping["allowed_per_index"][f"{unit:04x}"]}' for unit in SELECTED]
    rows += ['SIGN_GAMEPLAY_UTF16 cap=15 excess-same-line=1 substring-empty-alias=1 sdk-supplementary-units=d83dde00 rejected=2',
             'SIGN_GAMEPLAY_DIRECT raw-high=d83d raw-low=de00 rejected=2 same-line=1 sdk-delivery=0 tick-marker-dirty-unchanged=1']
    rows += done(0) + ['SIGN_GAMEPLAY_STRAFE attempt=1 key=A x=1.5', 'SIGN_GAMEPLAY_SLOT selected=1 sdk-key=2 stack=323 count=1', place(1)] + done(1)
    rows += ['SIGN_GAMEPLAY_BOUNDED placements=2 editors=2 done=2 frames=100 field-repair=0 physical-input=0 fps-claim=0'] + save
    rows += ['SIGN_GAMEPLAY_READY epoch=1 distinct-world-player=1'] + row_group('reloaded', 0) + row_group('reloaded', 1)
    rows += ['SIGN_GAMEPLAY_RELOAD signs=2 rows=8 exact-utf16=1 consumed-slots=2 statistic=2 distinct-world-player=1 actual-world-draw=1'] + host('after-sign-gameplay')
    rows += ['SIGN_GAMEPLAY_STOP actual-frame-close=1 source-frame-retry=0', 'SIGN_GAMEPLAY_FINISH cleanup=1 windows-absent=1 checks=100 source-field-repair=0']
    raw = '\n'.join(rows) + '\n'
    require(receipts(raw, '', monitor, mapping, interval=(999, 1002))['checks'] == 100,
            'Authored complete positive closed-reader control')
    changes = [raw.rstrip('\n'), raw + 'unknown\n', raw + rows[-1] + '\n',
               raw.replace('cap=15', 'cap=16'), raw.replace('sdk-delivery=0', 'sdk-delivery=1'),
               raw.replace('rejected=2', 'rejected=0'), raw.replace('repeat=0 focused=1', 'repeat=1 focused=1'),
               raw.replace('attempts=1', 'attempts=13'), raw.replace('gui-frames=20', 'gui-frames=1'),
               raw.replace('slot-cleared=1', 'slot-cleared=0'), raw.replace('iz-statistic=1', 'iz-statistic=0'),
               raw.replace('placement-sound=0', 'placement-sound=1'), raw.replace('stack=323 count=1', 'stack=323 count=2'),
               raw.replace('distinct-world-player=1', 'distinct-world-player=0'),
               raw.replace('exact-utf16=1', 'exact-utf16=0'), raw.replace('windows-absent=1', 'windows-absent=0'),
               raw.replace('x=-1387', 'x=0'), raw.replace('canvas-id=4', 'canvas-id=3'),
               raw.replace('size-after=200 first=', 'size-after=201 first='),
               raw.replace('keyboard-after=4', 'keyboard-after=3'),
               raw.replace('phase=done sign=0 row=1 units=00e900200042', 'phase=done sign=0 row=1 units=006500200042'),
               raw.replace('phase=reloaded sign=1 row=3 units=-', 'phase=reloaded sign=1 row=3 units=d83d'),
               raw.replace('SIGN_GAMEPLAY_PLACE sign=1', 'SIGN_GAMEPLAY_PLACE sign=0'),
               raw.replace('SIGN_GAMEPLAY_EDIT sign=1 rows=4', 'SIGN_GAMEPLAY_EDIT sign=1 rows=3')]
    rejected = 0
    for changed in changes:
        require(changed != raw, 'Actual protocol control mutation')
        try:
            receipts(changed, '', monitor, mapping, interval=(999, 1002))
        except RuntimeError:
            rejected += 1
        else:
            raise RuntimeError('Corrupted gameplay protocol admitted')
    for altered_stderr, altered_mapping in [('source failure\n', mapping), ('', {'allowed_per_index': {**mapping['allowed_per_index'], 'd83d': 0}})]:
        try:
            receipts(raw, altered_stderr, monitor, altered_mapping, interval=(999, 1002))
        except RuntimeError:
            rejected += 1
        else:
            raise RuntimeError('Corrupted stderr/mapping join admitted')
    blocks, data = bytearray(32768), bytearray(16384)
    tiles = []
    for which in (0, 1):
        at = (which << 11) | (2 << 7) | 64
        blocks[at], blocks[at - 1], data[at // 2] = 63, 3, 8
        tiles.append({'id': [8, 'Sign'], 'x': [3, which], 'y': [3, 64], 'z': [3, 2],
                      **{f'Text{row+1}': [8, text] for row, text in enumerate(ROWS[which])}})
    level = {'Blocks': [7, bytes(blocks)], 'Data': [7, bytes(data)], 'TileEntities': [9, [10, tiles]]}
    player = {'Inventory': [9, [10, []]]}
    require(saved_semantics(level, player)['tiles'] == {i: [units(text) for text in ROWS[i]] for i in (0, 1)},
            'Authored actual-format positive Sign save semantics')
    mutations = [lambda l,p: l['TileEntities'][1][1][0].pop('Text4'),
                 lambda l,p: l['TileEntities'][1][1][0].update(EditRow=[3,-1]),
                 lambda l,p: l['TileEntities'][1][1][0]['Text1'].__setitem__(0, 3),
                 lambda l,p: l['TileEntities'][1][1][0]['Text1'].__setitem__(1, 'A'*16),
                 lambda l,p: l['TileEntities'][1][1][0]['Text4'].__setitem__(1, '\ud83d'),
                 lambda l,p: l['TileEntities'][1][1][1]['x'].__setitem__(1, 0),
                 lambda l,p: l['TileEntities'][1][1].pop(),
                 lambda l,p: l['TileEntities'][1][1].append(copy.deepcopy(tiles[0])),
                 lambda l,p: l['TileEntities'][1].__setitem__(0, 8),
                 lambda l,p: l.__setitem__('Blocks', [7, bytes(32768)]),
                 lambda l,p: l.__setitem__('Data', [7, bytes(16384)]),
                 lambda l,p: p['Inventory'][1][1].append({'Slot':[1,1], 'id':[2,323], 'Count':[1,1], 'Damage':[2,0]})]
    for mutate in mutations:
        altered, other = copy.deepcopy(level), copy.deepcopy(player)
        mutate(altered, other)
        try:
            saved_semantics(altered, other)
        except RuntimeError:
            rejected += 1
        else:
            raise RuntimeError('Corrupted actual-format Sign save semantics admitted')
    print(f'PASS: {rejected} Sign gameplay reader/semantic corruption controls; authored offline data only')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    parser.add_argument('--controls', action='store_true')
    args = parser.parse_args()
    if args.controls:
        controls(); return
    require(os.name == 'nt' and all((args.executable, args.assets, args.artifacts)), 'Windows monitor2 gameplay cohort')
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    monitor = G.module('sign_gameplay_monitor', ROOT / 'tests/tools/client/test_bed_sleep.py').second_monitor()
    world = G.module('sign_gameplay_world', ROOT / 'tests/tools/client/test_world_selection_loop.py')
    selected = G.module('sign_gameplay_selected', ROOT / 'tests/tools/client/test_block_selection.py')
    mining = G.module('sign_gameplay_mining', ROOT / 'tests/tools/client/test_mining_loop.py')
    nbt = G.module('sign_gameplay_nbt', ROOT / 'tests/tools/world/test_source_saving.py')
    args.artifacts.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix='sign-gameplay-', dir=args.artifacts.resolve()))
    for name in ('snapshots', 'entries'): (evidence / name).mkdir()
    before_inputs = G.fingerprints(G.source_inputs(executable))
    before_assets = world.snapshot(assets)
    require(before_inputs and all(type(value) is str for value in before_inputs.values()), 'Readable exact selected inputs')
    report = {'status': 'FAIL', 'scope': __doc__, 'monitor': monitor,
              'inputs_before': before_inputs, 'assets_before': before_assets,
              'limits': ['Two authored saved normal count-one Sign slots, not ordinary acquisition.',
                         'Authored SDL records traverse actual SDK/runtime; no physical key timing claim.',
                         'Raw unpaired units are separate direct YC component calls, not malformed UTF8 delivery.',
                         'Second editor comes from a second actual placement; existing Sign right-click is not editing.',
                         'Done proves the actual detailed dirty call returned; placement already dirtied the Chunk, so no fabricated clean-to-dirty transition is used.',
                         'No original pixel, whole-game parity, sustained-play or FPS claim.']}
    write(evidence / 'run.json', report)
    try:
        mapping = allowed_indices(assets / 'jar/font.txt')
        report['mapping'] = mapping
        with tempfile.TemporaryDirectory(prefix='appdata-', dir=evidence) as temporary:
            home = Path(temporary); game = home / '.minecraft'; game.mkdir()
            world.repeat_statistics(game, assets)
            (game / 'options.txt').write_bytes(b'viewDistance:3\ndifficulty:2\n')
            saves = game / 'saves'
            report['authored'] = authored(world, selected, mining, nbt, saves)
            before = world.save_title_files(saves, evidence / 'before')
            bounds = ','.join(map(str, monitor['selected']['work']))
            environment = dict(os.environ, APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home),
                ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS=bounds, MC_WORLD_SELECTION_MONITOR_BOUNDS=bounds,
                MC_WORLD_RELOAD_SAVES=str(saves), MC_WORLD_SAVE_TITLE_SAVES=str(saves),
                MC_WORLD_SAVE_TITLE_SNAPSHOTS=str(evidence / 'snapshots'), MC_WORLD_SAVE_TITLE_ENTRIES=str(evidence / 'entries'))
            first = time.time_ns() // 1000000
            child = native_run([str(executable), str(assets), str(evidence)], environment, evidence, saves, before, first)
            last = time.time_ns() // 1000000
            require(len(child.stdout) <= 16 * 1048576 and len(child.stderr) <= 1048576,
                    'Bounded complete retained physical native streams before semantic decode')
            stdout, stderr = child.stdout.decode('ascii', 'strict').replace('\r\n', '\n'), child.stderr.decode('ascii', 'strict').replace('\r\n', '\n')
            print(stdout, end=''); print(stderr, end='', file=sys.stderr)
            report['native_exit'] = child.returncode
            require(child.returncode == 0, 'Actual native gameplay cohort and checked closure returned')
            report['native'] = receipts(stdout, stderr, monitor, mapping, evidence=evidence, interval=(first, last))
            report['saved'] = saved_gate(nbt, evidence / 'snapshots/sign-saved/alpha')
            require(report['native']['rows_utf16'] == [report['saved']['tiles'][i] for i in range(2)],
                    'Actual edited/Done/reloaded rows join independent on-disk UTF16')
            require({path.name for path in evidence.glob('*.png')} == IMAGES, 'Exact five fresh image inventory')
            report['images'] = {name: G.png_gate(evidence / name, first) for name in sorted(IMAGES)}
            after = world.save_title_files(saves, evidence / 'after-verified')
            require(all(after.get(name) == pin for name, pin in before.items()
                        if not name.startswith('alpha/') or name == 'alpha/preserve.bin') and
                    all(name.startswith('alpha/') for name in set(after) - set(before)), 'Unrelated saved names/bytes preserved')
            saved, final = G.snapshot_files(evidence / 'snapshots/sign-saved'), G.snapshot_files(saves)
            require(set(saved) == set(final) and all(pin == final[name] for name, pin in saved.items()
                    if name != 'alpha/session.lock') and final['alpha/session.lock']['bytes'] == 8,
                    'Reselection preserves saved bytes except actual eight-byte SessionLock')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['inputs_after'], report['assets_after'] = G.fingerprints(G.source_inputs(executable)), world.snapshot(assets)
        if report['inputs_after'] != before_inputs or report['assets_after'] != before_assets:
            report['status'] = 'FAIL'; report['continuity_error'] = 'Actual selected source/helper/product/DLL or asset drift'
        write(evidence / 'run.json', report)
        print('Sign gameplay artifacts: ' + str(evidence))
    require(report['status'] == 'PASS', 'Actual cohort continuity')


if __name__ == '__main__':
    main()
