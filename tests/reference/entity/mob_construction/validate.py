"""Validate actual constructor fields, read-only checkpoints and independent RNG replay."""
import hashlib
import json
from pathlib import Path
import re
import struct

MASK = (1 << 48) - 1
MULT = 0x5deece66d
KINDS = ['cn', 'uz', 'fr', 'gb', 'uw']


def need(ok, message):
    if not ok: raise ValueError(message)


def rows(path):
    need(path.stat().st_size < 2097152, 'bounded original output')
    return [json.loads(line) for line in path.read_text().splitlines()]


def advance(seed):
    return (seed * MULT + 11) & MASK


def doubles(seed):
    values = []
    states = [seed]
    for _ in range(4):
        first = advance(seed)
        seed = advance(first)
        values.append((((first >> 22) << 27) + (seed >> 21)) * 2.0 ** -53)
        states.append(seed)
    return values, states


def next_int(seed, bound):
    count = 0
    while True:
        seed = advance(seed)
        count += 1
        bits = seed >> 17
        if bound & (bound - 1) == 0: return seed, (bound * bits) >> 31, count
        value = bits % bound
        if bits - value + bound - 1 < 1 << 31: return seed, value, count


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def fhex(value):
    return struct.pack('>f', f32(value)).hex()


def canonical(value):
    if isinstance(value, dict):
        if 'class' in value: return {'class': value['class'], **({'string': value['string']} if 'string' in value else {})}
        return {key: canonical(item) for key, item in value.items()}
    if isinstance(value, list): return [canonical(item) for item in value]
    return value


def math_check(row):
    values, states = doubles(int(row['math_seed_before'], 16))
    need(row['draws'] == [struct.pack('>d', value).hex() for value in values], 'four actual Math oracle draws')
    need(int(row['math_seed_after'], 16) == states[-1], 'four actual Math consumption')
    expected = {'ls.aj': fhex(values[0] * .8999999761581421 + .10000000149011612),
                'ls.G': fhex(f32(values[1] + 1.) * f32(.01)),
                'ls.F': fhex(f32(values[2]) * f32(12398.)),
                'sn.aS': fhex(values[3] * 3.1415927410125732 * 2.)}
    for key, value in expected.items(): need(row['fields'][key] == {'F': value}, 'original Math formula ' + key)
    return states


def validate(directory, pins):
    directory = Path(directory)
    normal = rows(directory / 'normal/observations.ndjson')
    debug = rows(directory / 'debugger/target-observations.ndjson')
    checkpoints = rows(directory / 'debugger/checkpoints.ndjson')
    need(normal[0]['event'] == debug[0]['event'] == 'setup', 'real constructor setup')
    normal, debug = normal[1:], debug[1:]
    need(len(normal) == len(debug) == 15, 'all five constructors in three input passes')
    code = [row for row in checkpoints if row['event'] == 'code-pin']
    for pin in code:
        method = next(m for m in pins['classes'][pin['class']]['methods'] if m['name'] == pin['method'] and m['descriptor'] == pin['descriptor'])
        need((pin['bytes'], pin['sha256']) == (method['code_bytes'], method['code_sha256']), 'live unchanged JVM bytecode')
    need(len(code) == 13, 'all observed source methods pinned live')
    clocks = [row for row in checkpoints if row['event'] == 'clock']
    points = [row for row in checkpoints if row['event'] == 'checkpoint']
    entropy = []
    fields_checked = 0
    for index, (ordinary, observed) in enumerate(zip(normal, debug)):
        kind = KINDS[index % 5]
        for row in (ordinary, observed):
            need(row['case'] == index and row['kind'] == kind and row['pass'] == index // 5, 'case identity')
            need(row['same_world'] and row['null_world'] == (index >= 10), 'actual World constructor alias')
            need(row['entity_counter_after'] == row['entity_counter_before'] + 1, 'actual Entity ID consumption')
            math_check(row)
            need(row['private_random']['cached'] is False and row['private_random']['gaussian'] == '0000000000000000', 'untouched Gaussian state')
        case = [point for point in points if point['case'] == index]
        def point(owner, pc, method='<init>'):
            found = [p for p in case if p['location']['class'] == owner and p['location']['method'] == method and p['location']['pc'] == pc]
            need(len(found) == 1, f'one actual checkpoint {index} {owner}.{method}:{pc}')
            return found[0]
        before = int(point('sn', 133)['private_random_seed'], 16)
        need(point('sn', 231)['private_random_seed'] == f'{before:012x}', 'no private draws in base/watch hook')
        states = math_check(observed)
        for pc, state in zip((5, 108, 168, 195, 211, 217), (states[0], states[1], states[2], states[3], states[4], states[4])):
            need(point('ls', pc)['math_seed48'] == f'{state:012x}', 'read-only four Math poststore boundary')
        after, outcomes, counts = before, [], []
        if kind == 'uw':
            after, size_draw, count = next_int(after, 3)
            need(point('uw', 27)['private_random_seed'] == f'{after:012x}', 'actual Slime size draw state')
            outcomes.append(size_draw); counts.append(count)
            after, jump_draw, count = next_int(after, 20)
            need(point('uw', 48)['private_random_seed'] == f'{after:012x}', 'actual Slime jump draw state')
            outcomes.append(jump_draw); counts.append(count)
            size = 1 << size_draw
            need(observed['fields']['uw.c'] == jump_draw + 10 and observed['fields']['ls.Y'] == size * size, 'actual Slime random-derived constructor state')
            need(observed['fields']['sn.bg'] == observed['fields']['sn.bh'] == {'F': fhex(f32(.6) * size)}, 'actual Slime size formula')
            need(observed['watcher']['records'][-1]['value'] == size, 'actual Slime watcher update')
        need(observed['private_random']['seed48'] == f'{after:012x}', 'actual private Random consumption')
        case_clocks = [row for row in clocks if row['case'] == index]
        need([row['kind'] for row in case_clocks] == ['u', 'nano'], 'exact actual default-Random U/nano boundary pair')
        mixed = int(case_clocks[0]['value'], 16) ^ int(case_clocks[1]['value'], 16)
        need(((mixed ^ MULT) & MASK) == before, 'actual U/nano pair produces observed Random state')
        ignored = {'ls.aj', 'ls.G', 'ls.F', 'sn.aS', 'sn.aD'}
        if kind == 'uw': ignored |= {'uw.c', 'ls.Y', 'sn.bg', 'sn.bh'}
        for key, value in ordinary['fields'].items():
            if key not in ignored:
                need(canonical(value) == canonical(observed['fields'][key]), 'normal/JDI deterministic field ' + key)
                fields_checked += 1
        if kind != 'uw': need(ordinary['bounds'] == observed['bounds'], 'normal/JDI actual bounds')
        if kind != 'uw': need(ordinary['watcher'] == observed['watcher'], 'normal/JDI actual watcher')
        if kind == 'fr': need(ordinary['skeleton_stack'] == observed['skeleton_stack'] and ordinary['skeleton_stack_alias'] and observed['skeleton_stack_alias'], 'actual static Skeleton Stack fields/alias')
        if kind == 'fr': need(ordinary['skeleton_stack_same'] and observed['skeleton_stack_same'], 'same actual static Skeleton Stack across constructors')
        entropy.append({'case': index, 'kind': kind, 'u': case_clocks[0]['value'], 'nano': case_clocks[1]['value'], 'private_seed_before': f'{before:012x}', 'private_seed_after': f'{after:012x}', 'next_int_outcomes': outcomes, 'next_int_draw_counts': counts, 'math_draws': observed['draws']})
    for mode, path in [('normal', directory / 'normal/target.stdout'), ('debugger', directory / 'debugger/target.stdout')]:
        lines = path.read_text().splitlines()
        need([line for line in lines if line.startswith('CASE ')] == [f'CASE {i} {KINDS[i%5]}' for i in range(15)], 'all original completion receipts')
        bindings = [line.split() for line in lines if line.startswith('BIND ')]
        need(len(bindings) == len(pins['classes']), 'all source binding receipts')
        for _, name, size, digest in bindings:
            expected = pins['classes'][name.replace('.', '/')]
            need((int(size), digest) == (expected['class_bytes'], expected['class_sha256']), 'actual source origin/resource binding')
        log = directory / mode / ('class-init.log' if mode == 'normal' else 'target-class-init.log')
        initialized = set(re.findall(r"\bInitializing '([^']+)'", log.read_text()))
        original = initialized & set(pins['jar_class_names'])
        need(original == set(pins['original_initialized_classes']), 'exact observed original initialization closure')
        need(not {'px', 'net/minecraft/client/Minecraft', 'yo', 'vy'} & initialized and
             not any(name.startswith(('org/lwjgl/', 'paulscode/')) for name in initialized), 'device/client bootstrap remains excluded')
    (directory / 'observations.json').write_text(json.dumps({'normal_rows': normal, 'debugger_rows': debug, 'entropy_profiles': entropy, 'checkpoints': points, 'clock_rows': clocks}, indent=2) + '\n', newline='\n')
    return {'cases_per_vm': 15, 'constructors': KINDS, 'live_method_pins': len(code), 'checkpoints': len(points), 'normal_debugger_deterministic_fields': fields_checked, 'math_formula_fields': 120, 'private_profiles': len(entropy), 'clock_rows': len(clocks), 'original_initialized_classes': len(pins['original_initialized_classes']), 'observed_slime_sizes': {mode: sorted({row['watcher']['records'][-1]['value'] for row in source if row['kind'] == 'uw'}) for mode, source in [('normal', normal), ('debugger', debug)]}}
