"""Compare prepared GS/CW prefix rows; keep constructed native controls separate."""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / 'tests/reference/entity/player_sleep'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def selected(row):
    after = row['cw_after']
    return {'case': row['case'], 'outcome': row['outcome'], 'pc': row['site']['pc'],
            'result': '-' if row['result'] is None else row['result']['static_field'],
            'preserved': row['preserved'], 'cw_initialized': after['initialized'],
            'values_length': 5 if after['initialized'] else 0,
            'identity_matrix': after['identity_matrix'],
            'statics': [None if value is None else {
                'name_utf16': [ord(c) for c in value['name']], 'ordinal': value['ordinal'],
                'values_same': after['values_same'][index]}
                for index, value in enumerate(after['statics'])]}


def native_rows(stdout):
    lines = stdout.splitlines()
    footer = re.fullmatch(r'PASS: ([1-9][0-9]*) Player sleep-prefix/CW checks', lines[-1] if lines else '')
    require(footer is not None, 'completed native sleep/CW fixture marker')
    rows = []
    # Startup output remains outside the finite scalar comparison.
    position = 0
    while position < len(lines) - 1 and lines[position] in ('16 achievements', '151 recipes'):
        position += 1
    for index in range(48):
        require(position + 7 <= len(lines) - 1, 'complete native sleep row extent')
        head = re.fullmatch(r'SLEEP_CASE ([0-9]+) (RETURN|SOURCE_NULL|FRONTIER119) ([0-9]+) ([a-e-]) ([01]) ([01]) ([05])', lines[position])
        require(head is not None and int(head[1]) == index, 'native sleep case schema/order')
        identity = re.fullmatch(r'SLEEP_IDENT ([0-9]+) ([01]{25})', lines[position + 1])
        require(identity is not None and int(identity[1]) == index, 'native CW identity matrix schema/order')
        statics = []
        for slot in range(5):
            item = re.fullmatch(r'SLEEP_STATIC ([0-9]+) ([0-4]) (-|(?:[0-9a-f]{4})+) (-1|[0-4]) ([01]) ([01])', lines[position + 2 + slot])
            require(item is not None and int(item[1]) == index and int(item[2]) == slot,
                    'native CW static schema/order')
            if item[3] == '-':
                require(item.groups()[3:] == ('-1', '0', '0'), 'native cold CW static shape')
                statics.append(None)
            else:
                require(item[4] != '-1' and item[5] == '1', 'native CW canonical candidate invariant')
                statics.append({'name_utf16': [int(item[3][i:i+4], 16) for i in range(0, len(item[3]), 4)],
                                'ordinal': int(item[4]), 'values_same': item[6] == '1'})
        matrix = [[identity[2][i * 5 + j] == '1' for j in range(5)] for i in range(5)]
        rows.append({'case': index, 'outcome': head[2], 'pc': int(head[3]), 'result': head[4],
                     'preserved': head[5] == '1', 'cw_initialized': head[6] == '1',
                     'values_length': int(head[7]), 'identity_matrix': matrix, 'statics': statics})
        position += 7
    require(position == len(lines) - 1, 'no extra native sleep rows/output')
    return rows, int(footer[1])


def compare(actual, expected):
    comparisons = 0
    def visit(a, b, path):
        nonlocal comparisons
        require(type(a) is type(b), 'sleep comparison type: ' + path)
        if isinstance(b, dict):
            require(a.keys() == b.keys(), 'sleep comparison keys: ' + path)
            for key in b:
                visit(a[key], b[key], path + '/' + key)
        elif isinstance(b, list):
            require(len(a) == len(b), 'sleep comparison sequence: ' + path)
            for index, (left, right) in enumerate(zip(a, b)):
                visit(left, right, path + '/' + str(index))
        else:
            comparisons += 1
            require(a == b, 'sleep scalar differs: ' + path + ': native=' + str(a) + '; original=' + str(b))
    visit(actual, expected, 'rows')
    return comparisons


def controls(expected):
    """Parser/comparison controls; authored rows never serve as execution evidence."""
    encoded = []
    for row in expected:
        encoded.append('SLEEP_CASE {} {} {} {} {} {} {}'.format(row['case'], row['outcome'], row['pc'],
            row['result'], int(row['preserved']), int(row['cw_initialized']), row['values_length']))
        encoded.append('SLEEP_IDENT {} {}'.format(row['case'], ''.join(str(int(x)) for line in row['identity_matrix'] for x in line)))
        for index, value in enumerate(row['statics']):
            text = '- -1 0 0' if value is None else '{} {} 1 {}'.format(
                ''.join(f'{unit:04x}' for unit in value['name_utf16']), value['ordinal'], int(value['values_same']))
            encoded.append(f'SLEEP_STATIC {row["case"]} {index} {text}')
    encoded.append('PASS: 1 Player sleep-prefix/CW checks')
    baseline = '\n'.join(encoded) + '\n'
    parsed, count = native_rows(baseline)
    require(count == 1 and compare(parsed, expected) > 0, 'positive authored parser control')
    rejected = 0
    def rejects(text):
        nonlocal rejected
        try:
            rows, _ = native_rows(text)
            compare(rows, expected)
        except (AssertionError, ValueError):
            rejected += 1
            return
        raise AssertionError('corrupted native sleep projection accepted')
    rejects('\n'.join(encoded[:-1]) + '\n')
    rejects(baseline + encoded[0] + '\n')
    rejects('\n'.join(encoded[:3] + encoded[4:]) + '\n')
    rejects(baseline.replace('SLEEP_CASE 0 ', 'SLEEP_CASE 1 ', 1))
    rejects(baseline.replace('SLEEP_IDENT 0 ', 'SLEEP_IDENT 1 ', 1))
    rejects(baseline.replace('SLEEP_STATIC 0 0 ', 'SLEEP_STATIC 0 1 ', 1))
    def changed_line(index, transform):
        lines = encoded.copy()
        lines[index] = transform(lines[index])
        rejects('\n'.join(lines) + '\n')
    changed_line(0, lambda line: line.replace('SOURCE_NULL', 'RETURN'))
    changed_line(0, lambda line: line.replace(' 4 ', ' 35 ', 1))
    changed_line(1, lambda line: line[:-1] + ('0' if line[-1] == '1' else '1'))
    changed_line(2, lambda line: line.replace('- -1 0 0', '- -1 1 0'))
    ready = next(index for index, row in enumerate(expected) if row['cw_initialized'])
    head = ready * 7
    def token(line, index, value):
        words = line.split(); words[index] = value
        return ' '.join(words)
    changed_line(head, lambda line: token(line, 4, '-'))
    changed_line(head, lambda line: token(line, 5, '0'))
    changed_line(head, lambda line: token(line, 6, '0'))
    changed_line(head, lambda line: token(line, 7, '0'))
    changed_line(head + 2, lambda line: token(line, 3, '0058'))
    changed_line(head + 2, lambda line: token(line, 4, '4'))
    changed_line(head + 2, lambda line: token(line, 5, '0'))
    changed_line(head + 2, lambda line: token(line, 6, '0'))
    changed_line(head + 2, lambda line: line.upper())
    for case, row in enumerate(expected):
        changed = copy.deepcopy(row)
        changed['preserved'] = not changed['preserved']
        altered = parsed.copy(); altered[case] = changed
        try:
            compare(altered, expected)
        except AssertionError:
            rejected += 1
        else:
            raise AssertionError('changed native source-state preservation accepted')
    return rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--controls', action='store_true')
    args = parser.parse_args()
    packet = module('player_sleep_original_packet', PACKET / 'packet.py')
    original = packet.read_reference()
    expected = [selected(row) for row in original['rows']]
    require(len(expected) == 48, 'complete original sleep finite extent')
    if args.controls:
        require(args.executable is None and args.work_dir is None, 'pure sleep reader controls take no executable')
        print('Player sleep native reader: ' + str(controls(expected)) + ' rejection controls')
        return
    require(args.executable is not None and args.work_dir is not None, 'sleep executable/work directory required')
    process = module('player_sleep_process_capture', ROOT / 'tests/reference/client/controller/capture.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-sleep-', dir=args.work_dir.resolve()))
    # One closure snapshot brackets the full fixture and original comparison.
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format'}
    for folder in ('src', 'include', 'cmake', 'tests/entity', 'tests/world/runtime',
                   'tests/reference/entity/player_sleep', 'tests/reference/client/controller', 'tests/support'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.inc', '.cmake', '.py', '.json', '.java', '.txt', '.tsv'))
    paths.update(executable.parent.glob('*.dll'))
    before = {str(path): process.digest(path) for path in sorted(paths)}
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before_sha256': before,
              'original_rows': 0, 'original_draw': False, 'night_success': False}
    environment = os.environ.copy()
    environment['ALSOFT_DRIVERS'] = 'null'
    try:
        receipt = process.capture([str(executable), 'sleep-reference',
            str(output / 'worlds').encode('utf-8').hex(), str((PACKET / 'profiles.tsv').resolve())],
            output, 'native', 120, environment, output)
        report['native'] = receipt
        process.require_success(receipt, empty_stderr=True)
        actual, checks = native_rows((output / receipt['stdout_file']).read_text(encoding='utf-8'))
        report['scalar_comparisons'] = compare(actual, expected)
        report['native_checks'] = checks
        # Re-read the actual strict packet after execution, too.
        require(packet.read_reference() == original, 'original sleep packet changed during replay')
        report['original_rows'] = len(actual)
        report['status'] = 'PASS'
    finally:
        after = {str(path): process.digest(path) for path in sorted(paths)}
        report['inputs_after_sha256'] = after
        if after != before:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'sleep replay inputs changed; report=' + str(output / 'report.json'))
    print('Player sleep: ' + str(report['scalar_comparisons']) + ' original scalar comparisons; report=' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
