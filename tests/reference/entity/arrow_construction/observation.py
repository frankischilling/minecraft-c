"""Typed observations and explicit entropy boundaries; never executes source code."""
import copy
import json
import math
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
PREFIX = 'ARROW_CONSTRUCTION '
MASK = (1 << 48) - 1
MULTIPLIER = 0x5DEECE66D
SNAPSHOT_KEYS = {'class', 'id', 'world', 'arrow', 'position', 'motion', 'history',
                 'angles', 'dimensions', 'bounds', 'dead', 'random', 'watcher'}


def need(value, message):
    if not value:
        raise ValueError(message)


def exact(a, b):
    return type(a) is type(b) and (a.keys() == b.keys() and all(exact(a[k], b[k]) for k in a)
        if isinstance(a, dict) else len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
        if isinstance(a, list) else a == b)


def unique(items):
    value = {}
    for key, word in items:
        need(key not in value, 'duplicate JSON key')
        value[key] = word
    return value


def loads(text):
    return json.loads(text, object_pairs_hook=unique,
        parse_constant=lambda value: (_ for _ in ()).throw(ValueError('nonfinite JSON token')))


def profiles(directory=HERE):
    result = [[int(word) for word in line.split()] for line in (Path(directory) / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    need(len(result) == 40 and all(len(row) == 12 and row[0] == i and
        0 <= row[1] <= 3 and 0 <= row[2] <= 3 and 0 <= row[3] <= 4 and
        0 <= row[4] <= 3 and 0 <= row[5] <= 10 and 0 <= row[6] <= 4 and
        0 <= row[7] <= 4 and row[9] in (0, 1) and row[10] in (-1, 16) and
        0 <= row[11] <= 3 for i, row in enumerate(result)), 'complete declared profile inventory')
    need([row[0] for row in result if row[10] != -1] == [18], 'single declared actual reuse')
    return result


def word(value, width):
    need(type(value) is str and re.fullmatch('[0-9a-f]{' + str(width) + '}', value) is not None, 'raw IEEE word')
    return value


def stream(value):
    if value is None:
        return
    need(type(value) is list and len(value) == 3 and type(value[1]) is bool, 'complete Random/cache snapshot')
    word(value[0], 16); word(value[2], 16)
    need(int(value[0], 16) <= MASK, 'actual Random 48-bit internal seed')


def vector(values, count, width):
    need(type(values) is list and len(values) == count, 'complete typed vector')
    for value in values:
        word(value, width)


def arguments(profile):
    nan = struct.unpack('>d', bytes.fromhex('7ff8000000001234'))[0]
    tiny = struct.unpack('>d', bytes.fromhex('0000000000000001'))[0]
    positions = [(7., -3., 11.), (-0., 0., -0.), (tiny, -tiny, -17.25), (nan, math.inf, -math.inf)]
    directions = [(1., 2., 3.), (0., 0., 0.), (-0., 0., -0.), (tiny, -tiny, tiny), (1e300, -1e300, 1e300),
                  (nan, 2., 3.), (1., nan, 3.), (1., 2., nan), (math.inf, 2., 3.), (1., -math.inf, 3.), (1., 2., math.inf)]
    def floating(selector, speed):
        return ('3fc00000' if speed else '3f800000', '00000000', 'bfc00000' if speed else 'bf800000',
                '7fc01234', '7f800000')[selector]
    return {'position': [struct.pack('>d', x).hex() for x in positions[profile[4]]],
            'direction': [struct.pack('>d', x).hex() for x in directions[profile[5]]],
            'speed': floating(profile[6], True), 'spread': floating(profile[7], False)}


def snapshot(value):
    if value is None:
        return
    need(type(value) is dict and set(value) == SNAPSHOT_KEYS, 'closed source snapshot')
    need(value['class'] in ('sl', 'fr', 'dc', 'gi', 'bp') and type(value['id']) is int and
         value['world'] in (None, 'world-a', 'world-b') and type(value['dead']) is bool, 'actual typed source identity')
    if value['arrow'] is not None:
        a = value['arrow']
        need(type(a) is dict and set(a) == set('abcdefghijk') and
             all(type(a[k]) is bool for k in ('a', 'i')) and all(type(a[k]) is int for k in 'bdefghjk') and
             a['c'] in (None, 'shooter') and value['class'] == 'sl', 'actual SL field types')
    for name, count in (('position', 3), ('motion', 3), ('history', 6)):
        vector(value[name], count, 16)
    vector(value['angles'], 4, 8); vector(value['dimensions'], 3, 8)
    if value['bounds'] is not None:
        vector(value['bounds'], 6, 16)
    stream(value['random'])
    w = value['watcher']
    if w is not None:
        need(type(w) is dict and set(w) == {'map_null', 'dirty', 'records'} and
             type(w['map_null']) is type(w['dirty']) is bool and type(w['records']) is list, 'actual watcher and Map')
        previous = -1
        for record in w['records']:
            need(type(record) is dict and set(record) == {'key', 'type', 'id', 'value', 'dirty'} and
                 all(type(record[k]) is int for k in ('key', 'type', 'id')) and record['key'] > previous and
                 type(record['dirty']) is bool, 'canonical actual watcher records')
            previous = record['key']; p = record['value']
            need(type(p) is dict and set(p) == {'kind', 'value'} and p['kind'] in ('Byte', 'Integer', 'String', 'Null'), 'independent payload kind')
            need((p['kind'] == 'Byte' and type(p['value']) is int and -128 <= p['value'] <= 127) or
                 (p['kind'] == 'Integer' and type(p['value']) is int and -(1 << 31) <= p['value'] < 1 << 31) or
                 (p['kind'] == 'String' and type(p['value']) is list and all(type(x) is int and 0 <= x <= 65535 for x in p['value'])) or
                 (p['kind'] == 'Null' and p['value'] is None), 'exact actual watcher payload')


def parse_rows(text, directory=HERE):
    need(type(text) is str and len(text) <= 16 * 1024 * 1024, 'bounded original/native stdout')
    values = [loads(line[len(PREFIX):]) for line in text.splitlines() if line.startswith(PREFIX)]
    need(len(values) == 40, 'forty complete original/native rows')
    for row, profile in zip(values, profiles(directory)):
        validate_row(row, profile)
    return values


def validate_row(row, profile):
    required = {'schema', 'case', 'input', 'arguments', 'returned', 'exception', 'before', 'after',
                'arrow_before', 'arrow_after', 'exception_message', 'exception_frames'}
    need(type(row) is dict and required <= set(row) and not (set(row) - required - {'source_raw', 'native'}), 'closed raw row')
    need(row['schema'] == 'ARROW_CONSTRUCTION_RAW_V1' and type(row['case']) is int and row['case'] == profile[0] and
         exact(row['input'], profile), 'actual row/profile identity')
    expected_exception = profile[0] in (14, 15, 38)
    need(type(row['returned']) is bool and row['returned'] is not expected_exception and
         row['exception'] == ('java/lang/NullPointerException' if expected_exception else 'NONE'), 'actual source return or first exception')
    need(row['exception_message'] is None or type(row['exception_message']) is str, 'retained exception text')
    need(type(row['exception_frames']) is list and all(type(x) is str for x in row['exception_frames']), 'retained exception frames')
    a = row['arguments']; need(type(a) is dict and set(a) == {'position', 'direction', 'speed', 'spread'}, 'closed actual arguments')
    vector(a['position'], 3, 16); vector(a['direction'], 3, 16); word(a['speed'], 8); word(a['spread'], 8)
    need(exact(a, arguments(profile)), 'exact declared input bit patterns')
    for side in ('before', 'after'):
        context = row[side]
        need(type(context) is dict and set(context) == {'counter', 'world_random', 'math_random', 'shooter'} and type(context['counter']) is int, 'actual context')
        stream(context['world_random']); stream(context['math_random']); snapshot(context['shooter'])
        snapshot(row['arrow_' + side])
    before, after = row['before'], row['after']
    need(exact(before['world_random'], after['world_random']) and exact(before['math_random'], after['math_random']), 'unwritten selected World/shared Math streams')
    need(exact(before['shooter'], after['shooter']), 'source does not mutate typed shooter')
    need(after['counter'] - before['counter'] == (0 if profile[1] == 3 else 1), 'actual constructor ID increment')
    need((row['arrow_before'] is None) == (profile[1] != 3), 'ordinary constructor receiver is inaccessible before entry')
    if profile[1] != 3 and row['arrow_after'] is not None:
        need(row['arrow_after']['id'] == before['counter'], 'actual source allocated ID')
    if profile[1] == 3:
        need(row['arrow_before'] is not None and row['arrow_after'] is not None and
             row['arrow_before']['id'] == row['arrow_after']['id'], 'same actual heading receiver')
    if profile[0] == 38:
        need(exact(row['arrow_before'], row['arrow_after']), 'null Random fails before heading stores')


def source_call(trace, row):
    need(type(trace) is dict and set(trace) == {'event', 'case', 'entry_arrow', 'exception_arrow', 'return_arrow',
         'math_initialized_before', 'math_initialized_after', 'events'} and trace['event'] == 'call' and trace['case'] == row['case'], 'complete actual call journal')
    need(type(trace['math_initialized_before']) is type(trace['math_initialized_after']) is bool, 'passive IN initialization observations')
    events = trace['events']; need(type(events) is list and len(events) < 32768, 'bounded actual source events')
    need([event['sequence'] for event in events] == list(range(len(events))), 'contiguous actual event order')
    events = [event['value'] for event in events]
    entries = [e for e in events if e['kind'] == 'ENTRY']
    returns = [e for e in events if e['kind'] == 'RETURN']
    errors = [e for e in events if e['kind'] == 'EXCEPTION']
    root_entry = next((e for e in entries if e['site']['class'] == 'sl' and e['site']['method'] in ('<init>', 'a')), None)
    need(root_entry is not None, 'actual complete selected source entry')
    p = row['input']; return_pc = (58, 72, 289, 198)[p[1]]
    root_returns = [e for e in returns if e['site']['class'] == 'sl' and e['site']['pc'] == return_pc]
    need(bool(root_returns) == row['returned'] and (bool(errors) != row['returned']), 'actual root return versus exception events')
    if errors:
        need(errors[0]['class'] == row['exception'] and errors[0]['site']['class'] == 'sl' and
             errors[0]['site']['pc'] == (73 if p[1] == 2 else 43), 'actual first failure stage')
        snapshot(trace['exception_arrow']); need(trace['exception_arrow'] is not None, 'actual retained failed constructor/heading receiver')
    snapshot(trace['entry_arrow']); snapshot(trace['return_arrow'])
    if row['arrow_after'] is not None and row['returned']:
        need(exact(trace['return_arrow'], row['arrow_after']), 'JDI and ordinary target snapshot of same returned receiver')
    gaussian_entries = [e for e in entries if e['site']['class'] == 'java.util.Random']
    gaussian_returns = [e for e in returns if e['site']['class'] == 'java.util.Random']
    expected = 3 if p[1] in (2, 3) and row['returned'] else 0
    need(len(gaussian_entries) == len(gaussian_returns) == expected, 'exact reached Gaussian API count')
    gaussians = []
    for entry, exit in zip(gaussian_entries, gaussian_returns):
        stream(entry['random']); stream(exit['random']); need(set(exit['result']) == {'D'}, 'actual returned Gaussian double')
        value = word(exit['result']['D'], 16); gaussian_transition(entry['random'], exit['random'], value); gaussians.append(value)
    if expected:
        heading = next(e for e in entries if e['site']['class'] == 'sl' and e['site']['method'] == 'a')
        heading_arithmetic([x['D'] for x in heading['arguments'][:3]], heading['arguments'][3]['F'],
                           heading['arguments'][4]['F'], gaussians, row['arrow_after'])
    if p[1] in (0, 1) or p[0] in (14, 15):
        need(not any(e['site']['class'] == 'in' for e in entries), 'no source Math active use on plain/position/null-shooter prefix')
    if p[0] == 0:
        need(not trace['math_initialized_before'] and not trace['math_initialized_after'], 'cold source IN remains uninitialized')
    return {'return_pc': return_pc if row['returned'] else None,
            'exception_pc': errors[0]['site']['pc'] if errors else None,
            'gaussian_calls': expected, 'gaussian_values': gaussians,
            'random_before': gaussian_entries[0]['random'] if expected else None,
            'random_after': gaussian_returns[-1]['random'] if expected else None}


def decode(value):
    return struct.unpack('>d' if len(value) == 16 else '>f', bytes.fromhex(value))[0]


def f32(value):
    try:
        return struct.unpack('>f', struct.pack('>f', value))[0]
    except OverflowError:
        return math.copysign(math.inf, value)


def divide(a, b):
    if b == 0:
        return math.nan if a == 0 or math.isnan(a) else math.copysign(math.inf, a * math.copysign(1., b))
    return a / b


def same_arithmetic(value, raw):
    actual = decode(raw)
    # NaN raw words remain exactly paired in seeded source/native comparison.
    # A host arithmetic check only establishes their nonfinite value class.
    return math.isnan(actual) if math.isnan(value) else struct.pack('>d' if len(raw) == 16 else '>f', value).hex() == raw


def heading_arithmetic(direction, speed, spread, gaussian, after):
    x, y, z = map(decode, direction); speed, spread = decode(speed), decode(spread)
    length = f32(math.sqrt((x * x + y * y) + z * z))
    values = [(divide(axis, length) + ((decode(draw) * .007499999832361937) * spread)) * speed
              for axis, draw in zip((x, y, z), gaussian)]
    need(all(same_arithmetic(value, raw) for value, raw in zip(values, after['motion'])), 'actual returned Gaussian locals produce reached velocity stores')
    x, y, z = values; horizontal = f32(math.sqrt(x * x + z * z))
    yaw = f32((math.atan2(x, z) * 180.) / 3.1415927410125732)
    pitch = f32((math.atan2(y, horizontal) * 180.) / 3.1415927410125732)
    need(all(same_arithmetic(value, raw) for value, raw in zip((yaw, pitch, yaw, pitch), after['angles'])) and
         after['arrow']['j'] == 0, 'actual heading angle and grounded-age stores')


def native_receipt(row, trace):
    n = row.get('native'); need(type(n) is dict and set(n) == {'site', 'heading_site', 'heading_entered', 'heading_returned',
        'gaussian_calls', 'random_captured', 'random_before', 'random_after', 'gaussian_values'}, 'closed native value receipt')
    p = row['input']; reached = p[1] == 3 or p[1] == 2 and row['returned']
    expected_calls = 3 if reached and row['returned'] else 0
    need(type(n['heading_entered']) is type(n['heading_returned']) is type(n['random_captured']) is bool and
         n['heading_entered'] == reached and n['heading_returned'] == (reached and row['returned']) and
         type(n['gaussian_calls']) is int and n['gaussian_calls'] == expected_calls and n['random_captured'] == bool(expected_calls), 'actual native reached heading/Gaussian scope')
    vector(n['gaussian_values'], 3, 16)
    for site in (n['site'], n['heading_site']):
        need(site is None or type(site) is dict and set(site) == {'class', 'method', 'descriptor', 'pc'} and
             all(type(site[k]) is str for k in ('class', 'method', 'descriptor')) and type(site['pc']) is int, 'typed native source site')
    need(n['site']['class'] == 'sl' and n['site']['pc'] == ((58, 72, 289, 198)[p[1]] if row['returned'] else 73 if p[1] == 2 else 43), 'actual native return or first failure site')
    if reached:
        need(n['heading_site']['class'] == 'sl' and n['heading_site']['method'] == 'a' and n['heading_site']['descriptor'] == '(DDDFF)V' and
             n['heading_site']['pc'] == (198 if row['returned'] else 43), 'separate actual heading site')
    if expected_calls:
        stream(n['random_before']); stream(n['random_after'])
        need(exact(n['random_after'], row['arrow_after']['random']), 'native receipt observes actual final Arrow stream')
        if p[1] == 3:
            need(exact(n['random_before'], row['arrow_before']['random']), 'standalone receipt observes actual input stream')
            direction, speed, spread = row['arguments']['direction'], row['arguments']['speed'], row['arguments']['spread']
        else:
            entries = [e['value'] for e in trace['events'] if e['value']['kind'] == 'ENTRY']
            heading = next(e for e in entries if e['site']['class'] == 'sl' and e['site']['method'] == 'a')
            direction = [a['D'] for a in heading['arguments'][:3]]
            speed, spread = heading['arguments'][3]['F'], heading['arguments'][4]['F']
        heading_arithmetic(direction, speed, spread, n['gaussian_values'], row['arrow_after'])


def gaussian_transition(before, after, returned):
    """Check actual cache branch and exact polar-rejection uniform consumption.

    Fresh branch transcendental outputs remain actual observations, not Python
    libm approximations to StrictMath. Seeded output bits compare across runs.
    """
    if before[1]:
        need(returned == before[2] and after == [before[0], False, before[2]], 'actual cached Gaussian branch')
        return
    state = int(before[0], 16)
    def uniform():
        nonlocal state
        state = (state * MULTIPLIER + 11) & MASK; a = state >> 22
        state = (state * MULTIPLIER + 11) & MASK; b = state >> 21
        return ((a << 27) + b) / float(1 << 53)
    for _ in range(4096):
        x, y = 2 * uniform() - 1, 2 * uniform() - 1
        square = x * x + y * y
        if 0 < square < 1:
            need(after[0] == f'{state:016x}' and after[1] is True, 'exact actual fresh Gaussian uniform/rejection state')
            return
    raise ValueError('bounded Gaussian rejection journal')


def selected_snapshot(value, entropy=False):
    if value is None:
        return None
    value = copy.deepcopy(value); del value['id']
    if entropy:
        del value['random']
    return value


def project(row, trace=None):
    """Explicit paired projection; raw observations are retained separately."""
    p = row['input']; actual = row['arrow_after']
    if actual is None and trace is not None:
        actual = trace['exception_arrow']
    need(actual is not None or p[0] in (14, 15), 'actual receiver evidence')
    before = selected_snapshot(row['arrow_before'])
    after = selected_snapshot(actual, p[1] != 3)
    if p[1] == 2 and row['returned']:
        # These are outputs of this instance's independent default Random.
        del after['motion']; del after['angles']
    result = {key: copy.deepcopy(row[key]) for key in ('case', 'input', 'arguments', 'returned', 'exception')}
    result.update(counter_increment=row['after']['counter'] - row['before']['counter'],
        shooter=selected_snapshot(row['before']['shooter'], True), arrow_before=before, arrow_after=after)
    return result


def compare(expected, actual):
    need(len(expected) == len(actual) == 40, 'complete paired inventory')
    for left, right in zip(expected, actual):
        need(exact(left, right), 'selected Arrow disagreement in case ' + str(left['case']))
