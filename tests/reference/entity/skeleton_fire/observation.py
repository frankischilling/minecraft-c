"""Current processing of retained FR observations; original suppliers stay frozen."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import struct

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
_spec = importlib.util.spec_from_file_location('skeleton_fire_constructor_observation', ROOT / 'tests/reference/entity/arrow_construction/observation.py')
C = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(C)
need, exact, loads, word = C.need, C.exact, C.loads, C.word
PREFIX = 'SKELETON_FIRE '
ERRORS = {3: 'java/lang/NullPointerException', 4: 'java/lang/NullPointerException', 13: 'java/lang/NullPointerException',
          14: 'java/lang/NullPointerException', 15: 'java/lang/IllegalStateException', 16: 'java/lang/NullPointerException',
          17: 'java/lang/IllegalStateException', 19: 'java/lang/NullPointerException', 22: 'java/lang/NullPointerException'}
VISIBLE = {7, 8, 9, 10, 11, 17, 18, 19, 20, 21, 23}
SECOND_RETURNS = {7, 8, 9, 10, 11, 12, 18, 20, 21, 23}


def profiles(directory=HERE):
    rows = []
    for line in (Path(directory) / 'profiles.tsv').read_text(encoding='ascii').splitlines():
        words = line.split(); need(len(words) == 10, 'complete literal profile columns')
        word(words[1], 8); rows.append([int(v) if i != 1 else v for i, v in enumerate(words)])
    need(len(rows) == 24 and [r[0] for r in rows] == list(range(24)) and
         all(-1 <= r[2] <= 1 and 0 <= r[3] <= 5 and 0 <= r[4] <= 2 and r[5] in (0, 1) and
             r[6] in (0, 1) and 0 <= r[7] <= 8 and r[8] in (0, 1) and r[9] in (0, 271828) for r in rows), 'bounded literal FR profiles')
    need([r[0] for r in rows if r[7] == 8] == [23] and rows[23][9] == 271828, 'sole literal callback reseed')
    return rows


def snapshot(value):
    if value is None:
        return
    clone = copy.deepcopy(value)
    if clone.get('arrow') is not None:
        need(clone['arrow']['c'] in (None, 'skeleton', 'wolf'), 'actual declared nullable Living source identity')
        if clone['arrow']['c'] is not None:
            clone['arrow']['c'] = 'shooter'
    C.snapshot(clone)


def actor(value):
    need(type(value) is dict and set(value) == {'entity', 'cooldown', 'attacking'} and
         type(value['cooldown']) is int and type(value['attacking']) is bool, 'closed actual Skeleton state')
    snapshot(value['entity']); need(value['entity']['class'] == 'fr', 'genuine canonical Skeleton')


def world(value):
    if value is None:
        return
    need(type(value) is dict and set(value) == {'identity', 'remote', 'entities', 'players', 'accesses', 'random', 'chunk'} and
         value['identity'] in ('world-a', 'world-b') and type(value['remote']) is bool and
         type(value['entities']) is type(value['players']) is type(value['accesses']) is list and
         all(x == 'arrow' for x in value['entities']) and value['players'] == [] and
         all(x in (None, 'probe') for x in value['accesses']), 'actual source World lists and callback receiver')
    C.stream(value['random']); c = value['chunk']
    if c is not None:
        need(type(c) is dict and set(c) == {'x', 'z', 'flags', 'slices'} and type(c['x']) is type(c['z']) is int and
             type(c['flags']) is list and len(c['flags']) == 4 and all(type(v) is bool for v in c['flags']) and
             type(c['slices']) is list and len(c['slices']) == 8 and all(type(s) is list and all(v == 'arrow' for v in s) for s in c['slices']),
             'actual loaded normal Chunk flags and ordered slice occurrences')


def context(value):
    need(type(value) is dict and set(value) == {'counter', 'actor', 'target', 'wolf', 'world_a', 'world_b', 'math_random'} and
         type(value['counter']) is int, 'closed actual firing context')
    actor(value['actor']); snapshot(value['target']); snapshot(value['wolf'])
    world(value['world_a']); world(value['world_b']); C.stream(value['math_random'])


def parse_rows(text, directory=HERE):
    need(type(text) is str and len(text.encode('utf-8')) <= 16 * 1024 * 1024, 'bounded complete original stream')
    rows = [loads(line[len(PREFIX):]) for line in text.splitlines() if line.startswith(PREFIX)]
    need(len(rows) == 24, 'all24 whole-parent source rows')
    for row, profile in zip(rows, profiles(directory)):
        need(type(row) is dict and set(row) == {'schema', 'case', 'input', 'returned', 'exception', 'before', 'after', 'arrow_after',
             'callbacks', 'exception_message', 'exception_frames', 'source_raw'} and row['schema'] == 'SKELETON_FIRE_RAW_V1' and
             type(row['case']) is int and row['case'] == profile[0] and exact(row['input'], profile), 'exact source profile and closed raw row')
        n = row['case']
        need(type(row['returned']) is bool and row['returned'] == (n not in ERRORS) and row['exception'] == ERRORS.get(n, 'NONE'), 'whole FR return or actual reached exception')
        context(row['before']); context(row['after']); snapshot(row['arrow_after'])
        need((row['arrow_after'] is not None) == (n in VISIBLE), 'ordinary receiver visibility comes only from genuine entity_added')
        need(type(row['callbacks']) is list and len(row['callbacks']) <= 4 and type(row['source_raw']) is dict and
             (row['exception_message'] is None or type(row['exception_message']) is str) and type(row['exception_frames']) is list and
             all(type(v) is str for v in row['exception_frames']), 'retained callbacks and complete exception evidence')
        before, after = row['before'], row['after']
        need(after['counter'] - before['counter'] == int(n >= 7), 'only actual observed NEW Arrow increments Entity ID')
        need(exact(before['target'], after['target']) and exact(before['wolf'], after['wolf']) and
             exact(before['math_random'], after['math_random']), 'unchanged target/Wolf/shared Math state')
        for w in ('world_a', 'world_b'):
            if before[w] is not None:
                need(exact(before[w]['random'], after[w]['random']), 'firing does not consume World Random')
        if n <= 4:
            need(exact(before, after), 'range/null-target prefix changes no source context')
        need(after['actor']['cooldown'] == (30 if n in SECOND_RETURNS else profile[2]), 'actual cooldown store follows successful second heading')
        need(after['actor']['attacking'] == (row['returned'] and n >= 5), 'actual Creature attacking store follows the facing tail')
    return rows


def site(value):
    need(type(value) is dict and set(value) == {'class', 'method', 'descriptor', 'pc'} and
         all(type(value[k]) is str for k in ('class', 'method', 'descriptor')) and type(value['pc']) is int, 'actual typed source site')


def source_call(trace, row):
    need(type(trace) is dict and set(trace) == {'event', 'case', 'entry_arrow', 'exception_arrow', 'return_arrow', 'final_arrow',
         'math_initialized_before', 'math_initialized_after', 'events'} and trace['event'] == 'call' and trace['case'] == row['case'], 'complete passive whole-parent journal')
    for name in ('entry_arrow', 'exception_arrow', 'return_arrow', 'final_arrow'):
        snapshot(trace[name])
    events = trace['events']; need(type(events) is list and 0 < len(events) < 32768 and
        [e['sequence'] for e in events] == list(range(len(events))), 'bounded contiguous actual event order')
    values = [e['value'] for e in events]
    for e in values:
        need(e['kind'] in ('ENTRY', 'RETURN', 'EXCEPTION', 'STORE'), 'actual source/helper event kind'); site(e['site'])
    entries = [e for e in values if e['kind'] == 'ENTRY']; returns = [e for e in values if e['kind'] == 'RETURN']; errors = [e for e in values if e['kind'] == 'EXCEPTION']
    same = lambda e, c, n, d: (e['site']['class'], e['site']['method'], e['site']['descriptor']) == (c, n, d)
    need(sum(same(e, 'fr', 'a', '(Lsn;F)V') for e in entries) == 1 and
         [e['site']['pc'] for e in returns if same(e, 'fr', 'a', '(Lsn;F)V')] == ([188] if row['returned'] else []), 'one complete FR entry and complete return188 only on success')
    n = row['case']; gauss_in = [e for e in entries if same(e, 'java.util.Random', 'nextGaussian', '()D')]
    gauss_out = [e for e in returns if same(e, 'java.util.Random', 'nextGaussian', '()D')]
    expected = (3 + (3 if n in SECOND_RETURNS else 0)) if n >= 7 else 0
    need(len(gauss_in) == len(gauss_out) == expected, 'actual first/final heading Gaussian API counts')
    for a, b in zip(gauss_in, gauss_out):
        need(a['receiver']['arrow_random'] and b['result'].keys() == {'D'}, 'genuine Arrow own Random receiver and result')
        C.stream(a['random']); C.stream(b['random']); C.gaussian_transition(a['random'], b['random'], b['result']['D'])
    floats_in = [e for e in entries if same(e, 'java.util.Random', 'nextFloat', '()F')]
    floats_out = [e for e in returns if same(e, 'java.util.Random', 'nextFloat', '()F')]
    need(len(floats_in) == len(floats_out) == int(n >= 7 and n != 14), 'one separate Skeleton nextFloat before sound invocation')
    for a, b in zip(floats_in, floats_out):
        need(a['receiver']['actor_random'] and not a['receiver']['arrow_random'], 'actual separate Skeleton Random receiver')
        state = (int(a['random'][0], 16) * C.MULTIPLIER + 11) & C.MASK
        result = struct.pack('>f', (state >> 24) / float(1 << 24)).hex()
        need(b['result'] == {'F': result} and b['random'] == [f'{state:016x}', *a['random'][1:]], 'exact actual Skeleton float/cache transition')
    reseeds = [e for e in entries if same(e, 'java.util.Random', 'setSeed', '(J)V') and e['receiver']['arrow_random']]
    need(len(reseeds) == int(n == 23), 'only the declared real publication callback reseeds an Arrow')
    if reseeds:
        need(reseeds[0]['arguments'] == [{'J': '00000000000425d4'}], 'literal271828 seed is workload input, never a captured tape')
    heading = []
    for index, event in enumerate(values):
        if event['kind'] == 'ENTRY' and same(event, 'sl', 'a', '(DDDFF)V'):
            heading.append((index, event))
        elif event['kind'] == 'RETURN' and same(event, 'sl', 'a', '(DDDFF)V'):
            need(heading and event['site']['pc'] == 198 and event['arrow'] is not None, 'whole source heading return and actual receiver')
            start, entry = heading.pop()
            draws = [e['result']['D'] for e in values[start:index] if e['kind'] == 'RETURN' and same(e, 'java.util.Random', 'nextGaussian', '()D')]
            need(len(draws) == 3, 'three actual returned Gaussian locals per completed heading')
            C.heading_arithmetic([a['D'] for a in entry['arguments'][:3]], entry['arguments'][3]['F'], entry['arguments'][4]['F'], draws, event['arrow'])
    if n >= 7:
        need(sum(same(e, 'sl', '<init>', '(Lfd;Lls;)V') for e in entries) == 1 and
             sum(same(e, 'sl', 'a', '(DDDFF)V') for e in entries) == (2 if n in SECOND_RETURNS or n == 19 else 1), 'actual constructor and both reached heading entries')
        need(trace['final_arrow'] is not None, 'actual local Arrow retained independently by passive JDI')
    else:
        need(trace['final_arrow'] is None, 'no invented Arrow on masked/nonzero-cooldown/null-target paths')
    if row['arrow_after'] is not None:
        need(exact(trace['final_arrow'], row['arrow_after']), 'ordinary genuine published argument equals actual JDI receiver')
    if not row['returned']:
        need(errors and errors[0]['class'] == row['exception'], 'actual first original/helper exception')
        frames = errors[0]['stack']; expected_pc = {3: 8, 4: 8, 13: 124, 14: 114, 15: 124, 16: 124, 17: 133, 19: 152, 22: 133}[n]
        need(any(s['class'] == 'fr' and s['method'] == 'a' and s['pc'] == expected_pc for s in frames), 'actual enclosing FR source failure prefix')
    publication = [e for e in returns if same(e, 'fd', 'b', '(Lsn;)Z')]
    need([e['result'] for e in publication] == ([n != 12] if n in SECOND_RETURNS or n == 19 else []), 'genuine publication Boolean retained before FR discards it')
    return {'gaussian_calls': expected, 'skeleton_float_calls': len(floats_in), 'publication_returns': [e['result'] for e in publication],
            'root_return': row['returned'], 'first_exception_site': errors[0]['site'] if errors else None}


# Current processing extends the captured validator. The functions above keep
# their captured bodies; only ROOT changes for this public package location.
PROFILE_SHA256 = '57ae11a3ab1ef9f168ea84ada57261e4b45cf8f678b9481564ebab7666ec5a29'
INVISIBLE = (12, 13, 14, 15, 16, 22)


def strict_profiles(directory=HERE):
    path = Path(directory) / 'profiles.tsv'
    need(hashlib.sha256(path.read_bytes()).hexdigest() == PROFILE_SHA256, 'exact captured literal profile bytes')
    return profiles(directory)


def validate_callbacks(row):
    n = row['case']
    expected = [] if n <= 6 or n in (13, 14, 16) else ['sound'] if n == 15 else \
        ['sound', 'sound_return'] if n in (12, 22) else ['sound', 'sound_return', 'added'] if n == 17 else \
        ['sound', 'sound_return', 'added', 'added_return']
    need([e.get('kind') for e in row['callbacks']] == expected, 'exact reached actual callback order')
    for event in row['callbacks']:
        kind = event['kind']
        if kind == 'sound':
            need(set(event) == {'kind', 'world', 'name', 'position', 'volume', 'pitch', 'actor'} and
                 event['world'] == 'world-a' and event['name'] == 'random.bow' and event['volume'] == '3f800000', 'closed actual bow sound arguments')
            actor(event['actor']); C.vector(event['position'], 3, 16); word(event['pitch'], 8)
            actual = event['actor']['entity']
            y = C.decode(actual['position'][1]) - C.decode(actual['dimensions'][2])
            need(event['position'] == [actual['position'][0], struct.pack('>d', y).hex(), actual['position'][2]], 'actual FD Entity-coordinate sound y-offset arithmetic')
        elif kind == 'sound_return':
            need(set(event) == {'kind', 'world', 'actor_world'} and event['world'] == 'world-a' and
                 event['actor_world'] == ('world-b' if n == 21 else None if n == 22 else 'world-a'), 'callback mutation changes only the subsequent World receiver')
        elif kind == 'added':
            need(set(event) == {'kind', 'world', 'arrow', 'world_state'} and event['world'] == ('world-b' if n == 21 else 'world-a'), 'closed genuine Entity publication callback')
            snapshot(event['arrow']); world(event['world_state'])
            need(event['world_state']['entities'] == ['arrow'] and
                 sum(s.count('arrow') for s in event['world_state']['chunk']['slices']) == 1, 'actual Chunk and World membership precede entity_added')
        else:
            need(set(event) == {'kind', 'world', 'arrow'} and event['world'] == ('world-b' if n == 21 else 'world-a'), 'closed actual publication callback return')
            snapshot(event['arrow'])
            a = event['arrow']['arrow']
            need(a['c'] == (None if n == 18 else 'wolf' if n == 20 else 'skeleton') and a['a'] is False,
                 'actual c mutation is independent of the Skeleton constructor pickup flag')
            need((event['arrow']['random'] is None) == (n == 19), 'only declared callback nulls Arrow Random')
    need(set(row['source_raw']) == {'actor', 'arrow', 'target'} and
         all(v is None or type(v) is dict for v in row['source_raw'].values()), 'closed retained raw receiver inventory')


def strict_rows(text, directory=HERE):
    strict_profiles(directory)
    rows = parse_rows(text, directory)
    for row in rows:
        validate_callbacks(row)
    return rows


def strict_source_call(trace, row):
    receipt = source_call(trace, row)
    validate_callbacks(row)
    values = [e['value'] for e in trace['events']]
    draws = [e for e in values if e['kind'] == 'RETURN' and e['site']['class'] == 'java.util.Random' and
             e['site']['method'] == 'nextFloat' and e['site']['descriptor'] == '()F']
    sound = [e for e in row['callbacks'] if e['kind'] == 'sound']
    if sound:
        need(len(draws) == 1, 'actual Skeleton float supplies bow pitch')
        draw = C.decode(draws[0]['result']['F'])
        denominator = C.f32(C.f32(draw * C.decode('3ecccccd')) + C.decode('3f4ccccd'))
        pitch = struct.pack('>f', C.f32(1.0 / denominator)).hex()
        need(sound[0]['pitch'] == pitch, 'actual returned Skeleton float produces the reached bow pitch')
    # These are witnesses from source/helper events. No native event stream
    # is invented or required by the source packet.
    return receipt


def selected_snapshot(value, *, arrow=False, seeded_final=False):
    if value is None:
        return None
    result = copy.deepcopy(value)
    del result['id']
    random_value = result.pop('random')
    result['random_present'] = random_value is not None
    if seeded_final:
        result['random'] = random_value
    if arrow and not seeded_final:
        del result['motion']; del result['angles']
    return result


def selected_actor(value):
    result = copy.deepcopy(value)
    result['entity'] = selected_snapshot(result['entity'])
    return result


def selected_world(value):
    if value is None:
        return None
    result = copy.deepcopy(value)
    result['random_present'] = result.pop('random') is not None
    return result


def selected_context(value, *, wolf_posed):
    wolf = selected_snapshot(value['wolf'])
    if not wolf_posed:
        # LS constructor Math.random196 -> SN.aS208 is genuine independent
        # fixture entropy. The source receiver is never rewritten; every raw
        # angle and full within-run Wolf equality remains validated above.
        angles = wolf['angles']
        wolf['angles'] = {'pitch': angles[1], 'previous_yaw': angles[2], 'previous_pitch': angles[3]}
    return {'actor': selected_actor(value['actor']), 'target': selected_snapshot(value['target']),
            'wolf': wolf, 'world_a': selected_world(value['world_a']),
            'world_b': selected_world(value['world_b'])}


def common_projection(row):
    """Only values observable in every ordinary and passive target VM."""
    result = {k: copy.deepcopy(row[k]) for k in ('case', 'input', 'returned', 'exception')}
    result.update(id_increment=row['after']['counter'] - row['before']['counter'],
                  before=selected_context(row['before'], wolf_posed=row['input'][3] == 2),
                  after=selected_context(row['after'], wolf_posed=row['input'][3] == 2),
                  arrow_after=selected_snapshot(row['arrow_after'], arrow=True, seeded_final=row['case'] == 23))
    callbacks = []
    for event in row['callbacks']:
        value = copy.deepcopy(event)
        if event['kind'] == 'sound':
            del value['pitch']
            value['actor'] = selected_actor(value['actor'])
        elif event['kind'] in ('added', 'added_return'):
            value['arrow'] = selected_snapshot(value['arrow'], arrow=True)
            if event['kind'] == 'added':
                value['world_state'] = selected_world(value['world_state'])
        callbacks.append(value)
    result['callbacks'] = callbacks
    return result


def child_projection(trace, row):
    """Additional actual local receiver fields unavailable to ordinary VMs."""
    need(row['case'] in INVISIBLE and row['arrow_after'] is None and trace['final_arrow'] is not None,
         'passive-only child domain never fills an ordinary row')
    return local_child_projection(row['case'], trace['final_arrow'])


def local_child_projection(case, actual_snapshot):
    """Select actual retained child fields without constructing a source event."""
    need(type(case) is int and case in INVISIBLE and actual_snapshot is not None,
         'declared additional actual local receiver')
    snapshot(actual_snapshot)
    return {'case': case, 'final_arrow': selected_snapshot(actual_snapshot, arrow=True)}


def prefix_projection(common_row):
    """Source/native callback prefixes have separately classified failure causes."""
    need(common_row['case'] in (15, 17) and set(common_row) == {'case', 'input', 'returned', 'exception', 'id_increment',
         'before', 'after', 'arrow_after', 'callbacks'}, 'only the two declared callback-prefix domains')
    result = copy.deepcopy(common_row)
    del result['returned']; del result['exception']
    return result


def compare_rows(expected, actual, cases):
    need(type(expected) is type(actual) is list and [r['case'] for r in expected] == list(cases) and
         [r['case'] for r in actual] == list(cases), 'complete closed source comparison domain')
    for left, right in zip(expected, actual):
        need(exact(left, right), 'source agreement differs in case ' + str(left['case']))


def scalars(value):
    return sum(map(scalars, value.values())) if isinstance(value, dict) else sum(map(scalars, value)) if isinstance(value, list) else 1
