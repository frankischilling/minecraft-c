"""Compare the actual native damage calls with selected unchanged-original state."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import packet as P

PLANES = {'blocks': 32768, 'metadata': 16384, 'sky': 16384, 'block_light': 16384, 'height': 256}
ROW_KEYS = {'case', 'input', 'result', 'exception', 'before', 'after'}


def input_digests(paths):
    """Hash actual build products without imposing the separate packet-size cap."""
    result = {}
    for path in paths:
        digest = hashlib.sha256()
        with Path(path).open('rb') as stream:
            for chunk in iter(lambda: stream.read(128 * 1024), b''):
                digest.update(chunk)
        result[str(path)] = digest.hexdigest()
    return result


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def plane(value, length):
    P.require(type(value) is list and len(value) <= length, 'native plane runs')
    result = bytearray(length)
    previous_end, previous_value = 0, 0
    for row in value:
        P.require(type(row) is list and len(row) == 3 and all(type(word) is int for word in row), 'native run operands')
        start, count, byte = row
        P.require(start >= previous_end and count > 0 and start + count <= length and 0 < byte < 256 and
                  (start != previous_end or byte != previous_value), 'native run order, bound and maximality')
        result[start:start + count] = bytes([byte]) * count
        previous_end, previous_value = start + count, byte
    return hashlib.sha256(result).hexdigest()


def native_rows(stdout):
    lines = stdout.splitlines()
    P.require(lines and re.fullmatch(r'PASS: 32 Player damage wake original rows; [1-9][0-9]* checks', lines[-1]), 'native checked completion')
    prefix = 'PLAYER_DAMAGE_WAKE_NATIVE '
    P.require(all(line.startswith(prefix) or line in ('16 achievements', '151 recipes')
                  for line in lines[:-1]), 'closed native stdout inventory')
    rows = [P.loads(line[len(prefix):]) for line in lines[:-1] if line.startswith(prefix)]
    P.require(len(rows) == 32, 'native row count')
    for index, (row, profile) in enumerate(zip(rows, P.profiles())):
        P.require(type(row) is dict and row.keys() == ROW_KEYS and type(row['case']) is int and row['case'] == index and
                  P.exact(row['input'], profile), 'native row identity')
        P.require(row['exception'] in ('NONE', 'java/lang/NullPointerException') and
                  (type(row['result']) is bool if row['exception'] == 'NONE' else row['result'] is None), 'native actual outcome')
        for name in ('before', 'after'):
            state = row[name]
            P.require(type(state) is dict and state.keys() == P.STATE_KEYS and type(state['planes']) is dict and
                      state['planes'].keys() == PLANES.keys(), 'native complete state and plane keys')
            state['planes'] = {key: plane(state['planes'][key], length) for key, length in PLANES.items()}
            P.state(state)
    return rows


def compare(rows, reference):
    P.require(type(rows) is list and len(rows) == len(reference['rows']) == 32, 'compared call count')
    count = 0

    def visit(actual, expected, path):
        nonlocal count
        P.require(type(actual) is type(expected), 'native/original type differs: ' + path)
        if isinstance(actual, dict):
            P.require(actual.keys() == expected.keys(), 'native/original keys differ: ' + path)
            for name in actual: visit(actual[name], expected[name], path + '/' + name)
        elif isinstance(actual, list):
            P.require(len(actual) == len(expected), 'native/original length differs: ' + path)
            for index, (a, b) in enumerate(zip(actual, expected)): visit(a, b, path + '/' + str(index))
        else:
            count += 1
            P.require(actual == expected, 'native/original value differs: ' + path + '; native=' + repr(actual) + '; original=' + repr(expected))

    for index, (actual, original) in enumerate(zip(rows, reference['rows'])):
        P.require(type(actual) is dict and actual.keys() == ROW_KEYS, 'closed comparison projection')
        # Original VM stack names remain in the packet. The effect API does not
        # expose a Java stack, so this comparison pairs the actual first-null
        # category and reached state without inventing a native exception frame.
        visit(actual, {name: original[name] for name in ROW_KEYS}, '/' + str(index))
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    reference = P.read_reference()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='player-damage-wake-', dir=args.work_dir.resolve()))
    capture = module('damage_wake_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    support = module('damage_wake_native_resources', ROOT / 'tests/reference/client/player_statistics/support.py')
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources); game.mkdir()
    source = ROOT / 'tests/entity/player/test_damage_wake_original.c'
    inputs = [executable, source, Path(__file__).resolve(), HERE / 'packet.py', HERE / 'finite.json',
              HERE / 'profiles.tsv', HERE / 'provenance.json',
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/reference/client/player_statistics/support.py']
    before = input_digests(inputs)
    report = {'schema': 'PLAYER_DAMAGE_WAKE_NATIVE_V1', 'status': 'FAIL', 'input_before_sha256': before,
              'scope': '32 actual damage calls and exact selected states/outcomes. Original Java exception stack names remain unpaired; no natural sleep/tick/frame/FPS qualification.'}
    try:
        receipt = capture.capture([str(executable), 'damage-wake-original', str(game).encode('utf-8').hex(),
            str(resources).encode('utf-8').hex(), str(HERE / 'profiles.tsv')], output, 'native', 180, os.environ.copy())
        report['native'] = receipt; capture.require_success(receipt, empty_stderr=True)
        actual = native_rows((output / receipt['stdout_file']).read_text(encoding='utf-8'))
        count = compare(actual, reference)
        report.update(status='PASS_SELECTED_COMPONENTS_ONLY', rows=32, scalar_comparisons=count)
    finally:
        report['input_after_sha256'] = input_digests(inputs)
        if report['input_after_sha256'] != before: report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        P.require(before == report['input_after_sha256'], 'native replay inputs stay unchanged')
    print('Player damage wake: 32 rows;', count, 'exact scalar comparisons;', output / 'report.json')


if __name__ == '__main__':
    main()
