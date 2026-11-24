"""Authored finite direct native YC draw controls; no original/full-editor/pixel claim.

Five fresh processes preserve healthy draws and four terminal Source failures.
The real device is hidden on monitor2 on Windows. Linux records its actual SDL
Display identity with monitor=0 and makes no physical monitor2 claim. Saved inputs, public marker3
and App.q split are declared before the actual calls. No reference executable,
full font alphabet, GUI event route, GL unwind or batch repair is supplied.
"""
import argparse
import copy
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
import zlib

sys.dont_write_bytecode = True
ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'src/client').is_dir())
MODES = ('healthy', 'captured-null', 'manager-null-on', 'manager-null-off', 'nw-open')
PROFILES = ((63, 8, 180.), (68, 2, 180.), (68, 3, 0.), (68, 4, 90.), (68, 5, -90.))
TICKS = (0, 5, 6, 11, 12)
LINES = ('Alpha', 'Beta', 'Gamma', 'Delta')
MAX_JSON = 2 * 1024 * 1024


def require(value, message):
    if not value:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, 'actual helper module')
    owner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(owner)
    return owner


def pin(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def snapshot(path):
    return {p.relative_to(path).as_posix(): pin(p.read_bytes()) for p in sorted(path.rglob('*'))
            if p.is_file()}


def sources():
    files = [ROOT / 'CMakeLists.txt']
    for name in ('src', 'include', 'tests', 'cmake', 'scripts'):
        files.extend(p for p in (ROOT / name).rglob('*') if p.is_file() and
                     '__pycache__' not in p.parts)
    return {p.relative_to(ROOT).as_posix(): pin(p.read_bytes()) for p in sorted(files)}


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key ' + key)
        result[key] = value
    return result


def nonfinite(value):
    raise ValueError('nonfinite JSON ' + value)


def keys(value, names, label):
    require(type(value) is dict and set(value) == set(names.split()), 'exact ' + label + ' keys')


def integer(value, low, high, label):
    require(type(value) is int and low <= value <= high, label)


def site(value):
    keys(value, 'class method descriptor pc', 'site')
    require(all(value[k] is None or type(value[k]) is str for k in ('class', 'method', 'descriptor')),
            'site strings')
    integer(value['pc'], 0, 65535, 'site PC')


def digest(row, text):
    data = text.encode('utf-16le')
    value = 14695981039346656037
    for byte in struct.pack('<IBQ', row, 0, len(data) // 2) + data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f'{value:016x}'


def gl(value):
    keys(value, 'marker matrix_mode modelview_depth projection_depth modelview projection texture '
         'depth_write blend error source nw', 'GL observation')
    integer(value['marker'], -1, 3, 'observed marker')
    require(value['matrix_mode'] in (5888, 5889), 'represented selected matrix')
    for name in ('modelview_depth', 'projection_depth'):
        integer(value[name], 0, 64, 'native facade stack extent')
    for name in ('modelview', 'projection'):
        require(type(value[name]) is list and len(value[name]) == 16 and
                all(type(x) is str and re.fullmatch('[0-9a-f]{8}', x) for x in value[name]),
                'exact represented matrix bits')
    integer(value['texture'], 0, 2**31 - 1, 'actual bound texture')
    require(type(value['depth_write']) is bool and type(value['blend']) is bool and
            type(value['error']) is int and value['error'] == 0, 'real core GL flags/error')
    keys(value['source'], 'texture alpha flat blend_known blend depth_known depth_write', 'source GL')
    require(all(type(x) is bool for x in value['source'].values()), 'typed source GL flags')
    keys(value['nw'], 'h o p w x q', 'NW prefix')
    for name in ('h', 'o', 'p'):
        integer(value['nw'][name], 0, 2097152, 'NW source count')
    require(all(type(value['nw'][k]) is bool for k in ('w', 'x', 'q')), 'NW Boolean cells')


def admit(raw, stdout, stderr, code, mode, *, monitor2_required):
    require(type(monitor2_required) is bool, 'explicit capture-host monitor policy')
    require(mode in MODES and type(code) is int and code == 0 and stderr == b'', 'successful child')
    require(type(raw) is bytes and 0 < len(raw) <= MAX_JSON and raw.endswith(b'\n'), 'bounded full JSON')
    count = 25 if mode == 'healthy' else 1
    footer = re.fullmatch(rb'16 achievements(?P<nl>\r?\n)151 recipes(?P=nl)PASS: native YC direct draw mode=' +
                          mode.encode() + rb' rows=' + str(count).encode() +
                          rb' checks=[1-9][0-9]{0,9} cleanup=1(?P=nl)', stdout)
    require(footer is not None, 'exact complete startup/footer stream with consistent LF or CRLF')
    data = json.loads(raw.decode('utf8'), object_pairs_hook=unique, parse_constant=nonfinite)
    keys(data, 'schema device font1 font2 rows cleanup rows_complete source_close terminal_gl_repair '
         'terminal_batch_draw swaps polls', 'draw result')
    require(data['schema'] == 'NATIVE_YC_DIRECT_DRAW_V1' and data['cleanup'] is True and
            type(data['rows_complete']) is int and data['rows_complete'] == count and
            data['source_close'] is False and data['terminal_gl_repair'] is False and
            data['terminal_batch_draw'] is False and type(data['swaps']) is int and data['swaps'] == 0 and
            type(data['polls']) is int and data['polls'] == 0, 'exact completion and finite scope')
    keys(data['device'], 'vendor renderer version hidden monitor display_id', 'device')
    require(all(type(data['device'][k]) is str and data['device'][k] for k in ('vendor', 'renderer', 'version')) and
            data['device']['hidden'] is True and type(data['device']['monitor']) is int and
            data['device']['monitor'] == (2 if monitor2_required else 0),
            'real hidden device under explicit capture-host monitor policy')
    integer(data['device']['display_id'], 1, 2**32 - 1, 'actual selected Display identity')
    for name in ('font1', 'font2'):
        keys(data[name], 'texture lists', 'Font allocation')
        for key in ('texture', 'lists'):
            integer(data[name][key], 1, 2**31 - 1, 'real Font allocation')
    require(data['font1']['texture'] != data['font2']['texture'] and
            data['font1']['lists'] != data['font2']['lists'], 'distinct actual Font1/Font2 allocations')
    require(type(data['rows']) is list and len(data['rows']) == count, 'closed ordered row census')
    for index, row in enumerate(data['rows']):
        keys(row, 'index world stage tick row returned before state preview_rows failure after after_retire '
             'retired retire_idempotent', 'draw row')
        expected_world, stage = divmod(index, 5) if mode == 'healthy' else (0, 0)
        tick = TICKS[stage] if mode == 'healthy' else 6 if mode == 'manager-null-off' else 0
        selected = stage % 4 if mode == 'healthy' else 0
        require(all(type(row[k]) is int for k in ('index', 'world', 'stage', 'tick', 'row')) and
                (row['index'], row['world'], row['stage'], row['tick'], row['row']) ==
                (index, expected_world, stage, tick, selected), 'actual ordered tick/down progression')
        require(row['retired'] is True and row['retire_idempotent'] is True and
                row['returned'] is (mode == 'healthy'), 'Source return/actual terminal child census')
        for name in ('before', 'after', 'after_retire'):
            gl(row[name])
        before, after, state, failure = (row[k] for k in ('before', 'after', 'state', 'failure'))
        require(before['marker'] == 3 and after == row['after_retire'], 'declared marker3 and no retirement GL/NW repair')
        keys(state, 'pc background title push pop preview marker_stored marker_cleared buttons measurements '
             'font_draws angle title_width title_x', 'draw state')
        require(all(type(state[k]) is bool for k in ('background', 'title', 'push', 'pop', 'preview',
                                                    'marker_stored', 'marker_cleared')), 'draw Boolean prefixes')
        for name in ('pc', 'buttons', 'measurements', 'font_draws'):
            integer(state[name], 0, 65535, 'bounded draw state integer')
        require(type(state['angle']) is str and re.fullmatch('[0-9a-f]{8}', state['angle']), 'angle bits')
        for name in ('title_width', 'title_x'):
            integer(state[name], -4096, 4096, 'finite title centering')
        if state['title']:
            require(state['title_width'] >= 0 and state['title_x'] == 160 - state['title_width'] // 2,
                    'actual captured-Font title centering result')
        keys(failure, 'status site caller helper native exception error class', 'failure')
        for name in ('site', 'caller', 'helper'):
            site(failure[name])
        require(all(type(failure[k]) is bool for k in ('native', 'exception', 'error')), 'typed Throwable flags')
        integer(failure['status'], 0, 64, 'screen status')
        if mode == 'healthy':
            require(all(state[k] for k in ('background', 'title', 'push', 'pop', 'preview', 'marker_cleared')) and
                    state['marker_stored'] is ((tick // 6) % 2 == 0) and state['pc'] == 236 and
                    (state['buttons'], state['measurements'], state['font_draws']) == (1, 2, 2) and
                    state['angle'] == struct.pack('>f', PROFILES[expected_world][2]).hex() and
                    failure['status'] == 0 and failure['native'] is False and failure['exception'] is False and
                    failure['error'] is False and failure['class'] is None and
                    failure['site'] == {'class': 'yc', 'method': 'a', 'descriptor': '(IIF)V', 'pc': 236} and
                    after['marker'] == -1 and after['modelview_depth'] == before['modelview_depth'] and
                    after['projection_depth'] == before['projection_depth'] and not after['nw']['w'] and
                    after['texture'] == data['font2']['texture'], 'complete genuine YC/PO/KE result and Font2 final bind')
        else:
            require(failure['status'] != 0 and failure['native'] is False and failure['exception'] is True and
                    failure['error'] is False and not any(state[k] for k in ('pop', 'preview', 'marker_cleared')) and
                    state['buttons'] == 0, 'retained failed Source prefix')
            if mode == 'captured-null':
                require(state['background'] and not state['title'] and not state['push'] and
                        not state['marker_stored'] and state['pc'] == 23 and
                        failure['class'] == 'java/lang/NullPointerException' and
                        failure['site'] == {'class': 'ub', 'method': 'a',
                                           'descriptor': '(Lsj;Ljava/lang/String;III)V', 'pc': 5} and
                        after['marker'] == 3 and after['modelview_depth'] == before['modelview_depth'],
                        'captured null Font actual UB5 prefix')
            elif mode.startswith('manager-null'):
                require(state['background'] and state['title'] and state['push'] and state['pc'] == 215 and
                        state['marker_stored'] is (tick == 0) and
                        failure['class'] == 'java/lang/NullPointerException' and
                        after['marker'] == (0 if tick == 0 else 3) and not after['depth_write'] and
                        after['modelview_depth'] == before['modelview_depth'] + 2,
                        'actual LL null Font model/marker/depth/matrix prefix')
            else:
                require(not state['background'] and not state['title'] and not state['push'] and
                        not state['marker_stored'] and state['pc'] == 0 and
                        failure['class'] == 'java/lang/IllegalStateException' and
                        failure['site'] == {'class': 'nw', 'method': 'a', 'descriptor': '(I)V', 'pc': 16} and
                        failure['helper'] == {'class': 'ub', 'method': 'a', 'descriptor': '(IIIIII)V', 'pc': 154} and
                        before['nw']['w'] and after['nw']['w'] and
                        all(after['nw'][k] == before['nw'][k] for k in ('h', 'o', 'p')) and
                        after['marker'] == 3 and after['blend'] and after['source']['blend'] and
                        not after['source']['texture'] and not after['source']['alpha'] and not after['source']['flat'],
                        'actual already-open NW guard and preceding gradient GL prefix')
        receipts = row['preview_rows']
        require(type(receipts) is list and len(receipts) == (4 if mode == 'healthy' or mode.startswith('manager-null') else 0),
                'exact reached preview row receipts')
        for i, receipt in enumerate(receipts):
            keys(receipt, 'captured selected units digest measured drawn width x y', 'preview row')
            require(all(type(receipt[k]) is bool for k in ('captured', 'selected', 'measured', 'drawn')), 'preview Boolean cells')
            integer(receipt['units'], 0, 19, 'authored finite row units')
            require(type(receipt['digest']) is str and re.fullmatch('[0-9a-f]{16}', receipt['digest']), 'row argument digest')
            for name in ('width', 'x', 'y'):
                integer(receipt[name], -4096, 4096, 'row centering integer')
            if mode == 'healthy' or i == 0:
                marker = selected if state['marker_stored'] else 3
                text = '> ' + LINES[i] + ' <' if i == marker else LINES[i]
                require(receipt['captured'] and receipt['selected'] is (i == marker) and
                        receipt['units'] == len(text) and receipt['digest'] == digest(i, text),
                        'exact authored/decorated UTF16 argument')
            if mode == 'healthy':
                require(receipt['measured'] and receipt['drawn'] and receipt['width'] >= 0 and
                        receipt['x'] == -(receipt['width'] // 2) and receipt['y'] == i * 10 - 20,
                        'real row measure/draw and Java centering')
            elif i == 0:
                require(not receipt['measured'] and not receipt['drawn'] and
                        (receipt['width'], receipt['x'], receipt['y']) == (0, 0, 0),
                        'null Font fails before row measure result/centering/draw')
            else:
                require(receipt == dict(captured=False, selected=False, units=0,
                                       digest='0000000000000000', measured=False, drawn=False,
                                       width=0, x=0, y=0), 'all later row calls remain unreached')
    return {'mode': mode, 'rows': count, 'cleanup': True, 'original_comparison': False,
            'monitor2_required': monitor2_required}


def prepared_world(path, profile, picking, metadata):
    block, orientation, _ = profile
    region = path / 'region'
    region.mkdir(parents=True)
    path.joinpath('level.dat').write_bytes(gzip.compress(metadata.metadata(), mtime=0))
    path.joinpath('session.lock').write_bytes(bytes(8))
    header, sectors = bytearray(8192), []
    for x in range(2):
        for z in range(2):
            wire = picking.chunk(x, z)
            if x == z == 0:
                blocks, data = bytearray(32768), bytearray(16384)
                def cell(cx, cy, cz, value, meta=0):
                    at = (cx << 11) | (cz << 7) | cy
                    blocks[at] = value
                    data[at // 2] = (data[at // 2] & (15 if at & 1 else 240)) | (meta << (4 if at & 1 else 0))
                cell(3, 64, 3, block, orientation)
                support = (3, 63, 3) if block == 63 else {2: (3, 64, 4), 3: (3, 64, 2),
                                                                      4: (4, 64, 3), 5: (2, 64, 3)}[orientation]
                cell(*support, 1)
                for name, array in (('Blocks', blocks), ('Data', data)):
                    old = picking.tag(7, name, struct.pack('>i', len(array)) + bytes(len(array)))
                    require(wire.count(old) == 1, 'one authored Chunk array')
                    wire = wire.replace(old, picking.tag(7, name, struct.pack('>i', len(array)) + array))
                text = lambda value: struct.pack('>H', len(value)) + value.encode('ascii')
                tile = picking.tag(8, 'id', text('Sign'))
                for name, value in (('x', 3), ('y', 64), ('z', 3)):
                    tile += picking.tag(3, name, struct.pack('>i', value))
                for i, value in enumerate(LINES):
                    tile += picking.tag(8, 'Text' + str(i + 1), text(value))
                empty = picking.tag(9, 'TileEntities', b'\x0a' + bytes(4))
                require(wire.count(empty) == 1, 'one authored Tile list')
                wire = wire.replace(empty, picking.tag(9, 'TileEntities', b'\x0a' + struct.pack('>i', 1) + tile + b'\0'))
            payload = b'\2' + zlib.compress(wire)
            record = struct.pack('>I', len(payload)) + payload
            require(len(record) <= 4096, 'finite saved record')
            struct.pack_into('>I', header, 4 * (x + 32 * z), ((2 + len(sectors)) << 8) | 1)
            sectors.append(record + bytes(4096 - len(record)))
    region.joinpath('r.0.0.mcr').write_bytes(header + b''.join(sectors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    if args.controls:
        require(not any((args.executable, args.assets, args.artifacts)), 'offline controls have no native arguments')
        controls()
        return
    require((os.name == 'nt' or sys.platform.startswith('linux')) and
            all((args.executable, args.assets, args.artifacts)), 'actual Windows/Linux product/assets/artifacts')
    # Derived from the actual Python capture host, never from executable name,
    # a claimed target platform, the native JSON, or an optional CLI relaxation.
    monitor2_required = os.name == 'nt'
    picking = module('yc_draw_authored_chunk', ROOT / 'tests/tools/client/test_picking.py')
    metadata = module('yc_draw_authored_metadata', ROOT / 'tests/tools/world/world_query_inputs.py')
    sdk = module('yc_draw_authored_icons', ROOT / 'tests/tools/client/test_display_sdk.py')
    capture = module('yc_draw_bounded_capture', ROOT / 'tests/reference/client/statistics/support.py')
    protocol = module('yc_draw_capture_admission', ROOT / 'tests/reference/client/render_resources/protocol.py')
    products = module('yc_draw_products', ROOT / 'tests/tools/client/native_product_capture.py')
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='yc-direct-', dir=args.artifacts.resolve()))
    executable, assets = args.executable.resolve(strict=True), args.assets.resolve(strict=True)
    textures = (assets / 'jar').resolve(strict=True)
    require(all((textures / name).is_file() for name in ('font.txt', 'font/default.png', 'item/sign.png', 'gui/gui.png', 'pack.png')),
            'actual imported class resources')
    before_assets, before_sources = snapshot(assets), sources()
    report = {'status': 'FAIL', 'scope': __doc__, 'assets_before': before_assets,
              'sources_before': before_sources, 'processes': [], 'original_comparison': False,
              'capture_host': {'os_name': os.name, 'sys_platform': sys.platform,
                               'monitor2_required': monitor2_required}}
    try:
        for mode in MODES:
            work = output / mode
            work.mkdir()
            for name, value in picking.RESOURCES.items():
                target = work / 'resources' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(value)
            sdk.authored_resources(work / 'icons')
            worlds = work / 'worlds'
            for index, profile in enumerate(PROFILES):
                prepared_world(worlds / f'case{index:02d}', profile, picking, metadata)
            before_worlds = snapshot(worlds)
            native = work / 'native.json'
            command = [str(executable), str(work / 'resources'), str(textures), str(worlds),
                       str(work / 'icons'), 'saved-signs-v1', mode, str(native)]
            process = {'mode': mode, 'command': command, 'worlds_before': before_worlds,
                       'native_products': products.capture_native_products(command, work)}
            report['processes'].append(process)
            env = os.environ.copy()
            home = work / 'home'; home.mkdir()
            env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home), ALSOFT_DRIVERS='null')
            try:
                result = capture.run_bounded(command, work, 'native', timeout=240, limit=MAX_JSON, env=env)
            except BaseException as failure:
                if hasattr(failure, 'capture_report'):
                    process['capture'] = failure.capture_report
                raise
            process['capture'] = result.capture_report
            protocol.completed(result.capture_report, result.stdout, result.stderr, executable)
            require(result.capture_report['command'] == command, 'exact genuine direct command')
            raw = native.read_bytes(); process['native_json'] = pin(raw)
            process['admission'] = admit(raw, result.stdout, result.stderr, result.returncode, mode,
                                         monitor2_required=monitor2_required)
            process['stdout'], process['stderr'] = pin(result.stdout), pin(result.stderr)
            after_worlds = snapshot(worlds); process['worlds_after'] = after_worlds
            locks = {f'case{i:02d}/session.lock' for i in range(len(PROFILES))}
            require(set(before_worlds) == set(after_worlds) and
                    all(before_worlds[n] == after_worlds[n] for n in before_worlds if n not in locks) and
                    all(after_worlds[n]['bytes'] == 8 for n in locks), 'complete saved inventory; only actual SessionLocks change')
        report['status'] = 'PASS'
    except BaseException as failure:
        report['error'] = type(failure).__name__ + ': ' + str(failure)
        raise
    finally:
        report['assets_after'], report['sources_after'] = snapshot(assets), sources()
        unchanged = report['assets_after'] == before_assets and report['sources_after'] == before_sources
        if not unchanged:
            report.update(status='FAIL', bracket_error='asset or public source changed during direct probe')
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8', newline='\n')
        print('Capture: ' + str(output))
        require(unchanged, 'asset/public-source brackets')
    print('PASS: authored native YC direct draw; 25 returned draws; 4 terminal Source failures; cleanup=1')


def controls():
    # Independent synthetic positive rows exercise every acceptance branch.
    # They are offline reader controls and receive no native/device credit.
    def snapshot_value():
        matrix = ['3f800000' if i % 5 == 0 else '00000000' for i in range(16)]
        return {'marker': 3, 'matrix_mode': 5888, 'modelview_depth': 0, 'projection_depth': 0,
                'modelview': matrix, 'projection': matrix.copy(), 'texture': 1, 'depth_write': True,
                'blend': False, 'error': 0, 'source': dict(texture=True, alpha=True, flat=True,
                    blend_known=True, blend=False, depth_known=True, depth_write=True),
                'nw': dict(h=0, o=0, p=0, w=False, x=False, q=False)}
    def make(mode):
        rows = []
        for index in range(25 if mode == 'healthy' else 1):
            world, stage = divmod(index, 5) if mode == 'healthy' else (0, 0)
            tick = TICKS[stage] if mode == 'healthy' else 6 if mode == 'manager-null-off' else 0
            selected = stage % 4 if mode == 'healthy' else 0
            healthy, manager = mode == 'healthy', mode.startswith('manager-null')
            before, after = snapshot_value(), snapshot_value()
            state = dict(pc=236 if healthy else 215 if manager else 23 if mode == 'captured-null' else 0,
                         background=mode != 'nw-open', title=healthy or manager, push=healthy or manager,
                         pop=healthy, preview=healthy, marker_stored=(healthy or manager) and (tick // 6) % 2 == 0,
                         marker_cleared=healthy, buttons=int(healthy), measurements=2 if healthy else int(manager),
                         font_draws=2 if healthy else int(manager), angle=struct.pack('>f', PROFILES[world][2] if healthy else 0).hex(),
                         title_width=94 if healthy or manager else 0, title_x=113 if healthy or manager else 0)
            zero = {'class': None, 'method': None, 'descriptor': None, 'pc': 0}
            failure = dict(status=0 if healthy else 1, site=dict(zero), caller=dict(zero), helper=dict(zero),
                           native=False, exception=not healthy, error=False,
                           **{'class': None if healthy else 'java/lang/IllegalStateException' if mode == 'nw-open'
                              else 'java/lang/NullPointerException'})
            if healthy:
                after.update(marker=-1, texture=2)
                failure['site'] = dict(zip(('class', 'method', 'descriptor', 'pc'), ('yc', 'a', '(IIF)V', 236)))
            elif manager:
                after.update(marker=0 if tick == 0 else 3, modelview_depth=2, depth_write=False)
            elif mode == 'captured-null':
                failure['site'] = dict(zip(('class', 'method', 'descriptor', 'pc'), ('ub', 'a', '(Lsj;Ljava/lang/String;III)V', 5)))
            else:
                before['nw']['w'] = after['nw']['w'] = True
                after['blend'] = after['source']['blend'] = True
                after['source'].update(texture=False, alpha=False, flat=False)
                failure['site'] = dict(zip(('class', 'method', 'descriptor', 'pc'), ('nw', 'a', '(I)V', 16)))
                failure['helper'] = dict(zip(('class', 'method', 'descriptor', 'pc'), ('ub', 'a', '(IIIIII)V', 154)))
            receipts = []
            for i in range(4 if healthy or manager else 0):
                marker = selected if state['marker_stored'] else 3
                text = '> ' + LINES[i] + ' <' if i == marker else LINES[i]
                receipts.append(dict(captured=healthy or i == 0, selected=i == marker,
                                     units=len(text), digest=digest(i, text), measured=healthy,
                                     drawn=healthy, width=20, x=-10, y=i * 10 - 20))
                if manager:
                    receipts[-1].update(measured=False, drawn=False, width=0, x=0, y=0)
                    if i != 0:
                        receipts[-1].update(captured=False, selected=False, units=0,
                                            digest='0000000000000000')
            rows.append(dict(index=index, world=world, stage=stage, tick=tick, row=selected, returned=healthy,
                             before=before, state=state, preview_rows=receipts, failure=failure, after=after,
                             after_retire=copy.deepcopy(after), retired=True, retire_idempotent=True))
        return dict(schema='NATIVE_YC_DIRECT_DRAW_V1', device=dict(vendor='authored', renderer='offline',
                    version='no device', hidden=True, monitor=2, display_id=1), font1=dict(texture=1, lists=1),
                    font2=dict(texture=2, lists=289), rows=rows, cleanup=True, rows_complete=len(rows),
                    source_close=False, terminal_gl_repair=False, terminal_batch_draw=False, swaps=0, polls=0)
    encode = lambda value: (json.dumps(value, separators=(',', ':')) + '\n').encode()
    positives, refused = 0, 0
    for mode, monitor2_required in ((mode, required) for mode in MODES for required in (True, False)):
        value = make(mode)
        value['device']['monitor'] = 2 if monitor2_required else 0
        for nl in (b'\n', b'\r\n'):
            stdout = nl.join((b'16 achievements', b'151 recipes',
                             f'PASS: native YC direct draw mode={mode} rows={len(value["rows"])} checks=1 cleanup=1'.encode(), b''))
            admit(encode(value), stdout, b'', 0, mode, monitor2_required=monitor2_required); positives += 1
        stdout = b'16 achievements\n151 recipes\n' + f'PASS: native YC direct draw mode={mode} rows={len(value["rows"])} checks=1 cleanup=1\n'.encode()
        mutations = []
        for name, mutate in (
            ('missing row', lambda x: x['rows'].pop()),
            ('false cleanup', lambda x: x.update(cleanup=False)),
            ('wrong chronology', lambda x: x['rows'][0].update(tick=5)),
            ('marker repair', lambda x: x['rows'][0]['after_retire'].update(marker=2)),
            ('wrong Source class', lambda x: x['rows'][0]['failure'].update(**{'class': 'invented'})),
            ('same Fonts', lambda x: x['font2'].update(texture=x['font1']['texture'])),
            ('boolean counter', lambda x: x['rows'][0].update(index=False)),
            ('extra field', lambda x: x.update(fabricated=True)),
            ('wrong host monitor', lambda x: x['device'].update(monitor=0 if monitor2_required else 2)),
            ('missing Display identity', lambda x: x['device'].update(display_id=0)),
        ):
            changed = copy.deepcopy(value); mutate(changed); mutations.append((name, encode(changed), stdout, b'', 0))
        mutations += [('stderr', encode(value), stdout, b'warning', 0),
                      ('nonzero exit', encode(value), stdout, b'', 1),
                      ('truncated JSON', encode(value)[:-2], stdout, b'', 0),
                      ('mixed separators', encode(value), stdout.replace(b'\n', b'\r\n', 1), b'', 0),
                      ('bare CR', encode(value), stdout.replace(b'\n', b'\r'), b'', 0),
                      ('extra stdout', encode(value), stdout + b'extra\n', b'', 0),
                      ('duplicate JSON key', encode(value).replace(b'"cleanup":true', b'"cleanup":true,"cleanup":true'), stdout, b'', 0)]
        for name, raw, out, err, code in mutations:
            try:
                admit(raw, out, err, code, mode, monitor2_required=monitor2_required)
            except (ValueError, KeyError, TypeError, UnicodeError):
                refused += 1
            else:
                raise ValueError('accepted corruption ' + mode + ': ' + name)
    print(f'PASS: YC draw offline reader controls; positives={positives} refused={refused}; native_credit=0')


if __name__ == '__main__':
    main()
