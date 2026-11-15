"""Pair real native Fire lifecycle/ticks with the closed unchanged-original packet."""
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
import packet as W

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
P = W.P


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


N = module('fire_native_plane_helpers', ROOT / 'tests/reference/entity/remaining_plants_wake/run_native.py')
MODES = tuple(profile['mode'] for profile in W.profiles())
STATE_KEYS = {'phase', 'target', 'world_random', 'math_random', 'time', 'world_o', 'sky_seen', 'scheduled',
              'lighting', 'fire_arrays', 'entities', 'entity_class', 'entity_count', 'player_count', 'players',
              'weather', 'random_work', 'chunk_planes'}
OUTCOME_KEYS = {'mode', 'input', 'prepared_class', 'raw', 'returned', 'result', 'exception', 'native_policy',
                'hook', 'cause_hook', 'source_sites', 'world_tick'}
EVENT_COMMON = {'kind', 'target', 'world_o', 'world_random', 'math_random', 'entities'}
TNT_KEYS = {'class', 'id', 'position', 'motion', 'previous', 'dimensions', 'offset', 'bounds', 'fuse',
            'prevent_spawning', 'removed', 'on_ground', 'world_same'}
PLAYER_KEYS = {'class', 'id', 'position', 'motion', 'dimensions', 'offset', 'bounds', 'removed', 'on_ground', 'health', 'sleeping'}
SCOPE = ('Selected original Fire added/neighbor/tick, StillLava ignition, Portal creation and TNT priming effects; '
         'actual World.l scheduled and Player-centered random dispatch; seeded World/Math Random, complete loaded '
         'Chunk planes, ordered lighting/scheduled queues, callbacks and native cleanup. Retained World callback '
         'receipts receive separate native lifetime checks; direct ordinary diagnostics borrow their producer graph. The exact original prepared '
         'qy/SN class state is an explicit input. Natural class initialization, unused constructor Random entropy, '
         'Java stack frames, graphics and FPS remain unpaired.')


def require(condition, message):
    P.require(condition, 'Fire native: ' + message)


def integer(value):
    return type(value) is int


def vector(value, length, predicate=integer):
    return type(value) is list and len(value) == length and all(predicate(entry) for entry in value)


def word(value, width):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(width) + '}', value) is not None


def profile_for(mode):
    require(type(mode) is str and mode in MODES, 'declared mode')
    return next(profile for profile in W.profiles() if profile['mode'] == mode)


def random_state(value):
    require(vector(value, 3, lambda _: True) and word(value[0], 16) and int(value[0], 16) < 1 << 48 and
            type(value[1]) is bool and word(value[2], 16), 'complete typed Random state')


def class_state(value):
    require(type(value) is dict and set(value) == {'initialized', 'next_id'} and
            type(value['initialized']) is bool and integer(value['next_id']), 'actual Entity class state')


def prepared_class(value):
    require(type(value) is dict and set(value) == {'scheduled', 'entity'}, 'closed explicit prepared class state')
    class_state(value['entity'])
    scheduled = value['scheduled']
    require(type(scheduled) is dict and set(scheduled) == {'initialized', 'sequence'} and
            type(scheduled['initialized']) is bool and word(scheduled['sequence'], 16), 'prepared qy class state')


def entity(value, player=False):
    require(type(value) is dict and set(value) == (PLAYER_KEYS if player else TNT_KEYS) and
            value['class'] == ('dc' if player else 'qw') and integer(value['id']), 'closed actual Entity fields')
    for key in ('position', 'motion') + (() if player else ('previous',)):
        require(vector(value[key], 3, lambda entry: word(entry, 16)), 'actual Entity double vector ' + key)
    require(vector(value['dimensions'], 2, lambda entry: word(entry, 8)) and word(value['offset'], 8) and
            vector(value['bounds'], 6, lambda entry: word(entry, 16)), 'actual Entity dimensions/bounds')
    for key in ('removed', 'on_ground') + (('sleeping',) if player else ('prevent_spawning', 'world_same')):
        require(type(value[key]) is bool, 'actual Entity boolean ' + key)
    require(integer(value['health'] if player else value['fuse']), 'actual concrete Entity scalar')


def entities(values):
    require(type(values) is list and len(values) <= 16, 'bounded actual TNT list')
    for value in values:
        entity(value)
    require(len({value['id'] for value in values}) == len(values), 'distinct published TNT identities')


def scheduled(value):
    require(type(value) is dict and set(value) == {'initialized', 'sequence', 'ordered', 'members'} and
            type(value['initialized']) is bool and word(value['sequence'], 16), 'complete actual scheduled state')
    for key in ('ordered', 'members'):
        require(type(value[key]) is list and len(value[key]) <= 4096, 'bounded actual queue')
        for entry in value[key]:
            require(type(entry) is dict and set(entry) == {'xyz', 'block', 'time', 'ordinal'} and
                    vector(entry['xyz'], 3) and integer(entry['block']) and word(entry['time'], 16) and
                    word(entry['ordinal'], 16), 'actual scheduled record')


def lighting(value):
    require(type(value) is dict and set(value) == {'depth', 'regions'} and integer(value['depth']) and
            type(value['regions']) is list and len(value['regions']) <= 4096, 'complete actual lighting queue')
    for region in value['regions']:
        require(type(region) is dict and set(region) == {'kind', 'bounds'} and region['kind'] in ('Sky', 'Block') and
                vector(region['bounds'], 6), 'ordered actual lighting region')


def state(value, phase, profile):
    require(type(value) is dict and set(value) == STATE_KEYS and value['phase'] == phase and
            vector(value['target'], 2) and 0 <= value['target'][0] <= 255 and 0 <= value['target'][1] <= 15 and
            type(value['world_o']) is bool and type(value['sky_seen']) is bool and word(value['time'], 16),
            'closed actual ' + phase + ' state')
    random_state(value['world_random']); random_state(value['math_random'])
    scheduled(value['scheduled']); lighting(value['lighting']); class_state(value['entity_class'])
    arrays = value['fire_arrays']
    require(type(arrays) is dict and set(arrays) == {'encouragement', 'flammability'}, 'actual Fire tables')
    for field in arrays.values():
        require(field is None or vector(field, 256), 'complete actual nullable Fire table')
    entities(value['entities'])
    require(type(value['players']) is list and len(value['players']) <= 1 and
            integer(value['entity_count']) and integer(value['player_count']) and
            value['player_count'] == len(value['players']) and
            value['entity_count'] == len(value['entities']) + len(value['players']), 'complete actual World membership')
    for player in value['players']:
        entity(player, True)
    weather = value['weather']
    require(type(weather) is dict and set(weather) == {'rain_previous', 'rain_current', 'thunder_previous',
            'thunder_current', 'rain_time', 'thunder_time', 'raining', 'thundering'} and
            all(word(weather[key], 8) for key in ('rain_previous', 'rain_current', 'thunder_previous', 'thunder_current')) and
            all(integer(weather[key]) for key in ('rain_time', 'thunder_time')) and
            all(type(weather[key]) is bool for key in ('raining', 'thundering')), 'complete actual weather')
    work = value['random_work']
    require(type(work) is dict and set(work) == {'g', 'cooldown', 'candidates'} and integer(work['g']) and
            integer(work['cooldown']) and type(work['candidates']) is list and len(work['candidates']) <= 361 and
            all(vector(candidate, 2) for candidate in work['candidates']) and
            len({tuple(candidate) for candidate in work['candidates']}) == len(work['candidates']),
            'actual LCG/cooldown/candidate traversal')
    chunks = value['chunk_planes']
    footprint = [[x, z] for x in range(-10, 9) for z in range(-7, 12)] if profile['random_player'] else \
                [[x, z] for x in range(2) for z in range(2)]
    require(type(chunks) is list and len(chunks) == len(footprint), 'complete actual loaded Chunk footprint')
    for chunk, xyz in zip(chunks, footprint):
        require(type(chunk) is dict and set(chunk) == {'xyz', 'planes'} and P.exact(chunk['xyz'], xyz) and
                type(chunk['planes']) is dict and set(chunk['planes']) == set(N.PLANES), 'ordered complete Chunk planes')
        chunk['planes'] = {name: N.plane(chunk['planes'][name], length) for name, length in N.PLANES.items()}


def event(value):
    require(type(value) is dict and value.get('kind') in ('cell', 'range', 'sound', 'particle', 'added', 'sky'), 'callback kind')
    kind = value['kind']
    extra = {'xyz', 'cell'} if kind == 'cell' else {'xyz'} if kind == 'range' else \
            {'name', 'position', 'volume', 'pitch'} if kind == 'sound' else \
            {'name', 'position', 'motion'} if kind == 'particle' else {'entity'} if kind == 'added' else set()
    require(set(value) == EVENT_COMMON | extra and vector(value['target'], 2) and type(value['world_o']) is bool,
            'closed complete callback')
    random_state(value['world_random']); random_state(value['math_random']); entities(value['entities'])
    if kind in ('cell', 'range'):
        require(vector(value['xyz'], 3 if kind == 'cell' else 6), 'actual callback coordinates')
        if kind == 'cell': require(vector(value['cell'], 2), 'actual notified Cell value')
    elif kind in ('sound', 'particle'):
        require(type(value['name']) is str and 0 < len(value['name']) < 64 and
                vector(value['position'], 3, lambda entry: word(entry, 16)), 'actual presentation arguments')
        if kind == 'sound':
            require(word(value['volume'], 8) and word(value['pitch'], 8), 'actual sound arguments')
        else:
            require(vector(value['motion'], 3, lambda entry: word(entry, 16)), 'actual particle motion')
    elif kind == 'added':
        entity(value['entity'])
        require(any(P.exact(value['entity'], candidate) for candidate in value['entities']), 'callback follows genuine publication')


def receipt_expected(profile):
    return profile['operation'] == 'world-tick' and any(profile[key] for key in
           ('cell_error_at', 'range_error_at', 'added_error_at', 'sound_error_at'))


def receipt(value):
    keys = {'busy_before', 'before_holds', 'after_busy_holds', 'after_call_holds', 'retire',
            'same_identity', 'selected_source', 'exception', 'attempts'}
    require(type(value) is dict and set(value) == keys and
            all(integer(value[key]) for key in keys - {'same_identity', 'selected_source', 'exception'}) and
            P.exact([value[key] for key in ('busy_before', 'before_holds', 'after_busy_holds', 'after_call_holds', 'retire', 'attempts')],
                    [9, 2, 2, 1, 0, 2]) and value['same_identity'] is True and value['selected_source'] is True and
            type(value['exception']) is str, 'actual retained producer identity, BUSY retirement and one checked final consumption')


def parse(stdout, mode):
    profile = profile_for(mode)
    require(type(stdout) is str and stdout.endswith('\n'), 'complete decoded stdout')
    lines = stdout.splitlines()
    for startup in ('16 achievements', '151 recipes'):
        if lines and lines[0] == startup: lines.pop(0)
    lifetime = None
    if len(lines) >= 3 and lines[-3].startswith('FIRE_RECEIPT '):
        lifetime = P.loads(lines.pop(-3).removeprefix('FIRE_RECEIPT '))
        receipt(lifetime)
    require((lifetime is not None) is receipt_expected(profile), 'exact declared retained receipt control inventory')
    require(len(lines) >= 6 and re.fullmatch(r'PASS: [1-9][0-9]* World Fire checks;mode=' + re.escape(mode), lines[-1]),
            'exact checked completion footer')
    require(lines[0].startswith('FIRE_STATE ') and lines[-4].startswith('FIRE_STATE ') and
            lines[-3].startswith('FIRE_STATE ') and lines[-2].startswith('FIRE_STATE ') and
            lines[-5].startswith('FIRE_OUTCOME '), 'ordered before, outcome and stable cleanup records')
    events = []
    for line in lines[1:-5]:
        require(line.startswith('FIRE_EVENT '), 'only declared callback records before outcome')
        value = P.loads(line.removeprefix('FIRE_EVENT ')); event(value); events.append(value)
    require(len(events) <= 4096, 'bounded complete callback inventory')
    snapshots = [P.loads(lines[index].removeprefix('FIRE_STATE ')) for index in (0, -4, -3, -2)]
    for snapshot, phase in zip(snapshots, ('before', 'after', 'retired', 'cleanup')):
        state(snapshot, phase, profile)
    steady = [{key: value for key, value in snapshot.items() if key != 'phase'} for snapshot in snapshots[1:]]
    require(all(P.exact(snapshot, steady[0]) for snapshot in steady[1:]), 'retirement/cleanup preserve every observed effect')
    outcome = P.loads(lines[-5].removeprefix('FIRE_OUTCOME '))
    require(type(outcome) is dict and set(outcome) == OUTCOME_KEYS and outcome['mode'] == mode and
            P.exact(outcome['input'], profile) and integer(outcome['raw']) and
            type(outcome['returned']) is bool and type(outcome['native_policy']) is bool and
            (outcome['result'] is None or type(outcome['result']) is bool) and
            (outcome['exception'] is None or type(outcome['exception']) is str), 'closed typed outcome and exact literal inputs')
    prepared_class(outcome['prepared_class'])
    for key in ('hook', 'cause_hook'):
        site = outcome[key]
        require(type(site) is dict and set(site) == {'class', 'method', 'pc'} and integer(site['pc']) and site['pc'] >= 0 and
                all(site[field] is None or type(site[field]) is str for field in ('class', 'method')), 'actual diagnostic site')
    source = outcome['source_sites']
    require(type(source) is dict and set(source) == {'null', 'array_index', 'query_null', 'query_pc', 'provider_pc', 'fire'} and
            all(type(source[key]) is bool for key in ('null', 'array_index', 'query_null')) and
            all(integer(source[key]) and source[key] >= 0 for key in ('query_pc', 'provider_pc')) and
            type(source['fire']) is dict and set(source['fire']) == {'added', 'neighbor', 'tick', 'can_place', 'neighbors',
                'flammable', 'encouragement', 'spread', 'burn'} and
            all(integer(value) and value >= 0 for value in source['fire'].values()), 'actual typed Source origin and helper sites')
    work = outcome['world_tick']
    require((profile['operation'] == 'world-tick') is (work is not None), 'real World tick retained result')
    if work is not None:
        require(type(work) is dict and set(work) == {'pc', 'scheduled_returned', 'random_returned', 'chunks', 'samples', 'block_ticks'} and
                all(integer(work[key]) and work[key] >= 0 for key in ('pc', 'chunks', 'samples', 'block_ticks')) and
                all(type(work[key]) is bool for key in ('scheduled_returned', 'random_returned')), 'actual World tick continuation')
    require((outcome['raw'] == 0) is outcome['returned'] and
            (not outcome['returned'] or outcome['exception'] is None) and
            outcome['native_policy'] is (not outcome['returned'] and outcome['exception'] is None),
            'ordinary return, genuine Source failure or explicit native frontier')
    if lifetime is not None:
        require(outcome['returned'] is False and lifetime['exception'] == outcome['exception'],
                'owned receipt preserves the actual World tick Source failure category')
    return {'before': snapshots[0], 'after': snapshots[1], 'events': events, 'outcome': outcome, 'receipt': lifetime}


def exception_class(original):
    value = original.get('exception')
    return None if value in (None, 'NONE') else value if type(value) is str else value['class']


def selected_state(source):
    return {key: copy.deepcopy(source[key]) for key in STATE_KEYS - {'phase'}}


def scalar_count(value):
    if type(value) is dict: return sum(scalar_count(entry) for entry in value.values())
    if type(value) is list: return sum(scalar_count(entry) for entry in value)
    return 1


def validate(actual, mode, originals):
    original = originals[mode]
    outcome = actual['outcome']
    profile = profile_for(mode)
    expected_exception = exception_class(original)
    require(P.exact(outcome['prepared_class'], original['prepared_class']), 'exact original pre-preparation class inputs')
    require(P.exact(selected_state(actual['before']), selected_state(original['before'])), 'selected complete prepared original state/planes')
    require(P.exact(selected_state(actual['after']), selected_state(original['after'])), 'selected complete original after state/planes')
    expected_events = [{key: copy.deepcopy(value) for key, value in entry.items() if key not in ('stack', 'source_stack')}
                       for entry in original['events']]
    require(P.exact(actual['events'], expected_events), 'complete actual callback order, values, RNG and publication prefix')
    require(outcome['returned'] is (expected_exception is None) and outcome['native_policy'] is False and
            outcome['exception'] == expected_exception and P.exact(outcome['result'], original['result']),
            'actual original return/exception and no native frontier')
    if expected_exception is not None:
        source_frames = {(entry['class'], entry['method']) for entry in original['exception_stack']
                         if entry['class'] != 'FireObservation' and not entry['class'].startswith('FireObservation$')}
        require(outcome['hook']['class'] is not None and outcome['hook']['method'] is not None,
                'failed concrete source caller remains present')
        for key in ('hook', 'cause_hook'):
            site = outcome[key]
            if site['class'] is not None:
                require(site['method'] is not None and (site['class'], site['method'].split('(')[0]) in source_frames,
                        'reported concrete caller occurs in actual original exception frames')
    if expected_exception == 'java/lang/NullPointerException':
        source = outcome['source_sites']
        origin = W.model(profile, original['prepared_class'])['exception_origin']
        require(source['null'] is True and source['array_index'] is False, 'actual reached Source null witness')
        if origin['class'] == 'yq':
            helper = 'flammable' if origin['method'] == 'c' else 'burn'
            require(source['fire'][helper] == origin['pc'] and source['query_null'] is False,
                    'actual Fire array-null instruction after the completed source getter')
        else:
            require(P.exact([origin['class'], origin['method'], origin['descriptor']], ['fd', 'c', '(II)Llm;']) and
                    source['query_null'] is True and source['provider_pc'] == origin['pc'] and
                    source['fire']['flammable'] == 0 and source['fire']['neighbors'] == 0,
                    'actual provider getter null precedes the unrelated null Fire table')
    if outcome['world_tick'] is not None and outcome['returned']:
        work = outcome['world_tick']
        chunks = 361 if profile['random_player'] else 0
        require(P.exact([work['pc'], work['scheduled_returned'], work['random_returned'], work['chunks'], work['samples']],
                        [198, True, True, chunks, chunks * 80]) and
                work['block_ticks'] == (W.model(profile, original['prepared_class'])['block_ticks'] if chunks else 0),
                'natural World.l return, actual scheduled drain and complete random sample loops')
    elif outcome['world_tick'] is not None:
        work = outcome['world_tick']
        require(profile['prequeue_delay'] >= 0 and
                P.exact([work[key] for key in ('pc', 'scheduled_returned', 'random_returned', 'chunks', 'samples', 'block_ticks')],
                        [190, False, False, 0, 0, 0]), 'scheduled Source failure stops before the random-work caller')
    count = scalar_count(selected_state(actual['before'])) + scalar_count(selected_state(actual['after'])) + scalar_count(expected_events) + 3
    return {'matched': True, 'compared_scalars': count, 'receipt_lifetime_checked': actual['receipt'] is not None}


def records(values):
    return ';'.join(','.join(str(value) for value in entry) for entry in values) or '-'


def native_arguments(executable, profile, work, prepared):
    prepared_class(prepared)
    return [str(executable), profile['mode'], str(work).encode('utf-8').hex(), profile['operation'],
            *(str(value) for value in profile['xyz']), str(profile['entry_id']), str(profile['source_id']),
            str(profile['random_seed']), str(int(profile['rain'])), profile['biome'],
            *(str(profile[key]) for key in ('cell_error_at', 'range_error_at', 'added_error_at', 'sound_error_at', 'prequeue_delay')),
            records(profile['cells']), records(profile['heights']), prepared['scheduled']['sequence'], str(int(prepared['scheduled']['initialized'])),
            *(str(int(profile[key])) for key in ('null_encouragement', 'null_flammability', 'null_provider', 'random_player')),
            str(profile['g']), str(profile['cooldown']), str(int(prepared['entity']['initialized'])), str(prepared['entity']['next_id']), 'none']


def digests(paths):
    return {str(path.resolve()): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    originals = {row['mode']: row for row in W.read_reference()['rows']}
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='fire-', dir=args.work_dir.resolve()))
    capture = module('fire_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    inputs = [executable, Path(__file__).resolve(), HERE / 'packet.py', HERE / 'finite.json', HERE / 'profiles.json', HERE / 'provenance.json',
              ROOT / 'tests/world/runtime/test_fire.c', ROOT / 'tests/world/runtime/test_fluids.c',
              ROOT / 'tests/reference/entity/remaining_plants_wake/run_native.py', ROOT / 'tests/reference/client/controller/capture.py']
    before = digests(inputs)
    report = {'schema': 'WORLD_FIRE_NATIVE_V1', 'status': 'FAIL', 'scope': SCOPE, 'input_before_sha256': before, 'cases': []}
    try:
        for profile in W.profiles():
            mode = profile['mode']
            work = output / mode
            work.mkdir()
            receipt = capture.capture(native_arguments(executable, profile, work, originals[mode]['prepared_class']),
                                      work, 'native', 120 if profile['random_player'] else 60, os.environ.copy())
            row = {'mode': mode, 'native': receipt}
            report['cases'].append(row)
            capture.require_success(receipt, empty_stderr=True)
            row['comparison'] = validate(parse((work / receipt['stdout_file']).read_text(encoding='utf-8'), mode), mode, originals)
        report.update(status='PASS_SELECTED_COMPONENTS_ONLY', original_pairs=len(MODES), native_controls=0,
                      retained_receipt_controls=sum(row['comparison']['receipt_lifetime_checked'] for row in report['cases']),
                      original_scalar_comparisons=sum(row['comparison']['compared_scalars'] for row in report['cases']))
    finally:
        report['input_after_sha256'] = digests(inputs)
        if before != report['input_after_sha256']: report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(before == report['input_after_sha256'], 'native replay inputs unchanged')
    print('Fire:', len(MODES), 'original pairs;', report['retained_receipt_controls'], 'retained receipt controls;',
          report['original_scalar_comparisons'], 'selected original scalars;', output / 'report.json')


if __name__ == '__main__':
    main()
