"""Complete native inventory calls and passive GL_COMPILE batch observations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
HERE = ROOT / 'tests/reference/client/inventory_geometry'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def f32(value):
    return struct.pack('>f', value).hex()


def f64(value):
    return struct.pack('>d', value).hex()


def commands(profiles):
    """Serialize only declared pre-call inputs, never captured output values."""
    lines = []
    for index, profile in enumerate(profiles):
        require(profile['profile'] == index and profile['cv_constructor'] == 'new cv()',
                'Exact cumulative constructor/profile input order')
        lines.append('PROFILE {} {} {} {}'.format(index, profile['id'], profile['metadata'],
                                                 profile['brightness']))
        for name, value in profile['cv_field_writes'].items():
            kind = 'b' if type(value) is bool else 'i' if type(value) is int else 'f'
            require(kind != 'f' or type(value) is str, 'Typed raw float CV input')
            operand = int(value) if kind == 'b' else value
            lines.append(f'FIELD {name} {kind} {operand}')
        for bounds in profile['bounds_setter_calls']:
            require(len(bounds) == 6, 'Six actual bound-setter operands')
            lines.append('BOUNDS ' + ' '.join(f32(value) for value in bounds))
        for write in profile['texture_field_writes']:
            lines.append('TEXTURE {} {}'.format(write['block_id'], write['bm']))
        source = profile['stair_source_write']
        if source is not None:
            lines.append('STAIR {} {}'.format(source['block_id'],
                                              -1 if source['source_id'] is None else source['source_id']))
        lines.append('TRANSLATION ' + ' '.join(f64(value) for value in profile['nw_translation']))
        require(profile['gl_projection'] == 'identity', 'Actual identity projection input')
        operands = profile['gl_modelview_translation'] + profile['gl_current_color']
        require(len(operands) == 7, 'Actual translate/color operand extents')
        lines.append('GL ' + ' '.join(f32(value) for value in operands))
        lines.append('CALL')
    return '\n'.join(lines) + '\n'


def records(stdout, tag, compare):
    return [json.loads(line[len(tag) + 1:], object_pairs_hook=compare.unique)
            for line in stdout.splitlines() if line.startswith(tag + ' ')]


def source_files(executable):
    selected = {executable, Path(__file__), ROOT / 'tests/client/render/test_inventory_geometry.c',
                ROOT / 'tests/client/render/test_render_blocks_faces.c',
                ROOT / 'cmake/client_inventory_geometry.cmake'}
    selected.update(path for path in HERE.iterdir() if path.is_file())
    for name in ('src/client/render/blocks/inventory.c', 'src/client/render/blocks/faces.c',
                 'src/client/render/blocks/access.c', 'src/client/render_engine/nw.c',
                 'src/client/render_engine/nw.h', 'src/client/render_engine/nw_owner.c',
                 'src/client/display/source_surface.inc', 'src/client/display/source_surface.h',
                 'src/client/display/camera_surface.inc', 'src/block/registry/texture.c',
                 'src/block/registry/rendering.c'):
        selected.add(ROOT / name)
    selected.update(executable.parent.glob('*.dll'))
    return sorted(selected)


def check_bindings(compare):
    provenance = compare.read(HERE / 'provenance.json')
    checked = 0
    for group in ('source_files', 'post_capture_audit_sources', 'fixtures'):
        for relative, expected in provenance[group].items():
            path = (HERE / relative).resolve()
            require(path.is_relative_to(ROOT) and path.is_file(), 'Bound public packet path')
            require(digest(path) == expected, 'Current public packet hash ' + relative)
            checked += 1
    return checked


def compare_returns(inputs, observed, reference, source_inputs, rows, mode):
    require(len(inputs) == len(observed) == len(reference) == len(source_inputs) == 17,
            'Seventeen complete native input/return observations')
    stats = {'calls': 17, 'source_written_nw_fields': 0, 'own_unchanged_nw_fields': 0,
             'last_active_words': 0, 'own_inactive_words': 0,
             'known_current_color_profiles': [], 'unknown_current_color_profiles': [],
             'color_scope': 'Native passive source ledger; unknown direct-array current attributes are not queried or matched.'}
    last = {row['profile']: row for row in rows}
    untouched = {'A', 'B', 'x', 'z'}
    for index, (before, actual, expected, original_before) in enumerate(
            zip(inputs, observed, reference, source_inputs)):
        require(before['profile'] == actual['profile'] == expected['profile'] == index,
                'Actual input/return profile sequence')
        for key in ('id', 'metadata', 'brightness', 'cv', 'bounds', 'texture', 'source_block'):
            require(before[key] == original_before[key], f'Actual declared input{index} {key}')
        require(actual['returned'] == expected['returned'], f'Return/failure outcome{index}')
        for key in ('cv', 'bounds', 'source_block'):
            require(actual[key] == expected[key], f'Complete CPU return{index} {key}')
        a, b, original = actual['nw_after'], before['nw_before'], expected['nw_after']
        require(set(a) == set(b) == set(original), 'Exact NW primitive field extent')
        for key in a:
            if key in untouched or key == 'k' and not a['l']:
                require(a[key] == b[key], f'Native own unwritten NW field{index} {key}')
                stats['own_unchanged_nw_fields'] += 1
            else:
                require(a[key] == original[key], f'Source-written NW field{index} {key}')
                stats['source_written_nw_fields'] += 1
        require(actual['gl_error'] == expected['gl_error'] == 0, 'Actual source GL error query')
        for key in ('modelview', 'projection'):
            require(actual['gl_after'][key] == expected['gl_after'][key],
                    f'Actual reached matrix operands{index} {key}')
            require(before['gl_before'][key] == original_before['gl_before'][key],
                    f'Actual authored matrix operands{index} {key}')
        require(before['gl_before']['color_known'] is True and
                before['gl_before']['color'] == original_before['gl_before']['color'],
                'Fresh explicit actual pre-call color')
        if actual['gl_after']['color_known']:
            require(actual['gl_after']['color'] == expected['gl_after']['color'],
                    f'Known native current color{index}')
            stats['known_current_color_profiles'].append(index)
        else:
            require(mode == 'immediate' and actual['cv']['m'],
                    'Unknown color follows a real direct color-array draw')
            # This checks only the retained last explicit source Color operands.
            # It does not reinterpret the original device query as native state.
            operand = before['brightness'] if before['cv']['b'] else None
            wanted = ([operand, operand, operand, '3f800000'] if operand is not None
                      else before['gl_before']['color'])
            require(actual['gl_after']['color'] == wanted,
                    f'Own reached explicit Color operands remain retained{index}')
            stats['unknown_current_color_profiles'].append(index)
        active = set(last[index]['active_indices']) if actual['returned'] else set()
        require(len(actual['last_words']) == len(before['prior_words']) == 48,
                'Actual native backing snapshot extent')
        for word in range(48):
            if word in active:
                require(actual['last_words'][word] == expected['last_words'][word],
                        f'Native final active word{index}:{word}')
                stats['last_active_words'] += 1
            else:
                require(actual['last_words'][word] == before['prior_words'][word],
                        f'Native own untouched final word{index}:{word}')
                stats['own_inactive_words'] += 1
        if actual['returned']:
            require(actual['failure'] is None, 'No invented failure on successful native call')
        else:
            failure = actual['failure']
            require(expected['exception'] == 'java.lang.NullPointerException' and
                    failure['native_policy'] is False and failure['source_pc'] == 5 and
                    failure['caller_pc'] == 836 and failure['site'] ==
                    {'class': 'ss', 'method': 'a', 'descriptor': '(I)I', 'pc': 5},
                    'Genuine nested SS null failure, not a supplied Java exception')
    require(stats['unknown_current_color_profiles'] == ([11, 12, 13] if mode == 'immediate' else []),
            'Declared native direct-array current-color limitation')
    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--work-dir', required=True, type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='inventory-geometry-', dir=args.work_dir)).resolve()
    compare = module('native_inventory_geometry_compare', HERE / 'compare.py')
    inputs = compare.read(HERE / 'profiles.json')['profiles']
    source = {mode: compare.read(HERE / name) for mode, name in
              (('immediate', 'finite.json'), ('compile', 'compile-finite.json'))}
    bindings = check_bindings(compare)
    command_path = out / 'inputs.commands'
    command_path.write_text(commands(inputs), encoding='ascii', newline='\n')
    selected = source_files(executable) + [command_path]
    before = {str(path): digest(path) for path in selected}
    report = {'status': 'FAIL', 'scope': __doc__, 'source_bindings': bindings,
              'inputs_before': before, 'runs': {},
              'limits': ['Passive submitted batches are not invented native NW draw-entry PC/bounds snapshots.',
                         'Native fresh NW and original Font-warmed NW retain their own prior inactive words.',
                         'Immediate AO profiles retain unknown native current-color state; no matched pixels, whole frame or FPS claim.']}
    actual = {}
    try:
        for mode in ('compile', 'immediate'):
            command = [str(executable), mode, str(command_path)]
            result = subprocess.run(command, cwd=out, capture_output=True, text=True, timeout=100)
            (out / (mode + '.stdout')).write_text(result.stdout, encoding='utf-8', newline='\n')
            (out / (mode + '.stderr')).write_text(result.stderr, encoding='utf-8', newline='\n')
            report['runs'][mode] = {'command': command, 'exit_code': result.returncode}
            require(result.returncode == 0 and not result.stderr,
                    f'Complete actual native {mode} calls: {result.stderr}; artifacts={out}')
            rows = records(result.stdout, 'INVENTORY_ROW', compare)
            entered = records(result.stdout, 'INVENTORY_INPUT', compare)
            returned = records(result.stdout, 'INVENTORY_RETURN', compare)
            finish, = records(result.stdout, 'INVENTORY_FINISH', compare)
            require(finish['mode'] == mode and finish['checks'] > 0 and finish['profiles'] == 17 and
                    finish['returned'] == 16 and finish['failed'] == 1 and finish['faces'] == 216 and
                    finish['cleanup'] is True, 'Complete actual program and owner retirement')
            if mode == 'compile':
                require(finish['batches'] == 216 and finish['words'] == 10368 and
                        finish['inactive_words'] == 2772, 'Actual passive batch and own-inactive totals')
                report['runs'][mode]['geometry'] = compare.compare_native_geometry(rows, source[mode]['rows'])
            else:
                require(not rows and finish['batches'] == finish['words'] == finish['inactive_words'] == 0,
                        'Immediate mode supplies actual state, not invented passive draw buffers')
            report['runs'][mode]['returns'] = compare_returns(entered, returned, source[mode]['returns'],
                                                             source[mode]['inputs'], source[mode]['rows'], mode)
            report['runs'][mode]['finish'] = finish
            actual[mode] = returned
        for compiled, immediate in zip(actual['compile'], actual['immediate']):
            for field in ('profile', 'returned', 'cv', 'bounds', 'source_block', 'nw_after', 'last_words', 'failure'):
                require(compiled[field] == immediate[field], 'Own actual cross-mode CPU state ' + field)
        command = [str(executable), 'admission']
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, timeout=60)
        (out / 'admission.stdout').write_text(result.stdout, encoding='utf-8', newline='\n')
        (out / 'admission.stderr').write_text(result.stderr, encoding='utf-8', newline='\n')
        report['runs']['admission'] = {'command': command, 'exit_code': result.returncode,
                                     'scope': 'Actual nested Stairs dispatch and native bounded cyclic-alias admission; no original StackOverflowError claim.'}
        require(result.returncode == 0 and not result.stderr,
                f'Actual native recursive texture admission: {result.stderr}; artifacts={out}')
        steps = records(result.stdout, 'INVENTORY_ADMISSION_STEP', compare)
        finish, = records(result.stdout, 'INVENTORY_ADMISSION', compare)
        require([step['kind'] for step in steps] == ['nested_success', 'nested_null', 'cycle_native_limit'] and
                [step['returned'] for step in steps] == [True, False, False] and
                [step['depth'] for step in steps] == [0, 3, 16] and
                [step['faces'] for step in steps] == [12, 0, 0] and
                [step['native_policy'] for step in steps] == [False, False, True] and
                [step['batch_open'] for step in steps] == [False, True, True],
                'Actual nested dispatch/failure outcomes and retained batch state')
        require(finish['checks'] > 0 and finish['nested_success'] == finish['nested_null'] ==
                finish['cycle_native_limit'] == 1 and finish['max_frames'] == 16 and finish['cleanup'] is True,
                'Complete actual admission calls and native ownership retirement')
        report['runs']['admission'].update(steps=steps, finish=finish)
        report['status'] = 'PASS'
    finally:
        after = {str(path): digest(path) for path in selected}
        report['inputs_after'] = after
        report['inputs_unchanged'] = before == after
        if before != after:
            report['status'] = 'FAIL'
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    require(report['status'] == 'PASS', 'Complete immutable native comparison')
    print(json.dumps({'status': 'PASS', 'profiles_per_mode': 17, 'batches': 216,
                      'raw_words': 10368, 'active_words': 7596, 'own_inactive_words': 2772,
                      'artifacts': str(out)}))


if __name__ == '__main__':
    main()
