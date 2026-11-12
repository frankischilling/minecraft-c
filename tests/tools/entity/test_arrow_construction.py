"""Eight native Arrow constructor, heading and reference component controls.

Real App/World/Session/Player and concrete Living owners; actual constructor
clock entropy remains independent. Seeded heading calls declare their inputs.
No original runtime/capture, Arrow flight, frame, loaded gameplay, allocation
rate, CPU or FPS qualification is supplied by these controls.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[3]
TIMEOUT_SECONDS = 180
STREAM_LIMIT = 2 * 1024 * 1024
FOOTER = rb'Arrow construction: ([1-9][0-9]*) checks;8 native component controls'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, 'Unavailable capture support')
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def protected_inputs(executable, resources):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt',
             ROOT / '.clang-format', ROOT / 'tests/entity/test_arrow_construction.c',
             ROOT / 'tests/client/statistics/test_player_dispatch.c',
             ROOT / 'tests/reference/client/player_statistics/support.py',
             ROOT / 'tests/reference/client/statistics/support.py'}
    for folder in ('src', 'include', 'cmake'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    paths.update(path for path in resources.rglob('*') if path.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return {str(path): digest(path) for path in sorted(paths)}


def completed_native(child):
    capture = child.capture_report
    require(all(type(capture.get(key)) is int for key in
                ('exit', 'timeout_seconds', 'limit', 'stdout_bytes', 'stderr_bytes')),
            'Native capture counts and budgets require actual integers')
    require(type(child.returncode) is int and child.returncode == 0 and
            type(capture.get('exit')) is int and capture['exit'] == child.returncode and
            capture.get('timed_out') is False and capture.get('output_overflow') is False and
            capture.get('pipe_capture_complete') is True and capture.get('reaped') is True and
            capture.get('reader_errors') == {} and
            capture.get('timeout_seconds') == TIMEOUT_SECONDS and
            capture.get('limit') == STREAM_LIMIT and
            capture.get('stdout_bytes') == len(child.stdout) and
            capture.get('stderr_bytes') == len(child.stderr), 'Incomplete bounded native capture')
    require(child.stderr == b'' and child.stdout.endswith(b'\n'),
            'Native controls must complete quietly; retain first failure')
    declared = [b'16 achievements', b'151 recipes']
    declared += [('ARROW_CONSTRUCTION_CASE id=' + str(index)).encode('ascii')
                 for index in range(8)]
    lines = child.stdout.splitlines()
    require(len(lines) == len(declared) + 1 and lines[:-1] == declared and
            re.fullmatch(FOOTER, lines[-1]) is not None,
            'Exact constructor/case/cleanup transport required')
    return int(re.fullmatch(FOOTER, lines[-1]).group(1))


def self_test():
    lines = [b'16 achievements', b'151 recipes']
    lines += [('ARROW_CONSTRUCTION_CASE id=' + str(index)).encode('ascii')
              for index in range(8)]
    lines += [b'Arrow construction: 1 checks;8 native component controls']
    stdout = b'\n'.join(lines) + b'\n'
    capture = {'exit': 0, 'timed_out': False, 'output_overflow': False,
               'pipe_capture_complete': True, 'reaped': True, 'reader_errors': {},
               'timeout_seconds': TIMEOUT_SECONDS, 'limit': STREAM_LIMIT,
               'stdout_bytes': len(stdout), 'stderr_bytes': 0}
    positive = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'', capture_report=capture)
    require(completed_native(positive) == 1, 'Authored parser positive')
    refused = 0

    def reject(child):
        nonlocal refused
        try:
            completed_native(child)
        except (ValueError, KeyError, TypeError, AttributeError):
            refused += 1
            return
        raise ValueError('Corrupt native transport admitted')

    for name, bad in (('exit', 1), ('exit', False), ('timed_out', True),
                      ('output_overflow', True), ('pipe_capture_complete', False),
                      ('reaped', False), ('reader_errors', {'stdout': []}),
                      ('timeout_seconds', 1), ('limit', 0), ('stdout_bytes', 0),
                      ('stderr_bytes', 1), ('stderr_bytes', False), ('stderr_bytes', 0.0),
                      ('stdout_bytes', float(len(stdout))),
                      ('timeout_seconds', float(TIMEOUT_SECONDS)),
                      ('limit', float(STREAM_LIMIT))):
        child = copy.deepcopy(positive); child.capture_report[name] = bad; reject(child)
    for name, bad in (('returncode', 1), ('returncode', False), ('stderr', b'failure\n')):
        child = copy.deepcopy(positive); setattr(child, name, bad)
        child.capture_report['stderr_bytes'] = len(child.stderr); reject(child)
    for changed in (lines[:-1], lines + lines[-1:], lines[:2] + lines[2:3] + lines[2:],
                    [b'unexpected'] + lines, lines[1:], list(reversed(lines)),
                    [lines[1], lines[0], *lines[2:]],
                    [*lines[:2], b'ARROW_CONSTRUCTION_CASE id=1', *lines[3:]],
                    [*lines[:-1], b'Arrow construction: 0 checks;8 native component controls']):
        child = copy.deepcopy(positive); child.stdout = b'\n'.join(changed) + b'\n'
        child.capture_report['stdout_bytes'] = len(child.stdout); reject(child)
    child = copy.deepcopy(positive); child.stdout = child.stdout[:-1]
    child.capture_report['stdout_bytes'] -= 1; reject(child)
    print(f'Arrow construction parser: positive and {refused} corruption refusals')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    require(args.executable is not None and args.work_dir is not None,
            'Native controls require executable and work directory')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='arrow-construction-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    report = {'schema': 'ARROW_CONSTRUCTION_NATIVE_CONTROLS_V1', 'status': 'FAIL',
              'scope': __doc__, 'original_rows': 0, 'native_component_cases': 8,
              'constructor_entropy': 'actual independent default Random clocks',
              'minecraft_frame_execution': False, 'loaded_gameplay': False,
              'performance_qualification': False, 'timeout_seconds': TIMEOUT_SECONDS,
              'capture_cleanup_budgets_seconds': {'reap': 5, 'reader_join_each': 1,
                                                'incomplete_capture_reap': 5},
              'stream_limit_bytes_each': STREAM_LIMIT}
    before = None
    first = None
    try:
        support = module('arrow_construction_statistics',
                         ROOT / 'tests/reference/client/player_statistics/support.py')
        support.resources_at(resources); game.mkdir()
        before = protected_inputs(executable, resources)
        report['inputs_before_sha256'] = before
        environment = os.environ.copy(); environment['ALSOFT_DRIVERS'] = 'null'
        child = support.run([str(executable), str(resources), str(game)], output, 'native',
                            timeout=TIMEOUT_SECONDS, env=environment)
        report['native'] = child.capture_report
        report['fixture_checks'] = completed_native(child)
        report['status'] = 'PASS'
    except BaseException as error:
        first = error
        if getattr(error, 'capture_report', None) is not None:
            report['native'] = error.capture_report
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        if before is not None:
            try:
                after = protected_inputs(executable, resources)
                report['inputs_after_sha256'] = after
                report['unchanged_protected_inputs'] = after == before
                if after != before:
                    report['status'] = 'FAIL'
            except BaseException as error:
                report['status'] = 'FAIL'
                report['guard_error'] = {'type': type(error).__name__, 'message': str(error)}
        report['stream_sha256'] = {name: digest(output / ('native.' + name))
            for name in ('stdout', 'stderr') if (output / ('native.' + name)).is_file()}
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n',
                                           encoding='utf-8', newline='\n')
        if first is None:
            require(report['status'] == 'PASS', 'Protected inputs changed; retain this opportunity')
    print('Arrow construction native controls: PASS; ' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
