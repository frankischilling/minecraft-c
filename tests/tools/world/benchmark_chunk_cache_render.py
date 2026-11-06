"""Compare fixed-work EW render getters and retain every CPU attempt.

Matching Release binaries must use one shared workload object. Measurements
compare public complete diagnostics with internal failure-only diagnostics;
they do not measure scene FPS or establish new original-game parity.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ('brightness', 'min_brightness', 'material', 'opaque', 'normal_cube', 'manager')
KEYS = ('context', 'modes', 'samples', 'finish')


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


def parse_cpu_run(stdout, measure):
    """Require complete ordered records and every declared native guard."""
    lines = stdout.splitlines()
    count = 7 if measure else 0
    require(len(lines) == 4 + 6 * (1 + count) and
            lines[:2] == ['16 achievements', '151 recipes'], 'Exact complete CPU records')
    require(lines[2].startswith('EW_RENDER_CPU_CONTEXT '), 'One context')
    context = fields(lines[2])
    require(set(context) == {'adapter', 'success-error', 'error-bytes', 'prefix-bytes',
                            'registry-bytes', 'calls', 'points', 'samples', 'chunks',
                            'planes', 'coordinates'}, 'Context schema')
    require((context['adapter'], context['success-error']) in
            (('public', 'full'), ('failure', 'untouched')), 'Actual accessor contract')
    numbers = {key: unsigned(value) for key, value in context.items()
               if key not in ('adapter', 'success-error')}
    require((numbers['calls'], numbers['points'], numbers['samples'], numbers['chunks']) ==
            (65536, 1024, count, 9) and numbers['planes'] > 0 and numbers['coordinates'] > 0 and
            0 < numbers['prefix-bytes'] + numbers['registry-bytes'] < numbers['error-bytes'],
            'Fixed work and actual diagnostic sizes')
    modes, samples = [], []
    index = 3
    for mode in MODES:
        require(lines[index].startswith('EW_RENDER_CPU_MODE '), 'Ordered mode record')
        record = fields(lines[index])
        require(set(record) == {'mode', 'expected', 'chunk-a-after'} and record['mode'] == mode,
                'Exact mode schema and order')
        require(unsigned(record['expected']) > 0 and record['chunk-a-after'] in ('0', '1'),
                'Expected output identity and brightness effect')
        modes.append(record)
        index += 1
        for sample in range(count):
            require(lines[index].startswith('EW_RENDER_CPU_SAMPLE '), 'Ordered sample record')
            row = fields(lines[index])
            require(set(row) == {'mode', 'sample', 'ticks', 'frequency', 'calls', 'result',
                                 'planes', 'chunk-a-before', 'chunk-a-after', 'owners-same',
                                 'world-same', 'classes-except-chunk-a-same', 'random-same',
                                 'holds-same', 'acquisition-gets'}, 'Sample schema')
            require(row['mode'] == mode, 'Sample mode')
            values = {key: unsigned(value) for key, value in row.items() if key != 'mode'}
            require(values['sample'] == sample and values['calls'] == 65536 and
                    values['ticks'] > 0 and values['frequency'] > 0 and
                    row['result'] == record['expected'] and row['planes'] == context['planes'] and
                    values['chunk-a-before'] == 0 and row['chunk-a-after'] == record['chunk-a-after'] and
                    all(values[key] == 1 for key in ('owners-same', 'world-same',
                        'classes-except-chunk-a-same', 'random-same', 'holds-same')) and
                    values['acquisition-gets'] == 0, 'Values, work, side effect and native guards')
            samples.append(row)
            index += 1
    require(re.fullmatch(r'EW_RENDER_CPU_FINISH checks=[1-9][0-9]* cleanup=1 '
                         r'genuine-world=1 genuine-ew=1 modes=6 measure=' + str(int(measure)),
                         lines[index]), 'Complete cleanup and measurement mode')
    return dict(context=context, modes=modes, samples=samples, finish=lines[index])


def compare_cpu_runs(profiles):
    """Require identical semantic observations for the complete cohort."""
    require(profiles, 'Complete accessor profiles required')
    normalized, adapters = [], set()
    for profile in profiles:
        row = {key: profile[key] for key in KEYS}
        wire = ['16 achievements', '151 recipes',
                'EW_RENDER_CPU_CONTEXT ' + ' '.join(f'{k}={v}' for k, v in row['context'].items())]
        for mode in MODES:
            record = next(value for value in row['modes'] if value['mode'] == mode)
            wire.append('EW_RENDER_CPU_MODE ' + ' '.join(f'{k}={v}' for k, v in record.items()))
            wire += ['EW_RENDER_CPU_SAMPLE ' + ' '.join(f'{k}={v}' for k, v in value.items())
                     for value in row['samples'] if value['mode'] == mode]
        require(parse_cpu_run('\n'.join([*wire, row['finish']]), bool(row['samples'])) == row,
                'Every compared profile retains its complete parsed contract')
        adapters.add(row['context']['adapter'])
        normalized.append({
            'context': {k: v for k, v in row['context'].items() if k not in ('adapter', 'success-error')},
            'modes': row['modes'], 'finish': row['finish'],
            'samples': [{k: v for k, v in sample.items() if k not in ('ticks', 'frequency')}
                        for sample in row['samples']],
        })
    require(adapters == {'public', 'failure'} and all(row == normalized[0] for row in normalized),
            'Both accessors retain identical values, work, side effects and ownership')
    return normalized[0]


def summarize(profiles, label):
    result = {}
    for mode in MODES:
        rows = [row for process in profiles if process['label'] == label
                for row in process['samples'] if row['mode'] == mode]
        times = [1000 * unsigned(row['ticks']) / unsigned(row['frequency']) for row in rows]
        result[mode] = {'samples': len(times), 'milliseconds': times}
        if times:
            result[mode].update(median_ms=statistics.median(times),
                                median_ns_per_call=statistics.median(times) * 1e6 / 65536)
    return result


def main():
    support_path = ROOT / 'tests/tools/world/benchmark_chunk_cache.py'
    spec = importlib.util.spec_from_file_location('render_cpu_authored_world', support_path)
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='public=PATH|failure=PATH')
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=4)
    parser.add_argument('--mode', choices=('measure', 'controls'), default='measure')
    args = parser.parse_args()
    pairs = [value.split('=', 1) for value in args.executable]
    require(1 <= args.rounds <= 10 and len(pairs) == 2 and all(len(p) == 2 for p in pairs),
            'Two bounded accessor profiles')
    executables = {label: Path(path).resolve(strict=True) for label, path in pairs}
    require(set(executables) == {'public', 'failure'}, 'One public and one failure executable')
    names = ['tests/benchmarks/chunk_cache_render' + suffix
             for suffix in ('.c', '_adapter.h', '_public.c', '_failure.c')]
    names += ['tests/benchmarks/chunk_cache_error_equal.inc', 'tests/benchmarks/world_queries.c',
              'tests/tools/world/benchmark_chunk_cache.py', 'tests/tools/world/test_source_saving.py',
              'tests/world/runtime/saving/mob_nbt_source_support.inc',
              'tests/world/runtime/tile_entities/test_tile_entities.c',
              'cmake/tests/source_world_saving.cmake', 'CMakeLists.txt']
    inputs = {Path(__file__).resolve(), *executables.values(), *(ROOT / name for name in names)}
    inputs.update(path for folder in ('src', 'include') for path in (ROOT / folder).rglob('*')
                  if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs.update(path for exe in executables.values() for path in exe.parent.glob('*.dll'))
    before = {str(path): support.digest(path) for path in sorted(inputs)}
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ew-render-', dir=args.artifacts.resolve()))
    report = {'status': 'FAIL', 'mode': args.mode, 'rounds': args.rounds, 'processes': [],
              'inputs_before': before, 'scope': __doc__,
              'input_hash_scope': 'Execution continuity, not binary build provenance.',
              'host': {'platform': platform.platform(), 'machine': platform.machine(),
                       'processor': os.environ.get('PROCESSOR_IDENTIFIER', platform.processor())}}
    saving = support.load('render_cpu_saved_inputs', ROOT / 'tests/tools/world/test_source_saving.py')
    try:
        for round_index in range(args.rounds):
            for label in (('public', 'failure') if round_index % 2 == 0 else ('failure', 'public')):
                folder = output / f'{round_index}-{label}'
                folder.mkdir()
                process = {'round': round_index, 'label': label, 'status': 'FAIL'}
                report['processes'].append(process)
                try:
                    process['authored_inputs'] = support.author(folder, saving)
                    command = [str(executables[label]), str(folder), args.mode]
                    process['command'] = command
                    started = time.monotonic()
                    try:
                        native = subprocess.run(command, cwd=folder, capture_output=True, timeout=180)
                        stdout, stderr, code = native.stdout, native.stderr, native.returncode
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
                    after = support.inventory(folder / 'world')
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
        report.update(status='PASS', summary={label: summarize(report['processes'], label)
                                            for label in executables})
    finally:
        report['inputs_after'] = {str(path): support.digest(path) for path in sorted(inputs)}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'status': report['status'], 'report': str(output / 'report.json')}), flush=True)
    require(report['status'] == 'PASS', 'Qualified render getter CPU comparison required')


if __name__ == '__main__':
    main()
