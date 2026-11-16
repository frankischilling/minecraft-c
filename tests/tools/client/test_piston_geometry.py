"""Compare actual native HY draws with the finite original direct-draw cohort."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / 'tests/reference/client/piston_draw'
MAX_JSON = 16 * 1024 * 1024
FOOTER = (rb'16 achievements(?P<nl>\r?\n)151 recipes(?P=nl)'
          rb'PASS: 27 Source Piston direct HY geometry rows; '
          rb'checks=(?P<checks>[1-9][0-9]{0,9}); cleanup=1(?P=nl)')
ROW_KEYS = {'index', 'profile', 'before', 'returned', 'failure', 'vertices', 'word_count',
            'words', 'progress_bits', 'translation_bits', 'skipped', 'tessellator_begun',
            'tessellator_drawn', 'after', 'texture'}
SNAPSHOT_EXTRA = {'world_iterating', 'world_entities', 'world_players', 'active_tiles',
                  'pending_tiles', 'mapped_tiles', 'cv_class', 'cv_world_same'}
GL_CORE = {'source_known', 'blend', 'cull', 'blend_src', 'blend_dst', 'bound_texture', 'error'}
GL_LEGACY = {'lighting', 'light0', 'light1', 'color_material', 'shade'}
EXPLICIT_INPUTS = (
    Path('CMakeLists.txt'), Path('tests/tools/client/test_piston_geometry.py'),
    Path('tests/client/render/tile_entity_renderer/test_piston_geometry.c'),
    Path('tests/client/startup/test_application_startup.c'),
    Path('tests/tools/client/test_picking.py'), Path('tests/tools/client/native_product_capture.py'))
NATIVE_CONTROLS = {'lighting_null_buffer': {
    'scope': 'authored-native-fault', 'source_returned': False, 'hy_pc': 360, 'u_pc': 74,
    'float_clear_pc': 3, 'exception_class': 'java/lang/NullPointerException',
    'tessellator_drawn': True, 'vertices': 60, 'completed_display_calls': 5, 'pooled_vectors': 2,
    'cleanup_source_unchanged': True, 'retired': True, 'retire_idempotent': True,
    'u_buffer_remains_null': True}}


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pin(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def snapshot(root):
    return {p.relative_to(root).as_posix(): pin(p.read_bytes())
            for p in sorted(root.rglob('*')) if p.is_file()}


def compiled_inputs(root=ROOT):
    """Exact compiled/public census, including added and removed inputs."""
    paths = set(EXPLICIT_INPUTS)
    for directory in ('src', 'include'):
        paths.update(p.relative_to(root) for p in (root / directory).rglob('*')
                     if p.is_file() and p.suffix in ('.c', '.h', '.inc'))
    paths.update(p.relative_to(root) for p in (root / 'cmake').rglob('*.cmake') if p.is_file())
    paths.update(p.relative_to(root) for p in (root / 'tests').rglob('*')
                 if p.is_file() and p.suffix in ('.h', '.inc'))
    paths.update(p.relative_to(root) for p in (root / 'tests/reference/client/piston_draw').rglob('*')
                 if p.is_file())
    return {p.as_posix(): pin((root / p).read_bytes()) for p in sorted(paths)}


def guard_worlds(before, after):
    locks = {'case%02d/session.lock' % i for i in range(27)}
    require(set(before) == set(after) and locks <= set(before), 'complete saved input wire census')
    require({n for n in before if n.endswith('/session.lock')} == locks, 'exact 27 owned SessionLocks')
    require(all(before[n]['bytes'] == 8 and after[n]['bytes'] == 8 for n in locks),
            'each owned SessionLock remains exactly eight bytes')
    require(all(after[n] == value for n, value in before.items() if n not in locks),
            'unchanged complete saved input wire bytes apart from SessionLocks')


def unique(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, 'duplicate native JSON key ' + key)
        value[key] = item
    return value


def shape(expected, actual, path, values=False):
    """Strict recursive types; Python bool never stands in for an int."""
    require(type(actual) is type(expected), path + ' type')
    if type(expected) is dict:
        require(set(actual) == set(expected), path + ' exact fields')
        for key, value in expected.items():
            shape(value, actual[key], path + '.' + key, values)
    elif type(expected) is list:
        require(len(actual) == len(expected), path + ' length')
        for index, (left, right) in enumerate(zip(expected, actual)):
            shape(left, right, path + '[' + str(index) + ']', values)
    elif values:
        require(actual == expected,
                path + ': expected ' + repr(expected) + ', native ' + repr(actual))
    elif type(expected) is str and re.fullmatch(r'[0-9a-f]{8}|[0-9a-f]{16}', expected):
        require(re.fullmatch('[0-9a-f]{%d}' % len(expected), actual) is not None, path + ' raw bits')


def profile(case):
    return {k: case[k] for k in ('folder', 'block', 'metadata', 'facing', 'ticks', 'partial_bits',
                                'extending', 'render_head', 'ambient_occlusion')} | {
        'seed': 123, 'name': 'Original Piston draw %02d' % case['index']}


def admit_gl(value, path, after):
    require(type(value) is dict and type(value.get('source_known')) is bool, path + ' source ledger')
    known = value['source_known']
    require(set(value) == GL_CORE | (GL_LEGACY if known else set()), path + ' exact fields')
    require(not after or known, path + ' reached legacy source state')
    for key in ('blend', 'cull') + (('lighting', 'light0', 'light1', 'color_material') if known else ()):
        require(type(value[key]) is bool, path + '.' + key + ' boolean')
    for key in ('blend_src', 'blend_dst', 'bound_texture', 'error') + (('shade',) if known else ()):
        require(type(value[key]) is int, path + '.' + key + ' integer')
    require(value['error'] == 0 and value['bound_texture'] >= 0, path + ' GL error/binding')


def admit_snapshot(value, original, path, after):
    require(type(value) is dict and set(value) == set(original) | SNAPSHOT_EXTRA,
            path + ' complete owner fields')
    for key in original:
        if key != 'gl':
            shape(original[key], value[key], path + '.' + key, values=after)
    for key in ('world_iterating', 'cv_world_same'):
        require(type(value[key]) is bool, path + '.' + key + ' boolean')
    for key in ('world_entities', 'world_players'):
        require(type(value[key]) is int and value[key] == 0, path + '.' + key + ' empty source list')
    require(value['world_iterating'] is False and value['cv_world_same'] is True and
            type(value['cv_class']) is str and value['cv_class'] == 'cv', path + ' source aliases')
    for key in ('active_tiles', 'mapped_tiles'):
        shape([{'class': 'uk', 'selected': True}], value[key], path + '.' + key, values=True)
    shape([], value['pending_tiles'], path + '.pending_tiles', values=True)
    admit_gl(value['gl'], path + '.gl', after)
    if after:
        shape(original['gl'], {k: value['gl'][k] for k in original['gl']}, path + '.gl', values=True)


def nonfinite(value):
    raise ValueError('nonfinite native JSON number ' + value)


def admit_native(raw, stdout, stderr, returncode, packet, *, timed_out=False,
                 pipe_capture_complete=True):
    """Pure observation validator. Original expected results are comparison-only;
    they never enter the native process, World authorer or execution profiles.
    """
    require(type(returncode) is int and returncode == 0, 'native process exit')
    require(type(timed_out) is bool and timed_out is False and
            type(pipe_capture_complete) is bool and pipe_capture_complete is True,
            'complete native process/pipe terminal state')
    require(type(stdout) is bytes and len(stdout) <= 4096 and type(stderr) is bytes and not stderr,
            'bounded complete stdout and empty stderr')
    footer = re.fullmatch(FOOTER, stdout)
    require(footer is not None, 'exact achievements/recipes prefix and one complete cleanup footer')
    checks = int(footer.group('checks'))
    require(checks <= 2147483647, 'bounded native check count')
    require(type(raw) is bytes and 0 < len(raw) <= MAX_JSON and raw.startswith(b'{') and
            (raw.endswith(b'}\n') or raw.endswith(b'}\r\n')), 'bounded complete native JSON/newline')
    actual = json.loads(raw.decode('utf8'), object_pairs_hook=unique, parse_constant=nonfinite)
    require(type(actual) is dict and set(actual) ==
            {'schema', 'device', 'rows', 'cleanup', 'rows_complete', 'native_controls'},
            'exact native root fields')
    require(type(actual['schema']) is str and actual['schema'] == 'NATIVE_PISTON_DIRECT_GEOMETRY_V1' and
            type(actual['rows_complete']) is int and actual['rows_complete'] == 27 and
            actual['cleanup'] is True, 'complete native direct-draw cohort')
    device = actual['device']
    require(type(device) is dict and set(device) ==
            {'vendor', 'renderer', 'version', 'hidden', 'monitor', 'display_id'}, 'native device fields')
    require(all(type(device[k]) is str and device[k] for k in ('vendor', 'renderer', 'version')) and
            type(device['monitor']) is int and device['monitor'] == 2 and device['hidden'] is True and
            type(device['display_id']) is int and device['display_id'] > 0, 'hidden monitor-2 context')
    require(type(actual['rows']) is list and len(actual['rows']) == 27, 'all 27 native rows')
    for index, (case, original, native) in enumerate(zip(packet['cases'], packet['expected'], actual['rows'])):
        path = 'case' + str(index)
        require(type(native) is dict and set(native) == ROW_KEYS, path + ' complete row fields')
        require(type(native['index']) is int and native['index'] == index == original['index'] == case['index'],
                path + ' exact ordered index')
        shape(profile(case), native['profile'], path + '.profile', values=True)
        require(native['returned'] is True and native['failure'] is None, path + ' returned HY')
        for key in ('vertices', 'word_count', 'words'):
            shape(original[key], native[key], path + '.' + key, values=True)
        require(native['word_count'] == native['vertices'] * 8, path + ' full eight-word vertices')
        for key in ('skipped', 'tessellator_begun', 'tessellator_drawn'):
            require(type(native[key]) is bool, path + '.' + key + ' boolean')
        active = native['vertices'] > 0
        require(native['skipped'] is (not active) and native['tessellator_begun'] is active and
                native['tessellator_drawn'] is active, path + ' actual source batch receipt')
        shape('00000000', native['progress_bits'], path + '.progress_bits')
        shape(['0000000000000000'] * 3, native['translation_bits'], path + '.translation_bits')
        admit_snapshot(native['before'], original['retained_state'], path + '.before', False)
        admit_snapshot(native['after'], original['retained_state'], path + '.after', True)
        texture = native['texture']
        require(type(texture) is dict and set(texture) == {'found', 'id', 'width', 'height'} and
                texture['found'] is True and all(type(texture[k]) is int for k in ('id', 'width', 'height')) and
                texture['id'] > 0 and texture['width'] == texture['height'] == 256 and
                native['after']['gl']['bound_texture'] == texture['id'], path + ' actual terrain texture')
    # This authored native fault probe is separate from the 27 original draws.
    shape(NATIVE_CONTROLS, actual['native_controls'], 'native_controls', values=True)
    return {'actual': actual, 'checks': checks, 'cases': 27,
            'active_batches': sum(row['vertices'] > 0 for row in actual['rows'])}


def controls(packet):
    """Synthetic protocol data validates the reader, with no Source execution credit."""
    synthetic = {'schema': 'NATIVE_PISTON_DIRECT_GEOMETRY_V1',
                 'device': {'vendor': 'synthetic reader control', 'renderer': 'no GL execution',
                            'version': 'protocol fixture', 'hidden': True, 'monitor': 2, 'display_id': 2},
                 'rows': [], 'cleanup': True, 'rows_complete': 27,
                 'native_controls': copy.deepcopy(NATIVE_CONTROLS)}
    for case, original in zip(packet['cases'], packet['expected']):
        after = copy.deepcopy(original['retained_state'])
        after.update(world_iterating=False, world_entities=0, world_players=0,
                     active_tiles=[{'class': 'uk', 'selected': True}], pending_tiles=[],
                     mapped_tiles=[{'class': 'uk', 'selected': True}], cv_class='cv', cv_world_same=True)
        after['gl'].update(source_known=True, bound_texture=1)
        active = original['vertices'] > 0
        synthetic['rows'].append({
            'index': case['index'], 'profile': profile(case), 'before': copy.deepcopy(after),
            'returned': True, 'failure': None, 'vertices': original['vertices'],
            'word_count': original['word_count'], 'words': list(original['words']),
            'progress_bits': '00000000', 'translation_bits': ['0000000000000000'] * 3,
            'skipped': not active, 'tessellator_begun': active, 'tessellator_drawn': active,
            'after': after, 'texture': {'found': True, 'id': 1, 'width': 256, 'height': 256}})
    encode = lambda value: json.dumps(value, separators=(',', ':'), allow_nan=False).encode() + b'\n'
    raw = encode(synthetic)
    stdout = b'16 achievements\r\n151 recipes\r\nPASS: 27 Source Piston direct HY geometry rows; checks=2036; cleanup=1\r\n'
    passed = 0

    def accept(data=raw, output=stdout):
        nonlocal passed
        admit_native(data, output, b'', 0, packet)
        passed += 1

    def reject(label, data=raw, output=stdout, stderr=b'', code=0, **transport):
        nonlocal passed
        try:
            admit_native(data, output, stderr, code, packet, **transport)
        except ValueError:
            passed += 1
            return
        raise ValueError('reader admitted corruption: ' + label)

    def changed(path, value, delete=False):
        result = copy.deepcopy(synthetic)
        at = result
        for key in path[:-1]:
            at = at[key]
        if delete:
            del at[path[-1]]
        else:
            at[path[-1]] = value
        return encode(result)

    accept()
    accept(output=stdout.replace(b'\r\n', b'\n'))
    accept(data=raw[:-1] + b'\r\n')
    first_unknown = copy.deepcopy(synthetic)
    first_unknown['rows'][0]['before']['gl']['source_known'] = False
    for key in GL_LEGACY:
        del first_unknown['rows'][0]['before']['gl'][key]
    accept(data=encode(first_unknown))
    corruptions = [
        ('raw word mismatch', ('rows', 0, 'words', 0), 'ffffffff'),
        ('raw word nonhex', ('rows', 0, 'words', 0), '0000000g'),
        ('raw word integer', ('rows', 0, 'words', 0), 0),
        ('word vector truncated', ('rows', 0, 'words'), synthetic['rows'][0]['words'][:-1]),
        ('word count', ('rows', 0, 'word_count'), 479),
        ('vertex count boolean', ('rows', 0, 'vertices'), True),
        ('duplicate case index', ('rows', 1, 'index'), 0),
        ('case index boolean', ('rows', 0, 'index'), False),
        ('missing row', ('rows',), synthetic['rows'][:-1]),
        ('extra row', ('rows',), synthetic['rows'] + [synthetic['rows'][0]]),
        ('row order', ('rows',), [synthetic['rows'][1], synthetic['rows'][0]] + synthetic['rows'][2:]),
        ('cleanup false', ('cleanup',), False),
        ('cleanup integer', ('cleanup',), 1),
        ('complete count boolean', ('rows_complete',), True),
        ('native renderer failure', ('rows', 0, 'failure'), {'status': 1}),
        ('native renderer did not return', ('rows', 0, 'returned'), False),
        ('return boolean integer', ('rows', 0, 'returned'), 1),
        ('source batch failure', ('rows', 0, 'tessellator_drawn'), False),
        ('batch boolean integer', ('rows', 0, 'tessellator_begun'), 1),
        ('false skipped batch', ('rows', 0, 'skipped'), True),
        ('wrong authored metadata', ('rows', 0, 'profile', 'metadata'), 1),
        ('profile boolean integer', ('rows', 0, 'profile', 'ambient_occlusion'), 0),
        ('profile integer boolean', ('rows', 0, 'profile', 'seed'), True),
        ('progress raw bits', ('rows', 0, 'progress_bits'), 'nan'),
        ('translation truncated', ('rows', 0, 'translation_bits'), ['0000000000000000']),
        ('retained block', ('rows', 0, 'after', 'block'), 1),
        ('retained bound', ('rows', 0, 'after', 'head_bounds', 0), '3ff0000000000000'),
        ('retained integer boolean', ('rows', 0, 'after', 'cv', 'g'), False),
        ('retained boolean integer', ('rows', 0, 'after', 'cv', 'f'), 0),
        ('before malformed bits', ('rows', 0, 'before', 'tile', 'current_bits'), 'XXXXXXXX'),
        ('before integer boolean', ('rows', 0, 'before', 'nw', 'p'), False),
        ('World alias', ('rows', 0, 'after', 'cv_world_same'), False),
        ('World list value', ('rows', 0, 'after', 'active_tiles', 0, 'selected'), False),
        ('World count boolean', ('rows', 0, 'after', 'world_entities'), False),
        ('GL error', ('rows', 0, 'after', 'gl', 'error'), 1280),
        ('GL integer boolean', ('rows', 0, 'after', 'gl', 'shade'), True),
        ('GL boolean integer', ('rows', 0, 'after', 'gl', 'lighting'), 1),
        ('unknown after GL state', ('rows', 0, 'after', 'gl', 'source_known'), False),
        ('texture mismatch', ('rows', 0, 'texture', 'id'), 2),
        ('texture dimension boolean', ('rows', 0, 'texture', 'width'), True),
        ('wrong monitor', ('device', 'monitor'), 1),
        ('visible window', ('device', 'hidden'), False),
        ('display ID boolean', ('device', 'display_id'), True),
        ('extra root field', ('extra',), True),
        ('extra row field', ('rows', 0, 'extra'), True),
        ('extra retained field', ('rows', 0, 'after', 'extra'), True),
    ]
    for key, value in NATIVE_CONTROLS['lighting_null_buffer'].items():
        wrong = not value if type(value) is bool else value + 1 if type(value) is int else 'wrong'
        corruptions.append(('native fault receipt ' + key,
                            ('native_controls', 'lighting_null_buffer', key), wrong))
    for label, path, value in corruptions:
        reject(label, changed(path, value))
    reject('native fault bool/int', changed(('native_controls', 'lighting_null_buffer', 'retired'), 1))
    reject('native fault int/bool', changed(('native_controls', 'lighting_null_buffer', 'vertices'), True))
    for path in (('rows', 0, 'after', 'tile'), ('rows',), ('native_controls',)):
        reject('missing field ' + repr(path), changed(path, None, delete=True))
    reject('duplicate root key', raw.replace(b'{"schema":', b'{"cleanup":true,"schema":', 1))
    reject('duplicate nested key', raw.replace(b'"returned":true', b'"returned":true,"returned":true', 1))
    reject('truncated JSON', raw[:-10])
    reject('missing JSON newline', raw[:-1])
    reject('extra JSON newline', raw + b'\n')
    reject('extra JSON document', raw + b'{}\n')
    reject('invalid UTF8', raw.replace(b'protocol fixture', b'\xff', 1))
    reject('nonfinite JSON', raw.replace(b'"rows_complete":27', b'"rows_complete":NaN', 1))
    reject('JSON BOM', b'\xef\xbb\xbf' + raw)
    reject('oversized JSON', raw + b' ' * MAX_JSON + b'}\n')
    reject('missing stdout prefix', output=stdout.split(b'\r\n', 2)[2])
    reject('incorrect achievements', output=stdout.replace(b'16 achievements', b'17 achievements'))
    reject('extra stdout prefix', output=b'noise\r\n' + stdout)
    reject('extra stdout suffix', output=stdout + b'noise\r\n')
    reject('duplicate terminal', output=stdout + stdout.split(b'\r\n', 2)[2])
    reject('truncated terminal', output=stdout[:-15])
    reject('missing terminal newline', output=stdout[:-2])
    reject('extra terminal newline', output=stdout + b'\r\n')
    reject('mixed stdout newlines', output=stdout.replace(b'\r\n', b'\n', 1))
    reject('oversized stdout', output=b'x' * 4097)
    reject('stderr', stderr=b'unexpected diagnostic\n')
    reject('native exit failure', code=1)
    reject('exit boolean', code=False)
    reject('native timeout', timed_out=True)
    reject('incomplete raw pipes', pipe_capture_complete=False)

    before = {'case%02d/%s' % (i, name): pin(bytes(8) if name == 'session.lock' else b'synthetic wire')
              for i in range(27) for name in ('level.dat', 'region/r.0.0.mcr', 'session.lock')}
    after = copy.deepcopy(before)
    for name in after:
        if name.endswith('/session.lock'):
            after[name] = pin(b'newclock')
    guard_worlds(before, after)
    passed += 1
    missing = copy.deepcopy(after)
    del missing['case00/session.lock']
    try:
        guard_worlds(before, missing)
    except ValueError:
        passed += 1
    else:
        raise ValueError('wire guard admitted missing SessionLock')
    for label, name, value in (
            ('short lock', 'case00/session.lock', pin(bytes(7))),
            ('long lock', 'case00/session.lock', pin(bytes(9))),
            ('changed World wire', 'case00/region/r.0.0.mcr', pin(b'changed')),
            ('extra World file', 'unexpected.dat', pin(b'extra'))):
        bad = copy.deepcopy(after)
        bad[name] = value
        try:
            guard_worlds(before, bad)
        except ValueError:
            passed += 1
        else:
            raise ValueError('wire guard admitted ' + label)
    with tempfile.TemporaryDirectory(prefix='piston-reader-source-') as directory:
        root = Path(directory)
        files = set(EXPLICIT_INPUTS) | {
            Path('src/control.c'), Path('src/control.h'), Path('include/control.inc'),
            Path('cmake/control.cmake'), Path('tests/control.h'), Path('tests/control.inc'),
            Path('tests/reference/client/piston_draw/packet.py'),
            Path('tests/reference/client/piston_draw/control.json')}
        for relative in files:
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'synthetic source guard fixture\n')
        pins = compiled_inputs(root)
        require(set(pins) == {p.as_posix() for p in files}, 'complete compiled/public guard inventory')
        require(pins == compiled_inputs(root), 'stable compiled/public source guard')
        passed += 2
        for relative in files:
            path = root / relative
            path.write_bytes(b'changed source\n')
            require(compiled_inputs(root) != pins, 'source guard notices ' + relative.as_posix())
            path.write_bytes(b'synthetic source guard fixture\n')
            passed += 1
        extra = root / 'src/new.c'
        extra.write_bytes(b'new compiled input\n')
        require(compiled_inputs(root) != pins, 'source guard notices added compiled input')
        extra.unlink()
        (root / 'src/control.c').unlink()
        require(compiled_inputs(root) != pins, 'source guard notices removed compiled input')
        passed += 2
    print(json.dumps({'status': 'PASS', 'controls': passed,
                      'scope': 'Synthetic protocol/source-guard reader controls only; no native, Source, GL or gameplay execution'}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    expected = module('piston_geometry_packet', PACKET / 'packet.py').read_packet()
    if args.controls:
        require(args.executable is None and args.assets is None and args.artifacts is None,
                'portable controls take no executable/assets/artifact arguments')
        controls(expected)
        return
    require(args.executable is not None and args.assets is not None and args.artifacts is not None,
            'native comparison requires --executable, --assets and --artifacts')
    require(os.name == 'nt', 'Windows monitor-2 direct renderer fixture')
    writer = module('piston_geometry_worlds', PACKET / 'write_world.py')
    resources = module('piston_geometry_resources', Path(__file__).with_name('test_picking.py'))
    products = module('piston_geometry_products', Path(__file__).with_name('native_product_capture.py'))
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='piston-direct-', dir=args.artifacts.resolve()))
    worlds = output / 'worlds'
    writer.write_worlds(worlds)  # Authored declarations only; never expected comparison results.
    input_audit = writer.audit_worlds(worlds)
    for name, data in resources.RESOURCES.items():
        path = output / 'resources' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    executable = args.executable.resolve(strict=True)
    assets = args.assets.resolve(strict=True)
    texture_assets = (assets / 'jar').resolve(strict=True)
    require((texture_assets / 'pack.png').is_file() and (texture_assets / 'terrain.png').is_file(),
            'actual imported original class resources')
    before_assets, before_worlds = snapshot(assets), snapshot(worlds)
    before_sources = compiled_inputs()
    native_json = output / 'native.json'
    command = [str(executable), str(output / 'resources'), str(texture_assets), str(worlds),
               str(PACKET / 'profiles.tsv'), str(native_json)]
    report = {'status': 'FAIL', 'scope': __doc__, 'command': command,
              'input_audit': input_audit, 'assets_before': before_assets,
              'worlds_before': before_worlds, 'compiled_public_inputs_before': before_sources,
              'timed_out': False, 'pipe_capture_complete': False,
              'executable_sha256_before': hashlib.sha256(executable.read_bytes()).hexdigest()}
    env = os.environ.copy()
    home = output / 'appdata'
    home.mkdir()
    env.update(APPDATA=str(home), LOCALAPPDATA=str(home), USERPROFILE=str(home), HOME=str(home),
               ALSOFT_DRIVERS='null', SDL_DISPLAY_USABLE_BOUNDS='-1920,0,1920,1040',
               MC_WORLD_SELECTION_MONITOR_BOUNDS='-1920,0,1920,1040')
    error = None
    try:
        report['native_products'] = products.capture_native_products(command, output)
        start = time.perf_counter()
        process = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, env=env, timeout=300, check=False)
        report.update(returncode=process.returncode, elapsed_seconds=time.perf_counter() - start,
                      pipe_capture_complete=True)
        (output / 'stdout.bin').write_bytes(process.stdout)
        (output / 'stderr.bin').write_bytes(process.stderr)
        report.update(stdout_sha256=hashlib.sha256(process.stdout).hexdigest(),
                      stderr_sha256=hashlib.sha256(process.stderr).hexdigest())
        require(process.returncode == 0 and not process.stderr,
                'native direct draw process exit=' + str(process.returncode) + ': ' +
                process.stderr.decode(errors='replace')[:4096])
        require(native_json.stat().st_size <= MAX_JSON, 'bounded native geometry JSON')
        raw = native_json.read_bytes()
        report['native_json'] = pin(raw)
        admitted = admit_native(raw, process.stdout, process.stderr,
                                process.returncode, expected)
        report.update(status='PASS', cases=admitted['cases'], active_batches=admitted['active_batches'],
                      checks=admitted['checks'],
                      comparison='Exact active vertex words and every declared original retained cell',
                      device=admitted['actual']['device'],
                      native_controls=admitted['actual']['native_controls'],
                      native_control_scope='Authored native fault receipt; separate from 27 original geometry comparisons')
    except BaseException as failure:
        error = failure
        report['error'] = type(failure).__name__ + ': ' + str(failure)
        if hasattr(failure, 'receipt'):
            report['native_products'] = failure.receipt
        if isinstance(failure, subprocess.TimeoutExpired):
            report.update(timed_out=True, pipe_capture_complete=False)
            (output / 'stdout.bin').write_bytes(failure.stdout or b'')
            (output / 'stderr.bin').write_bytes(failure.stderr or b'')
    # Audit even a failed run; preserve the first process/preparation failure
    # and its raw streams instead of replacing it with a later guard failure.
    try:
        report['assets_after'] = snapshot(assets)
        report['worlds_after'] = snapshot(worlds)
        report['compiled_public_inputs_after'] = compiled_inputs()
        report['executable_sha256_after'] = hashlib.sha256(executable.read_bytes()).hexdigest()
        report['native_products_after'] = {
            item['source']: pin(Path(item['source']).read_bytes())
            for item in report.get('native_products', {}).get('products', [])}
        require(report['assets_after'] == before_assets and
                report['executable_sha256_after'] == report['executable_sha256_before'],
                'unchanged native product and original assets')
        require(report['compiled_public_inputs_after'] == before_sources,
                'unchanged exact compiled/public Source input hash map')
        guard_worlds(before_worlds, report['worlds_after'])
        require(all(report['native_products_after'][item['source']] == item['before']
                    for item in report.get('native_products', {}).get('products', [])),
                'unchanged every captured native product')
    except BaseException as failure:
        report['guard_errors'] = [type(failure).__name__ + ': ' + str(failure)]
        report['status'] = 'FAIL'
        if error is None:
            error = failure
            report['error'] = report['guard_errors'][0]
    for stream in ('stdout', 'stderr'):
        path = output / (stream + '.bin')
        if path.is_file():
            report[stream + '_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if error is not None:
        raise ValueError(str(error) + '; retained ' + str(output)) from error
    print(json.dumps({k: report[k] for k in ('status', 'cases', 'active_batches', 'checks', 'device')}))
    print('Retained ' + str(output))


if __name__ == '__main__':
    main()
