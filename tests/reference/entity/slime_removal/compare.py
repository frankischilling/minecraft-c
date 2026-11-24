"""Strictly compare completed native direct-K runs with a fresh unchanged-JAR capture.

Reads receipts and observations only; this tool never launches native code.
"""
import argparse
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
from capture_reference import (DIRECT_PROFILES, HERE, SOURCES, digest, ensure, exact,
                               footer, pins, read_json, require_success, rows, validate_row)


def reference(directory):
    directory = Path(directory).resolve(strict=True)
    report = read_json(directory / 'capture.json')
    ensure(report.get('status') == 'PASS' and report.get('inputs_unchanged') is True and
           report.get('helpers_unchanged') is True, 'successful unchanged original capture required')
    ensure(exact(report['input_before_sha256'], report['input_after_sha256']) and
           exact(report['helpers_before_sha256'], report['helpers_after_sha256']), 'original retained input/helper equality')
    ensure(exact(report['source_before_sha256'], {name: digest(HERE / name) for name in SOURCES}),
           'current collector/comparator/profile sources must match executed original inputs')
    for name, expected in report['output_sha256'].items():
        path = directory / name
        ensure(not Path(name).is_absolute() and '..' not in Path(name).parts and
               path.is_file() and not path.is_symlink() and digest(path) == expected,
               'unchanged retained original output: ' + name)
    capture = read_json(directory / 'original.capture.json')
    ensure(exact(capture, report['original']), 'actual original process receipt equality')
    require_success(capture)
    for stream in ('stdout', 'stderr'):
        path = directory / capture[stream + '_file']
        ensure(path.stat().st_size == capture[stream + '_bytes'] and
               digest(path) == capture[stream + '_sha256'], 'original stream bytes/hash')
    parsed = rows((directory / 'original.stdout').read_text(encoding='utf-8'), complete=True)
    ensure(type(report['rows']) is int and len(parsed) == report['rows'] and
           digest(directory / 'rows.json') == report['rows_sha256'] and
           exact(parsed, read_json(directory / 'rows.json')), 'original observations bound to actual process bytes')
    return parsed, digest(directory / 'capture.json')


def compare_subset(original, native, names):
    ensure(names and len(names) == len(set(names)) and all(name in DIRECT_PROFILES for name in names),
           'explicit unique native profile inventory')
    expected = [row for name in names for row in original if row['profile'] == name]
    ensure(len(expected) == len(names) and all(row['step'] == 0 for row in expected),
           'one real original observation per requested direct call')
    for row in native:
        validate_row(row)
    ensure(exact(native, expected), 'exact typed ordered native/original Slime removal observations')
    return len(expected)


def native_run(directory):
    directory = Path(directory).resolve(strict=True)
    report = read_json(directory / 'report.json')
    mode = report.get('mode')
    ensure(mode in DIRECT_PROFILES and report.get('passed') is True and
           report.get('guard_unchanged') is True and report.get('retained_capture_verified') is True and
           type(report.get('exit')) is int and report['exit'] == 0, 'completed declared native direct-K control')
    before, after = report.get('guard_before_sha256'), report.get('guard_after_sha256')
    ensure(type(before) is dict and before and exact(before, after), 'actual native input bindings before/after')
    ensure(exact(before, pins([Path(name) for name in before])), 'native executable/controller/source identities remain current')
    capture = read_json(directory / 'native.capture.json')
    ensure(exact(capture, report.get('capture')), 'actual native capture receipt equality')
    ensure(type(capture.get('exit')) is int and capture['exit'] == 0 and
           capture.get('reaped') is True and capture.get('pipe_capture_complete') is True and
           capture.get('timed_out') is False and capture.get('output_overflow') is False and
           capture.get('reader_errors') == {}, 'complete successful actual native process')
    stdout, stderr = (directory / 'native.stdout').read_bytes(), (directory / 'native.stderr').read_bytes()
    ensure(type(capture.get('stdout_bytes')) is int and capture['stdout_bytes'] == len(stdout) and
           type(capture.get('stderr_bytes')) is int and capture['stderr_bytes'] == len(stderr) == 0 and
           type(capture.get('limit')) is int and capture['limit'] == 1024 * 1024 and
           0 < len(stdout) <= capture['limit'] and type(capture.get('timeout_seconds')) is int and
           capture['timeout_seconds'] == 240,
           'bounded exact native output extents')
    command = capture.get('command')
    ensure(type(command) is list and len(command) == 3 and all(type(v) is str for v in command) and
           exact(command, report.get('command')) and
           command[1] == mode and str(Path(command[0]).resolve(strict=True)) in before and
           re.fullmatch(r'mc_test_slime_removal(?:\.exe)?', Path(command[0]).name),
           'actual bound Slime native fixture command')
    lines = stdout.decode('utf-8', errors='strict').splitlines()
    completed = (re.fullmatch(r'PASS: ([1-9][0-9]*) actual Slime removal checks;mode=' + re.escape(mode),
                             lines[-1]) if lines else None)
    ensure(lines and lines[0] == 'SLIME_REMOVAL_BEGIN mode=' + mode and completed is not None and
           type(report.get('checks')) is int and report['checks'] == int(completed[1]),
           'native terminal completion marker')
    result = rows(stdout.decode('utf-8', errors='strict'))
    ensure(len(result) == 1 and result[0]['profile'] == mode and result[0]['step'] == 0 and
           exact(result, report.get('observations')),
           'exact one named native direct-call row')
    return result[0], {'mode': mode, 'report_sha256': digest(directory / 'report.json'),
                      'capture_sha256': digest(directory / 'native.capture.json'),
                      'stdout_sha256': digest(directory / 'native.stdout'), 'guard_sha256': before}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original-capture', type=Path, required=True)
    parser.add_argument('--native-run', type=Path, action='append', required=True)
    parser.add_argument('--profiles', help='Explicit ordered subset; default is all11 comparable direct profiles')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    names = tuple(args.profiles.split(',')) if args.profiles is not None else DIRECT_PROFILES
    result = {'status': 'FAIL', 'scope': 'Direct UW.K records only; constructor entropy and native-only damage controls excluded'}
    try:
        original, source = reference(args.original_capture)
        actual, receipts = [], []
        for directory in args.native_run:
            row, receipt = native_run(directory); actual.append(row); receipts.append(receipt)
        result.update(rows=compare_subset(original, actual, names), profiles=list(names),
                      original_capture_sha256=source, native_receipts=receipts, status='PASS')
    except Exception as error:
        result['error'] = f'{type(error).__name__}: {error}'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    print('Slime original/native comparison ' + result['status'] + ': ' + str(args.output))
    if result['status'] != 'PASS':
        print(result['error'], file=sys.stderr)
        raise SystemExit(1)


if __name__ == '__main__':
    main()
