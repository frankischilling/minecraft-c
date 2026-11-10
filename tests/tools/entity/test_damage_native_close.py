"""Finite direct close controls; no Source replay or full victim-damage claim."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
MODES = ('sound-pop', 'sound-capture', 'ally-success', 'ally-unreached')
TIMEOUT = 180
LIMIT = 4 * 1024 ** 2
BUNDLE_SHA = '4da5a20fe859e80207dd540d7368d242859b03a7cb99f8a2c50ef65eb9703f0e'
OPEN_LINE = (b'DAMAGE_CLOSE_OPEN ally-pop-fault/ally-capture-fault/consumed-nonOK/'
             b'full-damage741/Source-parity/FPS')


def need(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    raw = Path(path).read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def load(path):
    def pairs(values):
        result = {}
        for key, value in values:
            need(key not in result, 'Duplicate JSON key')
            result[key] = value
        return result
    def invalid(value):
        raise ValueError('Nonfinite JSON: ' + value)
    return json.loads(Path(path).read_bytes(), object_pairs_hook=pairs, parse_constant=invalid)


def same(left, right):
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(same(left[k], right[k]) for k in left)
    if isinstance(left, list):
        return len(left) == len(right) and all(same(a, b) for a, b in zip(left, right))
    return left == right


def guard(bundle):
    for name, expected in bundle['files'].items():
        relative = Path(name)
        need(not relative.is_absolute() and '..' not in relative.parts and '\\' not in name,
             'Closed component binding')
        path = ROOT / relative
        need(path.resolve(strict=True).is_relative_to(ROOT), 'Bound source stays in project')
        need(pin(path) == expected, 'Exact component source/helper changed: ' + name)


def parse_protocol(raw, mode):
    """Check the whole stream; only the local line view is normalized."""
    need(type(raw) is bytes and type(mode) is str and mode in MODES,
         'Typed admitted protocol input')
    if b'\r' in raw:
        pairs = raw.count(b'\r\n')
        need(pairs == raw.count(b'\r') == raw.count(b'\n'),
             'Uniform CRLF protocol; no bare or mixed line endings')
        view, ending = raw.replace(b'\r\n', b'\n'), 'CRLF'
    else:
        view, ending = raw, 'LF'
    lines = view.split(b'\n')
    need(len(lines) == 7 and lines[-1] == b'',
         'Exactly six complete lines and one final newline')
    encoded = mode.encode('ascii')
    callbacks = 1 if mode.startswith('sound-') else 0
    captures = 1 if mode == 'sound-capture' else 0
    expected = [b'DAMAGE_CLOSE_BEGIN schema=1 mode=' + encoded,
                b'16 achievements', b'151 recipes',
                b'DAMAGE_CLOSE_END mode=' + encoded + b' entries=1 primes=1 callbacks=' +
                str(callbacks).encode('ascii') + b' captures=' + str(captures).encode('ascii'),
                OPEN_LINE]
    need(lines[:5] == expected, 'Exact ordered entry/boot/end/OPEN protocol')
    footer = re.fullmatch(rb'PASS: ([1-9][0-9]{0,9}) genuine direct damage-close checks; mode=' +
                          re.escape(encoded), lines[5])
    need(footer is not None, 'Exact positive native success footer')
    checks = int(footer.group(1))
    need(checks <= 4294967295, 'Native unsigned check-count domain')
    return {'mode': mode, 'checks': checks, 'callbacks': callbacks,
            'captures': captures, 'line_ending': ending}


def protocol_self_test():
    """Synthetic byte-only controls; no source guards, helpers or child run."""
    positives = refusals = 0
    for mode in MODES:
        encoded = mode.encode('ascii')
        callbacks = b'1' if mode.startswith('sound-') else b'0'
        captures = b'1' if mode == 'sound-capture' else b'0'
        lines = [b'DAMAGE_CLOSE_BEGIN schema=1 mode=' + encoded,
                 b'16 achievements', b'151 recipes',
                 b'DAMAGE_CLOSE_END mode=' + encoded +
                 b' entries=1 primes=1 callbacks=' + callbacks + b' captures=' + captures,
                 OPEN_LINE,
                 b'PASS: 123 genuine direct damage-close checks; mode=' + encoded]
        good = b'\n'.join(lines) + b'\n'
        for ending in (b'\n', b'\r\n'):
            raw = good.replace(b'\n', ending)
            before = raw
            parsed = parse_protocol(raw, mode)
            need(raw == before and parsed == {'mode': mode, 'checks': 123,
                 'callbacks': int(callbacks), 'captures': int(captures),
                 'line_ending': 'LF' if ending == b'\n' else 'CRLF'},
                 'Pure positive/raw-preservation control')
            positives += 1
        bad = [good[:-1], good + b'\n', good + b'trailing', b'prefix\n' + good,
               good.replace(b'\n', b'\r\n', 1), good.replace(b'\n', b'\r', 1),
               good.replace(b'\n', b'\r\r\n'), good.replace(b'\n', b'\r\n') + b'\n',
               good.replace(b'16 achievements', b'16 achievements\x00'),
               good.replace(b'16 achievements', b'17 achievements'),
               good.replace(b'151 recipes', b'150 recipes'),
               good.replace(b'schema=1', b'schema=2'),
               good.replace(b' entries=1 ', b' entries=2 '),
               good.replace(b' primes=1 ', b' primes=0 '),
               good.replace(b' callbacks=' + callbacks, b' callbacks=2'),
               good.replace(b' captures=' + captures, b' captures=2'),
               good.replace(OPEN_LINE, OPEN_LINE + b' extra'),
               good.replace(b'PASS: 123', b'PASS: 0'),
               good.replace(b'PASS: 123', b'PASS: 0123'),
               good.replace(b'PASS: 123', b'PASS: -1'),
               good.replace(b'PASS: 123', b'PASS: 4294967296'),
               good.replace(b'PASS: 123', b'PASS: 99999999999'),
               good.replace(b'PASS: 123', b'PASS: abc'),
               good.replace(b'PASS: 123', b'PASS: \xff'),
               good.replace(b'mode=' + encoded, b'mode=unknown', 1),
               good.replace(b'mode=' + encoded, b'mode=unknown'),
               good.replace(b'; mode=' + encoded, b'; mode=unknown')]
        for index in range(6):
            bad.append(b'\n'.join(lines[:index] + lines[index + 1:]) + b'\n')
            bad.append(b'\n'.join(lines[:index] + [lines[index]] + lines[index:]) + b'\n')
        for index in range(5):
            swapped = list(lines)
            swapped[index], swapped[index + 1] = swapped[index + 1], swapped[index]
            bad.append(b'\n'.join(swapped) + b'\n')
        for raw in bad:
            before = raw
            try:
                parse_protocol(raw, mode)
            except ValueError:
                need(raw == before, 'Pure refusal preserves raw bytes')
                refusals += 1
            else:
                raise ValueError('Malformed whole protocol admitted')
    for raw, mode in ((b'', MODES[0]), (bytearray(b''), MODES[0]),
                      ('not bytes', MODES[0]), (b'', 'unknown'), (b'', None)):
        try:
            parse_protocol(raw, mode)
        except ValueError:
            refusals += 1
        else:
            raise ValueError('Invalid typed protocol admitted')
    print(json.dumps({'status': 'PASS', 'scope': 'pure synthetic protocol and host-capture records only',
                      'positives': positives, 'refusals': refusals,
                      'returned_capture': capture_boundary_self_test()}), flush=True)
    return 0


def checked_returned_capture(child, command):
    """Only the pinned helper's actual returned CompletedProcess proves safety."""
    need(type(child) is subprocess.CompletedProcess, 'Actual returned child required')
    need(type(child.args) is list and same(child.args, command), 'Actual returned argv')
    capture = child.capture_report
    need(type(capture) is dict and set(capture) == {'command', 'exit', 'timed_out', 'timeout_seconds',
         'pipe_capture_complete', 'reader_errors', 'output_overflow', 'stdout_bytes',
         'stderr_bytes', 'limit', 'reaped'}, 'Closed actual capture')
    need(same(capture['command'], command) and type(child.returncode) is int and
         type(capture['exit']) is int and capture['exit'] == child.returncode and
         type(capture['timeout_seconds']) is int and capture['timeout_seconds'] == TIMEOUT and
         type(capture['limit']) is int and capture['limit'] == LIMIT,
         'Actual command/exit/bounds')
    for key, expected in (('reaped', True), ('pipe_capture_complete', True),
                          ('timed_out', False), ('output_overflow', False)):
        need(type(capture[key]) is bool and capture[key] is expected, 'Complete safe capture')
    need(type(capture['reader_errors']) is dict and not capture['reader_errors'] and
         type(child.stdout) is bytes and type(child.stderr) is bytes and
         type(capture['stdout_bytes']) is int and capture['stdout_bytes'] == len(child.stdout) and
         type(capture['stderr_bytes']) is int and capture['stderr_bytes'] == len(child.stderr),
         'Exact actual streams')
    need(len(child.stdout) <= LIMIT and len(child.stderr) <= LIMIT, 'Returned stream bounds')
    return capture


def exception_boundary_safe(child, launched, safe):
    # Exception diagnostics, including forged .capture_report.reaped, grant no proof.
    return False if child is None and launched else safe


def record_failure(report, row):
    if report['first_failure'] is None:
        report['first_failure'] = row
    else:
        report.setdefault('secondary_failures', []).append(row)
    report['status'] = 'FAIL'


def note_native_outcome(report, child):
    # Run after full returned-capture proof, BEFORE fallible retained reads.
    if child.returncode != 0:
        record_failure(report, {'type': 'NativeExit', 'message': 'Actual native exit was nonzero',
                               'exit': child.returncode, 'stderr_bytes': len(child.stderr)})
    elif child.stderr:
        record_failure(report, {'type': 'NativeStderr', 'message': 'Actual native wrote stderr',
                               'stderr_bytes': len(child.stderr)})


def capture_boundary_self_test():
    """Synthetic host records only; invoke the actual returned-child gate."""
    command = ['synthetic-no-process', 'mode']
    def child(code=0, stderr=b''):
        value = subprocess.CompletedProcess(command, code, b'host bytes', stderr)
        value.capture_report = {'command': list(command), 'exit': code, 'timed_out': False,
            'timeout_seconds': TIMEOUT, 'pipe_capture_complete': True, 'reader_errors': {},
            'output_overflow': False, 'stdout_bytes': len(value.stdout),
            'stderr_bytes': len(stderr), 'limit': LIMIT, 'reaped': True}
        return value
    forged = RuntimeError('Synthetic helper raised without a returned child')
    forged.capture_report = child().capture_report
    forged.args = command
    forged.returncode, forged.stdout, forged.stderr = 0, b'host bytes', b''
    need(not exception_boundary_safe(None, True, False) and
         forged.capture_report['reaped'] is True,
         'Actual no-returned-child exception branch ignores forged reaped metadata')
    incomplete = child()
    incomplete.capture_report['pipe_capture_complete'] = False
    missing = child()
    del missing.capture_report['reader_errors']
    wrong = child()
    wrong.capture_report['exit'] = False
    refused = 0
    for value in (None, forged, incomplete, missing, wrong):
        safe = False
        try:
            checked_returned_capture(value, command)
            safe = True
        except (ValueError, AttributeError, TypeError):
            pass
        need(not safe, 'Unknown/forged/incomplete returned boundary stays unsafe')
        refused += 1
    clean = child()
    checked_returned_capture(clean, command)
    report = {'status': 'RUNNING', 'first_failure': None}
    note_native_outcome(report, clean)
    need(report['first_failure'] is None, 'Complete successful child outcome')
    failed = child(7, b'Actual synthetic Native cause\n')
    checked_returned_capture(failed, command)
    safe = True  # Same placement as main, before fallible retained comparisons.
    report = {'status': 'RUNNING', 'first_failure': None}
    note_native_outcome(report, failed)
    try:
        raise OSError('Synthetic retained-file comparison failed')
    except OSError as error:
        safe = exception_boundary_safe(failed, True, safe)
        record_failure(report, {'type': type(error).__name__, 'message': str(error)})
    need(safe and report['first_failure']['type'] == 'NativeExit' and
         report['first_failure']['exit'] == 7 and report['status'] == 'FAIL' and
         len(report['secondary_failures']) == 1 and
         report['secondary_failures'][0]['type'] == 'OSError',
         'Observed nonzero stays first; retention failure is secondary')
    return {'positives': 2, 'refusals': refused, 'nonzero_retention_first_cause': 'PASS'}


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
        return protocol_self_test()
    need(args.mode is not None and args.executable is not None and args.work_dir is not None,
         'Native mode requires mode/executable/work-dir')
    bundle_path = Path(__file__).with_name('damage_native_close_bundle.json')
    need(pin(bundle_path)['sha256'] == BUNDLE_SHA, 'Exact direct-control bundle')
    bundle = load(bundle_path)
    need(bundle['modes'] == list(MODES), 'Exact four admitted direct modes')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.mode + '-', dir=args.work_dir.resolve()))
    worlds = output / 'worlds'
    worlds.mkdir()
    report = {'status': 'RUNNING', 'mode': args.mode, 'first_failure': None,
              'native_launch_attempted': False, 'unsafe_native_boundary': False,
              'scope': 'direct leaf ownership; ally fault/consumed-nonOK/full741 remain OPEN'}
    executable = args.executable.resolve(strict=True)
    need(executable.name in (bundle['target'], bundle['target'] + '.exe') and executable.is_file(),
         'Actual declared direct-control target file')
    binary = pin(executable)
    child = None
    safe = True
    try:
        guard(bundle)
        support = ROOT / 'tests/reference/client/statistics/support.py'
        spec = importlib.util.spec_from_file_location('damage_close_capture', support)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        command = [str(executable), args.mode, str(worlds).encode('utf-8').hex()]
        report.update(command=command, executable=binary, native_launch_attempted=True)
        safe = False
        child = module.run_bounded(command, output, 'native', timeout=TIMEOUT, limit=LIMIT)
        capture = checked_returned_capture(child, command)
        safe = True  # Full typed returned proof; retained files may now fail independently.
        report.update(exit=child.returncode, capture=capture)
        note_native_outcome(report, child)
        need((output / 'native.stdout').read_bytes() == child.stdout and
             (output / 'native.stderr').read_bytes() == child.stderr and
             same(load(output / 'native.capture.json'), capture), 'Retained actual capture')
        if report['first_failure'] is None:
            report['protocol'] = parse_protocol(child.stdout, args.mode)
            report['status'] = 'PASS'
    except BaseException as error:
        safe = exception_boundary_safe(child, report['native_launch_attempted'], safe)
        record_failure(report, {'type': type(error).__name__, 'message': str(error)})
        if hasattr(error, 'capture_report'):
            report['capture'] = error.capture_report
    finally:
        report['unsafe_native_boundary'] = not safe
        if safe:
            try:
                guard(bundle)
                need(pin(executable) == binary, 'Native executable unchanged')
            except BaseException as error:
                row = {'type': type(error).__name__, 'message': str(error)}
                if report['first_failure'] is None:
                    report['first_failure'] = row
                else:
                    report.setdefault('secondary_failures', []).append(row)
                report['status'] = 'FAIL'
        else:
            report['terminal_guard'] = 'DEFERRED_UNPROVED_CHILD_TERMINATION_ROOT_INTERVENTION_REQUIRED'
        try:
            report['retained'] = {path.name: pin(path) for path in output.glob('native.*')
                                  if path.is_file()} if safe else {}
        except BaseException as error:
            record_failure(report, {'type': type(error).__name__, 'message': str(error)})
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'status': report['status'], 'report': str(output / 'report.json')}), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
