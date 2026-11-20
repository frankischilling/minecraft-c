"""Constructed Arrow base/Math controls; no original execution or SL flight."""
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
ROOT = Path(__file__).resolve().parents[3]
TIMEOUT = 180
LIMIT = 2 * 1024**2


def need(value, message):
    if not value:
        raise ValueError(message)


def completed(child):
    c = child.capture_report
    need(type(child.returncode) is int and child.returncode == 0, 'Native exit is not success')
    need(type(c.get('exit')) is int and c['exit'] == 0 and c.get('reaped') is True and
         c.get('pipe_capture_complete') is True and c.get('timed_out') is False and
         c.get('output_overflow') is False and c.get('reader_errors') == {}, 'Incomplete native process')
    for key, expected in [('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                          ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        need(type(c.get(key)) is int and c[key] == expected, 'Wrong native bound/count: ' + key)
    need(child.stderr == b'' and child.stdout.endswith(b'\n'), 'Native stderr or incomplete final line')
    lines = child.stdout.splitlines()
    expected = [b'16 achievements', b'151 recipes']
    expected += [('ARROW_BASE_CASE id=' + str(i)).encode('ascii') for i in range(5)]
    need(len(lines) == len(expected) + 1 and lines[:-1] == expected, 'Wrong real setup/case sequence')
    footer = re.fullmatch(rb'PASS: ([1-9][0-9]*) Arrow base and contextual Math checks;5 native controls', lines[-1])
    need(footer is not None, 'Missing native terminal controls')
    return int(footer.group(1))


def self_test():
    lines = [b'16 achievements', b'151 recipes']
    lines += [('ARROW_BASE_CASE id=' + str(i)).encode('ascii') for i in range(5)]
    lines += [b'PASS: 1 Arrow base and contextual Math checks;5 native controls']
    stdout = b'\n'.join(lines) + b'\n'
    c = dict(exit=0, reaped=True, pipe_capture_complete=True, timed_out=False,
             output_overflow=False, reader_errors={}, timeout_seconds=TIMEOUT,
             limit=LIMIT, stdout_bytes=len(stdout), stderr_bytes=0)
    good = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'', capture_report=c)
    need(completed(good) == 1, 'Positive native transport')
    changes = [('returncode', 1), ('returncode', False), ('stderr', b'failure\n'),
               ('stdout', stdout[:-1]), ('stdout', stdout + lines[-1] + b'\n'),
               ('stdout', b'\n'.join(lines[1:]) + b'\n')]
    bad = []
    for key, value in changes:
        item = copy.deepcopy(good); setattr(item, key, value)
        item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
        bad.append(item)
    for key, value in [('exit', False), ('reaped', False), ('pipe_capture_complete', False),
                       ('timed_out', True), ('output_overflow', True), ('reader_errors', {'stderr':'lost'}),
                       ('timeout_seconds', 1), ('limit', 0), ('stdout_bytes', 0), ('stderr_bytes', False)]:
        item = copy.deepcopy(good); item.capture_report[key] = value; bad.append(item)
    refused = 0
    for item in bad:
        try:
            completed(item)
        except (ValueError, KeyError, TypeError):
            refused += 1
            continue
        raise ValueError('Corrupt native transport admitted')
    need(refused == len(bad), 'Every declared corrupt transport refused')
    print('PASS:', refused, 'Arrow base native transport refusals')


def pin(path):
    data = path.read_bytes()
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def protected(executable):
    paths = {executable, Path(__file__).resolve(), ROOT/'CMakeLists.txt', ROOT/'.clang-format',
             ROOT/'tests/reference/client/statistics/support.py', ROOT/'tests/entity/test_arrow_base.c'}
    for name in ['src', 'include', 'cmake', 'tests/support', 'tests/world', 'tests/entity']:
        paths.update(p for p in (ROOT/name).rglob('*') if p.is_file())
    paths.update(executable.parent.glob('*.dll'))
    return {str(p.resolve()):pin(p) for p in sorted(paths)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test(); return
    need(args.executable is not None and args.work_dir is not None, 'Native executable and fresh-work parent required')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='arrow-base-', dir=args.work_dir.resolve()))
    world = output/'world-\u03bb-\u4e16\u754c'; world.mkdir()
    spec = importlib.util.spec_from_file_location('arrow_base_bounded', ROOT/'tests/reference/client/statistics/support.py')
    support = importlib.util.module_from_spec(spec); spec.loader.exec_module(support)
    report = dict(status='RUNNING', scope='Five constructed native Arrow base/Math controls; no original, complete SL, frame or FPS claim')
    before = protected(executable)
    report['inputs_before'] = before
    try:
        child = support.run_bounded([str(executable), 'arrow-base', str(world).encode('utf-8').hex()],
                                    output, 'native', timeout=TIMEOUT, limit=LIMIT, env=os.environ.copy())
        report.update(capture=child.capture_report, checks=completed(child))
        after = protected(executable)
        need(after == before, 'Native execution modified a protected input')
        report.update(status='PASS', inputs_unchanged=True)
        print('PASS:', report['checks'], 'native Arrow base checks; capture', output)
    except BaseException as error:
        report.update(status='FAIL', failure=dict(type=type(error).__name__, message=str(error)))
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
        raise
    finally:
        (output/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
