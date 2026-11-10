"""Retain actual default World Arrow water-flight regressions and checked cleanup."""
from pathlib import Path
import argparse
import copy
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TARGET = 'mc_test_arrow_water_flight'
TIMEOUT, LIMIT = 240, 1024 ** 2
FIELDS = ('calls', 'completed', 'callbacks', 'failed_pc', 'failed_bubbles', 'classification')
EXPECTED = {
    'water-first': (1, 1, 4, 0, 0, 0),
    'water-multi': (8, 8, 43, 0, 0, 0),
    'dry-to-water': (2, 1, 27, 0, 0, 0),
    'water-to-air': (2, 2, 4, 0, 0, 0),
    'particle-failure': (1, 0, 3, 1459, 2, 3),
    'null-callback': (1, 0, 1, 1459, 0, 1),
}
OPEN = 'ARROW_WATER_FLIGHT_OPEN original-execution/visible-bubbles/full-victim/pickup/rider'
STATE_FIELDS = ('x', 'y', 'z', 'velocity_x', 'velocity_y', 'velocity_z', 'yaw', 'pitch',
                'previous_yaw', 'previous_pitch', 'min_x', 'min_y', 'min_z', 'max_x', 'max_y',
                'max_z', 'age', 'flight', 'embedded', 'shake', 'wet', 'dead', 'first',
                'random_state', 'random_cached', 'random_gaussian')
EVENT_FIELDS = ('age', 'ordinal', 'sound', 'name', 'x', 'y', 'z', 'velocity_x', 'velocity_y',
                'velocity_z', 'volume', 'pitch', 'wet', 'random_state', 'random_cached',
                'random_gaussian', 'parent_pc', 'completed_bubbles')
SAMPLES = {
    'water-first': ((1, True, 4),),
    'water-multi': tuple((i, i <= 4 or i == 8, 27 if i == 8 else 4 if i <= 4 else 0)
                         for i in range(1, 9)),
    'dry-to-water': ((2, True, 27),),
    'water-to-air': ((1, True, 4), (2, False, 0)),
    'particle-failure': ((1, True, 3),),
    'null-callback': ((1, True, 1),),
}


def need(ok, message):
    if not ok:
        raise ValueError(message)


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def pin(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def observation_rows(rows, mode):
    at = ordinal = 0
    states, events = [], []
    for age, wet, count in SAMPLES[mode]:
        need(at < len(rows), 'Missing actual state row')
        cells = rows[at].split('\t')
        at += 1
        need(len(cells) == len(STATE_FIELDS) + 1 and cells[0] == 'ARROW_WATER_STATE',
             'Complete ordered raw state row')
        state = dict(zip(STATE_FIELDS, cells[1:]))
        for name in STATE_FIELDS[:6] + STATE_FIELDS[10:16] + ('random_state', 'random_gaussian'):
            need(re.fullmatch('[0-9a-f]{16}', state[name]), 'Raw Double/Random word: ' + name)
        for name in STATE_FIELDS[6:10]:
            need(re.fullmatch('[0-9a-f]{8}', state[name]), 'Raw Float word: ' + name)
        for name in ('embedded', 'wet', 'dead', 'first', 'random_cached'):
            need(state[name] in ('true', 'false'), 'Actual typed state flag: ' + name)
        for name in ('age', 'flight', 'shake'):
            need(re.fullmatch(r'0|[1-9][0-9]*', state[name]) and int(state[name]) <= 2 ** 31 - 1,
                 'Canonical nonnegative Int32 state field: ' + name)
        need(state['age'] == str(age) and state['flight'] == str(age) and
             state['wet'] == str(wet).lower(), 'Exact selected tick/water sequence')
        states.append(state)
        for index in range(count):
            need(at < len(rows), 'Missing actual callback row')
            cells = rows[at].split('\t')
            at += 1
            need(len(cells) == len(EVENT_FIELDS) + 1 and cells[0] == 'ARROW_WATER_EVENT',
                 'Complete ordered raw callback row')
            event = dict(zip(EVENT_FIELDS, cells[1:]))
            for name in EVENT_FIELDS[4:10] + ('random_state', 'random_gaussian'):
                need(re.fullmatch('[0-9a-f]{16}', event[name]), 'Raw callback Double/Random word')
            for name in ('volume', 'pitch'):
                need(re.fullmatch('[0-9a-f]{8}', event[name]), 'Raw callback Float word')
            for name in ('sound', 'wet', 'random_cached'):
                need(event[name] in ('true', 'false'), 'Actual typed callback flag')
            base = count == 27 and index < 23
            name = 'random.splash' if base and index == 0 else 'splash' if base and index >= 12 else 'bubble'
            ordinal += 1
            need(event['age'] == str(age) and event['ordinal'] == str(ordinal) and
                 event['name'] == name and event['sound'] == str(base and index == 0).lower() and
                 event['wet'] == str(not base).lower() and
                 event['parent_pc'] == str(1 if base else 1459) and
                 event['completed_bubbles'] == str(0 if base else index - (23 if count == 27 else 0)),
                 'Exact entered Base/tail callback order and parent observations')
            events.append(event)
    need(at == len(rows), 'No extra raw observation rows')
    return states, events


def completed(child, mode, command):
    need(mode in EXPECTED and type(command) is list and len(command) == 3 and
         command[1] == mode, 'Exact selected native mode/command')
    transport = module('arrow_water_returned_transport', 'tests/tools/entity/test_arrow_tick_failures.py')
    transport.checked_returned_capture(child, command)
    need(child.returncode == 0 and child.stderr == b'' and child.stdout.endswith(b'\n'),
         'Actual zero exit, quiet stderr and final newline')
    normalized = child.stdout.replace(b'\r\n', b'\n')
    need(b'\r' not in normalized and
         child.stdout in (normalized, normalized.replace(b'\n', b'\r\n')), 'Uniform LF or CRLF transport')
    rows = normalized.decode('ascii').splitlines()
    need(len(rows) >= 8 and rows[:4] == [f'ARROW_WATER_FLIGHT_BEGIN schema=1 mode={mode}',
         '16 achievements', '151 recipes', f'ARROW_WATER_FLIGHT_CASE {mode} BEGIN'],
         'Exact complete native bootstrap/mode prefix')
    observation = f'ARROW_WATER_FLIGHT_OBSERVATION mode={mode}' + ''.join(
        f' {name}={value}' for name, value in zip(FIELDS, EXPECTED[mode]))
    need(rows[-4] == observation and rows[-3] == f'ARROW_WATER_FLIGHT_CASE {mode} END' and
         rows[-2] == OPEN, 'Exact water callback/completion/failure observations and scope')
    states, events = observation_rows(rows[4:-4], mode)
    footer = re.fullmatch(r'PASS: ([1-9][0-9]*) actual Arrow water flight checks;mode=' +
                          re.escape(mode), rows[-1])
    need(footer is not None and 1 <= int(footer[1]) <= 2 ** 32 - 1, 'Positive UInt32 check footer')
    return {'checks': int(footer[1]), 'observation': dict(zip(FIELDS, EXPECTED[mode])),
            'states': states, 'events': events, 'stdout_lines': rows}


def self_test():
    positives = refusals = 0
    for mode, values in EXPECTED.items():
        command = ['synthetic-no-process', mode, 'fresh-root-hex']
        body = []
        ordinal = 0
        for age, wet, count in SAMPLES[mode]:
            state = {name: '0000000000000000' for name in STATE_FIELDS}
            state.update({name: '00000000' for name in STATE_FIELDS[6:10]})
            state.update({name: 'false' for name in ('embedded', 'dead', 'first', 'random_cached')})
            state.update(age=str(age), flight=str(age), shake='0', wet=str(wet).lower())
            body.append('ARROW_WATER_STATE\t' + '\t'.join(state[name] for name in STATE_FIELDS))
            for index in range(count):
                event = {name: '0000000000000000' for name in EVENT_FIELDS}
                base = count == 27 and index < 23
                ordinal += 1
                event.update(age=str(age), ordinal=str(ordinal), sound=str(base and index == 0).lower(),
                    name='random.splash' if base and index == 0 else 'splash' if base and index >= 12 else 'bubble',
                    volume='00000000', pitch='00000000', wet=str(not base).lower(), random_cached='false',
                    parent_pc=str(1 if base else 1459),
                    completed_bubbles=str(0 if base else index - (23 if count == 27 else 0)))
                body.append('ARROW_WATER_EVENT\t' + '\t'.join(event[name] for name in EVENT_FIELDS))
        text = '\n'.join([f'ARROW_WATER_FLIGHT_BEGIN schema=1 mode={mode}', '16 achievements',
            '151 recipes', f'ARROW_WATER_FLIGHT_CASE {mode} BEGIN'] + body + [
            f'ARROW_WATER_FLIGHT_OBSERVATION mode={mode}' + ''.join(
                f' {name}={value}' for name, value in zip(FIELDS, values)),
            f'ARROW_WATER_FLIGHT_CASE {mode} END', OPEN,
            f'PASS: 1 actual Arrow water flight checks;mode={mode}']) + '\n'
        good = subprocess.CompletedProcess(command, 0, text.encode('ascii'), b'')
        good.capture_report = {'command': list(command), 'exit': 0, 'timed_out': False,
            'timeout_seconds': TIMEOUT, 'pipe_capture_complete': True, 'reader_errors': {},
            'output_overflow': False, 'stdout_bytes': len(good.stdout), 'stderr_bytes': 0,
            'limit': LIMIT, 'reaped': True}
        for ending in (b'\n', b'\r\n'):
            item = copy.deepcopy(good)
            item.stdout = good.stdout.replace(b'\n', ending)
            item.capture_report['stdout_bytes'] = len(item.stdout)
            need(completed(item, mode, command)['checks'] == 1, 'Synthetic transport positive')
            positives += 1
        malformed = [good.stdout[:-1], good.stdout + b'extra\n',
            good.stdout.replace(b'16 achievements\n', b''),
            good.stdout.replace(b' END\n', b' END calls=1\n'),
            good.stdout.replace(b'PASS: 1 ', b'PASS: 0 '),
            good.stdout.replace(b'PASS: 1 ', b'PASS: 4294967296 '),
            good.stdout.replace(b'original-execution/visible-bubbles', b'whole-method/frames'),
            good.stdout.replace(b'\n', b'\r\n', 1),
            good.stdout.replace((body[0] + '\n').encode(), b'', 1),
            good.stdout.replace(b'ARROW_WATER_STATE\t', b'ARROW_WATER_STATE\t0000\t', 1),
            good.stdout.replace(b'ARROW_WATER_STATE\t0000000000000000', b'ARROW_WATER_STATE\tnan', 1),
            good.stdout.replace((body[1] + '\n').encode(), b'', 1),
            good.stdout.replace((body[1] + '\n').encode(), (body[1] + '\n' + body[1] + '\n').encode(), 1)]
        for name, value in zip(FIELDS, values):
            before = f' {name}={value}'.encode('ascii')
            malformed.extend([good.stdout.replace(before, b''),
                good.stdout.replace(before, before + before),
                good.stdout.replace(before, f' {name}={value + 1}'.encode('ascii'))])
        mutations = []
        for raw in malformed:
            item = copy.deepcopy(good)
            item.stdout = raw
            item.capture_report['stdout_bytes'] = len(raw)
            mutations.append(item)
        for key, value in (('returncode', False), ('returncode', 1), ('stderr', b'failure')):
            item = copy.deepcopy(good)
            setattr(item, key, value)
            item.capture_report.update(exit=item.returncode, stderr_bytes=len(item.stderr))
            mutations.append(item)
        for key, value in (('exit', False), ('reaped', False), ('pipe_capture_complete', False),
                           ('timed_out', True), ('output_overflow', True), ('reader_errors', {'x': 'lost'}),
                           ('timeout_seconds', False), ('limit', LIMIT + 1), ('stdout_bytes', False),
                           ('stderr_bytes', False), ('command', ['wrong']), ('extra', 1)):
            item = copy.deepcopy(good)
            item.capture_report[key] = value
            mutations.append(item)
        for item in mutations:
            try:
                completed(item, mode, command)
            except (ValueError, TypeError, KeyError):
                refusals += 1
            else:
                raise ValueError('Synthetic malformed transport admitted')
    need(positives == 12 and refusals == 276, 'Every declared synthetic protocol case ran')
    print(f'PASS: {positives} synthetic positives; {refusals} transport refusals; no native execution')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=EXPECTED)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        need(args.mode is None and args.executable is None and args.work_dir is None,
             'Pure controls admit no native arguments')
        return self_test()
    need(args.mode is not None and args.executable is not None and args.work_dir is not None,
         'Native mode requires mode/executable/work-dir')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=args.mode + '-', dir=args.work_dir.resolve()))
    report = {'status': 'RUNNING', 'mode': args.mode, 'first_failure': None, 'secondary_failures': [],
        'native_launch_attempted': False, 'unsafe_native_boundary': False,
        'scope': 'Finite native default World water assertions derived from original bytecode; '
                 'original execution and visible gameplay remain unqualified'}
    child = before = native_root = identity = None
    safe = True

    def fail(error):
        cause = {'type': type(error).__name__, 'message': str(error)}
        if report['first_failure'] is None:
            report['first_failure'] = cause
        else:
            report['secondary_failures'].append(cause)
        report['status'] = 'FAIL'

    try:
        common = module('arrow_water_files', 'tests/tools/world/test_world_query_controls.py')
        support = module('arrow_water_capture', 'tests/reference/client/statistics/support.py')
        transport = module('arrow_water_capture_check', 'tests/tools/entity/test_arrow_tick_failures.py')
        executable = args.executable.resolve(strict=True)
        need(executable.stem == TARGET and executable.is_file(), 'Actual water regression executable basename')

        def guarded():
            selected = [executable, Path(__file__), ROOT / 'tests/world/runtime/test_arrow_water_flight.c',
                ROOT / 'tests/world/runtime/test_arrow_natural_flight.c', ROOT / 'tests/world/runtime/test_entity_tick.c',
                ROOT / 'tests/tools/entity/test_arrow_tick_failures.py',
                ROOT / 'tests/tools/world/test_world_query_controls.py',
                ROOT / 'tests/reference/client/statistics/support.py']
            selected += list(executable.parent.glob('*.dll'))
            for path in selected:
                common.literal_metadata(path)
            return {str(path): pin(path) for path in selected}

        before = guarded()
        report['selected_inputs_before'] = before
        native_root = out / 'worlds'
        native_root.mkdir()
        metadata = common.literal_metadata(native_root)
        identity = [metadata.st_dev, metadata.st_ino]
        need(common.fixture_files(native_root) == {}, 'Native producer starts from a fresh empty root')
        command = [str(executable), args.mode, str(native_root).encode('utf-8').hex()]
        report['command'] = command
        report['native_launch_attempted'] = True
        safe = False
        child = support.run_bounded(command, out, 'native', timeout=TIMEOUT, limit=LIMIT,
                                    env=os.environ.copy())
        report['capture'] = transport.checked_returned_capture(child, command)
        safe = True
        report.update(completed(child, args.mode, command))
    except BaseException as error:
        fail(error)
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
    finally:
        if safe:
            if before is not None:
                try:
                    report['selected_inputs_after'] = guarded()
                    need(report['selected_inputs_after'] == before, 'Selected test inputs/product changed')
                    report['selected_inputs_unchanged'] = True
                except BaseException as error:
                    fail(error)
            if native_root is not None and identity is not None:
                try:
                    metadata = common.literal_metadata(native_root)
                    need([metadata.st_dev, metadata.st_ino] == identity, 'Native root replaced')
                    report['native_authored_files_after'] = common.fixture_files(native_root)
                    report['native_root_identity_unchanged'] = True
                except BaseException as error:
                    fail(error)
            if child is not None:
                try:
                    payloads = {}
                    for leaf in ('native.stdout', 'native.stderr', 'native.capture.json'):
                        path = out / leaf
                        metadata = common.literal_metadata(path)
                        need(metadata.st_size <= LIMIT, 'Bounded retained payload')
                        payloads[leaf] = path.read_bytes()
                    common.verify_retained(child, payloads)
                    report['retained_payloads'] = {name: {'bytes': len(raw),
                        'sha256': hashlib.sha256(raw).hexdigest()} for name, raw in payloads.items()}
                    report['retained_capture_verified'] = True
                except BaseException as error:
                    fail(error)
        else:
            report['terminal_guard'] = 'DEFERRED_UNPROVED_RETURN_NO_SOURCE_PRODUCT_OR_ROOT_SCANS'
        report['unsafe_native_boundary'] = not safe
        if (report['first_failure'] is None and report.get('checks', 0) > 0 and
                report.get('selected_inputs_unchanged') and report.get('native_root_identity_unchanged') and
                report.get('retained_capture_verified')):
            report['status'] = 'PASS'
        with (out / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': retained ' + str(out / 'report.json'), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
