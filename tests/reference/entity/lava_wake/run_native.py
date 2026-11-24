"""Pair retained Lava wake observations with the closed unchanged-original packet."""
import argparse
import copy
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
import packet as W

P, HERE, ROOT = W.P, W.HERE, W.ROOT
N = W.module('lava_native_transport_helpers', HERE.parent / 'remaining_plants_wake/run_native.py')
CONTROLS = {'lava-obsidian-fp-refusal': ('lava-obsidian', 'fp', 1),
            'lava-obsidian-no-math-helper': ('lava-obsidian', 'no-math-helper', 2),
            'lava-obsidian-sound-descendant': ('lava-obsidian', 'sound-descendant', 3),
            'lava-obsidian-particle-descendant': ('lava-obsidian', 'particle-descendant', 4),
            'lava-hot-range-error-no-math-helper': ('lava-hot-range-error', 'no-math-helper', 2)}
MODES = tuple(profile['mode'] for profile in W.profiles()) + tuple(CONTROLS)
COMMON_FIELDS = tuple(key for key in N.STATE_FIELDS if key not in ('plant', 'plant_floor'))
RUNTIME_FIELDS = ('lava', 'water_cell', 'world_o', 'scheduled', 'lighting', 'sky_seen')
OUTCOME_KEYS = {'mode', 'input', 'control', 'scheduled_before', 'continued', 'raw', 'pc',
                'wake_entered', 'result', 'exception', 'native_status'}
EVENT_COMMON = {'kind', 'head', 'lava', 'water_cell', 'sleeping', 'world_o', 'world_random', 'math_random'}
SCOPE = ('Prepared retained Player damage/wake pairs selected original Player/World/Chunk, Lava/Water, '
         'seeded Random, scheduling and ordered Cell/Range/Sound/Particle observations. Java stacks and '
         'unexposed Player fields remain unpaired. Native FP refusal, sound-descendant ownership and '
         'missing MathHelper controls have separate scopes. Exact original scheduled-class before-state '
         'is a declared native input; natural class initialization, encounters, graphics and FPS remain unqualified.')


def require(condition, message):
    P.require(condition, 'Lava native: ' + message)


def mode_spec(mode):
    require(type(mode) is str and mode in MODES, 'declared mode')
    base, control, number = CONTROLS.get(mode, (mode, 'none', 0))
    return next(profile for profile in W.profiles() if profile['mode'] == base), control, number


def prepared(profile):
    return {'id': profile['id'], 'metadata': profile['metadata'],
            'xyz': [profile[key] for key in ('x', 'y', 'z')],
            **{key: profile[key] for key in ('water_direction', 'water_id', 'world_seed', 'cell_error_at',
                                            'sound_error_at', 'particle_error_at', 'range_error_at')}}


def water(value):
    require(value is None or P.vector(value, 5, P.integer), 'typed Water cell')


def runtime(value, phase):
    require(type(value) is dict and set(value) == {'phase', *RUNTIME_FIELDS} and value['phase'] == phase and
            P.vector(value['lava'], 2, P.integer) and type(value['world_o']) is bool and
            type(value['sky_seen']) is bool, 'closed runtime state')
    water(value['water_cell'])
    scheduled = value['scheduled']
    require(type(scheduled) is dict and set(scheduled) == {'initialized', 'sequence', 'ordered', 'members'} and
            type(scheduled['initialized']) is bool and P.word(scheduled['sequence'], 16), 'actual scheduled class state')
    for key in ('ordered', 'members'):
        require(type(scheduled[key]) is list and len(scheduled[key]) <= 16, 'bounded actual scheduled set')
        for entry in scheduled[key]:
            require(type(entry) is dict and set(entry) == {'xyz', 'block', 'time', 'ordinal'} and
                    P.vector(entry['xyz'], 3, P.integer) and type(entry['block']) is int and
                    P.word(entry['time'], 16) and P.word(entry['ordinal'], 16), 'complete scheduled record')
    lighting = value['lighting']
    require(type(lighting) is dict and set(lighting) == {'depth', 'regions'} and type(lighting['depth']) is int and
            type(lighting['regions']) is list and len(lighting['regions']) <= 1024, 'actual lighting queue')
    for region in lighting['regions']:
        require(type(region) is dict and set(region) == {'kind', 'bounds'} and region['kind'] in ('Sky', 'Block') and
                P.vector(region['bounds'], 6, P.integer), 'ordered actual light region')


def event(value):
    require(type(value) is dict and value.get('kind') in ('cell', 'range', 'sound', 'particle'), 'callback kind')
    kind = value['kind']
    keys = EVENT_COMMON | ({'xyz'} if kind in ('cell', 'range') else
                          {'name', 'position', 'volume', 'pitch'} if kind == 'sound' else {'name', 'position', 'motion'})
    require(set(value) == keys and P.vector(value['head'], 2, P.integer) and P.vector(value['lava'], 2, P.integer) and
            type(value['sleeping']) is bool and type(value['world_o']) is bool, 'closed actual callback fields')
    water(value['water_cell'])
    N.random_state(value['world_random']); N.random_state(value['math_random'])
    if kind in ('cell', 'range'):
        require(P.vector(value['xyz'], 3 if kind == 'cell' else 6, P.integer), 'actual notification bounds')
    else:
        require(type(value['name']) is str and value['name'] == ('random.fizz' if kind == 'sound' else 'largesmoke') and
                P.vector(value['position'], 3, lambda v: P.word(v, 16)), 'actual presentation name/position')
        if kind == 'sound': require(P.word(value['volume'], 8) and P.word(value['pitch'], 8), 'actual sound floats')
        else: require(P.vector(value['motion'], 3, lambda v: P.word(v, 16)), 'actual particle motion')


def decode_snapshot(snapshot, planes, reached, phase):
    N.state(snapshot)
    require(snapshot['phase'] == phase and snapshot['items'] == [] and snapshot['math_random']['present'] is True,
            'actual Player phase, seeded Math.random holder and no spawned Items')
    require(type(planes) is dict and set(planes) == {'phase', 'planes'} and planes['phase'] == phase and
            type(planes['planes']) is dict and set(planes['planes']) == set(N.PLANES), 'complete phase-bound planes')
    hashes = {name: N.plane(planes['planes'][name], size) for name, size in N.PLANES.items()}
    runtime(reached, phase)
    require(all(P.exact(snapshot[key], reached[key]) for key in set(snapshot) & set(RUNTIME_FIELDS)),
            'overlapping native snapshot fields agree exactly')
    snapshot.update({key: reached[key] for key in RUNTIME_FIELDS})
    return hashes


def parse(stdout, mode):
    require(type(stdout) is str, 'decoded stdout')
    lines = stdout.splitlines()
    if lines[:2] == ['16 achievements', '151 recipes']: del lines[:2]
    _, _, control = mode_spec(mode)
    descendant = None
    if control in (3, 4):
        require(len(lines) == 19 and lines[8].startswith('LAVA_WAKE_DESCENDANT '), 'actual native descendant receipt')
        descendant = P.loads(lines.pop(8).removeprefix('LAVA_WAKE_DESCENDANT '))
        keys = {'raw_callback', 'callback_kind', 'first_close', 'child_pop', 'second_close', 'parent_slot', 'parent_generation',
                'child_slot', 'child_generation', 'source_exception', 'explicit_child_pops'}
        require(type(descendant) is dict and set(descendant) == keys and
                all(type(descendant[key]) is int for key in keys - {'source_exception', 'callback_kind'}) and
                descendant['callback_kind'] == ('sound' if control == 3 else 'particle') and
                P.exact([descendant[key] for key in ('raw_callback', 'first_close', 'child_pop', 'second_close', 'explicit_child_pops')],
                        [0, 9, 0, 0, 1]) and descendant['source_exception'] is False and
                descendant['parent_slot'] >= 0 and descendant['child_slot'] >= 0 and
                descendant['parent_slot'] != descendant['child_slot'] and
                descendant['parent_generation'] > 0 and descendant['child_generation'] > 0,
                'raw OK/native BUSY, exact child close and checked parent retry')
    prefixes = ('PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'LAVA_WAKE_RUNTIME ', 'LAVA_WAKE_OUTCOME ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ',
                'LAVA_WAKE_RUNTIME ', 'LAVA_WAKE_EVENTS ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ',
                'LAVA_WAKE_RUNTIME ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'LAVA_WAKE_RUNTIME ',
                'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'LAVA_WAKE_RUNTIME ')
    require(len(lines) == 18 and re.fullmatch(r'PASS: [1-9][0-9]* retained Lava wake checks;mode=' + re.escape(mode), lines[-1]),
            'exact checked transport completion')
    require(all(line.startswith(prefix) for line, prefix in zip(lines, prefixes)), 'ordered transport records')
    values = [P.loads(line[len(prefix):]) for line, prefix in zip(lines, prefixes)]
    before, outcome, events = values[0], values[3], values[7]
    before_hashes = decode_snapshot(before, values[1], values[2], 'before')
    require(before['pending'] is False and before['cells'] == 0 and before['event_count'] == 0,
            'genuine prepared state before damage and callbacks')
    require(type(outcome) is dict and set(outcome) == OUTCOME_KEYS and outcome['mode'] == mode and
            all(type(outcome[key]) is int for key in ('control', 'raw', 'pc', 'native_status')) and
            all(type(outcome[key]) is bool for key in ('continued', 'wake_entered')), 'closed typed outcome')
    snapshots = [values[index] for index in (4, 8, 11, 14)]
    phases = ('returned' if outcome['continued'] else 'diagnostic', 'native-close', 'cleanup', 'retired')
    hashes = []
    for snapshot, planes, reached, phase in zip(snapshots, [values[i] for i in (5, 9, 12, 15)],
                                              [values[i] for i in (6, 10, 13, 16)], phases):
        hashes.append(decode_snapshot(snapshot, planes, reached, phase))
    steady = {key: value for key, value in snapshots[0].items() if key not in {'phase', 'pending'}}
    require(all(P.exact({key: value for key, value in snapshot.items() if key not in {'phase', 'pending'}}, steady)
                for snapshot in snapshots[1:]) and all(P.exact(value, hashes[0]) for value in hashes[1:]),
            'native close, cleanup and retirement preserve all observed state/planes')
    require(snapshots[0]['pending'] is (not outcome['continued']) and snapshots[1]['pending'] is snapshots[0]['pending'] and
            snapshots[2]['pending'] is False and snapshots[3]['pending'] is False, 'pending work clears at cleanup only')
    require(type(events) is list and 1 <= len(events) <= 64, 'complete bounded callback stream')
    for value in events: event(value)
    require(snapshots[0]['event_count'] == len(events) and snapshots[0]['cells'] == sum(value['kind'] == 'cell' for value in events),
            'actual callback occurrence counts')
    return {'before': before, 'before_planes': before_hashes, 'outcome': outcome, 'state': snapshots[0], 'planes': hashes[0], 'events': events,
            'descendant': descendant}


def projection(actual):
    snapshot = actual['state']
    return {'result': actual['outcome']['result'], 'exception': actual['outcome']['exception'],
            'state': {key: snapshot[key] for key in (*COMMON_FIELDS, *RUNTIME_FIELDS)},
            'math_random': [snapshot['math_random'][key] for key in ('state', 'cache', 'gaussian')],
            'planes': actual['planes'], 'events': actual['events']}


def original_projection(original):
    return {'result': original['result'], 'exception': original['exception'],
            'state': {key: original['after'][key] for key in (*COMMON_FIELDS, *RUNTIME_FIELDS)},
            'math_random': original['after']['random'][2], 'planes': original['after']['planes'],
            'events': [{key: value for key, value in event.items() if key != 'stack'} for event in original['events']]}


def control_expected(control, base, originals):
    source = originals['lava-obsidian-particle-error-fourth' if control == 4 else 'lava-obsidian-sound-error']
    expected = copy.deepcopy(original_projection(source))
    expected.update(result=None, exception='NONE')
    # These explicit native controls start with the first original profile's
    # actual class state, with no scheduled request reached before fizz.
    expected['state']['scheduled'] = copy.deepcopy(base['before']['scheduled'])
    if control == 1:
        expected['events'] = expected['events'][:-1]
        require(source['events'][-1]['kind'] == 'sound', 'declared fizz-first Sound prefix')
        expected['state']['world_random'] = copy.deepcopy(base['before']['world_random'])
        expected['math_random'] = copy.deepcopy(base['before']['random'][2])
    return expected


def validate(actual, mode, originals):
    profile, _, control = mode_spec(mode)
    original = originals[profile['mode']]
    outcome = actual['outcome']
    require(P.exact(outcome['input'], prepared(profile)) and outcome['control'] == control and
            P.exact(outcome['scheduled_before'], {key: original['before']['scheduled'][key] for key in ('sequence', 'initialized')}),
            'exact declared native inputs and source class alignment')
    before_expected = {'state': {key: original['before'][key] for key in (*COMMON_FIELDS, *RUNTIME_FIELDS)},
                       'math_random': original['before']['random'][2], 'planes': original['before']['planes']}
    before_actual = {'state': {key: actual['before'][key] for key in (*COMMON_FIELDS, *RUNTIME_FIELDS)},
                     'math_random': [actual['before']['math_random'][key] for key in ('state', 'cache', 'gaussian')],
                     'planes': actual['before_planes']}
    require(P.exact(before_actual, before_expected), 'actual prepared Player/World/Chunk/Random/class state')
    native_refused = control in (1, 3, 4)
    normal = original['exception'] == 'NONE' and not native_refused
    require(outcome['continued'] is normal and outcome['wake_entered'] is True and outcome['pc'] == 35 and
            P.exact(outcome['result'], False if normal else None) and
            outcome['exception'] == ('NONE' if native_refused else original['exception']) and
            outcome['native_status'] == (5 if control == 1 else 9 if control in (3, 4) else 0) and
            (outcome['raw'] == 0 if native_refused or normal else outcome['raw'] != 0),
            'separate reached Source outcome and native continuation')
    expected = control_expected(control, original, originals) if native_refused else original_projection(original)
    observed = projection(actual)
    require(P.exact(observed, expected), 'selected original/prefix values differ in ' + mode)
    require(actual['state']['math_random']['present'] is True and actual['state']['world_entities'] == 1,
            'genuine Math.random holder and unchanged Entity ledger')
    return {'matched': True, 'compared_scalars': 0 if native_refused else P.scalars(expected) + P.scalars(before_expected),
            'kind': 'native-only stopped prefix' if native_refused else 'selected original pair',
            'before_expected': before_expected, 'before_observed': before_actual,
            'expected': expected, 'observed': observed}


def native_arguments(executable, mode, profile, work, scheduled, control):
    numeric = [profile[key] for key in ('id', 'metadata', 'x', 'y', 'z', 'water_direction', 'water_id', 'world_seed',
                                       'cell_error_at', 'sound_error_at', 'particle_error_at', 'range_error_at')]
    require(all(type(value) is int for value in numeric), 'typed actual native fixture arguments')
    return [str(executable), mode, str(work).encode('utf-8').hex(), *(str(value) for value in numeric),
            scheduled['sequence'], '1' if scheduled['initialized'] else '0', control]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    originals = {row['mode']: row for row in W.read_reference()['rows']}
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='lava-wake-', dir=args.work_dir.resolve()))
    capture = W.module('lava_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    inputs = [executable, Path(__file__).resolve(), HERE / 'packet.py', HERE / 'finite.json', HERE / 'profiles.json',
              HERE / 'provenance.json', HERE.parent / 'remaining_plants_wake/run_native.py',
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/entity/player/test_lava_damage_wake.c',
              ROOT / 'tests/entity/player/test_plant_damage_wake.c', ROOT / 'tests/entity/player/test_sleep_wake.c']
    before = N.digests(inputs)
    report = {'schema': 'LAVA_WAKE_NATIVE_V1', 'status': 'FAIL', 'scope': SCOPE, 'input_before_sha256': before, 'cases': []}
    try:
        for mode in MODES:
            profile, control, _ = mode_spec(mode)
            work = output / mode
            work.mkdir()
            receipt = capture.capture(native_arguments(executable, mode, profile, work,
                                      originals[profile['mode']]['before']['scheduled'], control), work, 'native', 60, os.environ.copy())
            record = {'mode': mode, 'native': receipt}
            report['cases'].append(record)
            capture.require_success(receipt, empty_stderr=True)
            actual = parse((work / receipt['stdout_file']).read_text(encoding='utf-8'), mode)
            record['comparison'] = validate(actual, mode, originals)
        count = len(W.MODES)
        report.update(status='PASS_SELECTED_COMPONENTS_ONLY', original_pairs=count, native_controls=len(CONTROLS),
                      original_scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases'][:count]),
                      control_scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases'][count:]))
    finally:
        report['input_after_sha256'] = N.digests(inputs)
        if report['input_after_sha256'] != before: report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(before == report['input_after_sha256'], 'native replay inputs unchanged')
    print('Lava wake:', count, 'original pairs,', len(CONTROLS), 'native controls;', report['original_scalar_comparisons'],
          'selected original scalar comparisons;', output / 'report.json')


if __name__ == '__main__':
    main()
