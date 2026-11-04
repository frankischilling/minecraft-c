"""Compare 28 actual-owner Food Stack/controller calls with frozen original rows."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / 'tests/reference/item/food_use'
SOURCES = [
    'tests/item/use/food_reference.c', 'tests/client/statistics/test_player_dispatch.c',
    'src/item/use/use.c', 'include/mc/item/use/use.h', 'src/entity/living/fields.c',
    'src/entity/living/fields_internal.h', 'src/client/controller/use.c',
    'src/item/stack/stack.c', 'src/item/registry/runtime.c',
    'src/entity/player/statistics.c', 'tests/tools/item/test_food_use.py',
]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree(path):
    return {p.relative_to(path).as_posix(): digest(p) for p in sorted(path.rglob('*')) if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def compare(expected, actual):
    if len(expected) != 28 or len(actual) != 28:
        raise AssertionError('Expected exactly 28 ordinary original and native rows')
    for index, (wanted, got) in enumerate(zip(expected, actual)):
        if wanted != got:
            raise AssertionError(f'Food comparison row {index}: original={wanted!r}, native={got!r}')


def comparator_controls(expected, actual):
    compare(expected, actual)
    changes = [(0, 3, '-17'), (0, 4, '-17'), (0, 5, '-17'), (8, 6, '282'),
               (8, 7, '2'), (8, 8, '-1'), (8, 9, '-1'), (8, 10, 'true'),
               (11, 11, 'NONE'), (0, 12, '1'), (0, 13, 'true'),
               (15, 11, 'false')]
    for index, column, value in changes:
        changed = copy.deepcopy(actual)
        changed[index][column] = value
        try:
            compare(expected, changed)
        except AssertionError:
            continue
        raise AssertionError(f'Comparator accepted altered row {index}, column {column}')
    changed = copy.deepcopy(actual)
    changed[0], changed[1] = changed[1], changed[0]
    try:
        compare(expected, changed)
    except AssertionError:
        return len(changes) + 1
    raise AssertionError('Comparator accepted reordered source calls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    packet = load('food_packet_native', PACKET / 'packet.py')
    expected = packet.validate(PACKET)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='food-reference-', dir=args.work_dir.resolve()))
    support = load('food_native_resources', ROOT / 'tests/reference/client/player_statistics/support.py')
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    profiles = output / 'profiles.tsv'
    profiles.write_bytes((PACKET / 'profiles.tsv').read_bytes())
    command = [str(executable), str(resources), str(game), str(profiles)]

    def inputs():
        return {'executable': digest(executable), 'profiles': digest(profiles),
                'sources': {name: digest(ROOT / name) for name in SOURCES},
                'packet': tree(PACKET), 'resources': tree(resources)}

    before = inputs()
    report = {'status': 'PREPARED', 'command': command, 'returncode': None, 'timeout': False,
              'inputs_before': before, 'allowed_write_directory': str(game)}
    write(output / 'run.json', report)
    stdout = stderr = b''
    try:
        try:
            result = subprocess.run(command, capture_output=True, timeout=150)
            stdout, stderr = result.stdout, result.stderr
            report['returncode'] = result.returncode
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b'', error.stderr or b''
            report['timeout'] = True
        finally:
            (output / 'stdout.txt').write_bytes(stdout)
            (output / 'stderr.txt').write_bytes(stderr)
            report['inputs_after'] = inputs()
            report['input_guards_pass'] = before == report['inputs_after']
            report['stdout_sha256'] = hashlib.sha256(stdout).hexdigest()
            report['stderr_sha256'] = hashlib.sha256(stderr).hexdigest()
            report['game_outputs'] = tree(game)
            report['status'] = 'NATIVE_TERMINAL'
            write(output / 'run.json', report)
        if report['timeout'] or report['returncode'] != 0:
            raise RuntimeError('Native food observer failed: ' + str(output) + ': ' +
                               stderr.decode(errors='replace'))
        if not report['input_guards_pass']:
            raise AssertionError('Native observer changed a guarded input: ' + str(output))
        text = stdout.decode('utf-8', errors='strict')
        actual = packet.rows(text, 'FOOD') + packet.rows(text, 'CONTROLLERFOOD')
        negative_controls = comparator_controls(expected, actual)
        if re.search(r'PASS: 28 food Stack/controller rows, [1-9][0-9]* actual-owner checks', text) is None:
            raise AssertionError('Missing complete native observer marker')
        report['status'] = 'PASS'
        report['comparison'] = {'exact_rows': 28, 'stack_wrappers': 14, 'controller_wrappers': 14,
                                'negative_comparator_controls': negative_controls,
                                'null_player_failure_rows': 6,
                                'statistics': 'actual registered used-statistic owners; no substitution'}
        write(output / 'comparison.json', report['comparison'])
    except Exception as error:
        report['status'] = 'FAIL'
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        write(output / 'run.json', report)
    print(f'PASS: 28 exact original/native Food rows; {negative_controls} negative controls; {output}')


if __name__ == '__main__':
    main()
