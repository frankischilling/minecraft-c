"""Actual producer transport controls; no writer encoder implementation oracle."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import select
import subprocess
import time
import uuid

MAX_PLAN = 1024 * 1024
MAX_OUTPUT = 32 * 1024 * 1024


def require(value, reason):
    if not value:
        raise AssertionError(reason)


def pin(path):
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    require((before.st_size, before.st_ino, before.st_mtime_ns) ==
            (after.st_size, after.st_ino, after.st_mtime_ns), 'stable complete control input')
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def write(path, data):
    with path.open('xb') as output:
        output.write(data)


def plan(ops, *, mode='stream', faults=(), case='authored-control'):
    return ('JAVA_WRITER_INPUT_V1\ncase\t' + case + '\nmode\t' + mode + '\ncharset\tUTF-8\n' +
            ''.join(row + '\n' for row in [*faults, *ops])).encode('ascii')


def parse_jsonl(path):
    raw = path.read_bytes()
    require(len(raw) <= MAX_OUTPUT and raw.endswith(b'\n'), 'complete bounded actual JSONL')
    require(b'\r' not in raw, 'observer transport is literal LF, not source newline translation')

    def unique(items):
        result = {}
        for key, value in items:
            require(key not in result, 'no duplicate actual JSON key')
            result[key] = value
        return result

    def constant(value):
        raise AssertionError('nonfinite JSON constant: ' + value)

    return [json.loads(line, object_pairs_hook=unique, parse_constant=constant) for line in raw.splitlines()]


def run_file(executable, directory, raw, expected_exit, *, existing=None, missing=False):
    directory.mkdir()
    source, target = directory / 'input.plan', directory / 'observation.jsonl'
    if not missing:
        write(source, raw)
    if existing is not None:
        write(target, existing)
    before = None if missing else pin(source)
    with (directory / 'stdout').open('xb') as stdout, (directory / 'stderr').open('xb') as stderr:
        result = subprocess.run([str(executable), str(source), str(target)], stdin=subprocess.DEVNULL,
                                stdout=stdout, stderr=stderr, timeout=10, check=False)
    require(result.returncode == expected_exit, 'actual expected transport exit: ' + directory.name)
    require((directory / 'stdout').stat().st_size <= 65536 and (directory / 'stderr').stat().st_size <= 65536,
            'bounded actual control diagnostics')
    require(not (directory / 'stdout').read_bytes(), 'file mode writes no stdout observation')
    if not missing:
        require(pin(source) == before, 'actual plan unchanged')
    if expected_exit != 0:
        require((directory / 'stderr').read_bytes().startswith(b'NATIVE_WRITER_PROBE: '), 'failed producer diagnostic retained')
    if expected_exit == 2:
        require(not target.exists(), 'parser rejected before creating output')
    if missing:
        require(not target.exists(), 'missing input failed before creating output')
    if existing is not None:
        require(target.read_bytes() == existing, 'exclusive output preserved existing actual bytes')
    return {'case': directory.name, 'actual_exit': result.returncode, 'input': before,
            'stdout': pin(directory / 'stdout'), 'stderr': pin(directory / 'stderr'),
            'output': pin(target) if target.exists() else None}, target


def positive(executable, directory, raw, *, gaps=0, unclosed=False):
    record, target = run_file(executable, directory, raw, 0)
    require(not (directory / 'stderr').read_bytes(), 'successful producer stderr empty')
    rows = parse_jsonl(target)
    op_count = sum(line.startswith(b'op\t') for line in raw.splitlines())
    require(len(rows) == op_count + 4 and rows[0]['type'] == 'header' and rows[1]['type'] == 'constructed',
            'actual header/constructed/operation/terminal chronology')
    require(rows[0]['schema'] == 2 and rows[0]['scope'] == 'native-public-Java-writer-input-only-cut' and
            bytes.fromhex(rows[0]['plan_raw_hex']) == raw and rows[0]['plan_bytes'] == len(raw),
            'actual complete consumed plan bytes, independent of producer parsing')
    require(rows[0]['original_native_comparison_qualified'] is False and
            rows[0]['GameSettings_or_whole_client_qualified'] is False, 'no fabricated parity qualification')
    operations = rows[2:-2]
    require([row['ordinal'] for row in operations] == list(range(op_count)) and
            all(row['type'] == 'operation' and row['observer_failure'] is None for row in operations),
            'every actual operation observed without helper failure')
    thread_rows = [row for row in operations if row['outcome'] == 'unpaired-thread-control']
    require(len(thread_rows) == gaps and all(row['public_call_performed'] is False and
            row['native_status'] is None and row['return_value_observed'] is False and row['return_value'] is None
            for row in thread_rows), 'Thread controls remain explicitly unsupported, not simulated')
    terminal, retirement = rows[-2:]
    require(terminal['type'] == 'terminal' and terminal['status'] == 0 and terminal['operations'] == op_count and
            terminal['unpaired_thread_controls'] == gaps and terminal['source_close_inserted_by_helper'] is False and
            terminal['native_retirement_started'] is False and terminal['comparison_qualified'] is False,
            'complete source terminal before separate native retirement, without equality claim')
    require(retirement['type'] == 'native-retirement' and retirement['native_status'] == 0 and
            retirement['native_error']['status'] == 0 and retirement['complete'] is True and
            retirement['writer_owner_released'] is True and retirement['retirement_outputs_observed'] is True and
            retirement['observer_failure'] is None and
            retirement['source_fault_schedule_applied'] is False and retirement['source_call_counts_modified'] is False and
            retirement['callback_observation_complete'] is True and retirement['comparison_qualified'] is False,
            'actual separate successful native retirement and released owner')
    source_events = [event for row in operations for event in row['sink_events']]
    require([event['ordinal'] for event in source_events] == list(range(len(source_events))),
            'complete source callback chronology before retirement')
    callbacks = retirement['callbacks']
    require(retirement['cleanup_close_calls'] == retirement['callback_attempts'] == len(callbacks) and
            all(event['phase'] == 'native-retirement' and event['occurrence'] is None and
                event['native_retirement_occurrence'] == 0 and event['ordinal'] == len(source_events) and
                event['native_status'] == 0 and event['complete'] is True and event['configured_fault'] is None
                for event in callbacks), 'separate actual cleanup callback observations without source occurrences/faults')
    if unclosed:
        require(terminal['final_state']['source_close_reached'] is False and
                terminal['final_state']['sink_closed'] is False and retirement['additional_sink_close'] is True and
                retirement['sink_closed_after'] is True and len(callbacks) == 1 and
                callbacks[0]['sink_closed_before'] is False and callbacks[0]['sink_closed_after'] is True,
                'source cut is retained before actual native close; no source CLOSE invented')
    record['observed_rows'] = len(rows)
    return record


def read_first_row(child, prefix_path):
    """Persist every consumed byte before parsing/continuing, including failures."""
    descriptor = child.stdout.fileno()
    deadline = time.monotonic() + 10
    prefix = bytearray()
    if os.name == 'nt':
        import msvcrt
        from ctypes import wintypes
        api = ctypes.WinDLL('kernel32', use_last_error=True)
        api.PeekNamedPipe.argtypes = (wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                                     ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p)
        api.PeekNamedPipe.restype = wintypes.BOOL
        handle = wintypes.HANDLE(msvcrt.get_osfhandle(descriptor))
    with prefix_path.open('xb') as retained:
        while time.monotonic() < deadline:
            remaining = MAX_OUTPUT - len(prefix)
            require(remaining > 0, 'consumed pipe prefix bound; refuse before another read')
            if os.name == 'nt':
                available = wintypes.DWORD()
                if not api.PeekNamedPipe(handle, None, 0, None, ctypes.byref(available), None):
                    error = ctypes.get_last_error()
                    if error in (109, 232):
                        break
                    raise ctypes.WinError(error)
                ready = available.value
                if not ready:
                    time.sleep(0.005)
                    continue
            else:
                ready = 65536 if select.select([descriptor], [], [], min(0.1, max(0, deadline - time.monotonic())))[0] else 0
                if not ready:
                    continue
            block = os.read(descriptor, min(65536, ready, remaining))
            if not block:
                break
            prefix.extend(block)
            require(retained.write(block) == len(block), 'complete consumed-prefix retention write')
            retained.flush()
            if b'\n' in prefix:
                return bytes(prefix), bytes(prefix).split(b'\n', 1)[0] + b'\n'
    raise AssertionError('actual first observer row missing or timed out')


def retain_child_identity(child, ownership):
    """Linux kill authority comes only from an admitted held PIDFD."""
    if os.name == 'nt':
        ownership['kind'] = 'held-Popen-Windows-process-handle'
        ownership['admitted'] = True
        return
    require(hasattr(os, 'pidfd_open') and hasattr(signal, 'pidfd_send_signal'),
            'Linux held identity required; outer strict owner remains failure backstop')

    def identity():
        raw = Path('/proc') / str(child.pid) / 'stat'
        fields = raw.read_text().rsplit(')', 1)[1].split()
        return {'pid': child.pid, 'parent': int(fields[1]), 'starttime': int(fields[19])}

    before = identity()
    require(before['parent'] == os.getpid(), 'actual direct-child parent before PIDFD admission')
    ownership['kind'] = 'held-Linux-PIDFD'
    ownership['fd'] = os.pidfd_open(child.pid)
    after = identity()
    require(after == before, 'actual stable direct-child identity around held PIDFD')
    ownership.update(identity=after, admitted=True)


def terminate_held_child(child, ownership):
    if os.name == 'nt':
        ownership['signal_attempted'] = True
        child.kill()  # Windows Popen retains the process handle, not a reused numeric PID.
    else:
        require(ownership['admitted'] and ownership['fd'] is not None,
                'no numeric-PID kill fallback; outer held owner must clean unadmitted child')
        ownership['signal_attempted'] = True
        signal.pidfd_send_signal(ownership['fd'], signal.SIGKILL)


def closed_pipe(executable, directory, *, after_header):
    directory.mkdir()
    # Enough actual output to prevent completion inside the pipe before the
    # reader deliberately closes. Only this transport control drops bytes.
    raw = plan(['op\tWRITE\t512\t0041\t-', *(['op\tFLUSH', 'op\tWRITE\t512\t0041\t-'] * 8), 'op\tCLOSE'])
    source = directory / 'input.plan'
    write(source, raw)
    before = pin(source)
    intervened = False
    child, code, failure = None, None, None
    errors = []
    ownership = {'kind': None, 'fd': None, 'admitted': False, 'close_attempted': False,
                 'close_succeeded': False, 'signal_attempted': False}
    read_fd, write_fd = None, None

    def remember(stage, error):
        nonlocal failure
        failure = failure or error
        errors.append({'stage': stage, 'failure': type(error).__name__ + ': ' + str(error)})
    with (directory / 'stderr').open('xb') as stderr:
        try:
            if after_header:
                child = subprocess.Popen([str(executable), str(source), '-'], stdin=subprocess.DEVNULL,
                                         stdout=subprocess.PIPE, stderr=stderr, bufsize=0)
                retain_child_identity(child, ownership)
                prefix, header = read_first_row(child, directory / 'consumed-observer-prefix')
                require(header.endswith(b'\n') and len(header) <= MAX_OUTPUT, 'actual bounded first row before pipe close')
                require(json.loads(header)['type'] == 'header', 'actual producer reached observation output')
            else:
                read_fd, write_fd = os.pipe()
                closing = read_fd
                read_fd = None
                os.close(closing)
                with os.fdopen(write_fd, 'wb', buffering=0) as broken:
                    write_fd = None
                    child = subprocess.Popen([str(executable), str(source), '-'], stdin=subprocess.DEVNULL,
                                             stdout=broken, stderr=stderr)
                    retain_child_identity(child, ownership)
        except BaseException as error:
            remember('creation/header', error)
        finally:
            if child is not None:
                try:
                    if child.stdout is not None:
                        child.stdout.close()
                except BaseException as error:
                    remember('reader-close', error)
                try:
                    code = child.wait(timeout=10)
                except BaseException as error:
                    remember('natural-wait', error)
                    # Every wait exception gets an independent bounded reap
                    # attempt, not just TimeoutExpired. Its failure stays failed.
                    try:
                        code = child.wait(timeout=2)
                    except BaseException as final_error:
                        remember('independent-final-wait', final_error)
                        intervened = True
                        try:
                            terminate_held_child(child, ownership)
                        except ProcessLookupError:
                            errors.append({'stage': 'held-signal', 'failure': 'held original child already exited; status still requires reap'})
                        except BaseException as signal_error:
                            remember('held-signal', signal_error)
                        finally:
                            try:
                                code = child.wait(timeout=2)
                            except BaseException as reap_error:
                                remember('unconditional-post-signal-reap', reap_error)
            for label, descriptor in (('raw-read-fd', read_fd), ('raw-write-fd', write_fd),
                                      ('held-pidfd', ownership['fd'])):
                if descriptor is not None:
                    if label == 'held-pidfd':
                        ownership['close_attempted'] = True
                        ownership['fd'] = None  # Never retry a failed close of a potentially reused descriptor.
                    try:
                        os.close(descriptor)
                        if label == 'held-pidfd':
                            ownership['close_succeeded'] = True
                    except BaseException as close_error:
                        remember(label + '-close', close_error)
            with (directory / 'child-closure.json').open('x', encoding='utf-8') as output:
                json.dump({'actual_exit': code, 'intervention': intervened, 'child_created': child is not None,
                           'pid': None if child is None else child.pid,
                           'held_ownership': ownership, 'errors': errors,
                           'unknown_status_is_success': False, 'outer_strict_owner_backstop_required': True}, output)
                output.write('\n')
    if failure is not None:
        raise failure
    require(not intervened and code == 1, 'checked observer pipe failure is an actual nonzero normal exit')
    require(pin(source) == before and (directory / 'stderr').stat().st_size <= 65536 and
            b'observer' in (directory / 'stderr').read_bytes(), 'bounded explicit observer output failure, input preserved')
    return {'case': directory.name, 'actual_exit': code, 'intervention': intervened, 'input': before,
            'stderr': pin(directory / 'stderr'), 'scope': 'reader deliberately closed; complete later stdout is unobservable and no source outcome qualification follows'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    parent = args.work_dir.resolve(strict=True)
    require(parent.is_dir() and not parent.is_symlink(), 'existing owned work parent')
    if os.name != 'nt':
        require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, 'waitable actual direct child statuses')
    root = parent / ('java-writer-probe-inputs-' + uuid.uuid4().hex)
    root.mkdir()
    report = {'passed': False, 'scope': 'producer transport/admission/observer I/O controls only; no original/native writer equality',
              'cases': [], 'binary_before': pin(executable), 'started_unix_ns': time.time_ns()}
    code = 1
    try:
        good = plan(['op\tWRITE\t1\t0041\t-', 'op\tCLOSE'])
        report['cases'].append(positive(executable, root / 'positive-stream', good))
        unclosed = plan(['op\tFLUSH'])
        report['cases'].append(positive(executable, root / 'positive-unclosed-source-cut', unclosed, unclosed=True))
        gap = plan(['op\tINTERRUPT', 'op\tWRITE\t1\t0041\t-', 'op\tCLEAR_INTERRUPT', 'op\tCLOSE'])
        report['cases'].append(positive(executable, root / 'positive-thread-gap', gap, gaps=2))
        invalid = {
            'missing-LF': good[:-1], 'CRLF': good.replace(b'\n', b'\r\n'), 'NUL': good + b'\0\n',
            'uppercase-hex': good.replace(b'0041', b'00AF'), 'negative-zero': good.replace(b'WRITE\t1', b'WRITE\t-0'),
            'integer-overflow': plan(['op\tWRITE_INT\t2147483648']),
            'expanded-overflow': plan(['op\tWRITE\t32768\t00410042\t-']),
            'unknown-op': plan(['op\tNOT_A_WRITER_OPERATION']), 'empty-ops': plan([]),
            'expected-state-field': plan(['state\ttrouble\ttrue', 'op\tCLOSE']),
            'extra-field': plan(['op\tCLOSE\textra']), 'too-many-ops': plan(['op\tFLUSH'] * 65),
            'print-in-stream': plan(['op\tPRINT_NULL']),
            'duplicate-fault': plan(['op\tCLOSE'], faults=['fault\tclose\t0\tio\t0'] * 2),
            'fault-after-operation': plan(['op\tFLUSH', 'fault\tclose\t0\tio\t0']),
            'fault-nonwrite-prefix': plan(['op\tCLOSE'], faults=['fault\tclose\t0\tio\t1']),
            'plan-byte-limit': b'x' * (MAX_PLAN + 1),
        }
        for name, raw in invalid.items():
            if os.name == 'nt' and name == 'NUL':
                record, _ = run_file(executable, root / 'nul-byte', raw, 2)
                record['case'] = name
                record['directory'] = 'nul-byte'
            else:
                record, _ = run_file(executable, root / name, raw, 2)
            report['cases'].append(record)
        record, _ = run_file(executable, root / 'exclusive-existing-output', good, 1, existing=b'AUTHORED PRESERVATION SENTINEL\0\xff')
        report['cases'].append(record)
        record, _ = run_file(executable, root / 'missing-plan', b'', 1, missing=True)
        report['cases'].append(record)
        overflow = plan(['op\tWRITE\t1\t0041\t-', 'op\tFLUSH'], faults=['fault\twrite\t0\tio\t7'])
        record, target = run_file(executable, root / 'runtime-prefix-refusal', overflow, 1)
        rows = parse_jsonl(target)
        require(any(row['type'] == 'observer-failure' and row['bridge_failure_status_may_have_affected_writer'] is True for row in rows)
                and not any(row.get('type') == 'terminal' and row.get('status') == 0 for row in rows),
                'runtime observation bound is failed, not clipped into successful source output')
        report['cases'].append(record)
        report['cases'].append(closed_pipe(executable, root / 'closed-pipe-before-header', after_header=False))
        report['cases'].append(closed_pipe(executable, root / 'closed-pipe-after-header', after_header=True))
        report['binary_after'] = pin(executable)
        require(report['binary_before'] == report['binary_after'], 'actual producer binary unchanged')
        report['passed'], code = True, 0
    except BaseException as failure:
        report['failure'] = type(failure).__name__ + ': ' + str(failure)
    finally:
        try:
            report['binary_after'] = pin(executable)
            require(report['binary_after'] == report['binary_before'], 'independent final producer binary pin')
        except BaseException as failure:
            report['binary_after_failure'] = type(failure).__name__ + ': ' + str(failure)
            report['passed'], code = False, 1
        report['ended_unix_ns'] = time.time_ns()
        with (root / 'report.json').open('x', encoding='utf-8') as output:
            json.dump(report, output, indent=2, sort_keys=True)
            output.write('\n')
        print('PASS: ' + str(len(report['cases'])) + ' native writer producer transport controls' if report['passed'] else
              'FAIL: native writer producer transport control: ' + str(report.get('failure')))
        print('Capture: ' + str(root))
    return code


if __name__ == '__main__':
    raise SystemExit(main())
