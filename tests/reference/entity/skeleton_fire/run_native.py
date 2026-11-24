"""Retain genuine native FR calls and compare their declared source domains."""
import argparse
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re
import struct
import tempfile

import observation as O
import packet as P

ROOT = P.ROOT
TIMEOUT = 240
STREAM_LIMIT = 2 * 1024 * 1024
COMPLETE = tuple(n for n in range(24) if n not in (15, 17, 21))
PREFIXES = (15, 17)
NATIVE_RETURNS = frozenset((0, 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 18, 20, 23))
FINAL_RETURNS = frozenset((7, 8, 9, 10, 11, 12, 18, 20, 23))
PUBLICATION_RETURNS = FINAL_RETURNS | {19}
CALLBACK_KINDS = {'sound', 'sound_return', 'added', 'added_return'}
SCOPE = ('Direct native protected FR component over genuine fixture owners, compared with retained original source: '
         '21 complete outcomes, two separately classified callback prefixes, six additional passive local Arrows, '
         'and one explicitly unexecuted cross-World terminal-ownership profile. Independent default entropy; no native JDI events, '
         'whole scheduled encounter, SL tick, audiovisual or performance qualification.')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def keys(value, expected, message):
    P.need(type(value) is dict and set(value) == set(expected), message)


def site(value):
    if value is not None:
        O.site(value)


def check_gaussians(receipt):
    """Check actual endpoints/cache returns using independently derived LCG steps.

    Intermediate seeds are derived checks, not claimed native events. Fresh
    transcendental return words stay actual observations; seeded row23 supplies
    the separate original exact-return comparison.
    """
    before, after = receipt['random_before'], receipt['random_after']
    O.C.stream(before)
    O.C.stream(after)
    P.need(before is not None and after is not None, 'actual captured Gaussian endpoint states')
    values = receipt['gaussian_values']
    current = copy.deepcopy(before)
    for index, returned in enumerate(values):
        if current[1]:
            following = [current[0], False, current[2]]
        else:
            state = int(current[0], 16)

            def uniform():
                nonlocal state
                state = (state * O.C.MULTIPLIER + 11) & O.C.MASK
                a = state >> 22
                state = (state * O.C.MULTIPLIER + 11) & O.C.MASK
                b = state >> 21
                return ((a << 27) + b) / float(1 << 53)

            for _ in range(4096):
                x, y = 2 * uniform() - 1, 2 * uniform() - 1
                square = x * x + y * y
                if 0 < square < 1:
                    break
            else:
                raise ValueError('bounded native Gaussian rejection path')
            cached = values[index + 1] if index + 1 < len(values) else after[2]
            following = [f'{state:016x}', True, cached]
        O.C.gaussian_transition(current, following, returned)
        current = following
    P.need(O.exact(current, after), 'all native Gaussian endpoint/cache steps agree')


def heading(receipt, *, entered, returned):
    keys(receipt, ('site', 'heading_site', 'heading_entered', 'heading_returned', 'gaussian_calls',
                  'random_captured', 'arguments', 'random_before', 'random_after', 'gaussian_values'),
         'closed actual native Arrow receipt')
    for name in ('heading_entered', 'heading_returned', 'random_captured'):
        P.need(type(receipt[name]) is bool, 'native heading flags are actual Booleans')
    P.need(receipt['heading_entered'] is entered and receipt['heading_returned'] is returned and
           type(receipt['gaussian_calls']) is int and receipt['gaussian_calls'] == (3 if returned else 0) and
           receipt['random_captured'] is returned, 'exact reached native heading/Gaussian prefix')
    site(receipt['site'])
    site(receipt['heading_site'])
    if entered:
        P.need(receipt['heading_site'] == {'class': 'sl', 'method': 'a', 'descriptor': '(DDDFF)V',
                                          'pc': 198 if returned else 43}, 'actual whole-heading return or null Random43')
    args = receipt['arguments']
    keys(args, ('direction', 'speed', 'spread'), 'closed actual heading arguments')
    O.C.vector(args['direction'], 3, 16)
    O.word(args['speed'], 8)
    O.word(args['spread'], 8)
    O.C.vector(receipt['gaussian_values'], 3, 16)
    if returned:
        check_gaussians(receipt)
    else:
        P.need(receipt['random_before'] is receipt['random_after'] is None,
               'no invented Random endpoints before a successful draw')


def validate_callbacks(row):
    n = row['case']
    expected = [] if n <= 6 or n in (13, 14, 16) else ['sound'] if n == 15 else \
        ['sound', 'sound_return'] if n in (12, 22) else ['sound', 'sound_return', 'added'] if n == 17 else \
        ['sound', 'sound_return', 'added', 'added_return']
    events = row['callbacks']
    P.need(type(events) is list and [e.get('kind') for e in events] == expected,
           'exact actual native callback order')
    for event in events:
        kind = event['kind']
        P.need(kind in CALLBACK_KINDS, 'known actual callback slot')
        if kind == 'sound':
            keys(event, ('kind', 'world', 'name', 'position', 'volume', 'pitch', 'actor'), 'closed actual sound callback')
            P.need(event['world'] == 'world-a' and event['name'] == 'random.bow' and event['volume'] == '3f800000',
                   'actual bow callback receiver, literal and volume')
            O.actor(event['actor'])
            O.C.vector(event['position'], 3, 16)
            O.word(event['pitch'], 8)
            actor = event['actor']['entity']
            y = O.C.decode(actor['position'][1]) - O.C.decode(actor['dimensions'][2])
            P.need(event['position'] == [actor['position'][0], struct.pack('>d', y).hex(), actor['position'][2]],
                   'actual Entity-coordinate bow position')
        elif kind == 'sound_return':
            keys(event, ('kind', 'world', 'actor_world'), 'closed actual sound callback return')
            P.need(event['world'] == 'world-a' and event['actor_world'] ==
                   (None if n == 22 else 'world-a'), 'actual callback nullable World mutation')
        elif kind == 'added':
            keys(event, ('kind', 'world', 'arrow', 'world_state'), 'closed actual publication callback')
            P.need(event['world'] == 'world-a', 'supported native publication World')
            O.snapshot(event['arrow'])
            O.world(event['world_state'])
            P.need(event['world_state']['entities'] == ['arrow'] and
                   sum(s.count('arrow') for s in event['world_state']['chunk']['slices']) == 1,
                   'actual Chunk/World membership precedes native added callback')
        else:
            keys(event, ('kind', 'world', 'arrow'), 'closed actual publication callback return')
            P.need(event['world'] == 'world-a', 'actual native publication return World')
            O.snapshot(event['arrow'])
            P.need(event['arrow']['arrow']['c'] == (None if n == 18 else 'wolf' if n == 20 else 'skeleton') and
                   event['arrow']['arrow']['a'] is False and
                   (event['arrow']['random'] is None) == (n == 19), 'actual callback fields and independent pickup flag')


def validate_native_rows(rows):
    profiles = O.strict_profiles()
    P.need(type(rows) is list and len(rows) == 24, 'all24 profiles, including the typed unexecuted boundary')
    booleans = ('entered', 'returned', 'allocated', 'constructor_invoked', 'constructor_returned', 'eye_returned',
                'actor_random_captured', 'actor_float_returned', 'sound_invoked', 'sound_returned', 'publication_invoked',
                'publication_returned', 'publication_result', 'heading_invoked', 'heading_returned', 'cooldown_stored',
                'facing_stored', 'attacking_stored', 'target_same', 'arrow_same', 'actor_random_same')
    capture_fields = (*booleans, 'constructor_world', 'sound_world', 'publication_world', 'distance', 'eye_height', 'lift',
                      'direction', 'heading_arguments', 'actor_float', 'sound_volume', 'sound_pitch', 'actor_random_before', 'actor_random_after',
                      'constructor_result', 'constructor', 'final_heading')
    expected_errors = {n: error for n, error in O.ERRORS.items() if n not in PREFIXES}
    for row, profile in zip(rows, profiles):
        if profile[0] == 21:
            keys(row, ('schema', 'case', 'input', 'executed', 'complete_outcome', 'boundary'),
                 'closed unexecuted cross-World ownership boundary')
            P.need(row['schema'] == 'SKELETON_FIRE_NATIVE_BOUNDARY_V1' and type(row['case']) is int and
                   row['case'] == 21 and O.exact(row['input'], profile) and row['executed'] is False and
                   row['complete_outcome'] is False and row['boundary'] == 'cross-world-terminal-ownership',
                   'row21 has no fabricated native call, receiver, state, prefix or outcome')
            continue
        keys(row, ('schema', 'case', 'input', 'before', 'callbacks', 'executed', 'complete_outcome', 'returned',
                   'exception', 'after', 'arrow_after', 'local_arrow', 'native'), 'closed actual native FR row')
        n = row['case']
        P.need(row['schema'] == 'SKELETON_FIRE_NATIVE_RAW_V1' and type(n) is int and n == profile[0] and
               O.exact(row['input'], profile) and row['executed'] is True and type(row['complete_outcome']) is bool and
               row['complete_outcome'] == (n in COMPLETE) and type(row['returned']) is bool and
               row['returned'] == (n in NATIVE_RETURNS), 'exact literal profile and native outcome domain')
        O.context(row['before'])
        O.context(row['after'])
        O.snapshot(row['arrow_after'])
        O.snapshot(row['local_arrow'])
        validate_callbacks(row)
        native = row['native']
        keys(native, ('status', 'native_policy', 'source_exception', 'site', 'child_site', 'capture'), 'closed native component receipt')
        P.need(type(native['status']) is int and type(native['native_policy']) is type(native['source_exception']) is bool and
               (native['status'] == 0) is row['returned'], 'native status remains independent of source Boolean outcomes')
        expected_exception = expected_errors.get(n, 'NONE' if row['returned'] else None)
        P.need(row['exception'] == expected_exception and native['source_exception'] == (n in expected_errors) and
               native['native_policy'] == (n in PREFIXES), 'actual source nulls stay distinct from native callback refusals')
        O.site(native['site'])
        site(native['child_site'])
        expected_pc = 188 if row['returned'] else {3: 8, 4: 8, 13: 124, 14: 114, 15: 124, 16: 124,
                                                 17: 133, 19: 152, 22: 133}[n]
        P.need(native['site'] == {'class': 'fr', 'method': 'a', 'descriptor': '(Lsn;F)V', 'pc': expected_pc},
               'actual native enclosing FR return/failure prefix')
        c = native['capture']
        keys(c, capture_fields, 'closed actual captured component values')
        P.need(all(type(c[key]) is bool for key in booleans), 'actual typed capture flags')
        allocated = n >= 7
        sound = allocated and n != 14
        sound_returned = allocated and n not in (13, 14, 15, 16)
        publication = n in PUBLICATION_RETURNS
        P.need(c['entered'] and c['returned'] is row['returned'] and c['allocated'] is allocated and
               c['constructor_invoked'] is allocated and c['constructor_returned'] is allocated and
               c['eye_returned'] is allocated and c['actor_random_captured'] is (allocated and n != 14) and
               c['actor_float_returned'] is (allocated and n != 14) and c['sound_invoked'] is sound and
               c['sound_returned'] is sound_returned and c['publication_invoked'] is sound_returned and
               c['publication_returned'] is publication and c['publication_result'] is (publication and n != 12) and
               c['heading_invoked'] is publication and c['heading_returned'] is (n in FINAL_RETURNS) and
               c['cooldown_stored'] is (n in FINAL_RETURNS) and c['facing_stored'] is (row['returned'] and n >= 5) and
               c['attacking_stored'] is (row['returned'] and n >= 5) and c['target_same'] and c['arrow_same'],
               'exact actual source-call reach, stores and owned receiver correlation')
        for key in ('constructor_world', 'sound_world', 'publication_world'):
            P.need(c[key] in (None, 'world-a'), 'captured actual nullable World identity')
        for key in ('distance', 'eye_height', 'lift', 'actor_float', 'sound_volume', 'sound_pitch'):
            O.word(c[key], 8)
        P.need(c['distance'] == profile[1], 'actual passed range bits retained')
        O.C.vector(c['direction'], 3, 16)
        before, after = row['before'], row['after']
        P.need(before['world_b'] is after['world_b'] is None, 'no native second-World identity outside unexecuted21')
        P.need(after['counter'] - before['counter'] == int(allocated) and
               O.exact(before['target'], after['target']) and O.exact(before['wolf'], after['wolf']) and
               O.exact(before['math_random'], after['math_random']), 'native allocation count and unchanged target/Wolf/Math inputs')
        for world in ('world_a', 'world_b'):
            if before[world] is not None:
                P.need(O.exact(before[world]['random'], after[world]['random']), 'firing preserves actual World Random')
        P.need((row['local_arrow'] is not None) is allocated and
               (row['arrow_after'] is not None) == (n in O.VISIBLE), 'local ownership and ordinary callback visibility stay distinct')
        if row['arrow_after'] is not None:
            P.need(O.exact(row['arrow_after'], row['local_arrow']), 'ordinary genuine callback argument is the actual owned receiver')
        P.need(after['actor']['cooldown'] == (30 if n in FINAL_RETURNS else profile[2]) and
               after['actor']['attacking'] is (row['returned'] and n >= 5), 'actual native tail store prefix')
        O.C.stream(c['actor_random_before'])
        O.C.stream(c['actor_random_after'])
        if c['actor_float_returned']:
            start = c['actor_random_before']
            state = (int(start[0], 16) * O.C.MULTIPLIER + 11) & O.C.MASK
            draw = struct.pack('>f', (state >> 24) / float(1 << 24)).hex()
            P.need(c['actor_random_same'] and c['actor_float'] == draw and
                   c['actor_random_after'] == [f'{state:016x}', *start[1:]] and
                   O.exact(c['actor_random_after'], after['actor']['entity']['random']) and
                   O.exact(start, before['actor']['entity']['random']), 'independent actual Skeleton float/cache transition')
            denominator = O.C.f32(O.C.f32(O.C.decode(draw) * O.C.decode('3ecccccd')) + O.C.decode('3f4ccccd'))
            pitch = struct.pack('>f', O.C.f32(1.0 / denominator)).hex()
            P.need(c['sound_pitch'] == pitch and c['sound_volume'] == '3f800000', 'actual Skeleton draw produces evaluated bow arguments')
            for event in row['callbacks']:
                if event['kind'] == 'sound':
                    P.need(event['pitch'] == pitch, 'actual callback receives evaluated independent pitch')
        else:
            P.need(c['actor_random_before'] is c['actor_random_after'] is None and
                   O.exact(before['actor']['entity']['random'], after['actor']['entity']['random']),
                   'no invented actor Random transition before reached nextFloat')
        if allocated:
            P.need(c['constructor_world'] == (None if n == 13 else 'world-a') and
                   c['sound_world'] == (None if n == 13 else 'world-a') and
                   c['publication_world'] == (None if n in (13, 14, 15, 16, 22) else 'world-a'),
                   'independent actual constructor/sound/fresh-publication World captures')
            target = before['target']
            actor = before['actor']['entity']
            dx = O.C.decode(target['position'][0]) - O.C.decode(actor['position'][0])
            dz = O.C.decode(target['position'][2]) - O.C.decode(actor['position'][2])
            dy = ((O.C.decode(target['position'][1]) + O.C.decode(c['eye_height'])) - .20000000298023224) - \
                O.C.decode(row['local_arrow']['position'][1])
            direction = [struct.pack('>d', value).hex() for value in (dx, dy, dz)]
            lift = O.C.f32(O.C.f32(math.sqrt(dx * dx + dz * dz)) * O.C.decode('3e4ccccd'))
            P.need(c['direction'] == direction and c['lift'] == struct.pack('>f', lift).hex(),
                   'actual target/Arrow/eye fields produce reached FR direction and float lift')
            heading(c['constructor'], entered=True, returned=True)
            P.need(c['constructor']['site'] == {'class': 'sl', 'method': '<init>', 'descriptor': '(Lfd;Lls;)V', 'pc': 289},
                   'complete genuine Living Arrow constructor return')
            result = c['constructor_result']
            keys(result, ('motion', 'angles', 'j'), 'actual immediate post-constructor stores')
            O.C.vector(result['motion'], 3, 16)
            O.C.vector(result['angles'], 4, 8)
            P.need(type(result['j']) is int, 'actual constructor SL.j type')
            args = c['constructor']['arguments']
            O.C.heading_arithmetic(args['direction'], args['speed'], args['spread'],
                                   c['constructor']['gaussian_values'],
                                   {'motion': result['motion'], 'angles': result['angles'], 'arrow': {'j': result['j']}})
            if c['heading_invoked']:
                final = c['final_heading']
                P.need(c['heading_arguments'] == final['arguments'] ==
                       {'direction': [direction[0], struct.pack('>d', dy + lift).hex(), direction[2]],
                        'speed': '3f19999a', 'spread': '41400000'},
                       'actual FR caller arguments equal reached child arguments')
                heading(final, entered=True, returned=n in FINAL_RETURNS)
                if n in FINAL_RETURNS:
                    P.need(O.exact(final['random_after'], row['local_arrow']['random']), 'final receipt observes actual owned Arrow Random')
                    if n == 23:
                        P.need(final['random_before'] == [f'{(271828 ^ O.C.MULTIPLIER) & O.C.MASK:016x}', False,
                                                          c['constructor']['random_after'][2]], 'sole real271828 callback reseed preserves unused cache scalar')
                    else:
                        P.need(O.exact(c['constructor']['random_after'], final['random_before']), 'same actual own Random continues from constructor')
                    args = final['arguments']
                    O.C.heading_arithmetic(args['direction'], args['speed'], args['spread'], final['gaussian_values'], row['local_arrow'])
            else:
                P.need(c['final_heading'] is c['heading_arguments'] is None, 'no fabricated final-heading child')
            if not c['heading_returned'] and n != 19:
                P.need(O.exact(c['constructor']['random_after'], row['local_arrow']['random']), 'constructor state survives stopped parent prefix')
        else:
            P.need(c['constructor'] is c['constructor_result'] is c['final_heading'] is c['heading_arguments'] is None,
                   'no fabricated child or stores before actual NEW')
    return rows


def completed_native(child):
    c = child.capture_report
    P.need(all(type(c.get(key)) is int for key in ('exit', 'timeout_seconds', 'limit', 'stdout_bytes', 'stderr_bytes')),
           'native capture counts and budgets require actual integers')
    P.need(type(child.returncode) is int and child.returncode == c['exit'] == 0 and
           c.get('timed_out') is False and c.get('output_overflow') is False and c.get('pipe_capture_complete') is True and
           c.get('reaped') is True and c.get('reader_errors') == {} and c['timeout_seconds'] == TIMEOUT and
           c['limit'] == STREAM_LIMIT and c['stdout_bytes'] == len(child.stdout) and c['stderr_bytes'] == len(child.stderr),
           'complete bounded native process/stream receipt: ' + json.dumps(c, sort_keys=True))
    P.need(child.stderr == b'' and child.stdout.endswith(b'\n'), 'quiet complete native transport')
    lines = child.stdout.decode('utf-8').splitlines()
    P.need(len(lines) == 94, 'exact23 native fixture/bootstrap/row blocks, one unexecuted boundary and cleanup footer')
    rows = []
    cursor = 0
    for n in range(24):
        if n == 21:
            prefix = 'SKELETON_FIRE_NATIVE_BOUNDARY '
            P.need(lines[cursor].startswith(prefix), 'typed unexecuted21 before any fixture setup')
            rows.append(O.loads(lines[cursor][len(prefix):]))
            cursor += 1
            continue
        block = lines[cursor:cursor + 4]
        P.need(block[:3] == [f'SKELETON_FIRE_FIXTURE_BEGIN case={n}', '16 achievements', '151 recipes'] and
               block[3].startswith('SKELETON_FIRE_NATIVE '), 'exact actual native profile/bootstrap ordering')
        rows.append(O.loads(block[3][len('SKELETON_FIRE_NATIVE '):]))
        cursor += 4
    footer = re.fullmatch(r'Skeleton firing reference: ([1-9][0-9]*) checks;24 profiles;23 genuine calls;1 unexecuted;cleanup=1', lines[-1])
    P.need(cursor == len(lines) - 1 and footer is not None, 'actual checked cleanup for all23 executed graphs')
    return validate_native_rows(rows), int(footer.group(1))


def source_calls(finite):
    records = [O.loads(line) for line in finite['source_journals'][0].splitlines()]
    return {record['case']: record for record in records if record['event'] == 'call'}


def compare_native(finite, rows):
    validate_native_rows(rows)
    actual = [O.common_projection(row) if row['executed'] else None for row in rows]
    source = finite['ordinary_common']
    expected_complete = [source[n] for n in COMPLETE]
    O.compare_rows(expected_complete, [actual[n] for n in COMPLETE], COMPLETE)
    expected_prefixes = [O.prefix_projection(source[n]) for n in PREFIXES]
    O.compare_rows(expected_prefixes, [O.prefix_projection(actual[n]) for n in PREFIXES], PREFIXES)
    children = [O.local_child_projection(n, rows[n]['local_arrow']) for n in O.INVISIBLE]
    O.compare_rows(finite['passive_children'], children, O.INVISIBLE)
    calls = source_calls(finite)
    for row in rows:
        if not row['executed']:
            continue
        c = row['native']['capture']
        values = [event['value'] for event in calls[row['case']]['events']]
        arguments = [value['arguments'] for value in values if value['kind'] == 'ENTRY' and
                     value['site']['class'] == 'sl' and value['site']['method'] == 'a' and
                     value['site']['descriptor'] == '(DDDFF)V']
        if c['eye_returned']:
            eyes = [value['result']['F'] for value in values if value['kind'] == 'RETURN' and
                    value['site']['method'] == 'w' and value['site']['descriptor'] == '()F']
            P.need(eyes and c['eye_height'] == eyes[-1], 'actual target eye return matches the retained source API')
        for index, receipt in enumerate((c['constructor'], c['final_heading'])):
            if receipt is None:
                continue
            args = receipt['arguments']
            source_args = arguments[index]
            P.need(args == {'direction': [v['D'] for v in source_args[:3]], 'speed': source_args[3]['F'],
                            'spread': source_args[4]['F']}, 'actual constructor/final heading arguments match the source entry')
    # Full seeded motion/angles/Random are already in common row23. Also compare
    # all three actual second-heading return words to the retained source APIs.
    source_call = calls[23]
    seeded_returns = [event['value']['result']['D'] for event in source_call['events']
                      if event['value']['kind'] == 'RETURN' and event['value']['site']['class'] == 'java.util.Random' and
                      event['value']['site']['method'] == 'nextGaussian'][-3:]
    P.need(len(seeded_returns) == 3 and O.exact(rows[23]['native']['capture']['final_heading']['gaussian_values'], seeded_returns),
           'all three actual seeded271828 Gaussian return words match source')
    return {'complete_cases': list(COMPLETE), 'complete_scalars': O.scalars(expected_complete),
            'callback_prefix_cases': list(PREFIXES), 'callback_prefix_scalars': O.scalars(expected_prefixes),
            'additional_passive_cases': list(O.INVISIBLE), 'additional_passive_scalars': O.scalars(children),
            'executed_cases': [row['case'] for row in rows if row['executed']],
            'unexecuted_cases': [row['case'] for row in rows if not row['executed']],
            'source_only_complete_outcome_cases': [15, 17, 21],
            'actual_calls': sum(row['executed'] for row in rows),
            'native_jdi_events': False, 'seeded_gaussian_returns': 3}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    args = parser.parse_args()
    finite, _ = P.read()
    executable = args.executable.resolve(strict=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='skeleton-fire-comparison-', dir=args.work_dir.resolve()))
    resources, game = output / 'resources', output / 'game'
    game.mkdir()
    support_path = ROOT / 'tests/reference/client/player_statistics/support.py'
    support = module('skeleton_fire_statistics_support', support_path)
    support.resources_at(resources)
    inputs = {executable, Path(__file__).resolve(), ROOT / 'CMakeLists.txt', ROOT / '.clang-format',
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/entity/test_skeleton_fire.c',
              ROOT / 'tests/entity/test_skeleton_fire_reference.c', ROOT / 'tests/entity/test_arrow_construction.c',
              ROOT / 'tests/client/statistics/test_player_dispatch.c', support_path,
              ROOT / 'tests/reference/client/statistics/support.py', *P.HERE.rglob('*.*'),
              *resources.rglob('*.*'), *executable.parent.glob('*.dll'), *executable.parent.glob('*.so*')}
    inputs = {path for path in inputs if path.is_file() and '__pycache__' not in path.parts and path.suffix != '.pyc'}
    for directory in ('src', 'include', 'cmake'):
        inputs.update(path for path in (ROOT / directory).rglob('*') if path.is_file())
    before = {str(path): digest(path) for path in sorted(inputs)}
    report = {'schema': 'SKELETON_FIRE_NATIVE_COMPARISON_V1', 'status': 'FAIL', 'scope': SCOPE,
              'input_before_sha256': before, 'timeout_seconds': TIMEOUT, 'stream_limit_each': STREAM_LIMIT}
    failure = None
    child = None
    try:
        command = [str(executable), str(resources), str(game), str(P.HERE / 'profiles.tsv')]
        report['command'] = command
        child = support.run(command, output, 'native', timeout=TIMEOUT)
        report['returncode'], report['capture'] = child.returncode, child.capture_report
        rows, checks = completed_native(child)
        report['fixture_checks'] = checks
        actual = output / 'actual.json'
        actual.write_text(json.dumps(rows, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
        report.update(compare_native(finite, rows))
        report.update(status='PASS', actual_sha256=digest(actual))
    except BaseException as error:
        failure = error
        report['exception'] = {'type': type(error).__name__, 'message': str(error)}
        if getattr(error, 'capture_report', None) is not None:
            report['capture'] = error.capture_report
    finally:
        try:
            report['input_after_sha256'] = {str(path): digest(path) for path in sorted(inputs)}
            unchanged = before == report['input_after_sha256']
        except BaseException as error:
            unchanged = False
            report['recheck_exception'] = {'type': type(error).__name__, 'message': str(error)}
        if not unchanged:
            report['status'] = 'FAIL'
        for name in ('stdout', 'stderr'):
            path = output / ('native.' + name)
            if path.exists():
                report[name] = {'bytes': path.stat().st_size, 'sha256': digest(path), 'path': str(path)}
        destination = output / 'run.json'
        destination.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    if failure is not None:
        stderr = child.stderr.decode('utf-8', errors='replace') if child is not None else ''
        raise ValueError(f'{type(failure).__name__}: {failure}; retained report={destination}; '
                         f'native exit={report.get("returncode")}; capture={report.get("capture")}; '
                         f'captured stderr={stderr[:65536]}') from failure
    P.need(unchanged, 'native source/product/resource/packet inputs changed; retained report=' + str(destination))
    print(f'Skeleton firing original comparison: {report["actual_calls"]} actual calls,1 unexecuted; '
          f'21 complete profiles,2 callback prefixes,6 additional local Arrows; {destination}')


if __name__ == '__main__':
    main()
