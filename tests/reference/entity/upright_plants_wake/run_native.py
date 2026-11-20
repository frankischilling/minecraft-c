"""Pair retained native Cactus/Reeds wake calls with unchanged-original observations."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
import packet as W

P, HERE, ROOT = W.P, W.HERE, W.ROOT
N = W.module('upright_native_transport_helpers', HERE.parent / 'remaining_plants_wake/run_native.py')
MISSING = ('cactus-bed', 'reeds-dry-grass', 'reeds-west-water', 'reeds-column3')
MODES = tuple(profile['mode'] for profile in W.profiles()) + tuple(mode + '-math-missing' for mode in MISSING)
RUNTIME_FIELDS = ('world_o', 'water_cell', 'scheduled')
STATE_FIELDS = N.STATE_FIELDS + ('column',) + RUNTIME_FIELDS
OUTCOME_KEYS = N.OUTCOME_KEYS - {'model_draws'}
SCOPE = ('Twenty-one prepared Player damage/wake calls pair selected original Player/World/column states, '
         'full Chunk planes, seeded World/Player/Math Random, Items and ordered callbacks. Four native '
         'missing-Math controls and complete cleanup stability checks are separate. Original Item '
         'entropy, Java stacks, natural encounters, graphics and FPS remain unpaired. Native source-class '
         'scheduled initialization and sequence are explicit prepared inputs from each original before-state; '
         'natural shared class initialization is not qualified.')


def require(condition, message):
    P.require(condition, 'Upright plant native: ' + message)


def profile_for(mode):
    require(type(mode) is str and mode in MODES, 'declared mode')
    return next(profile for profile in W.profiles() if profile['mode'] == mode.removesuffix('-math-missing'))


def column(value):
    require(P.vector(value, 3, lambda row: P.vector(row, 2, P.integer)) and
            all(0 <= row[0] <= 255 and 0 <= row[1] <= 15 for row in value), 'typed three-cell column')


def water(value):
    require(value is None or P.vector(value, 5, P.integer), 'typed actual water cell')


def runtime(value, phase):
    require(type(value) is dict and set(value) == {'phase', *RUNTIME_FIELDS} and value['phase'] == phase and
            type(value['world_o']) is bool, 'closed actual runtime snapshot')
    water(value['water_cell'])
    scheduled = value['scheduled']
    require(type(scheduled) is dict and set(scheduled) == {'initialized', 'sequence', 'ordered', 'members'} and
            type(scheduled['initialized']) is bool and P.word(scheduled['sequence'], 16), 'scheduled class state')
    for name in ('ordered', 'members'):
        require(type(scheduled[name]) is list and len(scheduled[name]) <= 16, 'bounded actual queue')
        for entry in scheduled[name]:
            require(type(entry) is dict and set(entry) == {'xyz', 'block', 'time', 'ordinal'} and
                    P.vector(entry['xyz'], 3, P.integer) and type(entry['block']) is int and
                    P.word(entry['time'], 16) and P.word(entry['ordinal'], 16), 'complete scheduled record')


def parse(stdout, mode):
    require(type(stdout) is str, 'decoded stdout')
    lines = stdout.splitlines()
    if lines[:2] == ['16 achievements', '151 recipes']: del lines[:2]
    prefixes = ('UPRIGHT_WAKE_RUNTIME ', 'PLANT_WAKE_OUTCOME ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ',
                'UPRIGHT_WAKE_COLUMN ', 'UPRIGHT_WAKE_RUNTIME ', 'PLANT_WAKE_EVENTS ',
                'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'UPRIGHT_WAKE_COLUMN ', 'UPRIGHT_WAKE_RUNTIME ',
                'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'UPRIGHT_WAKE_COLUMN ', 'UPRIGHT_WAKE_RUNTIME ',
                'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'UPRIGHT_WAKE_COLUMN ', 'UPRIGHT_WAKE_RUNTIME ')
    require(len(lines) == 20 and re.fullmatch(r'PASS: [1-9][0-9]* retained upright plant wake checks;mode=' +
            re.escape(mode), lines[-1]), 'exact checked stdout completion')
    require(all(line.startswith(prefix) for line, prefix in zip(lines, prefixes)), 'ordered transport records')
    rows = [P.loads(line[len(prefix):]) for line, prefix in zip(lines, prefixes)]
    before, outcome, events = rows[0], rows[1], rows[6]
    runtime(before, 'before')
    require(type(outcome) is dict and set(outcome) == OUTCOME_KEYS and outcome['mode'] == mode and
            all(type(outcome[key]) is bool for key in ('continued', 'wake_entered', 'math_failed')) and
            all(type(outcome[key]) is int for key in ('raw', 'pc')), 'closed typed outcome')
    snapshots = [rows[index] for index in (2, 7, 11, 15)]
    phases = ('returned' if outcome['continued'] else 'diagnostic', 'native-close', 'cleanup', 'retired')
    plane_hashes = []
    for snapshot, phase, planes, cells, actual_runtime in zip(snapshots, phases,
            [rows[index] for index in (3, 8, 12, 16)], [rows[index] for index in (4, 9, 13, 17)],
            [rows[index] for index in (5, 10, 14, 18)]):
        N.state(snapshot)
        require(snapshot['phase'] == phase and type(planes) is dict and set(planes) == {'phase', 'planes'} and
                planes['phase'] == phase and type(planes['planes']) is dict and set(planes['planes']) == set(N.PLANES),
                'complete phase-bound plane snapshot')
        plane_hashes.append({name: N.plane(planes['planes'][name], size) for name, size in N.PLANES.items()})
        require(type(cells) is dict and set(cells) == {'phase', 'column'} and cells['phase'] == phase,
                'phase-bound column snapshot')
        column(cells['column'])
        snapshot['column'] = cells['column']
        runtime(actual_runtime, phase)
        snapshot.update({key: actual_runtime[key] for key in RUNTIME_FIELDS})
        require(P.exact(snapshot['plant'], snapshot['column'][0]), 'same actual base cell')
    steady = {key: value for key, value in snapshots[0].items() if key not in {'phase', 'pending'}}
    require(all(P.exact({key: value for key, value in snapshot.items() if key not in {'phase', 'pending'}}, steady)
                for snapshot in snapshots[1:]) and all(P.exact(value, plane_hashes[0]) for value in plane_hashes[1:]),
            'complete observed state and raw planes persist through cleanup')
    require(snapshots[0]['pending'] is (not outcome['continued']) and
            snapshots[1]['pending'] is snapshots[0]['pending'] and
            snapshots[2]['pending'] is False and snapshots[3]['pending'] is False,
            'pending ownership clears at cleanup')
    require(type(events) is list and 1 <= len(events) <= 8, 'bounded callbacks')
    for event in events:
        require(type(event) is dict and event.get('kind') in ('cell', 'added', 'range'), 'typed callback kind')
        added = event['kind'] == 'added'
        require(set(event) == {'kind', 'head', 'plant', 'column', 'sleeping', 'world_seed', 'world_o', 'water_cell',
                              'item' if added else 'xyz'} and P.vector(event['head'], 2, P.integer) and
                P.vector(event['plant'], 2, P.integer) and type(event['sleeping']) is bool and
                P.word(event['world_seed'], 16) and int(event['world_seed'], 16) < (1 << 48) and
                type(event['world_o']) is bool, 'closed callback')
        water(event['water_cell'])
        column(event['column'])
        require(P.exact(event['plant'], event['column'][0]), 'callback actual base cell')
        if added: N.item(event['item'])
        else: require(P.vector(event['xyz'], 6 if event['kind'] == 'range' else 3, P.integer), 'typed callback position')
    first = snapshots[0]
    require(first['cells'] == sum(event['kind'] == 'cell' for event in events) and
            first['event_count'] == len(events) and len(first['items']) == sum(event['kind'] == 'added' for event in events),
            'actual callback occurrence counts')
    return {'before': before, 'outcome': outcome, 'state': first, 'events': events, 'planes': plane_hashes[0]}


def projection(actual):
    snapshot = actual['state']
    return {'result': actual['outcome']['result'], 'exception': actual['outcome']['exception'],
            'state': {key: snapshot[key] for key in STATE_FIELDS}, 'planes': actual['planes'],
            'math_random': [snapshot['math_random'][key] for key in ('state', 'cache', 'gaussian')],
            'items': [N.selected_item(value) for value in snapshot['items']],
            'events': [{key: N.selected_item(value[key]) if key == 'item' else value[key]
                        for key in ('kind', 'head', 'plant', 'column', 'sleeping', 'world_o', 'water_cell',
                                    'item' if value['kind'] == 'added' else 'xyz')}
                       for value in actual['events']]}


def original_projection(original):
    return {'result': original['result'], 'exception': original['exception'],
            'state': {key: original['after'][key] for key in STATE_FIELDS},
            'planes': original['after']['planes'], 'math_random': original['after']['random'][2],
            'items': [N.selected_item(value) for value in original['after']['items']],
            'events': [{key: N.selected_item(value[key]) if key == 'item' else value[key]
                        for key in ('kind', 'head', 'plant', 'column', 'sleeping', 'world_o', 'water_cell',
                                    'item' if value['kind'] == 'added' else 'xyz')}
                       for value in original['events']]}


def validate(actual, mode, originals):
    profile = profile_for(mode)
    original = originals[profile['mode']]
    missing = mode.endswith('-math-missing')
    outcome, snapshot, events = (actual[key] for key in ('outcome', 'state', 'events'))
    expected_input = {key: value for key, value in profile.items() if key != 'mode'}
    expected_input['math_owned'] = not missing
    expected_input['scheduled_before'] = {key: original['before']['scheduled'][key] for key in ('sequence', 'initialized')}
    require(P.exact(outcome['input'], expected_input), 'declared prepared operands')
    require(P.exact({key: actual['before'][key] for key in RUNTIME_FIELDS},
                    {key: original['before'][key] for key in RUNTIME_FIELDS}), 'exact prepared source class and World state')
    math_failed = missing and bool(original['after']['items'])
    normal = original['exception'] == 'NONE' and not math_failed
    require(outcome['continued'] is normal and outcome['math_failed'] is math_failed and
            outcome['wake_entered'] is True and outcome['pc'] == 35 and
            P.exact(outcome['result'], False if normal else None) and
            outcome['exception'] == ('NONE' if normal or math_failed else 'java/lang/IllegalStateException') and
            (outcome['raw'] != 0 if not normal and not math_failed else outcome['raw'] == 0),
            'first result or failure at GS35')
    require(P.exact(snapshot['light'], [profile['light'], 0, 64]) and snapshot['world_f'] == 5 and
            snapshot['sky_seen'] is original['after']['light_inputs']['chunk_sky_seen_static'] and
            snapshot['math_random']['present'] is True and snapshot['world_entities'] == 1 + len(snapshot['items']),
            'actual light, Chunk flag and World ledger')
    observed = projection(actual)
    if not math_failed:
        expected = original_projection(original)
        require(P.exact(observed, expected), 'selected original values differ in ' + mode)
        result = {'matched': True, 'compared_scalars': P.scalars(expected), 'expected': expected, 'observed': observed}
    else:
        # The completed first Item constructor consumes four seeded profile-Math
        # doubles before the absent execution Math owner stops derived work.
        interrupted = originals['cactus-publication-error' if profile['id'] == 81 else 'reeds-publication-error']
        expected_state = {key: interrupted['after'][key] for key in STATE_FIELDS if key != 'column'}
        expected_state.update(column=original['before']['column'], world_entities=1,
                              plant=[profile['id'], 0], plant_floor=[profile['floor'], 0],
                              **{key: original['before'][key] for key in RUNTIME_FIELDS})
        require(P.exact(observed['state'], expected_state) and
                P.exact(observed['planes'], W.planes(profile, True, 0)) and
                P.exact(observed['math_random'], interrupted['after']['random'][2]) and
                snapshot['items'] == [] and snapshot['cells'] == 1 and len(events) == 1,
                'native missing-Math completed constructor prefix')
        result = {'matched': None, 'compared_scalars': 0, 'native_only': 'missing Math owner'}
    random = W.RandomModel(profile['world_seed'])
    # Chance and three position floats run for each reached upright-plant Item.
    item_index = 0
    for index, event in enumerate(events):
        require(P.exact(event['head'], [26, 8]) and event['sleeping'] is True, 'callback observes sleeping Player')
        if event['kind'] == 'added':
            for _ in range(4): random.floating()
            require(P.exact(event['item'], snapshot['items'][item_index]), 'same actual Item occurrence')
            item_index += 1
        elif event['kind'] == 'cell':
            require(P.exact(event['xyz'], [8, 64, 8] if index == 0 else [7, 63 + item_index, 8]),
                    'ordered base-to-top cell callback')
        require(event['world_seed'] == random.observation()[0], 'callback World Random prefix')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    originals = {row['mode']: row for row in W.read_reference()['rows']}
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='upright-plants-', dir=args.work_dir.resolve()))
    capture = W.module('upright_plants_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    inputs = [executable, Path(__file__).resolve(), HERE / 'packet.py', HERE / 'finite.json', HERE / 'profiles.json',
              HERE / 'provenance.json', HERE.parent / 'remaining_plants_wake/run_native.py',
              ROOT / 'tests/reference/client/controller/capture.py',
              ROOT / 'tests/entity/player/test_upright_plant_damage_wake.c',
              ROOT / 'tests/entity/player/test_plant_damage_wake.c', ROOT / 'tests/entity/player/test_sleep_wake.c']
    before = N.digests(inputs)
    report = {'schema': 'UPRIGHT_PLANTS_WAKE_NATIVE_V1', 'status': 'FAIL', 'scope': SCOPE,
              'input_before_sha256': before, 'cases': []}
    try:
        for mode in MODES:
            work = output / mode
            work.mkdir()
            scheduled = originals[profile_for(mode)['mode']]['before']['scheduled']
            receipt = capture.capture([str(executable), mode, str(work).encode('utf-8').hex(),
                                       scheduled['sequence'], '1' if scheduled['initialized'] else '0'],
                                      work, 'native', 60, os.environ.copy())
            record = {'mode': mode, 'native': receipt}
            report['cases'].append(record)
            capture.require_success(receipt, empty_stderr=True)
            actual = parse((work / receipt['stdout_file']).read_text(encoding='utf-8'), mode)
            record['comparison'] = validate(actual, mode, originals)
        report.update(status='PASS_SELECTED_COMPONENTS_ONLY', paired_calls=len(W.MODES), native_controls=4,
                      original_scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases'][:len(W.MODES)]),
                      control_scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases'][len(W.MODES):]),
                      scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases']))
    finally:
        report['input_after_sha256'] = N.digests(inputs)
        if report['input_after_sha256'] != before: report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(before == report['input_after_sha256'], 'native replay inputs unchanged')
    print('Upright plant wake:', len(W.MODES), 'original pairs, 4 native controls;', report['scalar_comparisons'],
          'exact selected scalar comparisons;', output / 'report.json')


if __name__ == '__main__':
    main()
