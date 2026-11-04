"""Compare fixed-work EW metadata accessors; retain every CPU attempt.

Use matching Release builds with one shared workload object. This measures
65536 actual accessor calls, not scene FPS or original-game parity.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'metadata_cpu_authored_world', Path(__file__).with_name('benchmark_chunk_cache.py'))
SUPPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUPPORT)
KEYS = ('context', 'samples', 'control_rows', 'finish')
CONTROL = re.compile(
    r'EW_METADATA_CPU_CONTROL name=(\S+) status=(\d+) value=(-?\d+) method=e '
    r'descriptor=\(III\)I pc=(\d+) child=(\d+) native=([01]) nested=0 '
    r'operation=(.*?) exact-public-failure=([01])')


def require(value, message):
    if not value:
        raise ValueError(message)


def fields(line):
    pairs = [value.split('=', 1) for value in line.split()[1:]]
    require(all(len(value) == 2 for value in pairs) and len(dict(pairs)) == len(pairs),
            'Complete unique typed fields')
    return dict(pairs)


def unsigned(value):
    number = int(value)
    require(0 <= number < 1 << 64, 'Unsigned 64-bit observation')
    return number


def expected_hashes():
    def fnv(raw):
        value = 1469598103934665603
        for byte in raw:
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
        return value
    points, values = bytearray(), bytearray()
    for i in range(1024):
        cx, cz = (i * 5) % 3, (i * 7 // 3) % 3
        x, z, y = (cx - 1) * 16 + (i * 13 & 15), (cz - 1) * 16 + (i * 11 & 15), 8 + i * 17 % 104
        cell = ((x & 15) << 11) | ((z & 15) << 7) | y
        byte = (cell // 2 * 37 + (cx * 3 + cz) * 13) & 255
        value = (byte >> ((y & 1) * 4)) & 15
        points.extend(struct.pack('<4i', x, y, z, value))
        values.extend(struct.pack('<i', value))
    return fnv(points), fnv(values * 64)


COORDINATES, RESULT = expected_hashes()


def parse_cpu_run(stdout, measure):
    """Parse complete native records; reject missing, extra or reordered output."""
    lines = stdout.splitlines()
    count = 7 if measure else 0
    require(len(lines) == 30 + count and lines[:2] == ['16 achievements', '151 recipes'],
            'Exact bootstrap and complete metadata records')
    controls = lines[2:28]
    cases = [('null-cache', 1, 777, 0, 0, 1, 'ew native accessor arguments'),
             ('null-out', 1, 777, 0, 0, 1, 'ew native accessor arguments'),
             ('null-out-before-y', 1, 777, 0, 0, 1, 'ew native accessor arguments')]
    cases += [(name, 2, 777, 0, 0, 1, 'ew native active/retiring accessor')
              for name in ('active', 'active-before-y', 'retiring')]
    cases += [('data-unread', 0, 4, 54, 0, 0, 'NULL'),
              ('null-nibble', 5, 777, 54, 10, 0, 'actual Chunk null Nibble backing'),
              ('blank-before-null-nibble', 0, 0, 54, 0, 0, 'NULL'),
              ('null-nibble-backing', 5, 777, 54, 10, 0, 'actual Chunk null Nibble backing'),
              ('short-nibble-backing', 6, 777, 54, 10, 0, 'actual Chunk Nibble index'),
              ('last-nibble-even', 0, 4, 54, 0, 0, 'NULL'),
              ('last-nibble-odd', 0, 13, 54, 0, 0, 'NULL'),
              ('null-outer', 5, 777, 41, 0, 0, 'actual ew null outer array'),
              ('y-before-null', 0, 0, 0, 0, 0, 'NULL')]
    cases += [('signed-array-index', 6, 777, pc, 0, 0,
               'actual ew ' + ('outer' if pc == 41 else 'inner') + ' array index')
              for pc in (41, 41, 44, 44, 41, 41, 44, 44)]
    cases += [('y-low', 0, 0, 0, 0, 0, 'NULL'), ('y-high', 0, 0, 0, 0, 0, 'NULL'),
              ('failed-constructor-null-cell', 5, 777, 54, 0, 0, 'actual ew null Chunk receiver')]
    for line, wanted in zip(controls, cases):
        match = CONTROL.fullmatch(line)
        require(match is not None, 'Full metadata control record')
        name, status, value, pc, child, native, operation, failure = match.groups()
        actual = (name, int(status), int(value), int(pc), int(child), int(native), operation)
        require(actual == wanted and int(failure) == (int(status) != 0),
                'Ordered exact metadata control outcome')
    require(lines[28].startswith('EW_METADATA_CPU_CONTEXT '), 'One metadata context')
    context = fields(lines[28])
    require(set(context) == {'adapter', 'success-error', 'error-bytes', 'calls', 'points', 'samples',
                            'chunks', 'planes', 'coordinates', 'public-init-per-call',
                            'public-copy-per-call', 'public-nominal-bytes-per-sample'}, 'Context schema')
    require((context['adapter'], context['success-error']) in
            (('public', 'full'), ('failure', 'untouched')), 'Actual accessor success-error contract')
    numbers = {key: unsigned(value) for key, value in context.items()
               if key not in ('adapter', 'success-error')}
    require((numbers['calls'], numbers['points'], numbers['samples'], numbers['chunks']) ==
            (65536, 1024, count, 9) and numbers['coordinates'] == COORDINATES and
            numbers['error-bytes'] > 0 and numbers['planes'] > 0 and
            numbers['public-init-per-call'] == numbers['public-copy-per-call'] == 1 and
            numbers['public-nominal-bytes-per-sample'] == 2 * 65536 * numbers['error-bytes'],
            'Declared actual work and full public error size')
    samples = []
    for index, line in enumerate(lines[29:-1]):
        require(line.startswith('EW_METADATA_CPU_SAMPLE '), 'Ordered sample record')
        row = fields(line)
        require(set(row) == {'sample', 'ticks', 'frequency', 'calls', 'result', 'planes',
                             'owner-same', 'world-same', 'random-same', 'holds-same',
                             'acquisition-gets'}, 'Sample schema')
        values = {key: unsigned(value) for key, value in row.items()}
        require(values['sample'] == index and values['calls'] == 65536 and
                values['ticks'] > 0 and values['frequency'] > 0 and
                values['result'] == RESULT and values['planes'] == numbers['planes'] and
                all(values[key] == 1 for key in ('owner-same', 'world-same', 'random-same', 'holds-same')) and
                values['acquisition-gets'] == 0, 'Exact values, ownership, Random and zero acquisition')
        samples.append(row)
    require(re.fullmatch(r'EW_METADATA_CPU_FINISH checks=[1-9][0-9]* cleanup=1 '
                         r'genuine-world=1 genuine-ew=1 gl=0', lines[-1]), 'Complete cleanup')
    return dict(context=context, samples=samples, control_rows=controls, finish=lines[-1])


def compare_cpu_runs(profiles):
    """Compare semantic work identities without pooling or changing timings."""
    require(profiles, 'At least two complete accessor profiles')
    normalized, adapters = [], set()
    for profile in profiles:
        row = {key: profile[key] for key in KEYS}
        wire = ['16 achievements', '151 recipes', *row['control_rows'],
                'EW_METADATA_CPU_CONTEXT ' + ' '.join(f'{k}={v}' for k, v in row['context'].items())]
        wire += ['EW_METADATA_CPU_SAMPLE ' + ' '.join(f'{k}={v}' for k, v in sample.items())
                 for sample in row['samples']]
        require(parse_cpu_run('\n'.join([*wire, row['finish']]), bool(row['samples'])) == row,
                'Every compared profile must retain its full parsed contract')
        context = row['context']
        adapters.add(context['adapter'])
        normalized.append({'context': {k: v for k, v in context.items()
                                       if k not in ('adapter', 'success-error')},
                           'control_rows': row['control_rows'], 'finish': row['finish'],
                           'samples': [{k: v for k, v in sample.items()
                                        if k not in ('ticks', 'frequency')} for sample in row['samples']]})
    require(adapters == {'public', 'failure'} and all(row == normalized[0] for row in normalized),
            'Both accessors must retain identical values, failures, work and ownership')
    return normalized[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='public=PATH|failure=PATH')
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=4)
    parser.add_argument('--mode', choices=('measure', 'controls'), default='measure')
    args = parser.parse_args()
    pairs = [value.split('=', 1) for value in args.executable]
    require(1 <= args.rounds <= 10 and len(pairs) == 2 and all(len(p) == 2 for p in pairs), 'Two bounded accessor profiles')
    executables = {label: Path(path).resolve(strict=True) for label, path in pairs}
    require(set(executables) == {'public', 'failure'}, 'One public and one failure executable')
    names = ['tests/benchmarks/chunk_cache_metadata' + suffix for suffix in
             ('.c', '_adapter.h', '_public.c', '_failure.c')]
    names += ['tests/benchmarks/chunk_cache_error_equal.inc', 'tests/benchmarks/world_queries.c',
              'tests/tools/world/benchmark_chunk_cache.py', 'tests/tools/world/test_source_saving.py',
              'tests/world/runtime/saving/mob_nbt_source_support.inc',
              'tests/world/runtime/tile_entities/test_tile_entities.c',
              'cmake/tests/source_world_saving.cmake', 'CMakeLists.txt']
    inputs = {Path(__file__).resolve(), *executables.values(), *(ROOT / name for name in names)}
    inputs.update(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                  if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs.update(path for exe in executables.values() for path in exe.parent.glob('*.dll'))
    before = {str(path): SUPPORT.digest(path) for path in sorted(inputs)}
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ew-metadata-', dir=args.artifacts.resolve()))
    report = {'status': 'FAIL', 'mode': args.mode, 'rounds': args.rounds, 'processes': [],
              'inputs_before': before, 'scope': __doc__, 'input_hash_scope': 'Execution continuity, not binary build provenance.',
              'host': {'platform': platform.platform(), 'machine': platform.machine(),
                       'processor': os.environ.get('PROCESSOR_IDENTIFIER', platform.processor())}}
    saving = SUPPORT.load('metadata_cpu_saved_inputs', ROOT / 'tests/tools/world/test_source_saving.py')
    try:
        for round_index in range(args.rounds):
            for label in (('public', 'failure') if round_index % 2 == 0 else ('failure', 'public')):
                folder = output / f'{round_index}-{label}'
                folder.mkdir()
                process = {'round': round_index, 'label': label, 'status': 'FAIL'}
                report['processes'].append(process)
                try:
                    process['authored_inputs'] = SUPPORT.author(folder, saving)
                    command = [str(executables[label]), str(folder), args.mode]
                    process['command'] = command
                    started = time.monotonic()
                    try:
                        result = subprocess.run(command, cwd=folder, capture_output=True, timeout=180)
                        stdout, stderr, code = result.stdout, result.stderr, result.returncode
                    except subprocess.TimeoutExpired as error:
                        stdout, stderr, code = error.stdout or b'', error.stderr or b'', None
                        process['timeout'] = True
                    except OSError as error:
                        stdout, stderr, code = b'', b'', None
                        process['launch_error'] = str(error)
                    process.update(seconds=time.monotonic() - started, returncode=code)
                    (folder / 'native.stdout').write_bytes(stdout)
                    (folder / 'native.stderr').write_bytes(stderr)
                    require(code == 0 and not stderr, 'Native process failed; exact streams retained')
                    process.update(parse_cpu_run(stdout.decode('ascii'), args.mode == 'measure'))
                    require(process['context']['adapter'] == label, 'Executable adapter identity')
                    after = SUPPORT.inventory(folder / 'world')
                    authored = process['authored_inputs']
                    require(set(after) == set(authored) | {'session.lock'} and
                            all(after[key] == value for key, value in authored.items()) and
                            after['session.lock']['bytes'] == 8, 'Authored save bytes changed')
                    process.update(status='PASS', authored_inputs_unchanged=True)
                except Exception as error:
                    process['error'] = {'type': type(error).__name__, 'message': str(error)}
                (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        require(all(p['status'] == 'PASS' for p in report['processes']), 'A retained process failed')
        report['semantic_identity'] = compare_cpu_runs(report['processes'])
        report.update(status='PASS', summary={label: SUPPORT.summarize(report['processes'], label)
                                            for label in executables})
    finally:
        report['inputs_after'] = {str(path): SUPPORT.digest(path) for path in sorted(inputs)}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'status': report['status'], 'report': str(output / 'report.json')}), flush=True)
    require(report['status'] == 'PASS', 'Qualified metadata CPU comparison required')


if __name__ == '__main__':
    main()
