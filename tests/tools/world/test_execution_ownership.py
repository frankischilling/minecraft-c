"""Retain bounded native World ownership controls; no whole Arrow tick claim."""
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
LIMIT = 4 * 1024**2
TABLES = ('application_owner', 'biped', 'render_resources', 'display_sdk',
          'loading_draw', 'resource_lookup_common')
SCHEMAS = {
    'handoff': ('world-access', (), rb'PASS: ([1-9][0-9]*) protected WorldAccess handoff checks'),
    'children': ('sound-particle', tuple(('EXECUTION_CHILD_CASE id=' + str(i)).encode('ascii')
                                       for i in range(5)),
                 rb'PASS: ([1-9][0-9]*) actual World execution-child checks;5 groups'),
}


def need(value, message):
    if not value:
        raise ValueError(message)


def completed(child, kind):
    c = child.capture_report
    need(type(child.returncode) is int and child.returncode == 0, 'Native exit is not success')
    need(type(child.stdout) is bytes and type(child.stderr) is bytes, 'Native streams are not bytes')
    need(type(c.get('exit')) is int and c['exit'] == 0 and c.get('reaped') is True and
         c.get('pipe_capture_complete') is True and c.get('timed_out') is False and
         c.get('output_overflow') is False and c.get('reader_errors') == {}, 'Incomplete native process')
    for key, expected in [('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                          ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        need(type(c.get(key)) is int and c[key] == expected, 'Wrong native bound/count: ' + key)
    need(child.stderr == b'' and child.stdout.endswith(b'\n'), 'Native stderr or incomplete final line')
    expected = [b'16 achievements', b'151 recipes'] * 2 + list(SCHEMAS[kind][1])
    lines = child.stdout.splitlines()
    need(len(lines) == len(expected) + 1 and lines[:-1] == expected, 'Wrong real setup/control sequence')
    footer = re.fullmatch(SCHEMAS[kind][2], lines[-1])
    need(footer is not None, 'Missing native ownership completion')
    return int(footer.group(1))


def self_test():
    refused = 0
    for kind in SCHEMAS:
        lines = [b'16 achievements', b'151 recipes'] * 2 + list(SCHEMAS[kind][1])
        footer = (b'PASS: 1 protected WorldAccess handoff checks' if kind == 'handoff' else
                  b'PASS: 1 actual World execution-child checks;5 groups')
        stdout = b'\n'.join(lines + [footer]) + b'\n'
        c = dict(exit=0, reaped=True, pipe_capture_complete=True, timed_out=False,
                 output_overflow=False, reader_errors={}, timeout_seconds=TIMEOUT,
                 limit=LIMIT, stdout_bytes=len(stdout), stderr_bytes=0)
        good = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'', capture_report=c)
        need(completed(good, kind) == 1, 'Positive native transport')
        bad = []
        for key, value in [('returncode', 1), ('returncode', False), ('stderr', b'failure\n'),
                           ('stdout', stdout[:-1]), ('stdout', stdout + footer + b'\n'),
                           ('stdout', b'\n'.join(lines[1:] + [footer]) + b'\n'),
                           ('stdout', stdout.replace(footer, b'PASS: 0 wrong completion'))]:
            item = copy.deepcopy(good)
            setattr(item, key, value)
            item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
            bad.append(item)
        for key, value in [('exit', False), ('reaped', False), ('pipe_capture_complete', False),
                           ('timed_out', True), ('output_overflow', True),
                           ('reader_errors', {'stderr': 'lost'}), ('timeout_seconds', 1),
                           ('limit', 0), ('stdout_bytes', 0), ('stderr_bytes', False)]:
            item = copy.deepcopy(good)
            item.capture_report[key] = value
            bad.append(item)
        for item in bad:
            try:
                completed(item, kind)
            except (ValueError, KeyError, TypeError):
                refused += 1
                continue
            raise ValueError('Corrupt native ownership transport admitted')
    need(refused == 34, 'Every declared corrupt transport refused')
    print('PASS:', refused, 'World execution ownership transport refusals')


def pin(path):
    data = path.read_bytes()
    return dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())


def protected(executable):
    source = ROOT/'tests/reference/client/resource_lookup_common/provenance.json'
    entries = json.loads(source.read_text(encoding='utf-8'))['source_files']
    paths = {executable, Path(__file__).resolve()}
    root = ROOT.resolve(strict=True)
    validated = {}
    for relative, expected in entries.items():
        path = (ROOT/relative).resolve(strict=True)
        need(path.is_relative_to(root), 'Source path outside project')
        actual = pin(path)
        need(actual == expected, 'Stale current source binding: ' + relative)
        validated[path] = actual
        paths.add(path)
    paths.update(ROOT/'tests/reference/client'/name/'provenance.json' for name in TABLES)
    paths.update(executable.parent.glob('*.dll'))
    return {str(path): validated[path] if path in validated else pin(path)
            for path in sorted(paths)}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--kind', choices=tuple(SCHEMAS))
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    need(args.kind is not None and args.executable is not None and args.work_dir is not None,
         'Kind, executable and fresh-work parent required')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=args.kind + '-', dir=args.work_dir.resolve()))
    world = out/'world-\u03bb-\u4e16\u754c'
    world.mkdir()
    spec = importlib.util.spec_from_file_location('world_execution_bounded',
                                                 ROOT/'tests/reference/client/statistics/support.py')
    support = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(support)
    report = dict(status='RUNNING', kind=args.kind, fixture_root=str(world),
                  scope='Constructed native owned World controls; no original execution, complete SL, frame or FPS claim')
    before = None
    try:
        before = protected(executable)
        report['inputs_before'] = before
        child = support.run_bounded([str(executable), SCHEMAS[args.kind][0],
                                     str(world).encode('utf-8').hex()], out, 'native',
                                    timeout=TIMEOUT, limit=LIMIT, env=os.environ.copy())
        report['capture'] = child.capture_report
        report.update(checks=completed(child, args.kind), status='PASS')
    except BaseException as error:
        report.update(status='FAIL', failure=dict(type=type(error).__name__, message=str(error)))
        print('Native ownership control failed; retained capture:', out, file=sys.stderr)
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
        raise
    finally:
        same = before is not None
        if before is not None:
            try:
                after = protected(executable)
                report['inputs_after'] = after
                same = after == before
            except BaseException as error:
                same = False
                report['input_guard_failure'] = dict(type=type(error).__name__, message=str(error))
        report['inputs_unchanged'] = same
        if not same:
            report['status'] = 'FAIL'
        (out/'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        need(same, 'Native execution input preflight or terminal guard failed')
    print('PASS:', report['checks'], 'native World ownership checks; capture', out)


if __name__ == '__main__':
    main()
