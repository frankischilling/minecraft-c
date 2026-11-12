"""Run actual Player damage/wake controls without display or original assets."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tempfile


AFTER_FIELDS = ('sleeping', 'timer', 'health', 'idle', 'head', 'orphan',
                'world_sleeping', 'world_same', 'position', 'dimensions')
CALLBACK_FIELDS = ('xyz', 'head', 'orphan', 'sleeping')
ORIGINAL_MODES = {'owned-wake-recursion': 'success',
                  'owned-wake-null-exit': 'null-exit',
                  'owned-wake-observer-error': 'observer-error'}

FAMILY_MODES = tuple('owned-wake-' + name for name in (
    'sapling', 'sapling-source', 'sapling-scope', 'sapling-math-missing',
    'sapling-removal-source', 'sapling-supported', 'tall-grass', 'tall-grass-source',
    'tall-grass-scope', 'tall-grass-math-missing', 'tall-grass-removal-source',
    'tall-grass-supported', 'tall-grass-seed1234', 'tall-grass-no-item',
    'tall-grass-no-item-math-missing'))
FAMILY_ORIGINAL = {
    'owned-wake-sapling-supported': 'sapling-supported',
    'owned-wake-sapling': 'sapling-unsupported',
    'owned-wake-sapling-source': 'sapling-publication-error',
    'owned-wake-tall-grass-supported': 'tallgrass-supported',
    'owned-wake-tall-grass': 'tallgrass-unsupported',
    'owned-wake-tall-grass-source': 'tallgrass-publication-error',
    'owned-wake-tall-grass-seed1234': 'tallgrass-unsupported-seed1234',
    'owned-wake-tall-grass-no-item': 'tallgrass-unsupported-seed7fffffff'}


def family_require(value, message):
    if not value:
        raise ValueError(message)


def family_exact(actual, expected):
    if type(actual) is not type(expected):
        return False
    if type(expected) is dict:
        return set(actual) == set(expected) and all(family_exact(actual[k], v) for k, v in expected.items())
    if type(expected) is list:
        return len(actual) == len(expected) and all(family_exact(a, b) for a, b in zip(actual, expected))
    return actual == expected


def family_json(value):
    def pairs(rows):
        result = {}
        for key, entry in rows:
            family_require(key not in result, 'Duplicate family JSON key')
            result[key] = entry
        return result
    def constant(_):
        raise ValueError('Nonfinite family JSON number')
    return json.loads(value, object_pairs_hook=pairs, parse_constant=constant)


def family_spec(mode):
    family_require(type(mode) is str and mode in FAMILY_MODES, 'Declared family mode required')
    sapling = mode.startswith('owned-wake-sapling')
    supported = mode.endswith('-supported')
    no_item = '-no-item' in mode
    missing_math = mode.endswith('-math-missing')
    publication = mode.endswith('-source') and not mode.endswith('-removal-source')
    removal = mode.endswith('-removal-source')
    scope = mode.endswith('-scope')
    normal = not (missing_math and not no_item or publication or removal or scope)
    seed = 0x7fffffff if no_item else 0x1234 if mode.endswith('-seed1234') else 1
    state = (seed ^ 0x5deece66d) & ((1 << 48) - 1)
    draws = 0
    def bits(count):
        nonlocal state, draws
        state = (state * 0x5deece66d + 11) & ((1 << 48) - 1)
        draws += 1
        return state >> (48 - count)
    item = False
    if not supported:
        bits(24)
        item = True if sapling else (8 * bits(31)) >> 31 == 0
        if item:
            for _ in range(3):
                bits(24)
    family_require(item == (not supported and not no_item), 'Declared Java LCG drop operand')
    added = int(item and not missing_math)
    cells = 1 if supported or publication or scope or missing_math and not no_item else 2
    return dict(sapling=sapling, family='sapling' if sapling else 'tall-grass',
                plant=[6, 2] if sapling else [31, 1], stack=[6, 1, 2] if sapling else [295, 1, 0],
                supported=supported, no_item=no_item, math_owned=not missing_math,
                normal=normal, publication=publication, removal=removal, scope=scope,
                seed=seed, state=state, draws=draws, added=added, cells=cells)


def family_receipts(mode, native, environment, drop, publication):
    s = family_spec(mode)
    expected_env = {'family': s['family'], 'plant_floor': [2 if s['supported'] else 0, 0],
                   'light': [15, 0, 64], 'sky_subtraction': 5, 'chunk_light_seen': True,
                   'world_seed': f"{s['state']:016x}",
                   'player_seed': f'{38 ^ 0x5deece66d:016x}', 'seed_input': s['seed'],
                   'items': s['added'], 'math_owned': s['math_owned'], 'model_draws': s['draws']}
    family_require(family_exact(environment, expected_env), 'Exact family input/environment/LCG receipt')
    family_require(type(native) is dict and set(native) == {'continued', 'raw', 'pc', 'wake_entered',
        'pending', 'result', 'exception', 'after', 'callbacks'}, 'Exact family native envelope')
    family_require(all(type(native[k]) is bool for k in ('continued', 'wake_entered', 'pending')) and
        type(native['raw']) is int and type(native['pc']) is int and native['pc'] == 35 and
        native['wake_entered'] is True and
        native['continued'] is s['normal'] and native['pending'] is (not s['normal']) and
        family_exact(native['result'], False if s['normal'] else None) and
        native['exception'] == ('java/lang/IllegalStateException' if s['publication'] or s['removal'] else 'NONE') and
        (native['raw'] != 0 if s['publication'] or s['removal'] else native['raw'] == 0),
        'Actual family continuation/Source/native outcome')
    after = native['after']
    family_require(type(after) is dict and set(after) == set(AFTER_FIELDS), 'Exact family Player fields')
    removed = not s['supported'] and (s['normal'] or s['removal'])
    prefix = {'sleeping': not s['normal'], 'timer': 0 if s['normal'] else 37, 'health': 20, 'idle': 0,
              'head': [26, 8], 'orphan': [0, 0] if removed else s['plant'],
              'world_sleeping': not s['normal'], 'world_same': True}
    family_require(all(family_exact(after[k], v) for k, v in prefix.items()), 'Committed family wake prefix')
    for key, size, width in (('position', 3, 16), ('dimensions', 3, 8)):
        family_require(type(after[key]) is list and len(after[key]) == size and
            all(type(v) is str and re.fullmatch(f'[0-9a-f]{{{width}}}', v) for v in after[key]),
            'Actual family raw pose fields')
    callbacks = native['callbacks']
    expected_callbacks = [dict(xyz=[8, 64, 8], head=[26, 8], orphan=s['plant'], sleeping=True)]
    if s['cells'] == 2:
        expected_callbacks.append(dict(xyz=[7, 64, 8], head=[26, 8], orphan=[0, 0], sleeping=True))
    family_require(family_exact(callbacks, expected_callbacks), 'Ordered family cell callbacks')
    if s['added']:
        family_require(type(drop) is dict and set(drop) == {'world_seed', 'player_seed', 'item'} and
            drop['world_seed'] == environment['world_seed'] and drop['player_seed'] == environment['player_seed'],
            'Exact actual family Item seeds')
        item = drop['item']
        family_require(type(item) is dict and set(item) == {'position', 'delay', 'stack', 'inserted', 'world_same'} and
            type(item['position']) is list and len(item['position']) == 3 and
            all(type(v) is str and re.fullmatch('[0-9a-f]{16}', v) for v in item['position']) and
            all(family_exact(item[k], v) for k, v in {'delay': 10, 'stack': s['stack'], 'inserted': True, 'world_same': True}.items()),
            'Actual family Item ID/damage independent of Block metadata')
        family_require(family_exact(publication, {'head': [26, 8], 'plant': s['plant'], 'sleeping': True, 'prior_cells': 1}),
            'Actual Item publication precedes plant removal')
    else:
        family_require(drop is None and publication is None, 'No actual Item callback or invented Item receipt')
    return s


def compare_family_original(mode, native, drop, publication, environment, output):
    directory = Path(__file__).resolve().parents[3] / 'tests/reference/entity/sapling_tall_grass_wake'
    spec = importlib.util.spec_from_file_location('plant_family_wake_comparison', directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    row = next(r for r in packet.read_reference()['rows'] if r['mode'] == FAMILY_ORIGINAL[mode])
    keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'world_sleeping', 'head')
    light = row['after']['light_inputs']
    expected = {'result': row['result'], 'exception': row['exception'],
                'after': {**{k: row['after'][k] for k in keys}, 'plant': row['after']['plant']},
                'environment': {'plant_floor': row['after']['plant_floor'],
                    'light': [light['sky_nibble'], light['block_light_nibble'], light['height']],
                    'sky_subtraction': light['world_f_sky_subtraction'], 'chunk_light_seen': light['chunk_sky_seen_static'],
                    'world_seed': row['after']['world_random'][0], 'player_seed': row['after']['player_random'][0]},
                'items': [{k: item[k] for k in ('position', 'delay', 'stack', 'inserted', 'world_same')} for item in row['after']['items']],
                'callbacks': [{k: event[k] for k in ('xyz', 'head', 'plant', 'sleeping')} for event in row['events'] if event['kind'] == 'cell']}
    actual = {'result': native['result'], 'exception': native['exception'],
              'after': {**{k: native['after'][k] for k in keys}, 'plant': native['after']['orphan']},
              'environment': {k: environment[k] for k in expected['environment']},
              'items': [] if drop is None else [drop['item']],
              'callbacks': [{**{k: event[k] for k in ('xyz', 'head', 'sleeping')}, 'plant': event['orphan']} for event in native['callbacks']]}
    if drop is not None:
        added = next(e for e in row['events'] if e['kind'] == 'added')
        expected['publication'] = {**{k: added[k] for k in ('head', 'plant', 'sleeping')},
            'prior_cells': sum(e['kind'] == 'cell' for e in row['events'][:row['events'].index(added)])}
        actual['publication'] = publication
    report = {'mode': mode, 'profile': FAMILY_ORIGINAL[mode], 'matched': packet.P.exact(actual, expected),
              'finite_sha256': packet.P.digest(directory / 'finite.json'), 'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': actual,
              'scope': 'Selected state/callback/Item/seed values only; wider original observations remain unpaired.'}
    (output / 'plant-family-original-comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    family_require(report['matched'], 'Original Sapling/Tall Grass selected projection differs')
    return report['compared_scalars']


def compare_original(mode, native, output):
    directory = (Path(__file__).resolve().parents[3] /
                 'tests/reference/entity/player_recursive_wake')
    spec = importlib.util.spec_from_file_location('player_recursive_wake_packet',
                                                  directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    finite_path = directory / 'finite.json'
    finite = packet.P.load(finite_path)
    packet.validate(finite['rows'])
    digest = packet.P.digest(finite_path)
    proof = packet.P.load(directory / 'provenance.json')
    if digest != proof['finite_sha256']:
        raise RuntimeError('Original recursive wake digest differs from its capture')
    selected = next(row for row in finite['rows'] if row['mode'] == ORIGINAL_MODES[mode])
    expected = {'result': selected['result'], 'exception': selected['exception'],
                'after': {key: selected['after'][key] for key in AFTER_FIELDS},
                'callbacks': [{key: row[key] for key in CALLBACK_FIELDS}
                              for row in selected['callbacks']]}
    observed = {key: native[key] for key in expected}
    matched = packet.P.exact(observed, expected)
    report = {'mode': mode, 'finite_sha256': digest, 'matched': matched,
              'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': observed}
    (output / 'original-comparison.json').write_text(json.dumps(report, indent=2) + '\n',
                                                      encoding='utf-8')
    if not matched:
        raise RuntimeError('Original recursive wake mismatch in ' + str(output))
    return report['compared_scalars']


def compare_attached_original(mode, native, drop, publication, output, supports=None):
    kind = 'torch' if mode.startswith('owned-wake-torch') else 'ladder'
    directory = Path(__file__).resolve().parents[3] / ('tests/reference/entity/' + kind + '_wake')
    spec = importlib.util.spec_from_file_location(kind + '_wake_comparison', directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    packet.check()
    finite = directory / 'finite.json'
    rows = packet.P.load(finite)['rows']
    row = rows[0 if mode == 'owned-wake-' + kind else 1]
    keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'world_sleeping', 'head')
    added = next(event for event in row['events'] if event['kind'] == 'added')
    expected = {'result': row['result'], 'exception': row['exception'],
                'after': {**{key: row['after'][key] for key in keys}, kind: row['after'][kind]},
                'world_seed': row['after']['world_random'][0],
                'player_seed': row['after']['player_random'][0],
                'item': {key: row['after']['items'][0][key] for key in drop['item']},
                'callbacks': [{key: event[key] for key in ('xyz', 'head', kind, 'sleeping')}
                              for event in row['events'] if event['kind'] == 'cell'],
                'publication': {**{key: added[key] for key in ('head', kind, 'sleeping')},
                    'prior_cells': sum(event['kind'] == 'cell'
                                       for event in row['events'][:row['events'].index(added)])}}
    actual = {'result': native['result'], 'exception': native['exception'],
              'after': {**{key: native['after'][key] for key in keys}, kind: native['after']['orphan']},
              **drop, 'publication': publication,
              'callbacks': [{**{key: event[key] for key in ('xyz', 'head', 'sleeping')},
                             kind: event['orphan']} for event in native['callbacks']]}
    if kind == 'torch':
        expected['supports'] = {key: row['after'][key] for key in ('torch_support', 'torch_floor')}
        actual['supports'] = supports
    report = {'mode': mode, 'finite_sha256': packet.P.digest(finite),
              'matched': packet.P.exact(actual, expected), 'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': actual}
    (output / (kind + '-original-comparison.json')).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not report['matched']:
        raise RuntimeError('Original ' + kind + ' wake/drop state differs: ' + str(output))
    return report['compared_scalars']


def flower_environment(mode, value):
    keys = {'flower_floor', 'light', 'sky_subtraction', 'chunk_light_seen', 'world_seed', 'player_seed'}
    supported = mode in ('owned-wake-flower-supported', 'owned-wake-flower-sky-supported')
    dark = mode == 'owned-wake-flower-dark'
    low = dark or mode == 'owned-wake-flower-sky-supported'
    if (type(value) is not dict or set(value) != keys or
            type(value['flower_floor']) is not list or len(value['flower_floor']) != 2 or
            type(value['light']) is not list or len(value['light']) != 3 or
            any(type(n) is not int for n in value['flower_floor'] + value['light']) or
            value['flower_floor'] != [2 if supported or dark else 0, 0] or
            value['light'] != [0 if low and not dark else 15, 0, 64] or
            type(value['sky_subtraction']) is not int or value['sky_subtraction'] != 5 or
            type(value['chunk_light_seen']) is not bool or value['chunk_light_seen'] != (not low) or
            any(type(value[key]) is not str or re.fullmatch(r'[0-9a-f]{16}', value[key]) is None
                for key in ('world_seed', 'player_seed'))):
        raise RuntimeError('Exact actual Flower floor/light/Random observation required')
    return value


def compare_flower_original(mode, native, drop, publication, environment, output):
    directory = Path(__file__).resolve().parents[3] / 'tests/reference/entity/flower_wake'
    spec = importlib.util.spec_from_file_location('flower_wake_comparison', directory / 'packet.py')
    packet = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(packet)
    finite = packet.read_reference()
    profile = {'owned-wake-flower-supported': 'supported', 'owned-wake-flower': 'unsupported',
               'owned-wake-flower-source': 'publication-error'}[mode]
    row = next(r for r in finite['rows'] if r['mode'] == profile)
    keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'world_sleeping', 'head')
    item_keys = ('position', 'delay', 'stack', 'inserted', 'world_same')
    light = row['after']['light_inputs']
    expected = {'result': row['result'], 'exception': row['exception'],
                'after': {**{key: row['after'][key] for key in keys}, 'flower': row['after']['flower']},
                'environment': {'flower_floor': row['after']['flower_floor'],
                    'light': [light['sky_nibble'], light['block_light_nibble'], light['height']],
                    'sky_subtraction': light['world_f_sky_subtraction'],
                    'chunk_light_seen': light['chunk_sky_seen_static'],
                    'world_seed': row['after']['world_random'][0],
                    'player_seed': row['after']['player_random'][0]},
                'items': [{key: item[key] for key in item_keys} for item in row['after']['items']],
                'callbacks': [{key: event[key] for key in ('xyz', 'head', 'flower', 'sleeping')}
                              for event in row['events'] if event['kind'] == 'cell']}
    actual = {'result': native['result'], 'exception': native['exception'],
              'after': {**{key: native['after'][key] for key in keys}, 'flower': native['after']['orphan']},
              'environment': environment, 'items': [] if drop is None else [drop['item']],
              'callbacks': [{**{key: event[key] for key in ('xyz', 'head', 'sleeping')},
                             'flower': event['orphan']} for event in native['callbacks']]}
    if drop is not None:
        added = next(event for event in row['events'] if event['kind'] == 'added')
        expected['publication'] = {**{key: added[key] for key in ('head', 'flower', 'sleeping')},
            'prior_cells': sum(e['kind'] == 'cell' for e in row['events'][:row['events'].index(added)])}
        actual['publication'] = publication
        if drop['world_seed'] != environment['world_seed'] or drop['player_seed'] != environment['player_seed']:
            raise RuntimeError('Flower publication/final Random receipts differ')
    report = {'mode': mode, 'profile': profile, 'finite_sha256': packet.P.digest(directory / 'finite.json'),
              'matched': packet.P.exact(actual, expected), 'compared_scalars': packet.P.scalars(expected),
              'expected': expected, 'observed': actual}
    (output / 'flower-original-comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not report['matched']:
        raise RuntimeError('Original Flower wake state differs: ' + str(output))
    return report['compared_scalars']


def family_parser_controls():
    def case(mode):
        s = family_spec(mode)
        env = {'family': s['family'], 'plant_floor': [2 if s['supported'] else 0, 0],
               'light': [15, 0, 64], 'sky_subtraction': 5, 'chunk_light_seen': True,
               'world_seed': f"{s['state']:016x}", 'player_seed': f'{38 ^ 0x5deece66d:016x}',
               'seed_input': s['seed'], 'items': s['added'], 'math_owned': s['math_owned'], 'model_draws': s['draws']}
        removed = not s['supported'] and (s['normal'] or s['removal'])
        after = dict(sleeping=not s['normal'], timer=0 if s['normal'] else 37, health=20, idle=0,
            head=[26, 8], orphan=[0, 0] if removed else s['plant'], world_sleeping=not s['normal'],
            world_same=True, position=['401e000000000000'] * 3, dimensions=['3f19999a', '3fe66666', '3fcf5c29'])
        callbacks = [dict(xyz=[8, 64, 8], head=[26, 8], orphan=s['plant'], sleeping=True)]
        if s['cells'] == 2:
            callbacks.append(dict(xyz=[7, 64, 8], head=[26, 8], orphan=[0, 0], sleeping=True))
        native = dict(continued=s['normal'], raw=1 if s['publication'] or s['removal'] else 0,
            pc=35, wake_entered=True, pending=not s['normal'], result=False if s['normal'] else None,
            exception='java/lang/IllegalStateException' if s['publication'] or s['removal'] else 'NONE',
            after=after, callbacks=callbacks)
        drop = dict(world_seed=env['world_seed'], player_seed=env['player_seed'], item=dict(
            position=['401ce19e82000000', '40501bf8bc000000', '4020ded390000000'], delay=10,
            stack=s['stack'], inserted=True, world_same=True)) if s['added'] else None
        publication = dict(head=[26, 8], plant=s['plant'], sleeping=True, prior_cells=1) if s['added'] else None
        return [native, env, drop, publication]
    for mode in FAMILY_MODES:
        n, e, d, p = case(mode)
        family_receipts(mode, n, e, d, p)
    changes = (
        ('owned-wake-sapling', lambda c: c[0].update(wake_entered=False)),
        ('owned-wake-sapling-source', lambda c: c[0].update(pc=34)),
        ('owned-wake-tall-grass-no-item', lambda c: c[0].update(pc=36)),
        ('owned-wake-sapling', lambda c: c[1].update(items=True)),
        ('owned-wake-sapling', lambda c: c[1].update(model_draws=3)),
        ('owned-wake-sapling', lambda c: c[2]['item'].update(stack=[6, 1, 0])),
        ('owned-wake-sapling', lambda c: c[3].update(plant=[6, 0])),
        ('owned-wake-sapling', lambda c: c[0]['after'].update(timer=True)),
        ('owned-wake-sapling-source', lambda c: c[0].update(raw=0)),
        ('owned-wake-sapling-source', lambda c: c[0].update(pending=False)),
        ('owned-wake-sapling-scope', lambda c: c[0].update(exception='java/lang/IllegalStateException')),
        ('owned-wake-tall-grass', lambda c: c[2]['item'].update(stack=[31, 1, 1])),
        ('owned-wake-tall-grass', lambda c: c[1].update(world_seed='0' * 16)),
        ('owned-wake-tall-grass', lambda c: c[0]['callbacks'].reverse()),
        ('owned-wake-tall-grass-no-item', lambda c: c.__setitem__(2, {})),
        ('owned-wake-tall-grass-no-item', lambda c: c[1].update(items=1)),
        ('owned-wake-tall-grass-no-item-math-missing', lambda c: c[1].update(math_owned=True)),
        ('owned-wake-tall-grass-supported', lambda c: c[1].update(model_draws=1)),
        ('owned-wake-tall-grass-supported', lambda c: c[0]['after'].update(orphan=[0, 0])),
        ('owned-wake-tall-grass-removal-source', lambda c: c[0]['callbacks'].pop()),
        ('owned-wake-tall-grass-math-missing', lambda c: c[1].update(light=[True, 0, 64])),
    )
    refused = 0
    for mode, change in changes:
        values = copy.deepcopy(case(mode))
        change(values)
        try:
            family_receipts(mode, *values)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupt family receipt accepted: ' + mode)
    for text in ('{"a":0,"a":1}', '{"a":NaN}', '{"a":Infinity}'):
        try:
            family_json(text)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Malformed family JSON accepted')
    family_require(refused == 24, 'Every malformed family receipt refused')
    print('PASS: 15 synthetic family modes; 24 malformed receipts refused; no native process')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=('damage-wake', 'owned-block-drops', 'owned-wake-recursion',
                                           'owned-wake-null-exit', 'owned-wake-observer-error',
                                           'owned-wake-scope-drift', 'owned-wake-null-chunk-world',
                                           'owned-wake-foot', 'owned-wake-foot-source',
                                           'owned-wake-foot-scope', 'owned-wake-foot-math-missing',
                                           'owned-wake-ladder', 'owned-wake-ladder-source',
                                           'owned-wake-ladder-scope', 'owned-wake-ladder-math-missing',
                                           'owned-wake-ladder-removal-source', 'owned-wake-ladder-supported',
                                           'owned-wake-torch', 'owned-wake-torch-source',
                                           'owned-wake-torch-scope', 'owned-wake-torch-math-missing',
                                           'owned-wake-torch-removal-source', 'owned-wake-torch-supported',
                                           'owned-wake-torch-early', 'owned-wake-torch-early-source',
                                           'owned-wake-torch-early-removal-source',
                                           'owned-wake-flower', 'owned-wake-flower-source',
                                           'owned-wake-flower-scope', 'owned-wake-flower-math-missing',
                                           'owned-wake-flower-removal-source', 'owned-wake-flower-supported',
                                           'owned-wake-flower-sky-supported', 'owned-wake-flower-dark') + FAMILY_MODES,
                        default='damage-wake')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        family_parser_controls()
        return
    if args.executable is None or args.work_dir is None:
        parser.error('--executable and --work-dir are required for native runs')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='damage-wake-', dir=args.work_dir.resolve()))
    child = subprocess.run([args.executable.resolve(strict=True), args.mode,
                            str(output).encode('utf-8').hex()], capture_output=True, timeout=120)
    (output / 'native.stdout').write_bytes(child.stdout)
    (output / 'native.stderr').write_bytes(child.stderr)
    if child.returncode != 0 or child.stderr:
        raise RuntimeError('Player damage wake failed in ' + str(output) + '\n' +
                           child.stderr.decode('utf-8', 'replace'))
    lines = child.stdout.splitlines()
    compared = 0
    if args.mode == 'owned-block-drops':
        expected = rb'PASS: [1-9][0-9]* owned Block drop checks'
    elif args.mode != 'damage-wake':
        expected = rb'PASS: [1-9][0-9]* owned Player wake recursion checks'
        diagnostics = [line for line in lines if line.startswith(b'OWNED_WAKE_RECURSION ')]
        cleanup = b'OWNED_WAKE_RECURSION_CLEANUP complete=1'
        if len(diagnostics) != 1 or lines.count(cleanup) != 1:
            raise RuntimeError('Expected one owned wake observation and checked cleanup in ' + str(output))
        native = (family_json if args.mode in FAMILY_MODES else json.loads)(diagnostics[0].partition(b' ')[2])
        family = family_spec(args.mode) if args.mode in FAMILY_MODES else None
        if (type(native) is not dict or
                set(native) != {'continued', 'raw', 'pc', 'wake_entered', 'pending',
                                'result', 'exception', 'after', 'callbacks'} or
                type(native['after']) is not dict or set(native['after']) != set(AFTER_FIELDS) or
                type(native['callbacks']) is not list or len(native['callbacks']) != (family['cells'] if family is not None else 1 if args.mode in (
                    'owned-wake-null-chunk-world', 'owned-wake-ladder-source',
                    'owned-wake-ladder-scope', 'owned-wake-ladder-math-missing',
                    'owned-wake-ladder-supported', 'owned-wake-torch-source', 'owned-wake-torch-scope',
                    'owned-wake-torch-math-missing', 'owned-wake-torch-supported',
                    'owned-wake-torch-early-source', 'owned-wake-flower-source',
                    'owned-wake-flower-scope', 'owned-wake-flower-math-missing',
                    'owned-wake-flower-supported', 'owned-wake-flower-sky-supported') else 2) or
                any(type(row) is not dict or set(row) != set(CALLBACK_FIELDS)
                    for row in native['callbacks'])):
            raise RuntimeError('Invalid owned wake observation in ' + str(output))
        if args.mode in ORIGINAL_MODES:
            compared = compare_original(args.mode, native, output)
        drop_lines = [line for line in lines if line.startswith(b'FOOT_WAKE_DROP ')]
        foot_modes = ('owned-wake-foot', 'owned-wake-foot-source', 'owned-wake-foot-scope')
        if args.mode in foot_modes:
            if len(drop_lines) != 1:
                raise RuntimeError('One actual foot Item observation required')
            drop = json.loads(drop_lines[0].partition(b' ')[2])
            if (type(drop) is not dict or set(drop) != {'world_seed', 'player_seed', 'item'} or
                    type(drop['item']) is not dict or
                    set(drop['item']) != {'position', 'delay', 'stack', 'inserted', 'world_same'}):
                raise RuntimeError('Exact foot Item observation fields required')
            if args.mode != 'owned-wake-foot-scope':
                directory = Path(__file__).resolve().parents[3] / 'tests/reference/entity/bed_foot_wake'
                spec = importlib.util.spec_from_file_location('bed_foot_comparison', directory / 'packet.py')
                packet = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(packet)
                packet.check()
                rows = packet.P.load(directory / 'finite.json')['rows']
                row = rows[0 if args.mode == 'owned-wake-foot' else 1]
                keys = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle',
                        'world_sleeping', 'head', 'orphan')
                selected = {'result': row['result'], 'exception': row['exception'],
                            'after': {key: row['after'][key] for key in keys},
                            'callbacks': [{key: event[key] for key in CALLBACK_FIELDS}
                                          for event in row['events'][:2]],
                            'world_seed': row['after']['world_random'][0],
                            'player_seed': row['after']['player_random'][0],
                            'item': {key: row['after']['items'][0][key] for key in drop['item']}}
                actual = {'result': native['result'], 'exception': native['exception'],
                          'after': {key: native['after'][key] for key in keys},
                          'callbacks': native['callbacks'], **drop}
                report = {'matched': packet.P.exact(actual, selected), 'expected': selected,
                          'observed': actual, 'compared_scalars': packet.P.scalars(selected)}
                (output / 'foot-original-comparison.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
                if not report['matched']:
                    raise RuntimeError('Original foot wake/drop state differs: ' + str(output))
                compared = report['compared_scalars']
            lines.remove(drop_lines[0])
        elif drop_lines:
            raise RuntimeError('Unexpected foot Item observation')
        supports = None
        support_lines = [line for line in lines if line.startswith(b'TORCH_WAKE_SUPPORT ')]
        if args.mode.startswith('owned-wake-torch'):
            if len(support_lines) != 1:
                raise RuntimeError('Expected actual Torch wall and floor state')
            supports = json.loads(support_lines[0].partition(b' ')[2])
            if (type(supports) is not dict or set(supports) != {'torch_support', 'torch_floor'} or
                    any(type(value) is not list or len(value) != 2 or
                        any(type(v) is not int for v in value) for value in supports.values())):
                raise RuntimeError('Exact Torch support cell fields required')
            lines.remove(support_lines[0])
        elif support_lines:
            raise RuntimeError('Unexpected Torch support observation')
        environment = None
        environment_lines = [line for line in lines if line.startswith(b'FLOWER_WAKE_ENV ')]
        if args.mode.startswith('owned-wake-flower'):
            if len(environment_lines) != 1:
                raise RuntimeError('One actual Flower environment observation required')
            environment = flower_environment(args.mode, json.loads(environment_lines[0].partition(b' ')[2]))
            lines.remove(environment_lines[0])
        elif environment_lines:
            raise RuntimeError('Unexpected Flower environment observation')
        range_lines = [line for line in lines if line.startswith(b'FLOWER_WAKE_RANGE ')]
        if args.mode == 'owned-wake-flower-dark':
            if range_lines != [b'FLOWER_WAKE_RANGE min=7,64,8 max=7,65,8 prior-cells=1 Item=1 height-before-store=65']:
                raise RuntimeError('Exact height-change notification before height store required')
            lines.remove(range_lines[0])
        elif range_lines:
            raise RuntimeError('Unexpected Flower height-change notification')
        if args.mode in ('owned-wake-ladder', 'owned-wake-ladder-source', 'owned-wake-ladder-scope',
                         'owned-wake-ladder-removal-source', 'owned-wake-torch',
                         'owned-wake-torch-source', 'owned-wake-torch-scope',
                         'owned-wake-torch-removal-source', 'owned-wake-torch-early',
                         'owned-wake-torch-early-source', 'owned-wake-torch-early-removal-source',
                         'owned-wake-flower', 'owned-wake-flower-source', 'owned-wake-flower-scope',
                         'owned-wake-flower-removal-source', 'owned-wake-flower-dark'):
            kind = ('flower' if args.mode.startswith('owned-wake-flower') else
                    'torch' if args.mode.startswith('owned-wake-torch') else 'ladder')
            neighbor_drops = [line for line in lines if line.startswith(kind.upper().encode('ascii') + b'_WAKE_DROP ')]
            if len(neighbor_drops) != 1:
                raise RuntimeError('Expected one actual ' + kind.title() + ' Item publication')
            lines.remove(neighbor_drops[0])
            publication_lines = [line for line in lines if line.startswith(kind.upper().encode('ascii') + b'_WAKE_PUBLICATION ')]
            if len(publication_lines) != 1:
                raise RuntimeError('Expected actual ' + kind.title() + ' cell state at publication')
            publication = json.loads(publication_lines[0].partition(b' ')[2])
            if (type(publication) is not dict or
                    publication != {'head': [26, 8], kind: ({'flower': [37, 0], 'torch': [50, 1], 'ladder': [65, 2]}[kind]), 'sleeping': True, 'prior_cells': 1} or
                    publication['sleeping'] is not True or type(publication['prior_cells']) is not int or
                    any(type(publication[key]) is not list or any(type(v) is not int for v in publication[key])
                        for key in ('head', kind))):
                raise RuntimeError(kind.title() + ' publication prefix differs')
            drop = json.loads(neighbor_drops[0].partition(b' ')[2])
            if (type(drop) is not dict or set(drop) != {'world_seed', 'player_seed', 'item'} or
                    type(drop['item']) is not dict or
                    set(drop['item']) != {'position', 'delay', 'stack', 'inserted', 'world_same'}):
                raise RuntimeError('Exact ' + kind.title() + ' Item observation required')
            if args.mode in ('owned-wake-ladder', 'owned-wake-ladder-source', 'owned-wake-torch', 'owned-wake-torch-source'):
                compared = compare_attached_original(args.mode, native, drop, publication, output, supports)
            if args.mode in ('owned-wake-flower', 'owned-wake-flower-source'):
                compared = compare_flower_original(args.mode, native, drop, publication, environment, output)
            lines.remove(publication_lines[0])
        if args.mode == 'owned-wake-flower-supported':
            compared = compare_flower_original(args.mode, native, None, None, environment, output)
        family_environment = [line for line in lines if line.startswith(b'PLANT_FAMILY_WAKE_ENV ')]
        family_drops = [line for line in lines if line.startswith(b'PLANT_FAMILY_WAKE_DROP ')]
        family_publications = [line for line in lines if line.startswith(b'PLANT_FAMILY_WAKE_PUBLICATION ')]
        if family is not None:
            family_require(len(family_environment) == 1 and len(family_drops) == family['added'] and
                len(family_publications) == family['added'], 'Exact family receipt occurrence counts')
            env = family_json(family_environment[0].partition(b' ')[2])
            drop = family_json(family_drops[0].partition(b' ')[2]) if family_drops else None
            publication = family_json(family_publications[0].partition(b' ')[2]) if family_publications else None
            family_receipts(args.mode, native, env, drop, publication)
            if args.mode in FAMILY_ORIGINAL:
                compared = compare_family_original(args.mode, native, drop, publication, env, output)
            for line in family_environment + family_drops + family_publications:
                lines.remove(line)
        elif family_environment or family_drops or family_publications:
            raise RuntimeError('Unexpected family receipt')
        lines.remove(diagnostics[0])
        lines.remove(cleanup)
    else:
        expected = rb'PASS: [1-9][0-9]* Player damage wake checks'
    if (not lines or re.fullmatch(expected, lines[-1]) is None or
            any(line not in (b'16 achievements', b'151 recipes') for line in lines[:-1])):
        raise RuntimeError('Unexpected Player damage wake output')
    print(lines[-1].decode('ascii') + '; capture=' + str(output))
    if compared:
        print(f'Original wake matches {compared} selected values')


if __name__ == '__main__':
    main()
