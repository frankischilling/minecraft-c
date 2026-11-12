"""Finite direct public-K controls; no natural owned delayed-death claim."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[3]
LIMIT = 1024 ** 2
TIMEOUT = 240
# set size, final size, health, callbacks, damage amount, exception, final remote
PROFILES = {
    'split': (2, 2, 0, 4, 4, 'NONE', False),
    'negative-health': (2, 2, -1, 0, 5, 'NONE', False),
    'direct-split': (2, 2, 0, 4, 0, 'NONE', False),
    'size-one': (1, 1, 0, 0, 0, 'NONE', False),
    'size-zero': (0, 0, 0, 0, 0, 'NONE', False),
    'odd-size': (3, 3, 0, 4, 0, 'NONE', False),
    'signed-size': (128, -128, 0, 0, 0, 'NONE', False),
    'negative-size': (-1, -1, 0, 0, 0, 'NONE', False),
    'remote': (2, 2, 0, 0, 0, 'NONE', True),
    'positive-health': (2, 2, 4, 0, 0, 'NONE', False),
    'null-world': (1, 1, 1, 0, 0, 'NPE', None),
    'watcher-type': (2, None, 0, 0, 0, 'CCE', None),
    'callback-pose': (2, 2, 0, 4, 0, 'NONE', False),
    'callback-size': (2, 4, 16, 4, 0, 'NONE', False),
    'callback-reseed': (2, 2, 0, 4, 0, 'NONE', False),
    'callback-null-world': (2, 2, 0, 1, 0, 'NPE', None),
}


def need(value, message):
    if not value:
        raise ValueError(message)


def same(left, right):
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(same(left[key], right[key]) for key in left)
    if isinstance(left, list):
        return len(left) == len(right) and all(same(a, b) for a, b in zip(left, right))
    return left == right


def decode_json(raw):
    def pairs(values):
        result = {}
        for key, value in values:
            need(key not in result, 'Duplicate Slime observation key')
            result[key] = value
        return result
    def invalid(value):
        raise ValueError('Nonfinite Slime observation: ' + value)
    return json.loads(raw, object_pairs_hook=pairs, parse_constant=invalid)


def expected_observation(mode):
    _, _, health, children, amount, exception, _ = PROFILES[mode]
    failure = exception != 'NONE'
    loaded = children + (0 if mode in ('null-world', 'watcher-type') else 1)
    pc = 1 if exception == 'CCE' else 9 if mode == 'null-world' else 129 if failure else 143
    return {'prepared': 1, 'damage_status': 0, 'accepted': int(amount != 0), 'amount': amount,
            'health': health, 'K_status': 10 if failure else 0,
            'K_cause': 13 if exception == 'CCE' else 2 if failure else 0, 'K_pc': pc,
            'children': children, 'world': loaded, 'chunk': loaded, 'dead': int(not failure),
            'random_matches': 1, 'callback_valid': 1, 'cleanup': 1}


def observation(line, mode):
    names = ('prepared', 'damage_status', 'accepted', 'amount', 'health', 'K_status',
             'K_cause', 'K_pc', 'children', 'world', 'chunk', 'dead', 'random_matches',
             'callback_valid', 'cleanup')
    pattern = (r'SLIME_REMOVAL_OBSERVATION mode=' + re.escape(mode) +
               ''.join(' ' + key + r'=(-?[0-9]+)' for key in names))
    match = re.fullmatch(pattern, line)
    need(match is not None, 'One exact ordered Slime summary')
    actual = dict(zip(names, map(int, match.groups())))
    need(same(actual, expected_observation(mode)), 'Exact successful Slime summary values')
    return actual


def hexword(value, digits):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(digits) + '}', value) is not None


def dbits(value):
    return struct.pack('>d', value).hex()


def validate_row(row, mode):
    keys = {'profile', 'step', 'set_size', 'size', 'health', 'remote', 'exception', 'dead',
            'random', 'world_loaded', 'children'}
    need(type(row) is dict and row.keys() == keys, 'Closed Slime row schema')
    size, final_size, health, count, _, exception, remote = PROFILES[mode]
    expected = {'profile': mode, 'step': 0, 'set_size': size, 'size': final_size,
                'health': health, 'remote': remote, 'exception': exception,
                'dead': exception == 'NONE',
                'world_loaded': count + (0 if mode in ('null-world', 'watcher-type') else 1)}
    need(all(same(row[key], value) for key, value in expected.items()),
         'Typed exact Slime profile operands and outcome')
    need(hexword(row['random'], 16) and int(row['random'], 16) < 2 ** 48,
         'Exact finite parent Random word')
    need(type(row['children']) is list and len(row['children']) == count,
         'Exact actual child occurrence count')
    for index, child in enumerate(row['children']):
        child_keys = {'index', 'size', 'health', 'position', 'yaw', 'pitch', 'world',
                      'world_member', 'chunk_members', 'parent_dead', 'random_after'}
        need(type(child) is dict and child.keys() == child_keys, 'Closed child schema')
        moved = mode == 'callback-pose' and index > 0
        offset = .375 if mode == 'odd-size' else .25
        expected_child = {'index': index, 'size': 1, 'health': 1, 'pitch': '00000000',
                          'world': 'PRIMARY', 'world_member': True, 'chunk_members': 1,
                          'parent_dead': False,
                          'position': [dbits((10.5 if moved else 8.5) +
                                             (-offset if index % 2 == 0 else offset)),
                                       dbits(65.5 if moved else 64.5),
                                       dbits((9.5 if moved else 8.5) +
                                             (-offset if index // 2 == 0 else offset))]}
        need(all(same(child[key], value) for key, value in expected_child.items()),
             'Typed child order, raw position, ownership and parent removal prefix')
        need(hexword(child['yaw'], 8) and hexword(child['random_after'], 16) and
             int(child['random_after'], 16) < 2 ** 48, 'Raw callback yaw and Random words')
    return row


def parse_protocol(raw, mode):
    need(type(raw) is bytes and mode in PROFILES, 'Typed admitted Slime protocol')
    if b'\r' in raw:
        pairs = raw.count(b'\r\n')
        need(pairs == raw.count(b'\r') == raw.count(b'\n'), 'Uniform CRLF Slime protocol')
        raw = raw.replace(b'\r\n', b'\n')
    lines = raw.decode('ascii', errors='strict').split('\n')
    need(len(lines) == 7 and lines[-1] == '', 'Exactly six complete Slime lines')
    need(lines[0] == 'SLIME_REMOVAL_BEGIN mode=' + mode and
         lines[1:3] == ['16 achievements', '151 recipes'], 'Exact entry and boot prefix')
    need(lines[3].startswith('SLIME_REMOVAL '), 'One actual JSON observation position')
    row = validate_row(decode_json(lines[3][len('SLIME_REMOVAL '):]), mode)
    summary = observation(lines[4], mode)
    footer = re.fullmatch(r'PASS: ([1-9][0-9]{0,9}) actual Slime removal checks;mode=' +
                          re.escape(mode), lines[5])
    need(footer is not None and int(footer.group(1)) <= 4294967295, 'Exact positive native footer')
    return row, summary, int(footer.group(1))


def self_test():
    positives = refused = 0
    for mode, profile in PROFILES.items():
        size, final_size, health, count, _, exception, remote = profile
        row = {'profile': mode, 'step': 0, 'set_size': size, 'size': final_size,
               'health': health, 'remote': remote, 'exception': exception,
               'dead': exception == 'NONE', 'random': '0000000000000001',
               'world_loaded': expected_observation(mode)['world'], 'children': []}
        for index in range(count):
            moved = mode == 'callback-pose' and index > 0
            offset = .375 if mode == 'odd-size' else .25
            row['children'].append({
                'index': index, 'size': 1, 'health': 1,
                'position': [dbits((10.5 if moved else 8.5) +
                                   (-offset if index % 2 == 0 else offset)),
                             dbits(65.5 if moved else 64.5),
                             dbits((9.5 if moved else 8.5) +
                                   (-offset if index // 2 == 0 else offset))],
                'yaw': '00000000', 'pitch': '00000000', 'world': 'PRIMARY',
                'world_member': True, 'chunk_members': 1, 'parent_dead': False,
                'random_after': '0000000000000001'})
        summary = ('SLIME_REMOVAL_OBSERVATION mode=' + mode + ''.join(
            ' ' + key + '=' + str(value) for key, value in expected_observation(mode).items()))
        lines = ['SLIME_REMOVAL_BEGIN mode=' + mode, '16 achievements', '151 recipes',
                 'SLIME_REMOVAL ' + json.dumps(row), summary,
                 'PASS: 1 actual Slime removal checks;mode=' + mode]
        good = ('\n'.join(lines) + '\n').encode('ascii')
        for ending in (b'\n', b'\r\n'):
            candidate = good.replace(b'\n', ending)
            before = candidate
            parsed = parse_protocol(candidate, mode)
            need(candidate == before and same(parsed[0], row), 'Pure positive protocol control')
            positives += 1
        wrong = copy.deepcopy(row)
        wrong['dead'] = not wrong['dead']
        bad = [good[:-1], good + b'\n', good + b'trailing', b'prefix\n' + good,
               good.replace(b'\n', b'\r\n', 1), good.replace(b'\n', b'\r', 1),
               good.replace(b'16 achievements', b'17 achievements'),
               good.replace(b'"step": 0', b'"step": true'),
               good.replace(b'"step": 0', b'"step": 0, "step": 0'),
               good.replace(b'"random": "0000000000000001"', b'"random": NaN'),
               good.replace(b'PASS: 1 ', b'PASS: 0 '),
               good.replace(b'PASS: 1 ', b'PASS: 4294967296 '),
               good.replace(b' cleanup=1', b' cleanup=0'),
               good.replace(json.dumps(row).encode('ascii'), json.dumps(wrong).encode('ascii'))]
        for candidate in bad:
            try:
                parse_protocol(candidate, mode)
            except (ValueError, UnicodeError, TypeError):
                refused += 1
            else:
                raise ValueError('Malformed Slime protocol accepted')
    print(f'PASS: {positives} Slime protocol positives; {refused} malformed streams refused')


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=tuple(PROFILES))
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    need(args.mode is not None and args.executable is not None and args.work_dir is not None,
         'Slime mode, executable and work directory required')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='slime-removal-', dir=args.work_dir)).resolve()
    root = out / 'worlds'
    root.mkdir()
    report = {'passed': False, 'mode': args.mode,
              'scope': 'Actual public damage/K or prepared direct K; no natural owned delayed death'}
    guarded = []
    before = child = None

    def pins():
        result = {}
        for path in guarded:
            need(path.is_file() and not path.is_symlink(), 'Regular guarded Slime source/product')
            result[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        return result

    try:
        executable = args.executable.resolve(strict=True)
        guarded = [executable, Path(__file__).resolve()] + [ROOT / name for name in (
            'tests/world/runtime/test_slime_removal.c',
            'tests/world/runtime/test_entity_tick.c',
            'tests/world/runtime/test_entity_publication.c',
            'tests/reference/client/statistics/support.py',
            'tests/tools/world/test_world_query_controls.py',
            'src/entity/mob/removal.c', 'include/mc/entity/mob/removal.h',
            'src/entity/mob/removal_internal.h',
            'src/entity/movement/effects.c', 'src/entity/mob/construction.c',
            'src/entity/living/construction.c', 'src/entity/entity.c',
            'src/entity/player/construction.c', 'src/entity/position.c',
            'src/world/runtime/entity_publication.c')]
        before = pins()
        report['guard_before_sha256'] = before
        command = [str(executable), args.mode, str(root).encode('utf-8').hex()]
        report['command'] = command
        support = load('slime_removal_capture', 'tests/reference/client/statistics/support.py')
        child = support.run_bounded(command, out, 'native', timeout=TIMEOUT,
                                    limit=LIMIT, env=os.environ.copy())
        report['exit'] = child.returncode
        report['capture'] = child.capture_report
        need(type(child.returncode) is int and child.returncode == 0 and
             type(child.stderr) is bytes and child.stderr == b'', 'Actual native completion')
        row, summary, count = parse_protocol(child.stdout, args.mode)
        report.update({'observations': [row], 'summary': summary, 'checks': count})
        report['passed'] = True
    except BaseException as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
    finally:
        if child is not None:
            try:
                controls = load('slime_removal_retention',
                                'tests/tools/world/test_world_query_controls.py')
                controls.verify_retained(child, {name: (out / name).read_bytes() for name in
                                                ('native.stdout', 'native.stderr',
                                                 'native.capture.json')})
                report['retained_capture_verified'] = True
            except BaseException as error:
                report['passed'] = False
                report['retention_failure'] = {'type': type(error).__name__, 'message': str(error)}
        if before is not None:
            try:
                report['guard_after_sha256'] = pins()
                report['guard_unchanged'] = same(before, report['guard_after_sha256'])
                need(report['guard_unchanged'], 'Slime fixture/executable changed')
            except BaseException as error:
                report['passed'] = False
                report['guard_failure'] = {'type': type(error).__name__, 'message': str(error)}
        with (out / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    if child is not None:
        print(child.stdout.decode('utf-8', errors='backslashreplace'), end='')
        if child.stderr:
            print(child.stderr.decode('utf-8', errors='backslashreplace'), end='', file=sys.stderr)
    if not report['passed']:
        print('Slime actual removal control failed: ' + str(out), file=sys.stderr)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
