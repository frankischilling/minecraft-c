"""Run finite Arrow parent admission, early-failure and first-use regressions."""
from pathlib import Path
import argparse
import copy
import hashlib
import importlib.util
import json
import os
import re
import stat
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TARGET = 'mc_test_arrow_tick_failures'
MODES = ('null-world', 'code-null', 'math-null', 'math-failed', 'math-cold', 'admission-fp')
TIMEOUT, LIMIT = 240, 1024 ** 2
FIELDS = ('returned', 'phase', 'classification', 'native', 'source_exception', 'source_error',
          'child', 'source_pc', 'base_status', 'base_pc', 'environment_status',
          'environment_cause', 'environment_pc', 'material_status', 'math_status',
          'ownership_status', 'begins', 'published', 'preallocation_refusals')
SIGNED = ('base_status', 'base_pc', 'environment_status', 'environment_cause',
          'environment_pc', 'material_status', 'math_status')
OPEN = 'ARROW_TICK_FAILURES_OPEN allocation-failure/diagnostic-oom/active-reentry/full-victim'


def need(ok, message):
    if not ok:
        raise ValueError(message)


def pin(path):
    raw = Path(path).read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def module(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def checked_returned_capture(child, command):
    """Only an actual returned complete child grants the post-launch read boundary."""
    need(type(child) is subprocess.CompletedProcess, 'Actual returned child required')
    need(type(command) is list and all(type(x) is str for x in command) and
         type(child.args) is list and child.args == command, 'Actual returned argv')
    capture = child.capture_report
    need(type(capture) is dict and set(capture) == {'command', 'exit', 'timed_out', 'timeout_seconds',
         'pipe_capture_complete', 'reader_errors', 'output_overflow', 'stdout_bytes',
         'stderr_bytes', 'limit', 'reaped'}, 'Closed actual capture')
    need(type(capture['command']) is list and all(type(x) is str for x in capture['command']) and
         capture['command'] == command and type(child.returncode) is int and
         type(capture['exit']) is int and capture['exit'] == child.returncode, 'Actual command/exit types')
    for key, expected in (('reaped', True), ('pipe_capture_complete', True),
                          ('timed_out', False), ('output_overflow', False)):
        need(type(capture[key]) is bool and capture[key] is expected, 'Complete returned capture')
    need(type(capture['reader_errors']) is dict and not capture['reader_errors'] and
         type(child.stdout) is bytes and type(child.stderr) is bytes, 'Complete actual byte streams')
    for key, expected in (('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                          ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))):
        need(type(capture[key]) is int and capture[key] == expected, 'Exact typed capture bound/count')
    need(len(child.stdout) <= LIMIT and len(child.stderr) <= LIMIT, 'Bounded actual streams')
    return capture


def counters(mode):
    return (5, 1, 4) if mode == 'admission-fp' else (1, 1, 0)


def completed(child, mode, command):
    need(mode in MODES and len(command) == 3 and command[1] == mode, 'Exact selected mode/command')
    checked_returned_capture(child, command)
    need(child.returncode == 0 and child.stderr == b'' and child.stdout.endswith(b'\n'),
         'Actual zero exit, quiet stderr and final newline')
    raw = child.stdout
    normalized = raw.replace(b'\r\n', b'\n')
    need(b'\r' not in normalized and raw in (normalized, normalized.replace(b'\n', b'\r\n')),
         'Uniform LF or CRLF transport')
    rows = normalized.decode('ascii').splitlines()
    need(len(rows) == 8 and rows[:4] == [f'ARROW_TICK_FAILURES_BEGIN schema=1 mode={mode}',
         '16 achievements', '151 recipes', f'ARROW_TICK_FAILURES_CASE {mode} BEGIN'],
         'Exact complete mode/bootstrap prefix')
    pattern = 'ARROW_TICK_FAILURES_OBSERVATION mode=' + re.escape(mode) + ''.join(
        ' ' + name + r'=(-?(?:0|[1-9][0-9]*))' for name in FIELDS)
    match = re.fullmatch(pattern, rows[4])
    need(match is not None and all(str(int(x)) == x for x in match.groups()),
         'Exact ordered canonical observation fields')
    observation = {name: int(value) for name, value in zip(FIELDS, match.groups())}
    for name, value in observation.items():
        need((-1 <= value <= 2 ** 31 - 1) if name in SIGNED else (0 <= value <= 2 ** 32 - 1),
             'Observation integer domain: ' + name)
    need(all(observation[name] in (0, 1) for name in ('native', 'source_exception', 'source_error')),
         'Typed observation flags')
    expected_counts = counters(mode)
    need(tuple(observation[name] for name in ('begins', 'published', 'preallocation_refusals')) == expected_counts,
         'Exact per-mode C API entry/publication/admission counts')
    need(rows[5] == f'ARROW_TICK_FAILURES_CASE {mode} END begins={expected_counts[0]} '
         f'published={expected_counts[1]} preallocation_refusals={expected_counts[2]}' and rows[6] == OPEN,
         'Exact complete end counters and scope')
    footer = re.fullmatch(r'PASS: ([1-9][0-9]*) Arrow tick failure checks;mode=' + re.escape(mode), rows[7])
    need(footer is not None and 1 <= int(footer[1]) <= 2 ** 32 - 1, 'Positive UInt32 check footer')
    return {'checks': int(footer[1]), 'observation': observation, 'stdout_lines': rows}


def self_test():
    positives = refusals = 0
    for mode in MODES:
        command = ['synthetic-no-process', mode, 'fresh-root-hex']
        values = {name: -1 if name in SIGNED else 0 for name in FIELDS}
        values.update(zip(('begins', 'published', 'preallocation_refusals'), counters(mode)))
        text = '\n'.join([f'ARROW_TICK_FAILURES_BEGIN schema=1 mode={mode}', '16 achievements',
            '151 recipes', f'ARROW_TICK_FAILURES_CASE {mode} BEGIN',
            'ARROW_TICK_FAILURES_OBSERVATION mode=' + mode + ''.join(f' {name}={values[name]}' for name in FIELDS),
            f'ARROW_TICK_FAILURES_CASE {mode} END begins={counters(mode)[0]} published=1 preallocation_refusals={counters(mode)[2]}',
            OPEN, f'PASS: 1 Arrow tick failure checks;mode={mode}']) + '\n'
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
            good.stdout.replace(b'16 achievements\n151 recipes', b'151 recipes\n16 achievements'),
            good.stdout.replace(b' returned=0', b''),
            good.stdout.replace(b' returned=0', b' returned=0 returned=0'),
            good.stdout.replace(b'returned=0 phase=0', b'phase=0 returned=0'),
            good.stdout.replace(b' returned=0', b' returned=00'),
            good.stdout.replace(b' returned=0', b' returned=-1'),
            good.stdout.replace(b' returned=0', b' returned=4294967296'),
            good.stdout.replace(b' base_status=-1', b' base_status=-2'),
            good.stdout.replace(b' base_status=-1', b' base_status=2147483648'),
            good.stdout.replace(b' native=0', b' native=2'),
            good.stdout.replace(b' published=1', b' published=0', 1),
            good.stdout.replace(b' END begins=', b' END entries='),
            good.stdout.replace(b'PASS: 1 ', b'PASS: 0 '),
            good.stdout.replace(b'PASS: 1 ', b'PASS: 4294967296 '),
            good.stdout.replace(b'active-reentry/full-victim', b'work-poison/performance'),
            good.stdout.replace(b'\n', b'\r\n', 1)]
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
                           ('timed_out', True), ('output_overflow', True), ('reader_errors', {'x':'lost'}),
                           ('timeout_seconds', False), ('limit', LIMIT+1), ('stdout_bytes', False),
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
    # A raised exception's reaped envelope is never an actual returned child.
    error = RuntimeError('Synthetic helper raised without returning')
    error.capture_report = {'reaped': True}
    try:
        checked_returned_capture(error, ['synthetic-no-process'])
    except ValueError:
        refusals += 1
    else:
        raise ValueError('Exception envelope granted a returned-child boundary')
    need(positives == 12 and refusals == 205, 'Every declared synthetic protocol case ran')
    print(f'PASS: {positives} synthetic positives; {refusals} transport refusals; no native execution')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=MODES)
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
    out = Path(tempfile.mkdtemp(prefix=args.mode+'-', dir=args.work_dir.resolve()))
    report = {'status':'RUNNING', 'mode':args.mode, 'first_failure':None, 'secondary_failures':[],
        'native_launch_attempted':False, 'unsafe_native_boundary':False,
        'scope':'Finite native Arrow parent regression assertions; no original-method or gameplay parity claim'}
    child = before = native_root = identity = None
    safe = True
    def fail(error):
        cause = {'type':type(error).__name__, 'message':str(error)}
        if report['first_failure'] is None:
            report['first_failure'] = cause
        else:
            report['secondary_failures'].append(cause)
        report['status'] = 'FAIL'
    try:
        common = module('arrow_tick_failure_files', 'tests/tools/world/test_world_query_controls.py')
        support = module('arrow_tick_failure_capture', 'tests/reference/client/statistics/support.py')
        executable = args.executable.resolve(strict=True)
        need(executable.stem == TARGET and executable.is_file(), 'Actual regression executable basename required')
        common.literal_metadata(executable)
        def guarded():
            selected = [executable, Path(__file__), ROOT/'tests/world/runtime/test_arrow_tick_failures.c',
                ROOT/'tests/world/runtime/test_arrow_natural_flight.c', ROOT/'tests/world/runtime/test_entity_tick.c',
                ROOT/'tests/tools/world/test_world_query_controls.py', ROOT/'tests/reference/client/statistics/support.py']
            selected += list(executable.parent.glob('*.dll'))
            for path in selected:
                common.literal_metadata(path)
            return {str(path):pin(path) for path in selected}
        before = guarded()
        report['selected_inputs_before'] = before
        native_root = out/'worlds'
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
        if type(child) is subprocess.CompletedProcess and type(child.returncode) is int and child.returncode != 0:
            fail(RuntimeError('Actual native exit ' + str(child.returncode)))
        report['capture'] = checked_returned_capture(child, command)
        safe = True
        report['exit'] = child.returncode
        if report['first_failure'] is None:
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
                    for leaf in ('native.stdout','native.stderr','native.capture.json'):
                        path = out/leaf
                        metadata = common.literal_metadata(path)
                        need(stat.S_ISREG(metadata.st_mode) and metadata.st_size <= LIMIT, 'Regular bounded retained payload')
                        payloads[leaf] = path.read_bytes()
                    common.verify_retained(child, payloads)
                    report['retained_payloads'] = {name:{'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
                                                   for name,raw in payloads.items()}
                    report['retained_capture_verified'] = True
                except BaseException as error:
                    fail(error)
        else:
            report['terminal_guard'] = 'DEFERRED_UNPROVED_RETURN_NO_SOURCE_PRODUCT_OR_ROOT_SCANS'
        report['unsafe_native_boundary'] = not safe
        if (report['first_failure'] is None and report.get('checks',0) > 0 and
                report.get('selected_inputs_unchanged') and report.get('native_root_identity_unchanged') and
                report.get('retained_capture_verified')):
            report['status'] = 'PASS'
        with (out/'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2)+'\n')
    print(report['status'] + ': retained ' + str(out/'report.json'), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
