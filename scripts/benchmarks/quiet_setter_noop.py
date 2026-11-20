"""Run one resident AIR quiet-setter no-op scene on fresh authored World files.

Build mc_benchmark_quiet_setter_noop with MC_BUILD_BENCHMARKS=ON, then use:
  python scripts/benchmarks/quiet_setter_noop.py --executable PATH \\
      --artifacts benchmark-runs --chunks 256 --depth 1 --setter block

Seven samples each issue 8,192 real quiet-setter calls. The timed loop includes
coordinate/packed-alias selection, the full setter, output-Boolean reset, status
and checksum folds. Setup, state/diagnostic guards, scope closure and cleanup
are unclocked. This is one current executable and one fresh process. It makes
no revision-speedup, changed-cell, natural-population, original-parity or FPS
claim. The unchanged finite V3 layout guard can reject a different native ABI.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import statistics
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'quiet_setter_provider_helpers', ROOT / 'scripts/benchmarks/provider_cache_scope.py')
BASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BASE)

CALLS, SAMPLES = 8192, 7
SOURCES = tuple(dict.fromkeys((
    'tests/benchmarks/quiet_setter_noop.c', 'tests/benchmarks/quiet_setter_diagnostics.h',
    'scripts/benchmarks/provider_cache_scope.py', *BASE.SOURCES,
    'include/mc/world/runtime/mutations.h', 'src/world/runtime/mutation/world.c',
    'src/world/runtime/mutation/mutation_internal.h', 'src/world/runtime/lighting_internal.h',
)))


def checksum():
    value = 1469598103934665603
    for index in range(CALLS):
        value = ((value ^ index) * 1099511628211) & ((1 << 64) - 1)
    return value


def parse(raw, count, depth, setter):
    BASE.require(count in (8, 256) and depth in (0, 1) and setter in ('block', 'metadata'),
                 'declared resident quiet-setter scene')
    BASE.require(type(raw) is bytes and len(raw) <= BASE.BOUND and raw.endswith(b'\n') and
                 b'\0' not in raw, 'bounded complete ordinary stdout')
    # C text streams use CRLF on Windows and LF on POSIX. Interpret record
    # terminators here only; the retained file and its capture hash stay raw.
    records = raw.replace(b'\r\n', b'\n')
    BASE.require(b'\r' not in records, 'only LF or CRLF record terminators')
    lines = records.decode('utf-8', errors='strict').split('\n')[:-1]
    if lines[:2] == ['16 achievements', '151 recipes']:
        lines = lines[2:]
    BASE.require(len(lines) == SAMPLES + 3, 'one context, seven samples, invariants and footer')
    context = BASE.fields(lines[0], 'QUIET_NOOP_CONTEXT',
                          ('count', 'depth', 'metadata', 'calls', 'samples',
                           'mutation_error_bytes', 'acquisition_error_bytes'))
    BASE.require({key: context[key] for key in ('count', 'depth', 'metadata', 'calls', 'samples')} ==
                 dict(count=count, depth=depth, metadata=int(setter == 'metadata'),
                      calls=CALLS, samples=SAMPLES) and context['mutation_error_bytes'] == 24296 and
                 context['acquisition_error_bytes'] > 0, 'exact requested work and finite layout')
    expected, samples = checksum(), []
    for index, line in enumerate(lines[1:SAMPLES + 1]):
        row = BASE.fields(line, 'QUIET_NOOP_SAMPLE',
                          ('sample', 'calls', 'ticks', 'frequency', 'checksum', 'changes'))
        BASE.require(row['sample'] == index and row['calls'] == CALLS and row['ticks'] > 0 and
                     row['frequency'] > 0 and row['checksum'] == expected and row['changes'] == 0,
                     'ordered raw clock, work, checksum and unchanged-output samples')
        samples.append(row)
    BASE.require(len({row['frequency'] for row in samples}) == 1,
                 'one observed clock frequency per process')
    invariants = BASE.fields(lines[-2], 'QUIET_NOOP_INVARIANTS',
                             ('count', 'depth', 'metadata', 'calls', 'planes', 'checksum',
                              'diagnostic', 'diagnostic_compared_bytes',
                              'diagnostic_excluded_padding', 'diagnostic_named_compared',
                              'diagnostic_unknown_compared', 'diagnostic_layout',
                              'source_state', 'scopes', 'ownership'))
    BASE.require(invariants['count'] == count and invariants['depth'] == depth and
                 invariants['metadata'] == int(setter == 'metadata') and
                 invariants['calls'] == CALLS * SAMPLES and invariants['planes'] > 0 and
                 invariants['checksum'] == expected and invariants['diagnostic'] > 0 and
                 invariants['diagnostic_compared_bytes'] == 22646 and
                 invariants['diagnostic_excluded_padding'] == 1650 and
                 invariants['diagnostic_named_compared'] == 6298 and
                 invariants['diagnostic_unknown_compared'] == 16348 and
                 invariants['diagnostic_layout'] == 8267465906294600077 and
                 all(invariants[key] == 1 for key in ('source_state', 'scopes', 'ownership')),
                 'complete finite Source, diagnostic, scope and ownership guards')
    finish = BASE.fields(lines[-1], 'QUIET_NOOP_FINISH',
                         ('checks', 'cleanup', 'genuine_source_world'))
    BASE.require(finish['checks'] > 0 and finish['cleanup'] == finish['genuine_source_world'] == 1,
                 'checked genuine World cleanup')
    return dict(context=context, samples=samples, invariants=invariants, finish=finish,
                median_ns_per_call=statistics.median(
                    row['ticks'] / row['frequency'] * 1e9 / row['calls'] for row in samples))


def write_report(path, report):
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--chunks', type=int, choices=(8, 256), default=256)
    parser.add_argument('--depth', type=int, choices=(0, 1), default=1)
    parser.add_argument('--setter', choices=('block', 'metadata'), default='block')
    parser.add_argument('--timeout', type=int, choices=range(25, 121), default=120,
                        metavar='25..120', help='Native process limit in seconds (default: 120)')
    options = parser.parse_args(argv)
    executable = options.executable.resolve(strict=True)
    helpers = [ROOT / 'tests/tools/world/test_source_saving.py',
               ROOT / 'tests/reference/client/controller/capture.py']
    paths = list(dict.fromkeys([Path(__file__).resolve(), executable, *helpers,
                               *(ROOT / name for name in SOURCES)]))
    before = {str(path): BASE.pin(path) for path in paths}
    options.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='quiet-setter-noop-', dir=options.artifacts.resolve()))
    report = dict(schema='QUIET_SETTER_NOOP_RUN_V1', status='FAIL', scope=__doc__,
                  chunks=options.chunks, depth=options.depth, setter=options.setter,
                  inputs_before=before,
                  qualification='One current executable, one fresh process. Finite native '
                  'state/layout guards and authored-file continuity are checked. Input hashes '
                  'record continuity, not an executable build proof or revision comparison.')
    path = output / 'report.json'
    write_report(path, report)
    try:
        saving = BASE.load_module('quiet_setter_public_seed', helpers[0])
        capture = BASE.load_module('quiet_setter_public_capture', helpers[1])
        work = output / 'owned'
        report['authored_inputs'] = BASE.source_inputs(work, options.chunks, saving)
        command = [str(executable), str(work), str(options.chunks), str(options.depth),
                   options.setter]
        try:
            report['native'] = capture.capture(command, output, 'native', options.timeout,
                                               working_directory=output)
        finally:
            # A failed process can still change files. Keep this evidence even
            # if capture/success validation raises, without hiding that failure.
            report['authored_files_unchanged'] = False
            try:
                report['files_after'] = BASE.inventory(work)
            except Exception as error:
                report['files_after_error'] = repr(error)
            else:
                try:
                    BASE.check_files(report['authored_inputs'], report['files_after'])
                    report['authored_files_unchanged'] = True
                except Exception as error:
                    report['file_continuity_error'] = repr(error)
        capture.require_success(report['native'], empty_stderr=True)
        BASE.require(report['authored_files_unchanged'], 'post-native authored-file continuity')
        for name in ('stdout', 'stderr'):
            stream = output / report['native'][name + '_file']
            BASE.require(BASE.pin(stream) == dict(bytes=report['native'][name + '_bytes'],
                                                   sha256=report['native'][name + '_sha256']),
                         'native ' + name + ' still matches its complete process capture')
        stream = output / report['native']['stdout_file']
        report['observation'] = parse(stream.read_bytes(), options.chunks, options.depth,
                                       options.setter)
        report['status'] = 'PASS_FIXED_WORK_SEMANTICS'
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        active_error = sys.exc_info()[0] is not None
        after, errors = {}, {}
        for source in paths:
            try:
                after[str(source)] = BASE.pin(source)
            except Exception as error:
                errors[str(source)] = repr(error)
        report.update(inputs_after=after, input_errors=errors, inputs_unchanged=before == after)
        if before != after:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Executable, source or helper changed during the run'
        write_report(path, report)
        if before != after and not active_error:
            raise ValueError(report['continuity_error'])
    print(f'Quiet setter no-op PASS chunks={options.chunks} depth={options.depth} '
          f'setter={options.setter}; report: {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
