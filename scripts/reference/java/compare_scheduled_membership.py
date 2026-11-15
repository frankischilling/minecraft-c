#!/usr/bin/env python3
"""Replay pinned JDK17 linked-bin scheduled-membership observations.

The actual C owner never creates expected values. Collision trees and their
identity tie ordering are outside this finite public-API collection profile.
"""
import argparse
import copy
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
HELPER = ROOT / 'tests/reference/java/ScheduledMembershipObservation.java'
FIXTURE = ROOT / 'tests/reference/java/scheduled_membership_fixtures.json'
FIXTURE_SHA256 = 'bf551f9fbc5ae41b36ccbcccfb701949e356906fca5fe543c091c62331981a38'
SCOPE = ('JDK17 default HashSet linked-bin scheduled membership; immutable xyz/block equality/hash, '
         'raw Comparable record shape, retained same-owner inventory, mutable due times, equal-distinct '
         'queries and stable growth splits. No collision-tree/identity-order, Java6, E or whole-World claim.')
spec = importlib.util.spec_from_file_location('membership_capture_helpers', HERE / 'compare_int_set.py')
S = importlib.util.module_from_spec(spec)
spec.loader.exec_module(S)


def require(value, message):
    if not value:
        raise ValueError('Scheduled membership: ' + message)


def requests():
    rows = ['new', 'record 0 15 64 15 51 21 9223372036854775807',
            'record 1 16 64 15 51 22 -5', 'record 2 15 64 15 51 900 999',
            'add 0', 'add 1', 'snapshot', 'add 2', 'contains 2',
            'time 0 -9223372036854775808', 'contains 2', 'shift 9223372036854775807',
            'snapshot', 'remove 2', 'contains 2', 'add 0', 'snapshot', 'remove 0',
            'add 2', 'record 3 143 64 15 51 400 0', 'add 3', 'contains 3',
            'shift 1', 'remove 2', 'contains 3', 'snapshot', 'new']
    blocks = [0, 16, 32] + [value for value in range(1, 64) if value not in (16, 32)]
    for slot, block in enumerate(blocks[:49]):
        rows.extend([f'record {slot} 0 0 0 {block} {700 - slot} {slot - 19}', f'add {slot}'])
        if slot + 1 in (3, 12, 13, 24, 25, 48, 49):
            rows.append('snapshot')
    rows.extend(['remove 1', 'add 1', 'time 0 9223372036854775807', 'shift 1',
                 'contains 0', 'remove 0', 'add 0', 'snapshot'])
    return ('\n'.join(rows) + '\n').encode('ascii')


def state(row):
    require(type(row) is dict and set(row) == {'result', 'entries', 'records'}, 'closed response')
    require(row['result'] is None or type(row['result']) is bool, 'typed ordinary return')
    entries, records = row['entries'], row['records']
    require(type(entries) is list and len(entries) <= 64 and all(type(i) is int and 0 <= i < 64 for i in entries) and
            len(set(entries)) == len(entries), 'complete unique retained identities')
    require(type(records) is list and len(records) <= 64, 'bounded live records')
    previous = -1
    for record in records:
        require(type(record) is list and len(record) == 7 and type(record[0]) is int and previous < record[0] < 64 and
                all(S.integer(value) for value in record[1:5]) and
                all(type(value) is int and -(1 << 63) <= value < 1 << 63 for value in record[5:]),
                'complete typed same-owner record fields')
        previous = record[0]
    require(set(entries) <= {record[0] for record in records}, 'entries are actual retained record owners')


def parse(raw, count):
    require(type(raw) is bytes and len(raw) <= S.MAX_OUTPUT and raw.endswith(b'\n'), 'complete bounded stream')
    lines = raw.splitlines()
    require(len(lines) == count, 'exact completed response count')
    rows = [S.strict_json(line) for line in lines]
    for row in rows:
        state(row)
    return rows


def linked_profile(rows, commands):
    capacity = 0
    for row, command in zip(rows, commands):
        if command == 'new':
            capacity = 0
        elif command.startswith('add ') and row['result'] is True:
            capacity = capacity or 16
            records = {record[0]: record for record in row['records']}
            bins = {}
            for slot in row['entries']:
                _, x, y, z, block, _, _ = records[slot]
                raw = (((x * 128 * 1024 + z * 128 + y) * 256) + block) & 0xffffffff
                bucket = (raw ^ (raw >> 16)) & (capacity - 1)
                bins[bucket] = bins.get(bucket, 0) + 1
            require(max(bins.values()) <= 8, 'declared history never reaches treeify policy')
            if len(row['entries']) > capacity * 3 // 4:
                capacity *= 2


def fixture(request):
    require(hashlib.sha256(FIXTURE.read_bytes()).hexdigest() == FIXTURE_SHA256, 'fixed independently captured fixture')
    value = S.strict_json(FIXTURE.read_bytes())
    require(type(value) is dict and set(value) == {'schema', 'scope', 'helper_sha256', 'request_sha256', 'rows', 'provenance'} and
            value['schema'] == 1 and value['scope'] == SCOPE, 'closed fixture inventory')
    require(value['helper_sha256'] == hashlib.sha256(HELPER.read_bytes()).hexdigest() and
            value['request_sha256'] == hashlib.sha256(request).hexdigest(), 'captured helper and exact declared history')
    require(type(value['rows']) is list and len(value['rows']) == len(request.splitlines()), 'complete captured history')
    for row in value['rows']:
        state(row)
    linked_profile(value['rows'], request.decode('ascii').splitlines())
    return value['rows']


def compare(actual, expected):
    require(len(actual) == len(expected), 'response inventory')
    for index, (left, right) in enumerate(zip(actual, expected)):
        require(S.encoded(left, sorted_keys=True) == S.encoded(right, sorted_keys=True),
                'complete state differs at operation ' + str(index))


def controls(actual, expected):
    count = 0
    for change in ('reverse', 'identity', 'time', 'return'):
        altered = copy.deepcopy(actual)
        if change == 'reverse':
            altered[6]['entries'].reverse()
        elif change == 'identity':
            altered[7]['entries'][0] = 2
        elif change == 'time':
            altered[12]['records'][2][5] += 1
        else:
            altered[7]['result'] = True
        try:
            compare(altered, expected)
        except ValueError:
            count += 1
        else:
            raise ValueError('Scheduled membership: corruption accepted ' + change)
    for raw in (b'{}\n', b'{"result":null,"result":null,"entries":[],"records":[]}\n',
                b'{"result":NaN,"entries":[],"records":[]}\n',
                b'{"result":0,"entries":[],"records":[]}\n'):
        try:
            parse(raw, 1)
        except (ValueError, S.Failure):
            count += 1
        else:
            raise ValueError('Scheduled membership: malformed response accepted')
    try:
        parse(S.encoded(actual[0]), len(actual))
    except ValueError:
        count += 1
    else:
        raise ValueError('Scheduled membership: truncated history accepted')
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--java')
    parser.add_argument('--javac')
    parser.add_argument('--work-dir', type=Path, default=ROOT / '.local/reference/scheduled-membership')
    args = parser.parse_args()
    require(bool(args.java) == bool(args.javac), 'supply both JDK tools')
    request = requests()
    expected = fixture(request)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='membership-', dir=args.work_dir.resolve()))
    report = {'schema': 1, 'scope': SCOPE, 'passed': False, 'processes': []}
    inputs = [args.executable.resolve(strict=True), Path(__file__).resolve(), HELPER, FIXTURE,
              HERE / 'compare_int_set.py', ROOT / 'src/world/schedule/membership.c', ROOT / 'src/world/schedule/entry.c',
              ROOT / 'include/mc/world/schedule/membership.h', ROOT / 'tests/world/schedule/membership_probe.c']
    before = {str(path): S.file_fingerprint(path) for path in inputs}
    report['inputs'] = before
    deadline = time.monotonic() + 120
    try:
        out, err = S.run([str(inputs[0])], request, work=work, name='native', records=report['processes'], deadline=deadline)
        require(not err, 'native stderr')
        actual = parse(out, len(expected))
        compare(actual, expected)
        report['rejection_controls'] = controls(actual, expected)
        if args.java:
            java, javac = shutil.which(args.java), shutil.which(args.javac)
            require(java is not None and javac is not None, 'available JDK tools')
            for key in ('JAVA_TOOL_OPTIONS', 'JDK_JAVA_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVAC_OPTIONS', 'CLASSPATH'):
                require(not os.environ.get(key), 'untracked JDK options')
            jdk_files = S.jdk_inputs(java, javac)
            jdk_before = {name: S.file_fingerprint(path) for name, path in jdk_files.items()}
            report['jdk_inputs'] = jdk_before
            classes, empty = work / 'classes', work / 'empty-lookups'
            classes.mkdir(); empty.mkdir()
            out, err = S.run([javac, '--release', '17', '-encoding', 'UTF-8', '-proc:none', '-implicit:none',
                '-classpath', str(empty), '-sourcepath', str(empty), '-d', str(classes), str(HELPER)],
                work=work, name='compile', records=report['processes'], deadline=deadline)
            require(not out and not err, 'quiet authored-helper compilation')
            class_before = {path.name: S.file_fingerprint(path) for path in classes.iterdir()}
            require(set(class_before) == {'ScheduledMembershipObservation.class', 'ScheduledMembershipObservation$Record.class'},
                    'two authored helper classes')
            report['helper_classes'] = class_before
            for index in range(2):
                out, err = S.run([java, '-cp', str(classes), 'ScheduledMembershipObservation'], request,
                    work=work, name='jdk-' + str(index), records=report['processes'], deadline=deadline)
                require(not err, 'JDK stderr')
                compare(parse(out, len(expected)), expected)
                require({path.name: S.file_fingerprint(path) for path in classes.iterdir()} == class_before, 'helper classes unchanged')
            require(all(S.file_fingerprint(path) == jdk_before[name] for name, path in jdk_files.items()), 'JDK inputs unchanged')
        require(all(S.file_fingerprint(path) == before[str(path)] for path in inputs), 'replay inputs unchanged')
        report.update(passed=True, responses=len(actual))
        print('PASS: scheduled membership;', len(actual), 'complete JDK/native responses;', report['rejection_controls'], 'rejection controls')
    finally:
        (work / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()
