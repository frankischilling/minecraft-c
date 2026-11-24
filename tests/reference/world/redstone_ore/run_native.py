"""Pair real native Redstone Ore lifecycle/ticks with the closed unchanged-original packet."""
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


N = module('ore_native_plane_helpers', ROOT / 'tests/reference/entity/remaining_plants_wake/run_native.py')
MODES = tuple(profile['mode'] for profile in W.profiles())
STATE_KEYS = {'phase', 'target', 'world_random', 'math_random', 'time', 'world_o', 'sky_seen', 'scheduled',
              'lighting', 'ore_statics', 'controller', 'held_stack', 'item_use_stat', 'provider_null',
              'world_access_null', 'entities', 'entity_class', 'entity_count', 'player_count', 'players',
              'weather', 'random_work', 'chunk_planes'}
OUTCOME_KEYS = {'mode', 'input', 'prepared_class', 'raw', 'returned', 'result', 'exception', 'native_policy',
                'site', 'source_sites', 'world_tick', 'continuation'}
EVENT_COMMON = {'kind', 'target', 'world_o', 'world_random', 'math_random', 'entities'}
TNT_KEYS = {'class', 'id', 'position', 'motion', 'previous', 'dimensions', 'offset', 'bounds', 'fuse',
            'prevent_spawning', 'removed', 'on_ground', 'world_same'}
PLAYER_KEYS = {'class', 'id', 'position', 'motion', 'dimensions', 'offset', 'bounds', 'removed', 'on_ground', 'health', 'sleeping'}
SCOPE = ('Selected unchanged-original Redstone Ore activation/fade/display and actual OS mining/OB Item continuation; '
         'full World.l scheduled and Player-centered random dispatch; seeded World/Math Random, all loaded Chunk '
         'planes, lighting/scheduled queues, source callbacks and native cleanup. Original prepared qy/SN class '
         'state and selected Item-use counter are explicit before inputs. The native App references are declared '
         'method inputs; App/statistics constructors, statistics dirty/unsent maps/files, inactive own Entity Random '
         'entropy, natural class initialization, Java stacks, graphics and FPS remain unpaired.')


def require(condition, message):
    P.require(condition, 'Redstone Ore native: ' + message)


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
    require(type(value) is dict and set(value) == {'scheduled', 'entity', 'item_use_stat'}, 'closed explicit prepared class state')
    require(integer(value['item_use_stat']), 'explicit selected prepared Item-use counter')
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
    require((value['world_random'] is None) is profile['null_random'], 'actual nullable World Random input')
    if value['world_random'] is not None: random_state(value['world_random'])
    random_state(value['math_random'])
    scheduled(value['scheduled']); lighting(value['lighting']); class_state(value['entity_class'])
    ore = value['ore_statics']
    require(type(ore) is dict and set(ore) == {'unlit', 'lit'} and
            all(entry is None or integer(entry) for entry in ore.values()), 'actual nullable Ore singleton cells')
    require(all(type(value[key]) is bool for key in ('provider_null', 'world_access_null')) and
            integer(value['item_use_stat']), 'actual source reference flags and Item-use counter')
    controller = value['controller']
    require((controller is not None) is profile['player'], 'actual OS controller inventory')
    if controller is not None:
        require(type(controller) is dict and set(controller) == {'c', 'd', 'e', 'f', 'g', 'h', 'i'} and
                all(integer(controller[key]) for key in ('c', 'd', 'e', 'i')) and
                all(word(controller[key], 8) for key in ('f', 'g', 'h')), 'complete selected OS source fields')
    held = value['held_stack']
    require((held is not None) is (profile['operation'] == 'right-use'), 'actual local held Stack inventory')
    if held is not None:
        require(type(held) is dict and set(held) == {'id', 'count', 'damage'} and
                all(integer(entry) for entry in held.values()), 'complete actual held Stack fields')
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
    if value['world_random'] is not None: random_state(value['world_random'])
    random_state(value['math_random']); entities(value['entities'])
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
    return profile['operation'] in ('left', 'right', 'world-tick', 'mining', 'right-use') and any(
        profile[key] for key in ('cell_error_at', 'range_error_at', 'particle_error_at', 'sound_error_at'))


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
    if len(lines) >= 3 and lines[-3].startswith('ORE_RECEIPT '):
        lifetime = P.loads(lines.pop(-3).removeprefix('ORE_RECEIPT '))
        receipt(lifetime)
    require((lifetime is not None) is receipt_expected(profile), 'exact declared retained receipt control inventory')
    require(len(lines) >= 6 and re.fullmatch(r'PASS: [1-9][0-9]* World Redstone Ore checks;mode=' + re.escape(mode), lines[-1]),
            'exact checked completion footer')
    require(lines[0].startswith('ORE_STATE ') and lines[-4].startswith('ORE_STATE ') and
            lines[-3].startswith('ORE_STATE ') and lines[-2].startswith('ORE_STATE ') and
            lines[-5].startswith('ORE_OUTCOME '), 'ordered before, outcome and stable cleanup records')
    events = []
    for line in lines[1:-5]:
        require(line.startswith('ORE_EVENT '), 'only declared callback records before outcome')
        value = P.loads(line.removeprefix('ORE_EVENT ')); event(value); events.append(value)
    require(len(events) <= 4096, 'bounded complete callback inventory')
    snapshots = [P.loads(lines[index].removeprefix('ORE_STATE ')) for index in (0, -4, -3, -2)]
    for snapshot, phase in zip(snapshots, ('before', 'after', 'retired', 'cleanup')):
        state(snapshot, phase, profile)
    steady = [{key: value for key, value in snapshot.items() if key != 'phase'} for snapshot in snapshots[1:]]
    require(all(P.exact(snapshot, steady[0]) for snapshot in steady[1:]), 'retirement/cleanup preserve every observed effect')
    outcome = P.loads(lines[-5].removeprefix('ORE_OUTCOME '))
    require(type(outcome) is dict and set(outcome) == OUTCOME_KEYS and outcome['mode'] == mode and
            P.exact(outcome['input'], profile) and integer(outcome['raw']) and
            0 <= outcome['raw'] <= (11 if profile['operation'] == 'mining' else 14 if profile['operation'] == 'right-use' else 9) and
            type(outcome['returned']) is bool and type(outcome['native_policy']) is bool and
            (outcome['result'] is None or type(outcome['result']) is bool) and
            (outcome['exception'] is None or type(outcome['exception']) is str), 'closed typed outcome and exact literal inputs')
    prepared_class(outcome['prepared_class'])
    site = outcome['site']
    require(type(site) is dict and set(site) == {'class', 'method', 'pc'} and integer(site['pc']) and site['pc'] >= 0 and
            all(site[key] is None or type(site[key]) is str for key in ('class', 'method')), 'actual diagnostic caller')
    source = outcome['source_sites']
    require(type(source) is dict and set(source) == {'step', 'mutation'}, 'closed actual child selections')
    step = source['step']
    if step is not None:
        require(type(step) is dict and set(step) == {'source', 'registry_status', 'caller_pc', 'activation_pc',
                'particle_pc', 'opaque_pc', 'face', 'query_null', 'query_pc', 'provider_pc'} and
                type(step['query_null']) is bool and all(integer(entry) for key, entry in step.items() if key != 'query_null') and
                0 <= step['source'] <= 7 and step['registry_status'] >= 0 and -1 <= step['face'] <= 5 and
                all(step[key] >= 0 for key in ('caller_pc', 'activation_pc', 'particle_pc', 'opaque_pc', 'query_pc', 'provider_pc')),
                'actual typed entered activation/particles/query witnesses')
    mutation = source['mutation']
    if mutation is not None:
        require(type(mutation) is dict and set(mutation) == {'ore_tick_pc', 'source_null', 'query_null', 'query_pc',
                'provider_pc', 'world_access_pc', 'access_fault'} and
                all(type(mutation[key]) is bool for key in ('source_null', 'query_null')) and
                all(integer(entry) and entry >= 0 for key, entry in mutation.items() if key not in ('source_null', 'query_null')) and
                mutation['access_fault'] <= 2, 'actual selected mutation/null/iterator witnesses')
    continuation = outcome['continuation']
    operation = profile['operation']
    require((continuation is not None) is (operation in ('mining', 'right-use')), 'exact real caller continuation inventory')
    if operation == 'mining':
        require(type(continuation) is dict and set(continuation) == {'kind', 'pc', 'block_id', 'metadata',
                'initial_hook_returned', 'strength', 'progress_stored', 'removal_returned'} and continuation['kind'] == operation and
                all(integer(continuation[key]) for key in ('pc', 'block_id', 'metadata')) and word(continuation['strength'], 8) and
                all(type(continuation[key]) is bool for key in ('initial_hook_returned', 'progress_stored', 'removal_returned')),
                'actual OS captured Block and reached initial hook/strength continuation')
    elif operation == 'right-use':
        require(type(continuation) is dict and set(continuation) == {'kind', 'pc', 'clicked_id', 'activation_entered',
                'activation_returned', 'activated', 'item_entered', 'item_returned', 'count_before', 'count_after'} and
                continuation['kind'] == operation and all(integer(continuation[key]) for key in ('pc', 'clicked_id', 'count_before', 'count_after')) and
                all(type(continuation[key]) is bool for key in ('activation_entered', 'activation_returned', 'activated', 'item_entered', 'item_returned')),
                'actual OB false-activation and Item continuation')
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
    require(P.exact(outcome['prepared_class'], original['prepared_class']), 'exact original explicit prepared class/counter inputs')
    require(P.exact(selected_state(actual['before']), selected_state(original['before'])), 'selected complete prepared original state/planes')
    require(P.exact(selected_state(actual['after']), selected_state(original['after'])), 'selected complete original after state/planes')
    expected_events = [{key: copy.deepcopy(value) for key, value in entry.items() if key not in ('stack', 'source_stack')}
                       for entry in original['events']]
    require(P.exact(actual['events'], expected_events), 'complete callback order, values and exact Random prefix')
    require(outcome['returned'] is (expected_exception is None) and outcome['native_policy'] is False and
            outcome['exception'] == expected_exception and P.exact(outcome['result'], original['result']),
            'actual original return/exception without a native frontier')
    if expected_exception is not None:
        frames = {(entry['class'], entry['method']) for entry in original['exception_stack']
                  if entry['class'] != 'RedstoneOreObservation' and not entry['class'].startswith('RedstoneOreObservation$')}
        site = outcome['site']
        require(site['class'] is not None and site['method'] is not None and
                (site['class'], site['method'].split('(')[0]) in frames, 'actual reported caller occurs in unchanged-original failure stack')
    if expected_exception == 'java/lang/NullPointerException':
        source = outcome['source_sites']
        step, mutation = source['step'], source['mutation']
        origin = W.model(profile, original['prepared_class'])['exception_origin']
        if origin['class'] == 'bs' and origin['method'] == 'i':
            require(step is not None and step['source'] == (1 if origin['pc'] == 1 else 2) and
                    step['particle_pc'] == origin['pc'], 'actual World/captured Random null precedes particles and activation')
        elif origin['class'] == 'bs' and origin['method'] == 'h':
            require(step is not None and step['source'] == 6 and step['activation_pc'] == origin['pc'],
                    'actual reached nullable aO/aP bn receiver after all particle effects')
        elif origin['class'] == 'bs' and origin['method'] == 'a':
            require(mutation is not None and mutation['source_null'] is True and mutation['ore_tick_pc'] == origin['pc'],
                    'actual tick singleton/World dereference, with nullable unused tick Random')
        elif origin['class'] == 'fd' and origin['method'] == 'c':
            require(origin['pc'] == 6 and step is not None and step['source'] == 4 and
                    step['query_null'] is True and step['provider_pc'] == 6, 'actual provider getter fails before later singleton access')
        else:
            require(origin['class'] == 'fd' and origin['method'] in ('a', 'b', 'j') and origin['pc'] == 9 and
                    mutation is not None and mutation['access_fault'] == 1 and mutation['world_access_pc'] == 9,
                    'actual null World.u size receiver in the selected particle/range/cell loop')
            if origin['method'] == 'a':
                require(step is not None and step['source'] == 5 and step['particle_pc'] == 291,
                        'actual particle callback list read after exactly three World Random draws')
            else:
                require(step is None and mutation['ore_tick_pc'] == 24 and mutation['source_null'] is False,
                        'actual reached tick setter retains range/cell iterator failure after its exact committed prefix')
    continuation = outcome['continuation']
    if profile['operation'] == 'mining':
        require(continuation['block_id'] == original['before']['target'][0] and
                continuation['initial_hook_returned'] is outcome['returned'] and
                continuation['progress_stored'] is False and continuation['removal_returned'] is False and
                continuation['pc'] == (113 if outcome['returned'] else 74),
                'OS keeps the captured old Block and reaches strength only after its actual initial hook')
    elif profile['operation'] == 'right-use':
        require(continuation['clicked_id'] == original['before']['target'][0] and continuation['activation_entered'] is True and
                continuation['activated'] is False, 'OB preserves the actual false Ore activation')
        activation_failed = expected_exception is not None and not profile['sound_error_at']
        require(continuation['activation_returned'] is (not activation_failed) and
                continuation['item_entered'] is (not activation_failed) and
                continuation['item_returned'] is outcome['returned'], 'actual Item continuation only after returned false activation')
        if outcome['returned']:
            require(P.exact([continuation['count_before'], continuation['count_after']], [1, 0]) and
                    actual['after']['item_use_stat'] == actual['before']['item_use_stat'] + 1,
                    'genuine placed Cobble consumes one Stack and increments actual use statistic')
    if outcome['world_tick'] is not None:
        work = outcome['world_tick']
        if outcome['returned']:
            chunks = 361 if profile['random_player'] else 0
            require(P.exact([work['pc'], work['scheduled_returned'], work['random_returned'], work['chunks'], work['samples']],
                            [198, True, True, chunks, chunks * 80]) and
                    work['block_ticks'] == (W.model(profile, original['prepared_class'])['block_ticks'] if chunks else 0),
                    'complete natural World.l scheduled/random work and selected Ore tick')
        else:
            require(profile['prequeue_delay'] >= 0 and
                    P.exact([work[key] for key in ('pc', 'scheduled_returned', 'random_returned', 'chunks', 'samples', 'block_ticks')],
                            [190, False, False, 0, 0, 0]), 'scheduled source failure stops before random work')
    count = scalar_count(selected_state(actual['before'])) + scalar_count(selected_state(actual['after'])) + scalar_count(expected_events) + 3
    return {'matched': True, 'compared_scalars': count, 'receipt_lifetime_checked': actual['receipt'] is not None}


def records(values):
    return ';'.join(','.join(str(value) for value in entry) for entry in values) or '-'


def native_arguments(executable, profile, work, prepared):
    prepared_class(prepared)
    return [str(executable), profile['mode'], str(work).encode('utf-8').hex(), profile['operation'],
            *(str(value) for value in profile['xyz']), str(profile['entry_id']), str(profile['random_seed']),
            *(str(profile[key]) for key in ('cell_error_at', 'range_error_at', 'particle_error_at', 'sound_error_at', 'prequeue_delay')),
            records(profile['cells']), records(profile['heights']), prepared['scheduled']['sequence'], str(int(prepared['scheduled']['initialized'])),
            str(int(profile['random_player'])), str(int(profile['player'])), str(profile['g']), str(profile['cooldown']),
            *(str(int(profile[key])) for key in ('null_random', 'null_tick_random', 'null_provider', 'null_unlit', 'null_lit', 'null_world', 'null_access')),
            str(int(prepared['entity']['initialized'])), str(prepared['entity']['next_id']), str(prepared['item_use_stat']), 'none']


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
    output = Path(tempfile.mkdtemp(prefix='redstone-ore-', dir=args.work_dir.resolve()))
    capture = module('ore_native_capture', ROOT / 'tests/reference/client/controller/capture.py')
    inputs = list(dict.fromkeys([executable, Path(__file__).resolve(),
              *(HERE / name for name in W.SOURCES), *(Path(path) for path in W.SHARED.values()),
              HERE / 'finite.json', HERE / 'provenance.json',
              ROOT / 'tests/world/runtime/test_redstone_ore.c', ROOT / 'tests/world/runtime/test_fire.c',
              ROOT / 'tests/world/runtime/test_fluids.c',
              ROOT / 'tests/reference/entity/remaining_plants_wake/run_native.py',
              ROOT / 'tests/reference/client/controller/capture.py']))
    before = digests(inputs)
    report = {'schema': 'WORLD_REDSTONE_ORE_NATIVE_V1', 'status': 'FAIL', 'scope': SCOPE, 'input_before_sha256': before, 'cases': []}
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
    print('Redstone Ore:', len(MODES), 'original pairs;', report['retained_receipt_controls'], 'retained receipt controls;',
          report['original_scalar_comparisons'], 'selected original scalars;', output / 'report.json')


if __name__ == '__main__':
    main()
