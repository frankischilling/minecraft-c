"""Portable source-trace, private-stream and corruption-refusal controls."""
import copy
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
from types import SimpleNamespace
import sys

import packet as P
from observation import MASK, MULTIPLIER, f32
import run_native as N


def refused(action):
    try:
        action()
    except (ValueError, TypeError, KeyError, IndexError, OSError, AttributeError):
        return
    raise AssertionError('changed evidence was admitted')


def entropy(value):
    checks = 0
    for run in value['entropy']:
        P.need([row['case'] for row in run] == list(range(32)), 'complete independent stream evidence')
        for row in run:
            P.need(row['world_before'] == row['world_after'] and row['math_before'] == row['math_after'], 'World/Math preserved')
            checks += 2
            P.need(len(row['sounds']) <= 1, 'at most one actual hurt sound')
            for entity in row['entities']:
                before, after = entity['before'], entity['after']
                P.need(type(before) is list and len(before) == 3 and type(before[0]) is str and
                       re.fullmatch('[0-9a-f]{16}', before[0]) is not None and
                       0 <= int(before[0], 16) <= MASK and before[1:] == [False, '0000000000000000'], 'actual default private Random shape')
                state = int(before[0], 16)
                draws = []
                for _ in range(2 if entity['id'] == 0 and row['sounds'] else 0):
                    state = (state * MULTIPLIER + 11) & MASK
                    draws.append((state >> 24) / 16777216.0)
                P.need(after == [f'{state:016x}', False, '0000000000000000'], 'exact per-run private consumption')
                if draws:
                    pitch = struct.pack('>f', f32(f32(f32(draws[0] - draws[1]) * f32(.2)) + f32(1))).hex()
                    P.need(row['sounds'][0]['pitch'] == pitch, 'independent actual hurt pitch')
                    checks += 1
                checks += 1
    P.need(any(left['entities'][0]['before'] != right['entities'][0]['before']
               for left, right in zip(*value['entropy'])), 'independent unseeded default constructor inputs retained')
    return checks + 1


def traces(value):
    normalized = copy.deepcopy(value['traces'])
    for run_index, run in enumerate(normalized):
        for row in run:
            for wrapped in row['events']:
                event = wrapped['value']
                if event['kind'] == 'ENTRY' and event['site']['class'] == 'fd' and event['site']['descriptor'] == '(Lsn;Ljava/lang/String;FF)V':
                    sound = value['entropy'][run_index][row['case']]['sounds']
                    P.need(len(sound) == 1 and event['arguments'][3] == {'F': sound[0]['pitch']}, 'actual sound call uses this run private pitch')
                    event['arguments'][3] = 'PRIVATE_PITCH_VALIDATED'
    P.need(P.exact(normalized[0], normalized[1]), 'independent actual entry/return traces agree')
    count = 0
    for rows in value['traces']:
        P.need([row['case'] for row in rows] == list(range(32)), 'complete passive call inventory')
        for row in rows:
            expected = value['rows'][row['case']]
            events = [event['value'] for event in row['events']]
            root = '(Lsn;F)V'
            entered = [event for event in events if event['kind'] == 'ENTRY' and event['site']['class'] == 'gz'
                       and event['site']['method'] == 'a' and event['site']['descriptor'] == root]
            returned = [event for event in events if event['kind'] == 'RETURN' and event['site']['class'] == 'gz'
                        and event['site']['method'] == 'a' and event['site']['descriptor'] == root]
            P.need(len(entered) == 1 and len(returned) == int(expected['returned']), 'actual whole parent return or exception')
            from observation import parent_receipt
            P.need(P.exact(parent_receipt(row), expected['parent']), 'actual hurt call/result and parent return receipt')
            stores = [event for event in events if event['kind'] == 'STORE' and event['site']['class'] == 'gz']
            P.need(len(stores) == int(expected['parent']['hurt_invoked']) and
                   all(event['site']['pc'] == 52 and event['field'] == 'ls.ae' and event['value'] == 20 for event in stores), 'actual cooldown20 store before every hurt invocation')
            if expected['parent']['hurt_invoked']:
                P.need(expected['parent']['hurt_damage'] == (0 if expected['input'][12] else 5), 'canonical UZ5 versus declared zero input')
            helpers = [event for event in events if event['kind'] == 'ENTRY' and event['site']['class'] == 'gs'
                       and event['site']['descriptor'] == '(Lls;Z)V']
            if helpers:
                P.need(len(helpers) == 1 and helpers[0]['arguments'][1] is False, 'actual damage-false/direct helper argument')
                if not helpers[0]['arguments'][1]:
                    P.need(not any(event['kind'] == 'ENTRY' and event['site']['class'] == 'gi' and event['site']['method'] == 'B'
                                   for event in events), 'false flag bypasses sitting getter entirely')
            if expected['exception'] != 'NONE':
                P.need(any(event['kind'] == 'EXCEPTION' and event['class'] == expected['exception'] for event in events), 'actual first source cause retained')
            count += len(events)
    return count


def capture_controls():
    lines = []
    for index in range(32):
        lines += [b'16 achievements', b'151 recipes', ('MONSTER_DAMAGE_NATIVE case=' + str(index)).encode('ascii')]
    lines += [b'Monster damage: 1 checks;32 genuine profiles']
    stdout = b'\n'.join(lines) + b'\n'
    capture = {'exit': 0, 'timed_out': False, 'output_overflow': False, 'pipe_capture_complete': True,
               'reaped': True, 'reader_errors': {}, 'timeout_seconds': N.TIMEOUT, 'limit': N.STREAM_LIMIT,
               'stdout_bytes': len(stdout), 'stderr_bytes': 0}
    positive = SimpleNamespace(returncode=0, stdout=stdout, stderr=b'', capture_report=capture)
    P.need(N.completed_native(positive) == 1, 'authored completion-parser positive')
    rejected = 0
    for name, wrong in (('exit', 1), ('exit', False), ('timed_out', True), ('output_overflow', True),
                        ('pipe_capture_complete', False), ('reaped', False), ('reader_errors', {'stdout': []}),
                        ('timeout_seconds', 1), ('limit', 0), ('stdout_bytes', 0), ('stderr_bytes', 1)):
        bad = copy.deepcopy(positive); bad.capture_report[name] = wrong
        refused(lambda bad=bad: N.completed_native(bad)); rejected += 1
    for name, wrong in (('returncode', 3), ('returncode', False), ('stderr', b'warning\n')):
        bad = copy.deepcopy(positive); setattr(bad, name, wrong)
        bad.capture_report['stderr_bytes'] = len(bad.stderr)
        refused(lambda bad=bad: N.completed_native(bad)); rejected += 1
    changed = [lines[:-1], lines + [lines[-1]], lines[:2] + [lines[2]] + lines[2:],
               [b'unexpected'] + lines, lines[1:], list(reversed(lines)),
               [*lines[:2], b'MONSTER_DAMAGE_NATIVE case=1', *lines[3:]],
               [lines[1], lines[0], *lines[2:]], [*lines[:-1], b'Monster damage: 0 checks;32 genuine profiles']]
    for data in changed:
        bad = copy.deepcopy(positive); bad.stdout = b'\n'.join(data) + b'\n'
        bad.capture_report['stdout_bytes'] = len(bad.stdout)
        refused(lambda bad=bad: N.completed_native(bad)); rejected += 1
    bad = copy.deepcopy(positive); bad.stdout = bad.stdout[:-1]; bad.capture_report['stdout_bytes'] -= 1
    refused(lambda: N.completed_native(bad)); rejected += 1
    support = N.module('monster_capture_control_support', N.ROOT / 'tests/reference/client/statistics/support.py')
    with tempfile.TemporaryDirectory(prefix='monster-bounded-capture-') as temporary:
        folder = Path(temporary)
        child = support.run_bounded([sys.executable, '-c', "import sys;print('partial',flush=True);sys.exit(3)"], folder, 'nonzero')
        P.need(child.returncode == 3 and child.stdout.replace(b'\r\n', b'\n') == b'partial\n' and child.capture_report['reaped'], 'real nonzero child output retained')
        for label, code, timeout, limit, reason in (
            ('timeout', "import time;print('partial',flush=True);time.sleep(5)", .3, 256, 'timed_out'),
            ('overflow', "import os;os.write(1,b'x'*4096)", 10, 256, 'output_overflow')):
            try:
                support.run_bounded([sys.executable, '-c', code], folder, label, timeout=timeout, limit=limit)
            except ValueError as failure:
                receipt = getattr(failure, 'capture_report', None)
                P.need(receipt is not None and receipt[reason] is True and receipt['reaped'] is True and
                       receipt['stdout_bytes'] <= limit and (folder / (label + '.stdout')).stat().st_size == receipt['stdout_bytes'], 'real bounded refusal retained')
                if label == 'timeout':
                    P.need((folder / 'timeout.stdout').read_bytes().replace(b'\r\n', b'\n') == b'partial\n', 'timeout preserves emitted prefix')
            else:
                raise AssertionError('real timeout/overflow was admitted')
            rejected += 1
    return rejected


def main():
    value, provenance = P.read()
    stream_checks = entropy(value)
    event_count = traces(value)
    rejected = capture_controls()
    for original in value['rows']:
        for key in original:
            if original[key] is None:
                continue
            bad = copy.deepcopy(original)
            bad[key] = None
            refused(lambda bad=bad: P.compare_row(original, bad)); rejected += 1
        for phase in ('before', 'after'):
            for key in original[phase]:
                if original[phase][key] is None:
                    continue
                bad = copy.deepcopy(original)
                bad[phase][key] = None
                refused(lambda bad=bad: P.compare_row(original, bad)); rejected += 1
    for bad in ([], value['rows'][:-1], value['rows'][::-1], value['rows'] + value['rows'][:1]):
        refused(lambda bad=bad: P.compare_rows(value['rows'], bad)); rejected += 1
    for text in ('{"a":0,"a":1}', '{"a":NaN}', '{"a":Infinity}'):
        refused(lambda text=text: P.loads(text)); rejected += 1
    with tempfile.TemporaryDirectory(prefix='monster-damage-packet-') as temporary:
        folder = Path(temporary)
        for name in (*P.SOURCES, 'finite.json', 'provenance.json', 'capture-receipt.json'):
            shutil.copyfile(P.HERE / name, folder / name)
        P.read(folder)
        for name in (*P.SOURCES, 'finite.json', 'capture-receipt.json'):
            path = folder / name
            original = path.read_bytes(); path.write_bytes(original + b' ')
            refused(lambda: P.read(folder)); rejected += 1
            path.write_bytes(original)
        original = (folder / 'provenance.json').read_bytes()
        for name in provenance:
            bad = copy.deepcopy(provenance); bad[name] = None
            (folder / 'provenance.json').write_text(json.dumps(bad), encoding='utf-8')
            refused(lambda: P.read(folder)); rejected += 1
        (folder / 'provenance.json').write_bytes(original)
        with (folder / 'finite.json').open('wb') as output:
            output.truncate(8 * 1024 * 1024 + 1)
        refused(lambda: P.read(folder)); rejected += 1
    print(f'Monster damage: {stream_checks} stream checks; {event_count} passive events; {P.scalars(value["rows"])} selected scalars; {rejected} corruption refusals')


if __name__ == '__main__':
    main()
