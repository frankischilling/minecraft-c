"""Retain a fresh genuine default Arrow flight control; no old RED replay route."""
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

sys.dont_write_bytecode = True
TIMEOUT = 240
LIMIT = 1024 ** 2
MODES = {'air-first': 1, 'air-multi': 8, 'block': 5, 'victim-frontier': 1}
TARGET = 'mc_test_arrow_natural_flight'
BUNDLE_SHA = '2b2807d63d69b8e30a3ff3292854538821da75da50cf22c2c78366d20de1edef'


def need(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def lines(mode):
    need(mode in MODES, 'Unknown natural flight mode')
    return [f'ARROW_NATURAL_FLIGHT_BEGIN schema=1 mode={mode}',
            '16 achievements', '151 recipes',
            f'ARROW_NATURAL_FLIGHT_CASE {mode} BEGIN',
            f'ARROW_NATURAL_FLIGHT_CASE {mode} END calls={MODES[mode]}',
            'ARROW_NATURAL_FLIGHT_OPEN full-victim-damage/original/render/pickup/rider']


def completed(child, mode, command):
    need(mode in MODES and type(command) is list and len(command) == 3 and
         all(type(item) is str for item in command) and command[1] == mode,
         'Exact selected command required')
    need(type(child.args) is list and child.args == command and
         type(child.returncode) is int and child.returncode == 0 and
         type(child.stdout) is bytes and type(child.stderr) is bytes,
         'Native command/exit/stream types')
    capture = child.capture_report
    need(type(capture) is dict and type(capture.get('command')) is list and
         capture['command'] == command and type(capture.get('exit')) is int and
         capture['exit'] == 0 and capture.get('reaped') is True and
         capture.get('pipe_capture_complete') is True and capture.get('timed_out') is False and
         capture.get('output_overflow') is False and type(capture.get('reader_errors')) is dict and
         capture['reader_errors'] == {}, 'Incomplete native capture')
    for key, expected in [('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                          ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        need(type(capture.get(key)) is int and capture[key] == expected,
             'Wrong actual capture bound/count: ' + key)
    need(len(child.stdout) <= LIMIT and child.stderr == b'' and child.stdout.endswith(b'\n'),
         'Native output extent/stderr/final newline')
    normalized = child.stdout.replace(b'\r\n', b'\n')
    need(b'\r' not in normalized, 'Malformed native newline')
    parts = normalized.split(b'\n')
    need(child.stdout in (b'\n'.join(parts[:-1]) + b'\n',
                          b'\r\n'.join(parts[:-1]) + b'\r\n'), 'Mixed native newline transport')
    expected = [item.encode('ascii') for item in lines(mode)]
    need(len(parts) == 8 and parts[-1] == b'' and parts[:-2] == expected,
         'Wrong exact native mode/banner/control sequence')
    match = re.fullmatch(rb'PASS: ([1-9][0-9]*) actual default Arrow flight checks;mode=' +
                         mode.encode('ascii'), parts[-2])
    need(match is not None, 'Wrong native completion footer')
    checks = int(match.group(1))
    need(1 <= checks <= 2 ** 32 - 1, 'Native check count outside UInt32')
    return checks


def source_successor_valid(value):
    """Admit the exact additive current-source declaration, never old evidence."""
    def valid_pin(item):
        return (type(item) is dict and set(item) == {'bytes', 'sha256'} and
                type(item['bytes']) is int and item['bytes'] >= 0 and
                type(item['sha256']) is str and
                re.fullmatch(r'[0-9a-f]{64}', item['sha256']) is not None)
    if (type(value) is not dict or set(value) !=
            {'schema', 'from_commit', 'previous_metadata', 'role', 'groups', 'current_semantics'}):
        return False
    if (value['schema'] != 'CURRENT_SOURCE_ADMISSION_SUCCESSOR_V1' or
            type(value['from_commit']) is not str or
            re.fullmatch(r'[0-9a-f]{40}', value['from_commit']) is None or
            not valid_pin(value['previous_metadata']) or
            type(value['role']) is not str or not value['role'] or
            type(value['groups']) is not dict or not value['groups']):
        return False
    for name, group in value['groups'].items():
        if (name not in ('sources', 'helpers', 'files') or type(group) is not dict or
                set(group) != {'previous', 'added', 'current_count'} or
                type(group['previous']) is not dict or
                not all(type(path) is str and valid_pin(pin)
                        for path, pin in group['previous'].items()) or
                type(group['added']) is not list or
                not all(type(path) is str for path in group['added']) or
                len(group['added']) != len(set(group['added'])) or
                type(group['current_count']) is not int or
                group['current_count'] < len(group['added'])):
            return False
    semantics = value['current_semantics']
    return (type(semantics) is dict and set(semantics) == {'scope', 'qualification'} and
            all(type(text) is str and bool(text) for text in semantics.values()))


def source_successor_self_test():
    pin_value = {'bytes': 1, 'sha256': '0' * 64}
    good = {'schema': 'CURRENT_SOURCE_ADMISSION_SUCCESSOR_V1', 'from_commit': '0' * 40,
            'previous_metadata': pin_value, 'role': 'Current source admission only',
            'groups': {'sources': {'previous': {'src/old.c': pin_value},
                                   'added': ['src/new.c'], 'current_count': 2}},
            'current_semantics': {'scope': 'Current source only',
                                  'qualification': 'Historical evidence unchanged'}}
    need(source_successor_valid(good), 'Exact successor schema positive')
    bad = [None, {}, dict(good, extra=True), dict(good, schema='old'),
           dict(good, from_commit='0' * 39), dict(good, role=''), dict(good, groups={}),
           dict(good, previous_metadata={'bytes': True, 'sha256': '0' * 64}),
           dict(good, previous_metadata={'bytes': 1, 'sha256': '0' * 63}),
           dict(good, current_semantics={'scope': '', 'qualification': 'old'}),
           dict(good, current_semantics={'scope': 'current', 'qualification': False})]
    for key, value in [('current_count', True), ('added', ['a', 'a']), ('added', [1]),
                       ('previous', {'src/a.c': {'bytes': False, 'sha256': '0' * 64}})]:
        item = copy.deepcopy(good)
        item['groups']['sources'][key] = value
        bad.append(item)
    need(len(bad) == 15 and all(not source_successor_valid(item) for item in bad),
         'Every malformed successor schema refused')
    print('PASS: 1 source-successor schema positive; 15 malformed declarations refused')


def self_test():
    source_successor_self_test()
    positives = refused = 0
    for mode in MODES:
        command = ['actual-native', mode, 'fresh-root-hex']
        text = ('\n'.join(lines(mode) + [
            f'PASS: 1 actual default Arrow flight checks;mode={mode}']) + '\n').encode('ascii')
        good = SimpleNamespace(args=command, returncode=0, stdout=text, stderr=b'',
            capture_report={'command': command, 'exit': 0, 'reaped': True,
                'pipe_capture_complete': True, 'timed_out': False, 'output_overflow': False,
                'reader_errors': {}, 'timeout_seconds': TIMEOUT, 'limit': LIMIT,
                'stdout_bytes': len(text), 'stderr_bytes': 0})
        for ending in (b'\n', b'\r\n'):
            item = copy.deepcopy(good)
            item.stdout = text.replace(b'\n', ending)
            item.capture_report['stdout_bytes'] = len(item.stdout)
            need(completed(item, mode, command) == 1, 'Synthetic protocol positive')
            positives += 1
        mutations = []
        for key, value in [('returncode', False), ('returncode', 1), ('stderr', b'failure'),
                           ('stdout', text[:-1]), ('stdout', text + b'extra\n'),
                           ('stdout', text.replace(b'16 achievements\n', b'')),
                           ('stdout', text.replace(b'PASS: 1 ', b'PASS: 0 ')),
                           ('stdout', text.replace(b'PASS: 1 ', b'PASS: 4294967296 ')),
                           ('stdout', text.replace(b' END calls=', b' END extra='))]:
            item = copy.deepcopy(good)
            setattr(item, key, value)
            item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
            mutations.append(item)
        for key, value in [('exit', False), ('reaped', False), ('pipe_capture_complete', False),
                           ('timed_out', True), ('output_overflow', True), ('reader_errors', {'x': 'lost'}),
                           ('timeout_seconds', False), ('limit', LIMIT + 1), ('stdout_bytes', False),
                           ('stderr_bytes', False), ('command', ['wrong'])]:
            item = copy.deepcopy(good)
            item.capture_report[key] = value
            mutations.append(item)
        for item in mutations:
            try:
                completed(item, mode, command)
            except (ValueError, TypeError, KeyError):
                refused += 1
            else:
                raise ValueError('Synthetic malformed transport admitted')
    need(positives == 8 and refused == 80, 'Every declared synthetic case ran')
    print(f'PASS: {positives} synthetic positives; {refused} transport refusals; no native execution')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=tuple(MODES))
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--project-root', type=Path)
    parser.add_argument('--bundle', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    need(args.mode is not None and args.executable is not None and args.work_dir is not None and
         args.bundle is not None, 'Mode/executable/work-dir/bundle are required')
    root = (args.project_root or Path(__file__).resolve().parents[3]).resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='natural-flight-' + args.mode + '-', dir=args.work_dir.resolve()))
    report = {'status': 'RUNNING', 'mode': args.mode, 'scope': 'New genuine native default flight; '
              'old RED untouched; full victim damage/original/render/pickup/rider OPEN'}
    before = child = capture = native_root = identity = None

    def fail(key, error):
        detail = {'type': type(error).__name__, 'message': str(error)}
        report[key] = detail
        report.setdefault('first_failure', detail)
        report['status'] = 'FAIL'

    def load(name, relative):
        spec = importlib.util.spec_from_file_location(name, root / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    try:
        common = load('natural_flight_actual_common', 'tests/tools/world/test_world_query_controls.py')
        ownership = load('natural_flight_actual_ownership', 'tests/tools/world/test_execution_ownership.py')
        support = load('natural_flight_actual_capture', 'tests/reference/client/statistics/support.py')
        executable = args.executable.resolve(strict=True)
        need(executable.stem == TARGET, 'Wrong natural-flight executable basename; old RED is forbidden')
        common.literal_metadata(executable)
        need(pin(args.bundle)['sha256'] == BUNDLE_SHA, 'Wrong immutable flight bundle')
        bundle = json.loads(args.bundle.read_text(encoding='utf-8'))
        need(set(bundle) == {'schema', 'target', 'required_parent', 'sources',
                             'prospective_source_successor'} and
             source_successor_valid(bundle['prospective_source_successor']) and
             bundle['schema'] == 'ARROW_NATURAL_FLIGHT_BINDINGS_V1' and bundle['target'] == TARGET and
             bundle['required_parent'] == '05a1335fe4992e203398388234f672d3a5f439a4d37751303ae681011b31ccad' and
             type(bundle['sources']) is dict and
             'tests/world/runtime/test_arrow_natural_flight.c' in bundle['sources'], 'Wrong explicit bundle')

        def guarded():
            result = ownership.protected(executable)
            for relative, expected in bundle['sources'].items():
                need(type(relative) is str and not Path(relative).is_absolute(), 'Relative source pin required')
                path = (root / relative).resolve(strict=True)
                need(path.is_relative_to(root) and pin(path) == expected, 'Prospective source mismatch: ' + relative)
                result[str(path)] = expected
            for path in (Path(__file__).resolve(), args.bundle.resolve(strict=True)):
                result[str(path)] = pin(path)
            return result

        before = guarded()
        report['guard_before'] = before
        native_root = out / 'worlds'
        native_root.mkdir()
        metadata = common.literal_metadata(native_root)
        identity = [metadata.st_dev, metadata.st_ino]
        need(common.fixture_files(native_root) == {}, 'Native producer must start from a fresh empty root')
        command = [str(executable), args.mode, str(native_root).encode('utf-8').hex()]
        report['command'] = command
        child = support.run_bounded(command, out, 'native', timeout=TIMEOUT, limit=LIMIT,
                                    env=os.environ.copy())
        capture = child.capture_report
        report['capture'] = capture
        report['checks'] = completed(child, args.mode, command)
        report['world_tick_entry_attempts'] = MODES[args.mode]
        report['status'] = 'PASS'
    except BaseException as error:
        fail('initial_failure', error)
        if getattr(error, 'capture_report', None) is not None:
            capture = error.capture_report
            report['failed_capture'] = capture
    finally:
        # No heavy read/guard or next child while termination is unproved.
        safe = capture is None or capture.get('reaped') is True
        report['termination_proved_or_no_child'] = safe
        if not safe:
            fail('unsafe_boundary', ValueError('Root intervention required; terminal guards deferred'))
        if safe:
            if before is not None:
                try:
                    after = guarded()
                    report['guard_after'] = after
                    need(after == before, 'Source/product/helper/tables/bundle changed')
                    report['guards_unchanged'] = True
                except BaseException as error:
                    fail('terminal_guard_failure', error)
            if native_root is not None and identity is not None:
                try:
                    metadata = common.literal_metadata(native_root)
                    need([metadata.st_dev, metadata.st_ino] == identity, 'Native root replaced')
                    report['native_authored_files_after'] = common.fixture_files(native_root)
                    report['native_root_identity_unchanged'] = True
                except BaseException as error:
                    fail('native_root_guard_failure', error)
            if child is not None:
                try:
                    payloads = {}
                    for leaf in ('native.stdout', 'native.stderr', 'native.capture.json'):
                        path = out / leaf
                        metadata = common.literal_metadata(path)
                        need(metadata.st_size <= LIMIT, 'Retained payload extent')
                        payloads[leaf] = path.read_bytes()
                    common.verify_retained(child, payloads)
                    report['retained_payloads'] = {name: {'bytes': len(raw),
                        'sha256': hashlib.sha256(raw).hexdigest()} for name, raw in payloads.items()}
                    report['retained_capture_verified'] = True
                except BaseException as error:
                    fail('retention_failure', error)
        with (out / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': retained ' + str(out / 'report.json'), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
