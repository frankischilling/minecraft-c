"""Pair selected native plant wake observations with the closed original packet."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile

sys.dont_write_bytecode = True
import packet as W

P = W.P
HERE, ROOT = W.HERE, W.ROOT
MISSING = ('dead-bush-unsupported', 'crop-unsupported-stage0-seed1',
           'crop-unsupported-stage7-seed1', 'mushroom39-unsupported', 'mushroom40-unsupported')
MODES = W.MODES + tuple(name + '-math-missing' for name in MISSING)
STATE_FIELDS = ('position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'head',
                'plant', 'plant_floor', 'world_sleeping', 'world_same', 'world_random',
                'player_random', 'world_entities')
ITEM_FIELDS = ('position', 'delay', 'stack', 'inserted', 'world_same', 'phase', 'yaw', 'motion')
PLANES = {'blocks': 32768, 'metadata': 16384, 'sky': 16384, 'block_light': 16384, 'height': 256}
STATE_KEYS = set(STATE_FIELDS) | {'phase', 'pending', 'light', 'world_f', 'sky_seen',
                                'math_random', 'cells', 'event_count', 'items'}
OUTCOME_KEYS = {'mode', 'input', 'continued', 'raw', 'pc', 'wake_entered', 'result',
                'exception', 'math_failed', 'model_draws'}
SCOPE = ('25 prepared Player damage/wake calls pair selected original state, Items and callbacks. '
         'Five native-only missing-Math controls and cleanup stability checks are separate. '
         'Complete Chunk plane hashes and seeded Math streams also pair. Original Item entropy, Java stacks, '
         'natural encounters, graphics and FPS remain unpaired.')


def require(condition, message):
    P.require(condition, 'Plant wake native: ' + message)


def profile_for(mode):
    require(type(mode) is str and mode in MODES, 'declared mode')
    base = mode.removesuffix('-math-missing')
    return next(profile for profile in W.profiles() if profile['mode'] == base)


def random_state(value):
    require(P.vector(value, 3, lambda _: True) and P.word(value[0], 16) and
            int(value[0], 16) < (1 << 48) and type(value[1]) is bool and P.word(value[2], 16),
            'typed complete Random observation')


def item(value):
    require(type(value) is dict and set(value) == set(ITEM_FIELDS) | {'random'} and
            P.vector(value['position'], 3, lambda v: P.word(v, 16)) and
            P.vector(value['stack'], 3, P.integer) and P.word(value['phase'], 8) and
            P.word(value['yaw'], 8) and P.vector(value['motion'], 3, lambda v: P.word(v, 16)) and
            type(value['delay']) is int and
            value['delay'] == 10 and value['inserted'] is True and value['world_same'] is True,
            'complete native published Item')
    random_state(value['random'])


def state(value):
    require(type(value) is dict and set(value) == STATE_KEYS, 'closed state fields')
    require(P.vector(value['position'], 3, lambda v: P.word(v, 16)) and
            P.vector(value['dimensions'], 3, lambda v: P.word(v, 8)), 'raw Player values')
    for key in ('pending', 'sleeping', 'world_sleeping', 'world_same', 'sky_seen'):
        require(type(value[key]) is bool, 'Boolean ' + key)
    for key in ('timer', 'health', 'idle', 'world_entities', 'world_f', 'cells', 'event_count'):
        require(type(value[key]) is int and value[key] >= 0, 'integer ' + key)
    for key in ('head', 'plant', 'plant_floor'):
        require(P.vector(value[key], 2, P.integer) and 0 <= value[key][0] <= 255 and
                0 <= value[key][1] <= 15, 'cell ' + key)
    require(P.vector(value['light'], 3, P.integer), 'light operands')
    random_state(value['world_random']); random_state(value['player_random'])
    math = value['math_random']
    require(type(math) is dict and set(math) == {'present', 'phase', 'state', 'cache', 'gaussian'} and
            type(math['present']) is bool and type(math['phase']) is int and 0 <= math['phase'] <= 3,
            'closed native Math holder')
    random_state([math['state'], math['cache'], math['gaussian']])
    require(type(value['items']) is list and len(value['items']) <= 4, 'bounded Items')
    for entry in value['items']: item(entry)


def plane(value, length):
    require(type(value) is list and len(value) <= length, 'bounded plane runs')
    decoded = bytearray(length)
    end, previous = 0, 0
    for row in value:
        require(P.vector(row, 3, P.integer), 'typed plane run')
        start, count, byte = row
        require(start >= end and count > 0 and start + count <= length and 0 < byte <= 255 and
                (start != end or byte != previous), 'ordered nonzero maximal plane runs')
        decoded[start:start + count] = bytes([byte]) * count
        end, previous = start + count, byte
    return hashlib.sha256(decoded).hexdigest()


def parse(stdout, mode):
    require(type(stdout) is str, 'decoded stdout')
    lines = stdout.splitlines()
    # Source class initialization output may precede the authored transport.
    for startup in ('16 achievements', '151 recipes'):
        if lines and lines[0] == startup: lines.pop(0)
    prefixes = ('PLANT_WAKE_OUTCOME ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ',
                'PLANT_WAKE_EVENTS ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ',
                'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ', 'PLANT_WAKE_STATE ', 'PLANT_WAKE_PLANES ')
    require(len(lines) == 11 and re.fullmatch(
        r'PASS: [1-9][0-9]* retained plant wake checks;mode=' + re.escape(mode), lines[-1]),
        'exact completed stdout inventory')
    require(all(line.startswith(prefix) for line, prefix in zip(lines, prefixes)),
            'ordered outcome, snapshots and events')
    values = [P.loads(line[len(prefix):]) for line, prefix in zip(lines, prefixes)]
    outcome, initial, first_planes, events, closed, closed_planes, cleaned, cleaned_planes, retired, retired_planes = values
    require(type(outcome) is dict and set(outcome) == OUTCOME_KEYS, 'closed outcome')
    require(type(outcome['mode']) is str and outcome['mode'] == mode and
            all(type(outcome[key]) is bool for key in ('continued', 'wake_entered', 'math_failed')) and
            all(type(outcome[key]) is int for key in ('raw', 'pc', 'model_draws')) and
            outcome['model_draws'] >= 0, 'typed outcome')
    for snapshot in (initial, closed, cleaned, retired): state(snapshot)
    phases = ('returned' if outcome['continued'] else 'diagnostic', 'native-close', 'cleanup', 'retired')
    for snapshot, phase in zip((initial, closed, cleaned, retired), phases):
        require(snapshot['phase'] == phase, 'snapshot phase order')
    plane_hashes = []
    for value, phase in zip((first_planes, closed_planes, cleaned_planes, retired_planes), phases):
        require(type(value) is dict and set(value) == {'phase', 'planes'} and value['phase'] == phase and
                type(value['planes']) is dict and set(value['planes']) == set(PLANES), 'complete plane snapshot')
        plane_hashes.append({key: plane(value['planes'][key], length) for key, length in PLANES.items()})
    require(all(P.exact(value, plane_hashes[0]) for value in plane_hashes[1:]), 'cleanup preserves all raw Chunk planes')
    steady = {key: value for key, value in initial.items() if key not in {'phase', 'pending'}}
    for snapshot in (closed, cleaned, retired):
        require(P.exact({key: value for key, value in snapshot.items() if key not in {'phase', 'pending'}}, steady),
                'cleanup preserves complete observed state')
    require(initial['pending'] is (not outcome['continued']) and
            closed['pending'] is initial['pending'] and cleaned['pending'] is False and
            retired['pending'] is False, 'pending ownership clears only at cleanup')
    require(type(events) is list and 1 <= len(events) <= 6, 'bounded ordered callbacks')
    for event in events:
        require(type(event) is dict and event.get('kind') in ('cell', 'added'), 'callback kind')
        added = event['kind'] == 'added'
        require(set(event) == {'kind', 'head', 'plant', 'sleeping', 'world_seed',
                              'item' if added else 'xyz'} and
                P.vector(event['head'], 2, P.integer) and P.vector(event['plant'], 2, P.integer) and
                type(event['sleeping']) is bool and P.word(event['world_seed'], 16) and
                int(event['world_seed'], 16) < (1 << 48), 'closed typed callback')
        if added: item(event['item'])
        else: require(P.vector(event['xyz'], 3, P.integer), 'cell coordinates')
    require(initial['event_count'] == len(events) and
            initial['cells'] == sum(event['kind'] == 'cell' for event in events) and
            len(initial['items']) == sum(event['kind'] == 'added' for event in events),
            'callback occurrence counts')
    return {'outcome': outcome, 'state': initial, 'events': events, 'planes': plane_hashes[0]}


def selected_item(value):
    return {key: value[key] for key in ITEM_FIELDS}


def compare(actual, original):
    """Use actual original values, retaining every unpaired field in raw evidence."""
    outcome, snapshot, events = (actual[key] for key in ('outcome', 'state', 'events'))
    expected = {'result': original['result'], 'exception': original['exception'],
                'state': {key: original['after'][key] for key in STATE_FIELDS},
                'items': [selected_item(value) for value in original['after']['items']],
                'planes': original['after']['planes'], 'math_random': original['after']['random'][2],
                'events': [{key: selected_item(value[key]) if key == 'item' else value[key]
                            for key in ('kind', 'head', 'plant', 'sleeping',
                                        'item' if value['kind'] == 'added' else 'xyz')}
                           for value in original['events']]}
    observed = {'result': outcome['result'], 'exception': outcome['exception'],
                'state': {key: snapshot[key] for key in STATE_FIELDS},
                'items': [selected_item(value) for value in snapshot['items']],
                'planes': actual['planes'],
                'math_random': [snapshot['math_random'][key] for key in ('state', 'cache', 'gaussian')],
                'events': [{key: selected_item(value[key]) if key == 'item' else value[key]
                            for key in ('kind', 'head', 'plant', 'sleeping',
                                        'item' if value['kind'] == 'added' else 'xyz')}
                           for value in events]}
    require(P.exact(observed, expected), 'selected original values differ in ' + outcome['mode'])
    return {'matched': True, 'compared_scalars': P.scalars(expected),
            'expected': expected, 'observed': observed}


def validate(actual, mode, originals):
    profile = profile_for(mode)
    missing = mode.endswith('-math-missing')
    outcome, snapshot, events = (actual[key] for key in ('outcome', 'state', 'events'))
    expected_input = {key: value for key, value in profile.items() if key != 'mode'}
    expected_input.update(math_owned=not missing, initial_sky_seen=mode != MODES[0])
    require(P.exact(outcome['input'], expected_input), 'exact prepared operands')
    predicted = W.model(profile)
    math_failed = missing and bool(predicted['items'])
    normal = predicted['normal'] and not math_failed
    require(outcome['continued'] is normal and outcome['wake_entered'] is True and
            outcome['math_failed'] is math_failed and outcome['pc'] == 35 and
            P.exact(outcome['result'], False if normal else None) and
            outcome['exception'] == ('NONE' if normal or math_failed else 'java/lang/IllegalStateException') and
            (outcome['raw'] != 0 if not normal and not math_failed else outcome['raw'] == 0),
            'reached GS35 and first failure category')
    require(P.exact(snapshot['light'], [profile['light'], 0, 64]) and
            snapshot['world_f'] == 5 and snapshot['sky_seen'] is True and
            snapshot['math_random']['present'] is True and
            snapshot['world_entities'] == 1 + len(snapshot['items']), 'actual World/light operands')
    if not math_failed:
        require(outcome['model_draws'] == predicted['draws'], 'complete independent draw count')
        result = compare(actual, originals[profile['mode']])
    else:
        # The native owner failure occurs after the first Item's three position
        # draws and completed Item construction, before World insertion. The
        # Item constructor has consumed its four profile-Math doubles; the
        # missing execution Math owner stops the later derived-state work.
        random = W.RandomModel(profile['world_seed'])
        random.floating()
        if profile['id'] == 59 and profile['metadata'] != 7:
            for _ in range(3):
                if random.bounded(15) <= profile['metadata']: break
            else: raise ValueError('Declared missing-Math profile must reach an Item')
        for _ in range(3): random.floating()
        interrupted = originals['crop-publication-error-first' if profile['id'] == 59 else
                                f"mushroom{profile['id']}-publication-error"]['after']
        require(outcome['model_draws'] == random.draws and
                P.exact(snapshot['world_random'], random.observation()) and
                P.exact({key: snapshot[key] for key in ('position', 'dimensions', 'world_same')},
                        {key: interrupted[key] for key in ('position', 'dimensions', 'world_same')}) and
                snapshot['sleeping'] is True and snapshot['world_sleeping'] is True and
                snapshot['timer'] == 37 and snapshot['health'] == 20 and snapshot['idle'] == 0 and
                P.exact(snapshot['plant'], [profile['id'], profile['metadata']]) and
                P.exact(snapshot['head'], [26, 8]) and P.exact(snapshot['plant_floor'], [profile['floor'], 0]) and
                snapshot['items'] == [] and snapshot['cells'] == 1 and
                P.exact(actual['planes'], W.planes(profile, True, False)) and
                P.exact([snapshot['math_random'][key] for key in ('state', 'cache', 'gaussian')],
                        W.model({**profile, 'publication_error_at': 1})['math_random']) and
                P.exact(snapshot['player_random'], originals[profile['mode']]['before']['player_random']),
                'native missing-Math completed prefix')
        result = {'matched': None, 'compared_scalars': 0, 'native_only': 'missing Math owner'}
    require(len(events) == 1 + len(snapshot['items']) + (snapshot['cells'] - 1), 'callback count')
    initial_seed = W.RandomModel(profile['world_seed']).observation()[0]
    item_index = 0
    for index, event in enumerate(events):
        added = event['kind'] == 'added'
        removed = index != 0 and not added
        require(P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                P.exact(event['plant'], [0, 0] if removed else [profile['id'], profile['metadata']]),
                'callback observes committed cells while Player sleeps')
        if added:
            item_index += 1
            prefix = W.model({**profile, 'publication_error_at': item_index, 'removal_error': False})
            require(event['world_seed'] == prefix['world_random'][0] and
                    P.exact(event['item'], snapshot['items'][item_index - 1]), 'actual publication prefix')
        else:
            require(P.exact(event['xyz'], [7, 64, 8] if removed else [8, 64, 8]) and
                    event['world_seed'] == (snapshot['world_random'][0] if removed else initial_seed),
                    'cell callback coordinates and Random prefix')
    return result


def digests(paths):
    result = {}
    for path in paths:
        digest = hashlib.sha256()
        with Path(path).open('rb') as stream:
            for block in iter(lambda: stream.read(128 * 1024), b''): digest.update(block)
        result[str(path)] = digest.hexdigest()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    reference = W.read_reference()
    originals = {row['mode']: row for row in reference['rows']}
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='remaining-plants-', dir=args.work_dir.resolve()))
    capture = W.module('remaining_plants_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    inputs = [executable, Path(__file__).resolve(), HERE / 'packet.py', HERE / 'finite.json',
              HERE / 'provenance.json', ROOT / 'tests/reference/client/controller/capture.py',
              ROOT / 'tests/entity/player/test_plant_damage_wake.c',
              ROOT / 'tests/entity/player/test_damage_wake.c',
              ROOT / 'tests/entity/player/test_sleep_wake.c',
              ROOT / 'tests/world/runtime/test_entity_tick.c']
    before = digests(inputs)
    report = {'schema': 'REMAINING_PLANTS_WAKE_NATIVE_V1', 'status': 'FAIL', 'scope': SCOPE,
              'input_before_sha256': before, 'cases': []}
    try:
        for mode in MODES:
            work = output / mode
            work.mkdir()
            receipt = capture.capture([str(executable), mode, str(work).encode('utf-8').hex()],
                                      work, 'native', 60, os.environ.copy())
            record = {'mode': mode, 'native': receipt}
            report['cases'].append(record)
            capture.require_success(receipt, empty_stderr=True)
            actual = parse((work / receipt['stdout_file']).read_text(encoding='utf-8'), mode)
            record['comparison'] = validate(actual, mode, originals)
        report.update(status='PASS_SELECTED_COMPONENTS_ONLY', paired_calls=25, native_only_controls=5,
                      original_scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases'][:25]),
                      no_item_control_scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases'][25:]),
                      scalar_comparisons=sum(case['comparison']['compared_scalars'] for case in report['cases']))
    finally:
        report['input_after_sha256'] = digests(inputs)
        if report['input_after_sha256'] != before: report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(before == report['input_after_sha256'], 'replay inputs unchanged')
    print('Plant wake: 25 original pairs, 5 native controls;', report['scalar_comparisons'],
          'exact selected scalar comparisons;', output / 'report.json')


if __name__ == '__main__':
    main()
