"""Compare 39 genuine IZ Dye wrappers with their separately captured original cohort."""

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
PACKET = ROOT / 'tests/reference/item/dye_use'
OUTCOME_KEYS = set('index name inputs channel result exception stack_before stack_after '
                   'item_random_before item_random_after world_random_before world_random_after '
                   'gm_b_is_world_after used_delta remote_after observer_cells notifications changes '
                   'blocks_metadata_after'.split())
NATIVE_KEYS = set('index status native_policy source_exception source_error observer_refused '
                  'world_status retired_prefix_unchanged callback_cause'.split())
CALLBACK_EXCEPTION = 'java.lang.IllegalStateException'
NATIVE_REFUSAL = 'NATIVE_CALLBACK_REFUSAL'
ROWS = 39
BOOTSTRAP = ('16 achievements', '151 recipes')


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree_manifest(path):
    return {p.relative_to(path).as_posix(): digest(p)
            for p in sorted(path.rglob('*')) if p.is_file()}


def source_manifest():
    paths = {ROOT / 'CMakeLists.txt', Path(__file__).resolve(),
             ROOT / 'tests/item/use/dye_reference.c',
             ROOT / 'tests/client/statistics/test_player_dispatch.c',
             ROOT / 'tests/reference/client/player_statistics/support.py'}
    for directory in ('src', 'include', 'cmake'):
        paths.update(p for p in (ROOT / directory).rglob('*') if p.is_file())
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(paths)}


def build_manifest(executable):
    build = next((p for p in executable.parents if (p / 'CMakeCache.txt').is_file()), None)
    if build is None:
        return {}
    names = ('CMakeCache.txt', 'build.ninja', 'Makefile', 'compile_commands.json',
             'CMakeFiles/rules.ninja', 'CTestTestfile.cmake')
    return {name: digest(build / name) for name in names if (build / name).is_file()}


def guarded_inputs(executable, profiles, resources):
    return {'executable': digest(executable), 'profiles': digest(profiles),
            'sources': source_manifest(), 'build_metadata': build_manifest(executable),
            'original_packet': tree_manifest(PACKET), 'resources': tree_manifest(resources)}


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def canonical(value):
    # JSON distinguishes Boolean words from integers, unlike Python dict equality.
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)


def original_wrappers():
    packet = load_module('dye_use_reference_packet', PACKET / 'packet.py')
    rows = packet.read_packet(PACKET)
    require(len(rows) == ROWS and [r['index'] for r in rows] == list(range(ROWS)),
            'Expected the separately captured 39-row primary cohort')
    for row in rows:
        require(set(row) == OUTCOME_KEYS and row['channel'] == 'wrapper' and
                len(row['inputs']) == 15 and row['inputs'][0] == 1,
                'Primary data must contain genuine IZ wrappers only')
    return rows


def compare(expected, actual, native):
    """Compare every selected effect; retain foreign callback causes separately."""
    require(len(expected) == len(actual) == len(native) == ROWS,
            'Expected exactly 39 original, native effect and native policy rows')
    require([r['index'] for r in expected] == [r['index'] for r in actual] ==
            [r['index'] for r in native] == list(range(ROWS)),
            'Native primary profile identity/order differs')
    complete = callbacks = 0
    for wanted, got, policy in zip(expected, actual, native):
        require(set(got) == OUTCOME_KEYS and set(policy) == NATIVE_KEYS,
                'Closed native outcome/policy schema: ' + wanted['name'])
        require(type(policy['index']) is int and type(policy['status']) is int and
                type(policy['world_status']) is int and
                all(type(policy[k]) is bool for k in NATIVE_KEYS -
                    {'index', 'status', 'world_status', 'callback_cause'}),
                'Typed native policy fields: ' + wanted['name'])
        callback = wanted['inputs'][14] in (1, 3)
        if callback:
            require(wanted['exception'] == CALLBACK_EXCEPTION and
                    got['exception'] == NATIVE_REFUSAL,
                    'Declared callback classes must remain distinct: ' + wanted['name'])
            expected_policy = {
                'index': wanted['index'], 'status': 7, 'native_policy': True,
                'source_exception': False, 'source_error': False, 'observer_refused': True,
                'world_status': 7, 'retired_prefix_unchanged': True,
                'callback_cause': 'declared Dye observer native refusal',
            }
            require(policy == expected_policy,
                    'Native LIMIT callback refusal policy: ' + wanted['name'])
            wanted_effects = {k: v for k, v in wanted.items() if k != 'exception'}
            got_effects = {k: v for k, v in got.items() if k != 'exception'}
            callbacks += 1
        else:
            require(not policy['observer_refused'] and not policy['source_error'] and
                    policy['retired_prefix_unchanged'] and policy['callback_cause'] is None,
                    'Unexpected native refusal/error/retirement effect: ' + wanted['name'])
            if wanted['exception'] == 'NONE':
                require(policy['status'] == 0 and not policy['source_exception'] and
                        not policy['native_policy'] and policy['world_status'] == 0,
                        'Native successful return policy: ' + wanted['name'])
            else:
                require(wanted['exception'] == 'java.lang.NullPointerException' and
                        policy['status'] == 9 and policy['source_exception'] and
                        not policy['native_policy'] and policy['world_status'] == 0,
                        'Native reached source NULL policy: ' + wanted['name'])
            wanted_effects, got_effects = wanted, got
            complete += 1
        if canonical(wanted_effects) != canonical(got_effects):
            keys = sorted(k for k in wanted_effects
                          if canonical(got_effects.get(k)) != canonical(wanted_effects[k]))
            detail = '\n'.join(f'{k}: native={got_effects.get(k)!r}, original={wanted_effects[k]!r}'
                               for k in keys)
            raise AssertionError(f'Dye profile {wanted["index"]} {wanted["name"]}:\n{detail}')
    require((complete, callbacks) == (36, 3),
            'Expected 36 exact outcomes and three separately classified callback prefixes')
    return complete, callbacks


def comparator_controls(expected, actual, native):
    """Reject altered streams, store order, walk coordinates, arrays and outcomes."""
    positive = compare(expected, actual, native)
    controls = []

    def effect(label, index, key, value):
        changed = copy.deepcopy(actual)
        changed[index][key] = value
        controls.append((label, changed, native))

    def row(prefix):
        return next(i for i, r in enumerate(actual) if r['name'].startswith(prefix))

    grass, crop = row('grass_plane_'), row('crop_stage0_')
    callback = next(i for i, r in enumerate(native) if r['observer_refused'])
    changed_random = copy.deepcopy(actual[grass]['item_random_after'])
    changed_random['seed_bits'] = f'{(int(changed_random["seed_bits"], 16) + 1) & ((1 << 48) - 1):012x}'
    effect('omitted-random-transition', grass, 'item_random_after', changed_random)
    effect('item-world-stream-substitution', grass, 'item_random_after',
           actual[grass]['world_random_after'])
    effect('live-random-alias-omitted', row('grass_callback_live_gm_b_to_world_'),
           'gm_b_is_world_after', False)
    for index, count, label in ((grass, actual[grass]['stack_before'][0], 'grass-early-count-store'),
                                (crop, actual[crop]['stack_after'][0], 'crop-late-count-store')):
        notices = copy.deepcopy(actual[index]['notifications'])
        first = next(n for n in notices if n['event'] == 'cell')
        first['stack'][0] = count
        effect(label, index, 'notifications', notices)
    count_index = row('crop_callback_count99_')
    changed_stack = actual[count_index]['stack_after'].copy()
    changed_stack[0] = 1
    effect('late-live-count-reload', count_index, 'stack_after', changed_stack)
    notices = copy.deepcopy(actual[grass]['notifications'])
    walked = next(n for n in reversed(notices) if n['event'] == 'cell')
    walked['position'][1] += 1
    effect('walk-y-coordinate', grass, 'notifications', notices)
    notices = copy.deepcopy(actual[grass]['notifications'])
    first = next(n for n in notices if n['event'] == 'cell' and
                 n['item_random'] != n['world_random'])
    first['item_random'], first['world_random'] = first['world_random'], first['item_random']
    effect('callback-random-owner-labels', grass, 'notifications', notices)
    notices = copy.deepcopy(actual[grass]['notifications'])
    pair = next(i for i in range(len(notices) - 1) if notices[i] != notices[i + 1])
    notices[pair], notices[pair + 1] = notices[pair + 1], notices[pair]
    effect('notification-order', grass, 'notifications', notices)
    effect('notification-omission', grass, 'notifications', [])
    for plane, length in enumerate((32768, 16384, 16384, 16384, 256)):
        changes = copy.deepcopy(actual[grass]['changes'])
        changes.append([3, plane, length - 1, 0, 1])
        effect('array-plane-' + str(plane), grass, 'changes', changes)
    block_map = copy.deepcopy(actual[grass]['blocks_metadata_after'])
    block_map[-1][4] = (block_map[-1][4] + 1) & 15
    effect('final-block-metadata-map', grass, 'blocks_metadata_after', block_map)
    effect('used-statistic', crop, 'used_delta', -17)
    effect('typed-used-statistic', crop, 'used_delta', True)
    effect('callback-exception-normalization', callback, 'exception', CALLBACK_EXCEPTION)
    policies = copy.deepcopy(native)
    policies[callback]['source_exception'] = True
    controls.append(('invented-callback-source-exception', actual, policies))
    policies = copy.deepcopy(native)
    policies[grass]['retired_prefix_unchanged'] = False
    controls.append(('retirement-prefix-change', actual, policies))
    reordered = copy.deepcopy(actual)
    reordered[0], reordered[1] = reordered[1], reordered[0]
    controls.append(('profile-order', reordered, native))
    controls.append(('profile-omission', actual[:-1], native[:-1]))
    labels = []
    for label, rows, policies in controls:
        try:
            compare(expected, rows, policies)
        except AssertionError:
            labels.append(label)
            continue
        raise AssertionError('Comparator accepted corruption: ' + label)
    require(compare(expected, actual, native) == positive, 'Negative controls mutated positive inputs')
    return labels


def parse_output(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate native JSON key: ' + key)
            result[key] = value
        return result

    def invalid_constant(value):
        raise AssertionError('Invalid native JSON constant: ' + value)

    lines = raw.decode('utf-8', errors='strict').splitlines()
    require(len(lines) == len(BOOTSTRAP) + ROWS * 2 + 1 and
            tuple(lines[:len(BOOTSTRAP)]) == BOOTSTRAP and
            re.fullmatch(r'PASS: 39 Dye wrapper profiles, [1-9][0-9]* actual-owner checks', lines[-1]),
            'Exact bootstrap prelude/native observer marker/record inventory differs')
    lines = lines[len(BOOTSTRAP):]
    actual, native = [], []
    for i in range(ROWS):
        for line, prefix, destination in ((lines[i * 2], 'DYE\t', actual),
                                           (lines[i * 2 + 1], 'DYE_NATIVE\t', native)):
            require(line.startswith(prefix), 'Native record order/tag differs')
            destination.append(json.loads(line[len(prefix):], object_pairs_hook=unique,
                                          parse_constant=invalid_constant))
    return actual, native


def parser_controls(actual, native):
    """Generate closed output fixtures, including the exact bootstrap prelude."""
    lines = list(BOOTSTRAP)
    for row, policy in zip(actual, native):
        lines.extend(('DYE\t' + canonical(row), 'DYE_NATIVE\t' + canonical(policy)))
    lines.append('PASS: 39 Dye wrapper profiles, 123 actual-owner checks')

    def encode(records):
        return ('\n'.join(records) + '\n').encode('utf-8')

    raw = encode(lines)
    require(parse_output(raw) == (actual, native), 'Complete parser fixture roundtrip differs')
    controls = [
        ('missing-footer', encode(lines[:-1])),
        ('extra-record', raw + b'DYE\t{}\n'),
        ('duplicate-json-key', raw.replace(b'"index":0', b'"index":0,"index":0', 1)),
        ('nonfinite-json-value', raw.replace(b'"index":0', b'"index":NaN', 1)),
        ('wrong-first-bootstrap-line', encode(['15 achievements'] + lines[1:])),
        ('wrong-second-bootstrap-line', encode(lines[:1] + ['150 recipes'] + lines[2:])),
        ('extra-bootstrap-line', encode(lines[:2] + ['unexpected bootstrap'] + lines[2:])),
        ('missing-first-bootstrap-line', encode(lines[1:])),
        ('missing-second-bootstrap-line', encode(lines[:1] + lines[2:])),
        ('reversed-bootstrap-lines', encode(lines[1::-1] + lines[2:])),
    ]
    labels = []
    for label, changed in controls:
        try:
            parse_output(changed)
        except (AssertionError, ValueError):
            labels.append(label)
            continue
        raise AssertionError('Parser accepted malformed output fixture: ' + label)
    return labels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    expected = original_wrappers()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='dye-reference-', dir=args.work_dir.resolve()))
    support = load_module('dye_reference_resources',
                          ROOT / 'tests/reference/client/player_statistics/support.py')
    resources, game = output / 'resources', output / 'game'
    support.resources_at(resources)
    game.mkdir()
    profiles = output / 'profiles.tsv'
    profiles.write_text(''.join('\t'.join([str(r['index']), r['name']] +
                                         [str(v) for v in r['inputs']]) + '\n'
                                for r in expected), encoding='ascii', newline='\n')
    command = [str(executable), str(resources), str(game), str(profiles)]
    before = guarded_inputs(executable, profiles, resources)
    report = {'status': 'PREPARED', 'command': command, 'returncode': None, 'timeout': False,
              'inputs_before': before, 'allowed_write_directory': str(game)}
    write_json(output / 'run.json', report)
    stdout = stderr = b''
    try:
        try:
            result = subprocess.run(command, capture_output=True, timeout=210)
            stdout, stderr = result.stdout, result.stderr
            report['returncode'] = result.returncode
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b'', error.stderr or b''
            report['timeout'] = True
        finally:
            (output / 'stdout.txt').write_bytes(stdout)
            (output / 'stderr.txt').write_bytes(stderr)
            report['inputs_after'] = guarded_inputs(executable, profiles, resources)
            report['inputs_unchanged'] = before == report['inputs_after']
            report['stdout_sha256'] = hashlib.sha256(stdout).hexdigest()
            report['stderr_sha256'] = hashlib.sha256(stderr).hexdigest()
            report['game_outputs'] = tree_manifest(game)
            report['status'] = 'NATIVE_TERMINAL'
            write_json(output / 'run.json', report)
        require(not report['timeout'] and report['returncode'] == 0,
                'Native Dye observer failed: ' + str(output) + ': ' + stderr.decode(errors='replace'))
        require(not stderr, 'Unexpected native stderr: ' + str(output))
        require(report['inputs_unchanged'], 'Native observer changed guarded inputs: ' + str(output))
        actual, native = parse_output(stdout)
        complete, prefixes = compare(expected, actual, native)
        controls = comparator_controls(expected, actual, native)
        parser_fixtures = parser_controls(actual, native)
        report['comparison'] = {
            'primary_ordered_wrapper_profiles': ROWS, 'worlds': 1, 'canonical_chunks': 4,
            'exact_outcomes_and_effects': complete, 'separate_callback_effect_prefixes': prefixes,
            'callback_exception_class_equivalence': 'UNQUALIFIED',
            'native_callback_cause': 'MC_ITEM_USE_WORLD / MC_WORLD_STORAGE_LIMIT',
            'native_direct_km_calls': 0, 'lighting_or_scheduled_queue_reset_or_drain': False,
            'negative_comparator_controls': controls,
            'malformed_output_fixture_controls': parser_fixtures,
            'fields': 'complete literal selected original fields, except three explicit callback classes',
            'limits': 'finite declared effects; full residual World queues/state, loaded gameplay and pixels unqualified',
        }
        report['status'] = 'PASS'
        write_json(output / 'comparison.json', report['comparison'])
    except Exception as error:
        report['status'] = 'FAIL'
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        write_json(output / 'run.json', report)
    print(f'PASS: 39 original/native Dye wrappers; 36 exact outcomes, 3 separate callback prefixes; '
          f'{len(controls)} negative controls, {len(parser_fixtures)} malformed-output fixtures; {output}')


if __name__ == '__main__':
    main()
