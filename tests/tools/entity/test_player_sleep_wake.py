"""Compare76 prepared genuine sleep/wake component states, excluding GUI/ticks."""
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

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / 'tests/reference/entity/player_sleep_wake'
PLANES = {'blocks': 32768, 'metadata': 16384, 'sky': 16384,
          'block_light': 16384, 'height': 256}
STATE_KEYS = ('position', 'motion', 'dimensions', 'sleep_offsets', 'sleeping',
              'timer', 'v', 'Q', 'private_bN', 'bounds', 'old_bounds', 'world_J',
              'remote', 'world_players', 'world_entities', 'other_sleeping',
              'world_same', 'bounds_same', 'v_same', 'Q_same', 'Q_v_same',
              'Q_before_v_same', 'private_bN_same', 'head', 'planes')
ROW_KEYS = ('case', 'input', 'result', 'exception', 'before', 'after',
            'unselected_actor_preserved', 'unselected_world_preserved',
            'random_before', 'random_after', 'source_failure_site')


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def pairs(entries):
    result = {}
    for key, value in entries:
        require(key not in result, 'duplicate native JSON key')
        result[key] = value
    return result


def loads(text):
    def invalid(value):
        raise AssertionError('nonfinite native JSON token: ' + value)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)


def keys(value, wanted, label):
    require(type(value) is dict and set(value) == set(wanted), label + ' closed keys')


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def sequence(value, length, predicate, label):
    require(type(value) is list and len(value) == length and all(predicate(x) for x in value), label)


def words(value, length, width, label):
    sequence(value, length, lambda x: type(x) is str and
             re.fullmatch('[0-9a-f]{' + str(width) + '}', x) is not None, label)


def plane_digest(runs, length):
    require(type(runs) is list and len(runs) <= length, 'bounded native plane runs')
    plane = bytearray(length)
    previous_end = 0
    previous_value = None
    for run in runs:
        sequence(run, 3, lambda x: type(x) is int, 'native plane run triple')
        start, count, value = run
        require(0 <= start < length and 1 <= count <= length - start and
                1 <= value <= 255 and start >= previous_end,
                'native nonzero ordered plane run extent')
        require(start != previous_end or value != previous_value,
                'native plane runs must be maximal')
        plane[start:start + count] = bytes([value]) * count
        previous_end, previous_value = start + count, value
    return hashlib.sha256(plane).hexdigest()


def state(value):
    keys(value, STATE_KEYS, 'native selected state')
    words(value['position'], 3, 16, 'native position raw doubles')
    words(value['motion'], 3, 16, 'native motion raw doubles')
    words(value['dimensions'], 3, 8, 'native dimensions raw floats')
    words(value['sleep_offsets'], 2, 8, 'native sleep offsets raw floats')
    for key in ('sleeping', 'world_J', 'remote', 'world_same', 'bounds_same',
                'v_same', 'Q_same', 'Q_v_same', 'Q_before_v_same', 'private_bN_same'):
        require(type(value[key]) is bool, 'native Boolean: ' + key)
    require(integer(value['timer'], -(2**31), 2**31 - 1), 'native signed timer')
    for key in ('v', 'Q', 'private_bN'):
        if value[key] is not None:
            sequence(value[key], 3, lambda x: integer(x, -(2**31), 2**31 - 1),
                     'native nullable coordinates: ' + key)
    if value['bounds'] is not None:
        words(value['bounds'], 6, 16, 'native nullable bounds')
    words(value['old_bounds'], 6, 16, 'native rooted original bounds')
    require(integer(value['world_players'], 1, 2) and integer(value['world_entities'], 1, 2),
            'native actual World list counts')
    require(value['other_sleeping'] is None or type(value['other_sleeping']) is bool,
            'native optional other Player sleeping')
    sequence(value['head'], 2, lambda x: type(x) is int, 'native head ID/metadata')
    require(0 <= value['head'][0] <= 255 and 0 <= value['head'][1] <= 15, 'native head values')
    keys(value['planes'], PLANES, 'native full Chunk planes')
    result = copy.deepcopy(value)
    result['planes'] = {name: plane_digest(value['planes'][name], length)
                        for name, length in PLANES.items()}
    return result


def random_state(value):
    require(type(value) is list and len(value) == 2, 'native actor/World Random pair')
    for row in value:
        require(type(row) is list and len(row) == 3 and type(row[1]) is bool,
                'native Random state shape')
        words([row[0], row[2]], 2, 16, 'native Random raw words')
        require(int(row[0], 16) < 2**48, 'native Java Random48 state')


def native_rows(stdout):
    require(type(stdout) is str and len(stdout.encode('utf-8')) <= 4 * 1024 * 1024,
            'bounded native sleep/wake transport')
    lines = stdout.splitlines()
    footer = re.fullmatch(r'PASS: ([1-9][0-9]*) Player sleep/wake component checks',
                          lines[-1] if lines else '')
    require(footer is not None, 'completed native sleep/wake marker')
    rows = []
    for line in lines[:-1]:
        # These exact real initializer writes precede complete records only.
        if line in ('16 achievements', '151 recipes'):
            continue
        require(line.startswith('SLEEP_WAKE '), 'unexpected native sleep/wake output')
        require(len(line) <= 128 * 1024, 'bounded single native state record')
        row = loads(line[len('SLEEP_WAKE '):])
        keys(row, ROW_KEYS, 'native sleep/wake row')
        number = len(rows)
        require(number < 76 and integer(row['case'], 0, 75) and row['case'] == number,
                'ordered native sleep/wake cases')
        sequence(row['input'], 5, lambda x: type(x) is int, 'native declared profile')
        require(row['input'][0] == number and row['input'][1] in (0, 1) and
                0 <= row['input'][2] < 4 and 0 <= row['input'][3] < 8 and
                0 <= row['input'][4] < 8, 'native declared input domains')
        require(row['exception'] in ('NONE', 'java/lang/NullPointerException'),
                'native actual exception classification')
        if row['exception'] == 'NONE':
            require(row['source_failure_site'] is None, 'no invented native source failure site')
        else:
            site = row['source_failure_site']
            keys(site, ('class', 'method', 'descriptor', 'pc'), 'native actual source fault site')
            require(all(type(site[key]) is str and re.fullmatch('[A-Za-z0-9_();/]+', site[key])
                        for key in ('class', 'method', 'descriptor')) and
                    integer(site['pc'], 0, 65535), 'native typed source fault site')
        if row['result'] is not None:
            keys(row['result'], ('name', 'ordinal', 'current_a'), 'native actual CW result')
            require(type(row['result']['name']) is str and row['result']['name'] == 'OK' and
                    integer(row['result']['ordinal'], 0, 0) and
                    type(row['result']['current_a']) is bool, 'native actual CW scalar types')
        for key in ('unselected_actor_preserved', 'unselected_world_preserved'):
            require(type(row[key]) is bool, 'native within-call source preservation Boolean')
        row['before'] = state(row['before'])
        row['after'] = state(row['after'])
        random_state(row['random_before'])
        random_state(row['random_after'])
        rows.append(row)
    require(len(rows) == 76, 'all76 native sleep/wake profiles')
    return rows, int(footer[1])


def selected(row):
    result = {key: copy.deepcopy(row[key]) for key in ROW_KEYS if key != 'source_failure_site'}
    faults = [event for event in row['journal']['events'] if event['kind'] == 'EXCEPTION']
    require(len(faults) == (0 if row['exception'] == 'NONE' else 1),
            'original actual terminal source fault count')
    result['source_failure_site'] = None if not faults else copy.deepcopy(faults[0]['site'])
    return result


def compare(actual, expected):
    comparisons = 0
    def visit(a, b, path):
        nonlocal comparisons
        if not (type(a) is type(b)):
            raise AssertionError('sleep/wake comparison type: ' + path)
        if type(b) is dict:
            if not (a.keys() == b.keys()):
                raise AssertionError('sleep/wake comparison keys: ' + path)
            for key in b:
                visit(a[key], b[key], path + '/' + key)
        elif type(b) is list:
            if not (len(a) == len(b)):
                raise AssertionError('sleep/wake comparison length: ' + path)
            for index, (left, right) in enumerate(zip(a, b)):
                visit(left, right, path + '/' + str(index))
        else:
            comparisons += 1
            if not (a == b):
                raise AssertionError('sleep/wake scalar differs: ' + path + ': native=' + str(a) +
                                     '; original=' + str(b))
    visit(actual, expected, 'rows')
    return comparisons


def encode(rows):
    return ''.join('SLEEP_WAKE ' + json.dumps(row, separators=(',', ':')) + '\n'
                   for row in rows) + 'PASS: 1 Player sleep/wake component checks\n'


def controls(expected):
    """Authored parser controls are never accepted as native execution evidence."""
    authored = copy.deepcopy(expected)
    for row in authored:
        for phase in ('before', 'after'):
            row[phase]['planes'] = {key: [] for key in PLANES}
    baseline = encode(authored)
    parsed, checks = native_rows(baseline)
    authored_expected = copy.deepcopy(expected)
    for row in authored_expected:
        for phase in ('before', 'after'):
            row[phase]['planes'] = {key: hashlib.sha256(bytes(length)).hexdigest()
                                    for key, length in PLANES.items()}
    require(checks == 1 and compare(parsed, authored_expected) > 0,
            'positive authored sleep/wake transport control')
    rejected = 0
    def rejects_text(text):
        nonlocal rejected
        try:
            actual, _ = native_rows(text)
            compare(actual, authored_expected)
        except (AssertionError, ValueError, TypeError, KeyError, OverflowError):
            rejected += 1
            return
        raise AssertionError('corrupted sleep/wake transport accepted')
    def mutate(case, action):
        changed = copy.deepcopy(authored)
        action(changed[case])
        rejects_text(encode(changed))
    rejects_text(baseline.rsplit('PASS:', 1)[0])
    rejects_text(baseline + baseline.splitlines()[0] + '\n')
    rejects_text(encode(authored[:-1]))
    rejects_text(encode(authored + [authored[-1]]))
    rejects_text(baseline.replace('"case":0', '"case":0,"case":0', 1))
    rejects_text(baseline.replace('"case":0', '"case":NaN', 1))
    rejects_text('unrecognized initializer\n' + baseline)
    mutate(0, lambda row: row.update(case=True))
    mutate(0, lambda row: row.update(extra=False))
    mutate(0, lambda row: row.pop('random_after'))
    mutate(0, lambda row: row['input'].__setitem__(2, True))
    mutate(0, lambda row: row.update(exception='NONE'))
    mutate(0, lambda row: row['source_failure_site'].update(pc=85))
    mutate(0, lambda row: row['source_failure_site'].update(pc=True))
    mutate(1, lambda row: row['result'].update(current_a=1))
    mutate(1, lambda row: row['result'].update(ordinal=False))
    for phase in ('before', 'after'):
        mutate(0, lambda row, p=phase: row[p].update(extra=0))
        mutate(0, lambda row, p=phase: row[p].pop('private_bN_same'))
        mutate(0, lambda row, p=phase: row[p]['position'].__setitem__(0, '0' * 15))
        mutate(0, lambda row, p=phase: row[p]['dimensions'].__setitem__(0, '3E4CCCCD'))
        mutate(0, lambda row, p=phase: row[p].update(world_J=0))
        mutate(0, lambda row, p=phase: row[p]['head'].__setitem__(1, 16))
        for plane, length in PLANES.items():
            for invalid in ([[-1, 1, 1]], [[0, 0, 1]], [[0, length + 1, 1]],
                            [[0, 1, 0]], [[0, 1, 256]], [[False, 1, 1]],
                            [[1, 2, 1], [0, 1, 2]], [[0, 2, 1], [1, 1, 2]],
                            [[0, 1, 1], [1, 1, 1]], [[0, 1]], [[0, 1, 1, 2]]):
                mutate(0, lambda row, p=phase, n=plane, v=invalid:
                       row[p]['planes'].__setitem__(n, v))
            mutate(0, lambda row, p=phase, n=plane:
                   row[p]['planes'].__setitem__(n, [[0, 1, 1]]))
    # Every original selected field/phase/case is independently comparison
    # sensitive. These changed copies are not regenerated expected outcomes.
    for number in range(76):
        for phase in ('before', 'after'):
            for key in STATE_KEYS:
                changed = expected.copy()
                changed[number] = expected[number].copy()
                changed[number][phase] = expected[number][phase].copy()
                value = changed[number][phase][key]
                replacement = not value if type(value) is bool else None if value is not None else False
                changed[number][phase][key] = replacement
                try:
                    compare(changed, expected)
                except AssertionError:
                    rejected += 1
                else:
                    raise AssertionError('changed original selected field accepted')
        for key in ('input', 'result', 'exception', 'unselected_actor_preserved',
                    'unselected_world_preserved', 'random_before', 'random_after',
                    'source_failure_site'):
            changed = expected.copy()
            changed[number] = expected[number].copy()
            value = changed[number][key]
            changed[number][key] = not value if type(value) is bool else None if value is not None else False
            try:
                compare(changed, expected)
            except AssertionError:
                rejected += 1
            else:
                raise AssertionError('changed original row evidence accepted')
    # A positive nonzero plane transport must reconstruct the actual bytes.
    for length in PLANES.values():
        runs = [[0, 2, 7], [3, length - 3, 255]]
        require(plane_digest(runs, length) ==
                hashlib.sha256(b'\x07\x07\x00' + bytes([255]) * (length - 3)).hexdigest(),
                'lossless native plane reconstruction')
    return rejected


def source_paths(executable):
    paths = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format'}
    for folder in ('src', 'include', 'cmake', 'tests/entity', 'tests/world/runtime',
                   'tests/reference/entity/player_sleep_wake', 'tests/reference/client/controller',
                   'tests/support'):
        paths.update(p for p in (ROOT / folder).rglob('*') if p.is_file() and
                     p.suffix in ('.c', '.h', '.inc', '.cmake', '.py', '.json', '.java', '.txt', '.tsv'))
    paths.update(executable.parent.glob('*.dll'))
    return sorted(paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--controls', action='store_true')
    args = parser.parse_args()
    packet = module('player_sleep_wake_original_packet', PACKET / 'packet.py')
    original = packet.read_reference()
    require(original['schema'] == 'BED_NIGHT_COMPONENT_FINITE_V1' and
            len(original['rows']) == 76, 'complete unchanged original night component packet')
    expected = [selected(row) for row in original['rows']]
    if args.controls:
        require(args.executable is None and args.work_dir is None,
                'pure sleep/wake controls take no executable/work directory')
        print('Player sleep/wake native reader: ' + str(controls(expected)) + ' rejection controls')
        return
    require(args.executable is not None and args.work_dir is not None,
            'sleep/wake executable/work directory required')
    process = module('player_sleep_wake_process_capture',
                     ROOT / 'tests/reference/client/controller/capture.py')
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-sleep-wake-', dir=args.work_dir.resolve()))
    paths = source_paths(executable)
    before = {str(path): process.digest(path) for path in paths}
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before_sha256': before,
              'original_rows': 0, 'original_journals_validated': 76,
              'native_original_journal_equality': False, 'selected_fields_per_state': 25,
              'source_ticks': 0, 'original_draw': False, 'loaded_night': False,
              'constructor_entropy_cross_vm_equality': False}
    environment = os.environ.copy()
    environment['ALSOFT_DRIVERS'] = 'null'
    try:
        receipt = process.capture([str(executable), 'sleep-wake',
            str(output / 'worlds').encode('utf-8').hex(), str((PACKET / 'profiles.tsv').resolve())],
            output, 'native', 180, environment, output)
        report['native'] = receipt
        process.require_success(receipt, empty_stderr=True)
        actual, checks = native_rows((output / receipt['stdout_file']).read_text(encoding='utf-8'))
        report['scalar_comparisons'] = compare(actual, expected)
        report['native_checks'] = checks
        require(packet.read_reference() == original,
                'actual strict original sleep/wake packet changed during replay')
        report['original_rows'] = len(actual)
        report['status'] = 'PASS'
    finally:
        after_paths = source_paths(executable)
        after = {str(path): process.digest(path) for path in after_paths}
        report['inputs_after_sha256'] = after
        if after != before:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    require(report['status'] == 'PASS', 'sleep/wake inputs changed; report=' + str(output / 'report.json'))
    print('Player sleep/wake: ' + str(report['scalar_comparisons']) +
          ' original scalar comparisons; report=' + str(output / 'report.json'))


if __name__ == '__main__':
    main()
