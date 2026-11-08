"""Eight finite native Arrow NBT semantic and storage controls.

Genuine App/Session/World/Arrow/Skeleton/Wolf/Ghast owners, immutable declared
inputs, independent constructor entropy, complete supported SN/SL read/write,
named save, factory, provenance/unknown overlay, and a real legacy GAME Chunk
load/adoption/write. This supplies no original rows, Java mutable Map identity,
Map-present-null/null-Map/null-String payload representation, Java writer OOM,
source Store qualification, Arrow flight/render/pickup, frame, loaded gameplay,
allocation-rate, CPU or FPS qualification.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import tempfile
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[3]
CASE_COUNT = 8
TIMEOUT_SECONDS = 180
STREAM_LIMIT = 2 * 1024 * 1024
FOOTER = rb'Arrow NBT: ([1-9][0-9]*) checks;8 native semantic and storage controls'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, 'Unavailable bounded capture support')
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
             ROOT / '.clang-format', ROOT / 'tests/entity/test_arrow_nbt.c',
             ROOT / 'tests/entity/test_arrow_construction.c',
             ROOT / 'tests/client/statistics/test_player_dispatch.c',
             ROOT / 'tests/reference/client/player_statistics/support.py',
             ROOT / 'tests/reference/client/statistics/support.py'}
    for folder in ('src', 'include', 'cmake'):
        paths.update(path for path in (ROOT / folder).rglob('*') if path.is_file())
    paths.update(path for path in resources.rglob('*') if path.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return {str(path): digest(path) for path in sorted(paths)}


def declared_lines():
    return [b'16 achievements', b'151 recipes'] + [
        ('ARROW_NBT_CASE id=' + str(index)).encode('ascii') for index in range(CASE_COUNT)]


def completed_native(child):
    capture = child.capture_report
    require(type(child.returncode) is int and child.returncode == 0 and
            type(capture.get('exit')) is int and capture['exit'] == child.returncode and
            capture.get('timed_out') is False and capture.get('output_overflow') is False and
            capture.get('pipe_capture_complete') is True and capture.get('reaped') is True and
            capture.get('reader_errors') == {} and
            type(capture.get('timeout_seconds')) is int and
            capture['timeout_seconds'] == TIMEOUT_SECONDS and
            type(capture.get('limit')) is int and capture['limit'] == STREAM_LIMIT and
            type(capture.get('stdout_bytes')) is int and
            capture['stdout_bytes'] == len(child.stdout) and
            type(capture.get('stderr_bytes')) is int and
            capture['stderr_bytes'] == len(child.stderr), 'Incomplete bounded native capture')
    require(type(child.stdout) is bytes and type(child.stderr) is bytes and child.stderr == b'' and
            child.stdout.endswith(b'\n'), 'Native controls must complete quietly; retain first failure')
    lines = child.stdout.splitlines()
    declared = declared_lines()
    require(len(lines) == len(declared) + 1 and lines[:-1] == declared and
            re.fullmatch(FOOTER, lines[-1]) is not None,
            'Exact constructor/case/cleanup transport required')
    return int(re.fullmatch(FOOTER, lines[-1]).group(1))


def self_test():
    lines = declared_lines() + [b'Arrow NBT: 1 checks;8 native semantic and storage controls']
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
                      ('timeout_seconds', 1), ('timeout_seconds', True), ('limit', 0),
                      ('limit', False), ('stdout_bytes', 0), ('stderr_bytes', 1),
                      ('stderr_bytes', False)):
        child = copy.deepcopy(positive)
        child.capture_report[name] = bad
        reject(child)
    for name, bad in (('returncode', 1), ('returncode', False), ('stderr', b'failure\n')):
        child = copy.deepcopy(positive)
        setattr(child, name, bad)
        child.capture_report['stderr_bytes'] = len(child.stderr)
        reject(child)
    for changed in (lines[:-1], lines + lines[-1:], lines[:2] + lines[2:3] + lines[2:],
                    [b'unexpected'] + lines, lines[1:], list(reversed(lines)),
                    [lines[1], lines[0], *lines[2:]],
                    [*lines[:2], b'ARROW_NBT_CASE id=1', *lines[3:]],
                    [*lines[:2], b'ARROW_NBT_CASE id=00', *lines[3:]],
                    [*lines[:-1], b'Arrow NBT: 0 checks;8 native semantic and storage controls'],
                    [*lines[:-1], b'Arrow NBT: 1 checks;7 native semantic and storage controls']):
        child = copy.deepcopy(positive)
        child.stdout = b'\n'.join(changed) + b'\n'
        child.capture_report['stdout_bytes'] = len(child.stdout)
        reject(child)
    child = copy.deepcopy(positive)
    child.stdout = child.stdout[:-1]
    child.capture_report['stdout_bytes'] -= 1
    reject(child)
    print(f'Arrow NBT parser: positive and {refused} corruption refusals')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(args.executable is not None and args.work_dir is not None,
            'Native controls require executable and work directory')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='arrow-nbt-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    report = {'schema': 'ARROW_NBT_NATIVE_CONTROLS_V1', 'status': 'FAIL',
              'scope': __doc__, 'original_rows': 0, 'native_component_cases': CASE_COUNT,
              'constructor_entropy': 'actual independent default Random clocks',
              'actual_store': 'legacy native GAME Store and region transport',
              'minecraft_frame_execution': False, 'loaded_gameplay': False,
              'performance_qualification': False, 'timeout_seconds': TIMEOUT_SECONDS,
              'capture_cleanup_budgets_seconds': {'reap': 5, 'reader_join_each': 1,
                                                'incomplete_capture_reap': 5},
              'stream_limit_bytes_each': STREAM_LIMIT}
    before = None
    first = None
    try:
        support = module('arrow_nbt_statistics',
                         ROOT / 'tests/reference/client/player_statistics/support.py')
        support.resources_at(resources)
        game.mkdir()
        before = protected_inputs(executable, resources)
        report['inputs_before_sha256'] = before
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'
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
    print('Arrow NBT native controls: PASS; ' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
