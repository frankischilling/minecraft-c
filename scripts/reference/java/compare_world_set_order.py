#!/usr/bin/env python3
"""Replay independently captured JDK17 packed-coordinate Set histories.

Every response binds complete before/after iteration, fresh first and mutation
returns. Full checkpoint arrays remain readable; intermediate responses retain
their independently captured canonical hashes. C never supplies expected data.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
HELPER = ROOT / 'tests/reference/java/HashSetOrderObservation.java'
FIXTURE = ROOT / 'tests/reference/java/world_set_order_fixtures.json'
FIXTURE_SHA256 = 'f47a454bd9b919d56c862374a777229846591f1a51065d44bbb455d170d7757f'
SCOPE = ('Pinned JDK17 public default HashSet traversal for distinct complete packed coordinate hashes; '
         'Integer and non-Comparable object peers. Fresh361, duplicate/removal/readd, resize, '
         'clear/reinsert and fresh-first Set.remove histories; no Java6 or general Object-key claim.')
spec = importlib.util.spec_from_file_location('world_set_capture_helpers', HERE / 'compare_int_set.py')
S = importlib.util.module_from_spec(spec)
spec.loader.exec_module(S)


def require(value, message):
    if not value:
        raise ValueError('World Set order: ' + message)


def packed(x, z):
    value = (0x80000000 if x < 0 else 0) | ((x & 32767) << 16) | (32768 if z < 0 else 0) | (z & 32767)
    return value if value < 1 << 31 else value - (1 << 32)


def requests():
    keys = [packed(x, z) for x in range(-10, 9) for z in range(-7, 12)]
    commands, checkpoints = ['new'], {}
    def checkpoint(name):
        checkpoints[name] = len(commands)
        commands.append('snapshot')
    commands.extend('add ' + str(key) for key in keys)
    checkpoint('fresh361')
    commands.extend('add ' + str(key) for key in keys[::41])
    commands.extend('remove ' + str(key) for key in keys[::3])
    checkpoint('removed121')
    commands.extend('add ' + str(key) for key in reversed(keys[::3]))
    checkpoint('readded121')
    commands.extend('add ' + str(packed(100 + i, 200 + i * 7)) for i in range(70))
    checkpoint('grown431')
    commands.append('clear')
    commands.extend('add ' + str(key) for key in keys)
    checkpoint('cleared-reinserted361')
    commands.extend(['take'] * 32)
    checkpoint('after32fresh-removals')
    return ('\n'.join(commands) + '\n').encode('ascii'), checkpoints


def canonical_hash(value):
    return hashlib.sha256(S.encoded(value, sorted_keys=True)).hexdigest()


def parse(raw, count):
    require(type(raw) is bytes and len(raw) <= S.MAX_OUTPUT and raw.endswith(b'\n'), 'bounded complete stream')
    lines = raw.splitlines()
    require(len(lines) == count, 'exact response count')
    rows = [S.strict_json(line) for line in lines]
    for row in rows:
        require(type(row) is dict, 'response object')
        if set(row) == {'size', 'iteration', 'fresh_first'}:
            S.snapshot(row)
        else:
            require(set(row) == {'before', 'empty_before_action', 'selected', 'result', 'after'}, 'closed mutation response')
            S.snapshot(row['before'])
            S.snapshot(row['after'])
            require(row['empty_before_action'] is None or type(row['empty_before_action']) is bool, 'typed empty result')
            require(row['selected'] is None or S.integer(row['selected']), 'typed selected key')
            require(row['result'] is None or type(row['result']) is bool, 'typed mutation result')
    return rows


def fixture(request, checkpoints):
    require(hashlib.sha256(FIXTURE.read_bytes()).hexdigest() == FIXTURE_SHA256, 'fixed independently captured fixture bytes')
    value = S.strict_json(FIXTURE.read_bytes())
    require(type(value) is dict and set(value) == {'schema', 'scope', 'helper_sha256', 'request_sha256',
            'response_sha256', 'checkpoints', 'provenance'} and value['schema'] == 1 and value['scope'] == SCOPE,
            'closed captured fixture')
    require(value['helper_sha256'] == hashlib.sha256(HELPER.read_bytes()).hexdigest() and
            value['request_sha256'] == hashlib.sha256(request).hexdigest(), 'exact helper and operation history')
    hashes = value['response_sha256']
    require(type(hashes) is list and len(hashes) == len(request.splitlines()) and
            all(type(word) is str and len(word) == 64 and all(c in '0123456789abcdef' for c in word) for word in hashes),
            'complete captured response hashes')
    require(type(value['checkpoints']) is dict and set(value['checkpoints']) == set(checkpoints), 'complete full-order checkpoints')
    for name, index in checkpoints.items():
        row = value['checkpoints'][name]
        S.snapshot(row)
        require(canonical_hash(row) == hashes[index], 'checkpoint binds full captured traversal')
    require(value['checkpoints']['fresh361']['size'] == 361 and
            set(value['checkpoints']['fresh361']['iteration']) == {packed(x, z) for x in range(-10, 9) for z in range(-7, 12)},
            'complete361 coordinate membership without traversal substitution')
    return value


def compare(rows, expected, checkpoints):
    require(len(rows) == len(expected['response_sha256']), 'complete response history')
    for index, (row, digest) in enumerate(zip(rows, expected['response_sha256'])):
        require(canonical_hash(row) == digest, 'complete JDK/native state differs at operation ' + str(index))
    for name, index in checkpoints.items():
        require(S.encoded(rows[index], sorted_keys=True) == S.encoded(expected['checkpoints'][name], sorted_keys=True),
                'full traversal differs at ' + name)


def controls(rows, expected, checkpoints):
    import copy
    corruptions = []
    for index, change in ((checkpoints['fresh361'], 'swap'), (checkpoints['grown431'], 'drop'),
                          (checkpoints['after32fresh-removals'], 'first')):
        altered = copy.deepcopy(rows)
        row = altered[index]
        if change == 'swap':
            row['iteration'][0], row['iteration'][1] = row['iteration'][1], row['iteration'][0]
        elif change == 'drop':
            row['iteration'].pop()
            row['size'] -= 1
        else:
            row['fresh_first'] = row['iteration'][1]
        try:
            compare(altered, expected, checkpoints)
        except ValueError:
            corruptions.append(change)
        else:
            raise ValueError('World Set order: accepted corruption ' + change)
    for raw in (b'{}\n', b'{"size":0,"size":0,"iteration":[],"fresh_first":null}\n',
                b'{"size":NaN,"iteration":[],"fresh_first":null}\n',
                b'{"size":true,"iteration":[],"fresh_first":null}\n'):
        try:
            parse(raw, 1)
        except (ValueError, S.Failure):
            corruptions.append('transport')
        else:
            raise ValueError('World Set order: accepted malformed transport')
    try:
        parse(S.encoded(rows[0]), len(rows))
    except ValueError:
        corruptions.append('truncated')
    else:
        raise ValueError('World Set order: accepted truncated history')
    return corruptions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--java')
    parser.add_argument('--javac')
    parser.add_argument('--work-dir', type=Path, default=ROOT / '.local/reference/world-set-order')
    args = parser.parse_args()
    require(bool(args.java) == bool(args.javac), 'supply both JDK tools')
    request, checkpoints = requests()
    expected = fixture(request, checkpoints)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='world-set-', dir=args.work_dir.resolve()))
    deadline = time.monotonic() + 120
    report = {'schema': 1, 'scope': SCOPE, 'passed': False, 'processes': [], 'inputs': {}}
    inputs = [args.executable.resolve(strict=True), Path(__file__).resolve(), HELPER, FIXTURE,
              HERE / 'compare_int_set.py', ROOT / 'src/core/java/int_set.c', ROOT / 'include/mc/core/java/int_set.h',
              ROOT / 'tests/core/java/int_set_probe.c']
    report['inputs'] = {str(path): S.file_fingerprint(path) for path in inputs}
    try:
        out, err = S.run([str(inputs[0])], request, work=work, name='native', records=report['processes'], deadline=deadline)
        require(not err, 'native stderr')
        rows = parse(out, len(request.splitlines()))
        compare(rows, expected, checkpoints)
        report['rejection_controls'] = controls(rows, expected, checkpoints)
        if args.java:
            java, javac = shutil.which(args.java), shutil.which(args.javac)
            require(java is not None and javac is not None, 'available JDK tools')
            for key in ('JAVA_TOOL_OPTIONS', 'JDK_JAVA_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVAC_OPTIONS', 'CLASSPATH'):
                require(not os.environ.get(key), 'untracked JDK option environment')
            classes = work / 'classes'
            classes.mkdir()
            empty = work / 'empty-lookups'
            empty.mkdir()
            jdk_files = S.jdk_inputs(java, javac)
            jdk_before = {name: S.file_fingerprint(path) for name, path in jdk_files.items()}
            report['jdk_inputs'] = jdk_before
            compiled, errors = S.run([javac, '--release', '17', '-encoding', 'UTF-8', '-proc:none',
                '-implicit:none', '-classpath', str(empty), '-sourcepath', str(empty), '-d', str(classes), str(HELPER)],
                work=work, name='compile', records=report['processes'], deadline=deadline)
            require(not compiled and not errors, 'quiet helper compilation')
            class_before = {path.name: S.file_fingerprint(path) for path in classes.iterdir()}
            require(set(class_before) == {'HashSetOrderObservation.class', 'HashSetOrderObservation$Key.class'},
                    'only two authored helper classes')
            report['helper_classes'] = class_before
            for index in range(2):
                out, err = S.run([java, '-cp', str(classes), 'HashSetOrderObservation'], request,
                                work=work, name='jdk-' + str(index), records=report['processes'], deadline=deadline)
                require(not err, 'JDK stderr')
                compare(parse(out, len(rows)), expected, checkpoints)
                require({path.name: S.file_fingerprint(path) for path in classes.iterdir()} == class_before,
                        'helper classes unchanged')
            require(all(S.file_fingerprint(path) == jdk_before[name] for name, path in jdk_files.items()),
                    'JDK profile unchanged')
        require(all(S.file_fingerprint(path) == report['inputs'][str(path)] for path in inputs), 'all replay inputs unchanged')
        report.update(passed=True, responses=len(rows), full_checkpoints=len(checkpoints))
        print('PASS: World Set order;', len(rows), 'complete JDK/native API responses;', len(checkpoints), 'full-order checkpoints')
    finally:
        (work / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
