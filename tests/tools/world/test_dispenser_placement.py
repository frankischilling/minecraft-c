"""Retain bounded native Dispenser orientation and typed-failure CPU controls."""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import subprocess
from types import SimpleNamespace

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TIMEOUT = 150
LIMIT = 4 * 1024**2
PROFILES = {
    'orientation': ('world-dispenser-placement', 'placement', 1),
    'placed-by': ('world-dispenser-placed-by', 'placed-by', 1),
    'registry-failure': ('world-dispenser-registry-failure', 'registry failure', 1),
    'registry-projection': ('inventory-dispenser-registry-projection', 'Registry projection', 0),
}


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(value, message):
    if not value:
        raise ValueError(message)


def completed(child, profile):
    receipt = child.capture_report
    require(type(child.returncode) is int and child.returncode == 0, 'Native exit failed')
    require(type(child.stdout) is bytes and type(child.stderr) is bytes, 'Incomplete streams')
    for key, value in [('exit', 0), ('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                       ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        require(type(receipt.get(key)) is int and receipt[key] == value,
                'Wrong native bound/count: ' + key)
    require(receipt.get('reaped') is True and receipt.get('pipe_capture_complete') is True and
            receipt.get('timed_out') is False and receipt.get('output_overflow') is False and
            receipt.get('reader_errors') == {}, 'Incomplete owned capture')
    require(child.stderr == b'' and child.stdout.endswith(b'\n'), 'Native stderr or partial line')
    lines = child.stdout.splitlines()
    expected = [b'16 achievements', b'151 recipes'] * PROFILES[profile][2]
    require(len(lines) == len(expected) + 1 and lines[:-1] == expected,
            'Unexpected actual Registry bootstrap sequence')
    footer = re.fullmatch(('Dispenser ' + PROFILES[profile][1] +
                           r' CPU controls passed: ([1-9][0-9]*)').encode('ascii'), lines[-1])
    require(footer is not None and int(footer[1]) <= 0xffffffff, 'Missing native completion')
    return int(footer[1])


def capture_owned(support, command, output, environment, execution, factory=None):
    """Keep the actual handle if the shared transport raises during setup/join."""
    handles = []
    original = support.subprocess
    actual = subprocess.Popen if factory is None else factory

    def spawn(*args, **kwargs):
        require(kwargs.get('env') is environment, 'Actual child environment changed')
        process = actual(*args, **kwargs)
        handles.append(process)
        return process

    support.subprocess = SimpleNamespace(Popen=spawn, DEVNULL=subprocess.DEVNULL,
                                         PIPE=subprocess.PIPE,
                                         CompletedProcess=subprocess.CompletedProcess,
                                         TimeoutExpired=subprocess.TimeoutExpired)
    try:
        return support.run_bounded(command, output, 'native', timeout=TIMEOUT,
                                   limit=LIMIT, env=environment)
    finally:
        support.subprocess = original
        for process in handles:
            item = dict(pid=process.pid, cleanup_kill=False, reaped=False, errors=[])
            execution.append(item)
            try:
                if process.poll() is None:
                    item['cleanup_kill'] = True
                    process.kill()
                item['exit'] = process.wait(timeout=5)
                item['reaped'] = process.poll() is not None
            except BaseException as error:
                item['errors'].append(dict(type=type(error).__name__, message=str(error)))
            finally:
                for name in ('stdout', 'stderr'):
                    stream = getattr(process, name, None)
                    if stream is not None:
                        try:
                            stream.close()
                        except BaseException as error:
                            item['errors'].append(dict(stream=name, type=type(error).__name__,
                                                       message=str(error)))


def custody_controls():
    class Process:
        pid = 17

        def __init__(self, done):
            self.code = 0 if done else None
            self.kills = 0

        def poll(self):
            return self.code

        def kill(self):
            self.kills += 1
            self.code = -1

        def wait(self, timeout):
            require(timeout == 5, 'Bounded owned reap')
            return self.code

    for failure in (None, RuntimeError, KeyboardInterrupt):
        environment = {}
        process = Process(failure is None)
        execution = []
        original = object()
        support = SimpleNamespace(subprocess=original)

        def run(*args, **kwargs):
            support.subprocess.Popen(env=kwargs['env'])
            if failure is not None:
                raise failure('authored transport failure')
            return 'done'

        support.run_bounded = run
        try:
            result = capture_owned(support, [], Path('.'), environment, execution,
                                   lambda **kwargs: process)
            require(failure is None and result == 'done', 'Natural transport return')
        except (RuntimeError, KeyboardInterrupt) as error:
            require(type(error) is failure, 'Original transport failure retained')
        require(support.subprocess is original and len(execution) == 1 and
                execution[0]['reaped'] and not execution[0]['errors'] and
                execution[0]['cleanup_kill'] == (failure is not None) and
                process.kills == int(failure is not None), 'Actual owned-handle cleanup')


def self_test():
    positives = negatives = 0
    for profile in PROFILES:
        stdout = b'16 achievements\n151 recipes\n' * PROFILES[profile][2]
        stdout += ('Dispenser ' + PROFILES[profile][1] +
                   ' CPU controls passed: 1\n').encode('ascii')
        receipt = dict(exit=0, timeout_seconds=TIMEOUT, limit=LIMIT, stdout_bytes=len(stdout),
                       stderr_bytes=0, reaped=True, pipe_capture_complete=True, timed_out=False,
                       output_overflow=False, reader_errors={})
        good = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'', capture_report=receipt)
        require(completed(good, profile) == 1, 'Positive protocol control')
        positives += 1
        bad = []
        for key, value in [('returncode', False), ('returncode', 1), ('stderr', b'failure\n'),
                           ('stdout', stdout[:-1]), ('stdout', stdout + b'extra\n'),
                           ('stdout', stdout.replace(b'passed: 1', b'passed: 0')),
                           ('stdout', stdout.replace(b'passed: 1', b'passed: 4294967296')),
                           ('stdout', stdout.replace(b'16 achievements\n', b'') if
                            PROFILES[profile][2] else b'16 achievements\n' + stdout)]:
            item = copy.deepcopy(good)
            setattr(item, key, value)
            item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
            bad.append(item)
        for key, value in [('exit', False), ('reaped', False), ('pipe_capture_complete', False),
                           ('timed_out', True), ('output_overflow', True),
                           ('reader_errors', {'stdout': 'lost'}), ('timeout_seconds', 1),
                           ('limit', 0), ('stdout_bytes', 0), ('stderr_bytes', False)]:
            item = copy.deepcopy(good)
            item.capture_report[key] = value
            bad.append(item)
        for item in bad:
            try:
                completed(item, profile)
            except (ValueError, TypeError, KeyError):
                negatives += 1
                continue
            raise ValueError('Invalid native transport admitted')
    custody_controls()
    print(json.dumps(dict(status='PASS', positive=positives, negative=negatives, custody=3,
                          scope='Synthetic transport controls; no native execution')))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--profile', choices=tuple(PROFILES))
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(args.profile is not None and args.executable is not None and args.work_dir is not None,
            'Profile, executable and output parent required')
    ownership = load('dispenser_placement_protected',
                     'tests/tools/world/test_execution_ownership.py')
    support = load('dispenser_placement_bounded', 'tests/reference/client/statistics/support.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix=args.profile + '-', dir=args.work_dir.resolve()))
    world = output / 'world-\u03bb-\u4e16\u754c'
    world.mkdir()
    report = dict(status='RUNNING', profile=args.profile, fixture_root=str(world),
                  scope=__doc__, execution=[])
    before = None
    try:
        before = ownership.protected(executable)
        # The current source map includes these too; explicit entries bind the
        # controller and its two helpers even when validating map membership.
        for path in [Path(__file__).resolve(), Path(ownership.__file__), Path(support.__file__)]:
            before[str(path)] = ownership.pin(path)
        report['inputs_before'] = before
        child = capture_owned(support, [str(executable), PROFILES[args.profile][0],
                                       str(world).encode('utf-8').hex()], output,
                              os.environ.copy(), report['execution'])
        report['capture'] = child.capture_report
        require(len(report['execution']) == 1 and
                report['execution'][0]['reaped'] and
                report['execution'][0]['exit'] == 0 and
                not report['execution'][0]['cleanup_kill'] and
                not report['execution'][0]['errors'], 'Actual natural child retirement')
        report['checks'] = completed(child, args.profile)
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', failure=dict(type=type(error).__name__, message=str(error)))
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
        raise
    finally:
        if before is not None:
            after = {path: ownership.pin(Path(path)) for path in before}
            report['inputs_after'] = after
            if before != after:
                report.update(status='FAIL', input_failure='Protected source/products changed')
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        require(report['status'] == 'PASS', 'Failed CPU controls; retained report: ' + str(output))
    print(json.dumps(dict(status='PASS', checks=report['checks'], report=str(output / 'report.json'))))


if __name__ == '__main__':
    main()
