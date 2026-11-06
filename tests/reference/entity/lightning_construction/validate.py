"""Validate unchanged C/YU/SN prefixes and independently observed actual entropy."""
import json
from pathlib import Path
import re

MASK = (1 << 48) - 1
MULT = 0x5deece66d


def need(condition, message):
    if not condition:
        raise ValueError(message)


def rows(path):
    need(path.stat().st_size < 2097152, 'bounded original observations')
    return [json.loads(line) for line in path.read_text().splitlines()]


def advance(seed):
    return (seed * MULT + 11) & MASK


def next_int3(seed):
    count = 0
    while True:
        seed = advance(seed)
        count += 1
        bits = seed >> 17
        value = bits % 3
        if bits - value + 2 < 1 << 31:
            return seed, value, count


def canonical(value):
    if isinstance(value, dict):
        if 'class' in value:
            return {'class': value['class']}
        return {key: canonical(item) for key, item in value.items()}
    if isinstance(value, list):
        return [canonical(item) for item in value]
    return value


def validate(directory, pins):
    directory = Path(directory)
    normal = rows(directory / 'normal/observations.ndjson')
    debug = rows(directory / 'debugger/target-observations.ndjson')
    need(normal[0]['event'] == debug[0]['event'] == 'setup', 'genuine World setup')
    normal, debug = normal[1:], debug[1:]
    need(len(normal) == len(debug) == 5, 'five actual constructor cases per VM')
    observed = rows(directory / 'debugger/checkpoints.ndjson')
    code = [row for row in observed if row['event'] == 'code-pin']
    points = [row for row in observed if row['event'] == 'checkpoint']
    clocks = [row for row in observed if row['event'] == 'clock']
    exceptions = [row for row in observed if row['event'] == 'source-exception']
    need(len(code) == 7 and len(points) == 84 and len(clocks) == 10, 'exact source method/checkpoint/clock counts')
    for pin in code:
        method = next(m for m in pins['classes'][pin['class']]['methods']
                      if m['name'] == pin['method'] and m['descriptor'] == pin['descriptor'])
        need((pin['bytes'], pin['sha256']) == (method['code_bytes'], method['code_sha256']), 'unchanged live Code')
    need(exceptions == [{'event': 'source-exception', 'case': 4, 'exception': 'java.lang.NullPointerException',
                         'class': 'c', 'method': '<init>', 'descriptor': '(Lfd;DDD)V', 'pc': 52}],
         'actual original NULL World exception at C52')
    profiles = []
    fields_checked = 0
    for index, (ordinary, target) in enumerate(zip(normal, debug)):
        for row in (ordinary, target):
            need(row['case'] == index and row['difficulty'] == [0, 1, 2, 2, 2][index], 'case identity')
            need(row['null_world'] == (index == 4) and row['exception'] ==
                 ('java.lang.NullPointerException' if index == 4 else None), 'actual source result')
            need(row['math_seed_before'] == row['math_seed_after'], 'no constructor Math draws')
            need(row['entity_counter_after'] == row['entity_counter_before'] + 1, 'one actual Entity ID')
            if index == 4:
                need(row['fields'] is row['bounds'] is row['watcher'] is row['private_random'] is None,
                     'ordinary failed object is not fabricated or recovered')
        case = [p for p in points if p['case'] == index]
        need(len(case) == [15, 15, 20, 20, 14][index], 'exact reached case checkpoints')

        def point(owner, pc, method='<init>'):
            found = [p for p in case if p['location']['class'] == owner and
                     p['location']['method'] == method and p['location']['pc'] == pc]
            need(len(found) == 1, f'one source checkpoint {index} {owner}.{method}:{pc}')
            return found[0]

        before = int(point('sn', 133)['private_random_seed'], 16)
        need(point('sn', 231)['private_random_seed'] == f'{before:012x}', 'SN and early hook consume no own draws')
        first = advance(before)
        after_long = advance(first)
        low = after_long >> 16
        signed_low = low if low < 1 << 31 else low - (1 << 32)
        word = (((first >> 16) << 32) + signed_low) & ((1 << 64) - 1)
        after, repeat, count = next_int3(after_long)
        need(point('c', 37)['private_random_seed'] == f'{after_long:012x}' and
             point('c', 37)['fields']['c.a'] == {'J': f'{word:016x}'}, 'actual nextLong signed-low arithmetic/store')
        need(point('c', 51)['private_random_seed'] == f'{after:012x}', 'actual nextInt3 poststate')
        final = point('c', 52) if index == 4 else point('c', 262)
        need(final['fields']['c.a'] == {'J': f'{word:016x}'} and
             final['fields']['c.b'] == 2 and final['fields']['c.c'] == repeat + 1,
             'actual completed own-field prefix')
        need(final['watcher'] == {'size': 1, 'dirty': False, 'records':
                                  [{'index': 0, 'type': 0, 'value': 0, 'dirty': True}]}, 'actual empty C watcher hook')
        need(all(p['math_seed48'] == target['math_seed_before'] for p in case), 'all read-only no-Math boundaries')
        need(point('c', 0, 'b')['fields']['c.b'] == point('c', 0, 'b')['fields']['c.c'] == 0,
             'early hook precedes C own initialization')
        pair = [row for row in clocks if row['case'] == index]
        need([row['kind'] for row in pair] == ['u', 'nano'], 'one actual default-Random U/nano pair')
        need(((int(pair[0]['value'], 16) ^ int(pair[1]['value'], 16) ^ MULT) & MASK) == before,
             'observed actual clock pair produces observed SN Random seed')
        if index != 4:
            need(target['private_random']['seed48'] == f'{after:012x}', 'no later own Random draws on skipped fire branch')
            need(ordinary['watcher'] == target['watcher'] and ordinary['bounds'] == target['bounds'],
                 'independent-VM deterministic watcher/bounds')
            for key, value in ordinary['fields'].items():
                if key not in {'c.a', 'c.c', 'sn.aD'}:
                    need(canonical(value) == canonical(target['fields'][key]), 'independent-VM deterministic field ' + key)
                    fields_checked += 1
        profiles.append({'case': index, 'u': pair[0]['value'], 'nano': pair[1]['value'],
                         'private_seed_before': f'{before:012x}', 'private_seed_after_long': f'{after_long:012x}',
                         'private_seed_after': f'{after:012x}', 'next_long_word': f'{word:016x}',
                         'next_int3': repeat, 'next_int3_draw_count': count,
                         'math_seed': target['math_seed_before']})
    for mode in ('normal', 'debugger'):
        lines = (directory / mode / 'target.stdout').read_text().splitlines()
        need([line for line in lines if line.startswith('CASE ')] == [f'CASE {i}' for i in range(5)], 'ordinary target completion receipts')
        bindings = [line.split() for line in lines if line.startswith('BIND ')]
        need(len(bindings) == len(pins['classes']), 'complete source origin/resource binding')
        for _, name, size, digest in bindings:
            pin = pins['classes'][name]
            need((int(size), digest) == (pin['class_bytes'], pin['class_sha256']), 'authentic supplied class resource')
        log = directory / mode / ('class-init.log' if mode == 'normal' else 'target-class-init.log')
        initialized = set(re.findall(r"\bInitializing '([^']+)'", log.read_text()))
        need(initialized & set(pins['jar_class_names']) == set(pins['original_initialized_classes']), 'exact initialized original closure')
        need(not {'px', 'net/minecraft/client/Minecraft', 'yo', 'vy'} & initialized and
             not any(n.startswith(('org/lwjgl/', 'paulscode/')) for n in initialized), 'no device/client bootstrap')
    (directory / 'observations.json').write_text(json.dumps({'normal_rows': normal, 'debugger_rows': debug,
        'entropy_profiles': profiles, 'checkpoints': points, 'clock_rows': clocks,
        'source_exceptions': exceptions}, indent=2) + '\n', newline='\n')
    return {'cases_per_vm': 5, 'successful_per_vm': 4, 'source_failures_per_vm': 1,
            'live_method_pins': len(code), 'checkpoints': len(points), 'clock_rows': len(clocks),
            'private_profiles': len(profiles), 'deterministic_fields': fields_checked,
            'original_initialized_classes': len(pins['classes']), 'constructor_math_draws': 0,
            'actual_null_exception_pc': 52}
