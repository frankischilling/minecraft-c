"""Compare real IZ farming wrappers with the frozen original farming packet."""

import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
PACKET = ROOT / 'tests/reference/item/farming_use'
CALLBACK_EXCEPTION = 'java.lang.IllegalStateException'
NATIVE_REFUSAL = 'NATIVE_CALLBACK_REFUSAL'
OUTCOME_KEYS = {
    'index', 'name', 'values', 'maximum', 'result', 'exception', 'stack_before',
    'stack_after', 'center_before', 'center_after', 'above_before', 'above_after',
    'remote_after', 'sounds', 'notifications', 'changes', 'used_delta', 'broken_delta',
}
NATIVE_KEYS = {
    'index', 'status', 'native_policy', 'source_exception', 'source_error', 'observer_refused',
}


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None


def tree_manifest(directory):
    return {str(p.relative_to(directory)).replace('\\', '/'): digest(p)
            for p in sorted(directory.rglob('*')) if p.is_file()}


def source_manifest():
    paths = [
        Path(__file__).resolve(), ROOT / 'tests/item/use/farming_reference.c',
        ROOT / 'tests/client/statistics/test_player_dispatch.c',
        ROOT / 'tests/reference/client/player_statistics/support.py',
        ROOT / 'cmake/item_farming_use.cmake',
    ]
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in paths}


def guarded_inputs(executable, profiles, resources):
    return {
        'executable': digest(executable), 'profiles': digest(profiles),
        'controller_inputs': source_manifest(), 'original_packet': tree_manifest(PACKET),
        'resources': tree_manifest(resources),
    }


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def original_wrappers():
    sys.path.insert(0, str(PACKET))
    packet = load_module('farming_use_reference_packet', PACKET / 'packet.py')
    rows = packet.read_packet(PACKET)
    if len(rows) != 118:
        raise AssertionError('Expected all 118 audited original farming profiles')
    wrappers = [r['observed'] for r in rows if r['profile']['values'][1] == 1]
    if len(wrappers) != 57:
        raise AssertionError('Expected 57 distinct original IZ wrapper profiles')
    for row in wrappers:
        if set(row) != OUTCOME_KEYS or row['values'][1] != 1 or row['values'][13] & 2:
            raise AssertionError(f'Unsupported wrapper input/schema: {row}')
    return wrappers


def compare(expected, actual, native):
    """All effect fields are exact; foreign callback exception types stay distinct."""
    if len(expected) != 57 or len(actual) != 57 or len(native) != 57:
        raise AssertionError('Expected exactly 57 original, native effect and native policy rows')
    if [r['index'] for r in expected] != [r['index'] for r in actual]:
        raise AssertionError('Native profile identity/order differs from the frozen wrapper inputs')
    if [r['index'] for r in expected] != [r['index'] for r in native]:
        raise AssertionError('Native policy profile identity/order differs')
    fully_compared = callbacks = 0
    for original, observed, policy in zip(expected, actual, native):
        if set(observed) != OUTCOME_KEYS or set(policy) != NATIVE_KEYS:
            raise AssertionError(f'Native schema mismatch: {original["name"]}')
        values = original['values']
        callback = values[15] == 1 or values[16] != 0
        if callback:
            if original['exception'] != CALLBACK_EXCEPTION or observed['exception'] != NATIVE_REFUSAL:
                raise AssertionError(f'Declared callback failure classification: {original["name"]}')
            wanted = {k: v for k, v in original.items() if k != 'exception'}
            got = {k: v for k, v in observed.items() if k != 'exception'}
            required_policy = {
                'index': original['index'], 'status': 7, 'native_policy': True,
                'source_exception': False, 'source_error': False, 'observer_refused': True,
            }
            if policy != required_policy:
                raise AssertionError(f'Native callback refusal policy: {original["name"]}: {policy}')
            callbacks += 1
        else:
            wanted, got = original, observed
            if policy['observer_refused'] or policy['source_error']:
                raise AssertionError(f'Unexpected native refusal/error: {original["name"]}')
            if original['exception'] == 'NONE':
                if policy['status'] != 0 or policy['source_exception'] or policy['native_policy']:
                    raise AssertionError(f'Native successful return policy: {original["name"]}')
            elif policy['status'] == 0 or not policy['source_exception'] or policy['native_policy']:
                raise AssertionError(f'Native reached source exception policy: {original["name"]}')
            fully_compared += 1
        if got != wanted:
            keys = sorted(k for k in wanted if got.get(k) != wanted[k])
            detail = '\n'.join(f'{k}: native={got.get(k)!r}, original={wanted[k]!r}' for k in keys)
            raise AssertionError(f'Farming profile {original["index"]} {original["name"]}:\n{detail}')
    if (fully_compared, callbacks) != (54, 3):
        raise AssertionError('Expected 54 full outcomes and 3 explicitly distinct callback failures')
    return fully_compared, callbacks


def parser_controls(expected, actual, native):
    """Prove comparison rejects changed nonlocal arrays, callbacks, bits and outcomes."""
    positive = compare(expected, actual, native)
    mutations = []
    effect_index = next(i for i, row in enumerate(actual) if row['changes'])
    callback_index = next(i for i, row in enumerate(native) if row['observer_refused'])
    sound_index = next(i for i, row in enumerate(actual) if row['sounds'])
    for key, value in [('used_delta', -17), ('broken_delta', -17),
                       ('stack_after', [987, 0, 0, 0]), ('remote_after', 'invalid')]:
        mutations.append((effect_index, key, value))
    changed = copy.deepcopy(actual[effect_index]['changes'])
    changed.append([3, 3, 16383, 0, 1])
    mutations.append((effect_index, 'changes', changed))
    mutations.append((effect_index, 'notifications', []))
    changed_sound = copy.deepcopy(actual[sound_index]['sounds'])
    changed_sound[0][4] = '00000000'
    mutations.append((sound_index, 'sounds', changed_sound))
    mutations.append((callback_index, 'exception', CALLBACK_EXCEPTION))
    for index, key, value in mutations:
        altered = copy.deepcopy(actual)
        altered[index][key] = value
        try:
            compare(expected, altered, native)
        except AssertionError:
            continue
        raise AssertionError(f'Comparator accepted changed {key}')
    altered_policy = copy.deepcopy(native)
    altered_policy[callback_index]['source_exception'] = True
    try:
        compare(expected, actual, altered_policy)
    except AssertionError:
        return positive, len(mutations) + 1
    raise AssertionError('Comparator accepted invented native source Throwable')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    initial_sources, initial_packet = source_manifest(), tree_manifest(PACKET)
    if any(value is None for value in initial_sources.values()) or not initial_packet:
        raise AssertionError('Missing native/controller/original packet input')
    expected = original_wrappers()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='farming-reference-', dir=args.work_dir.resolve()))
    support = load_module('farming_use_resources',
                          ROOT / 'tests/reference/client/player_statistics/support.py')
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    profiles = output / 'profiles.tsv'
    profiles.write_text(''.join('\t'.join(map(str, [r['index'], r['name'], *r['values']])) + '\n'
                                for r in expected), encoding='ascii', newline='\n')
    command = [str(executable), str(resources), str(game), str(profiles)]
    write_json(output / 'command.json', command)
    before = guarded_inputs(executable, profiles, resources)
    if before['controller_inputs'] != initial_sources or before['original_packet'] != initial_packet:
        raise AssertionError(f'Controller/packet changed during input preparation: {output}')
    report = {
        'status': 'PREPARED', 'command': command, 'returncode': None, 'timeout': False,
        'inputs_before': before, 'inputs_after': None, 'input_guards_pass': False,
        'allowed_write_directory': str(game),
    }
    write_json(output / 'run.json', report)
    stdout = stderr = b''
    try:
        try:
            child = subprocess.run(command, capture_output=True, timeout=180)
            stdout, stderr = child.stdout, child.stderr
            report['returncode'] = child.returncode
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b'', error.stderr or b''
            report['timeout'] = True
        finally:
            (output / 'stdout.txt').write_bytes(stdout)
            (output / 'stderr.txt').write_bytes(stderr)
            report['stdout_sha256'] = hashlib.sha256(stdout).hexdigest()
            report['stderr_sha256'] = hashlib.sha256(stderr).hexdigest()
            report['inputs_after'] = guarded_inputs(executable, profiles, resources)
            report['input_guards_pass'] = before == report['inputs_after']
            report['game_outputs'] = tree_manifest(game)
            report['status'] = 'NATIVE_TERMINAL'
            write_json(output / 'run.json', report)
        if report['timeout'] or report['returncode'] != 0:
            raise RuntimeError(f'Native farming bridge failed ({report["returncode"]}, '
                               f'timeout={report["timeout"]}): {output}: ' +
                               stderr.decode('utf-8', errors='replace'))
        if not report['input_guards_pass']:
            raise AssertionError(f'Native/controller/profile/resource input guard changed: {output}')
        actual, native = [], []
        for line in stdout.decode('utf-8').splitlines():
            if line.startswith('FARMING\t'):
                actual.append(json.loads(line.split('\t', 1)[1]))
            elif line.startswith('FARMING_NATIVE\t'):
                native.append(json.loads(line.split('\t', 1)[1]))
        (full, callbacks), negative_controls = parser_controls(expected, actual, native)
        receipt = {
            'effect_rows_compared': len(actual), 'fully_compared_outcome_rows': full,
            'callback_failure_prefix_rows': callbacks,
            'callback_exception_class_equivalence': 'UNQUALIFIED',
            'original_direct_item_rows_not_compared': 61,
            'negative_comparator_controls': negative_controls,
            'returncode': report['returncode'], 'input_guards_pass': report['input_guards_pass'],
            'inputs_before': before, 'inputs_after': report['inputs_after'],
            'stdout_sha256': report['stdout_sha256'], 'stderr_sha256': report['stderr_sha256'],
        }
        write_json(output / 'comparison.json', receipt)
        if b'PASS: 57 farming wrapper profiles' not in stdout:
            raise AssertionError(f'Missing native completion: {output}')
        report['status'] = 'PASS'
        report['comparison'] = {k: receipt[k] for k in (
            'effect_rows_compared', 'fully_compared_outcome_rows', 'callback_failure_prefix_rows',
            'callback_exception_class_equivalence', 'negative_comparator_controls')}
    except Exception as error:
        report['status'] = 'FAIL'
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        write_json(output / 'run.json', report)
    print(f'PASS: 57 exact farming effect rows; {full} full outcomes; '
          f'{callbacks} callback refusal prefixes (exception class unqualified); '
          f'{negative_controls} negative comparator controls')
    print(f'Artifacts: {output}')


if __name__ == '__main__':
    main()
