"""Typed observations and independent per-VM constructor/Math validation."""
import copy
import hashlib
import json
from pathlib import Path
import re
import struct

from capture import ensure

MULT = 0x5deece66d
MASK = (1 << 48) - 1
UMULT = 1181783497276652981


def strict_equal(a, b):
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(strict_equal(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(strict_equal(x, y) for x, y in zip(a, b))
    return a == b


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        ensure(key not in result, 'duplicate metadata key: ' + key)
        result[key] = value
    return result


def word(value, length=16):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', value)


def vector(value, length, width):
    return type(value) is list and len(value) == length and all(word(v, width) for v in value)


def random_word(value):
    return word(value) and int(value, 16) <= MASK


def item_shape(item):
    keys = {'stack', 'position', 'previous_position', 'velocity', 'rotation', 'size', 'bounds', 'body',
            'flags', 'chunk', 'private_random', 'world', 'world_member', 'chunk_members', 'stack_unique'}
    ensure(type(item) is dict and set(item) == keys, 'actual Item schema')
    for key, size in (('stack', 4), ('body', 7), ('chunk', 3)):
        ensure(type(item[key]) is list and len(item[key]) == size and all(type(x) is int for x in item[key]), 'actual Item integer fields')
    for key, size, width in (('position', 3, 16), ('previous_position', 3, 16), ('velocity', 3, 16),
                             ('rotation', 5, 8), ('size', 3, 8), ('bounds', 6, 16)):
        ensure(vector(item[key], size, width), 'actual Item primitive words: ' + key)
    ensure(type(item['flags']) is list and len(item['flags']) == 10 and all(type(x) is bool for x in item['flags']), 'actual Item flags')
    ensure(random_word(item['private_random']) and item['world'] == 'PRIMARY' and item['world_member'] is True and
           type(item['chunk_members']) is int and item['chunk_members'] == 1 and item['stack_unique'] is True, 'actual Item memberships/Stack identity')


def state_shape(state):
    keys = {'health', 'animation', 'velocity', 'attacked', 'dying', 'dead', 'position', 'yaw', 'bounds',
            'random', 'math_random', 'world_random', 'source_random', 'source_cooldown', 'source_unchanged',
            'world', 'watcher', 'sounds', 'items', 'world_loaded', 'chunk_count', 'events'}
    ensure(type(state) is dict and set(state) == keys, 'animal state schema')
    ensure(type(state['health']) is list and len(state['health']) == 10 and all(type(v) is int for v in state['health']), 'animal health/timer fields')
    for key, count, width in (('animation', 2, 8), ('velocity', 3, 16), ('position', 3, 16), ('bounds', 6, 16)):
        ensure(vector(state[key], count, width), 'animal primitive words: ' + key)
    ensure(word(state['yaw'], 8), 'animal yaw')
    for key in ('attacked', 'dying', 'dead', 'source_unchanged'):
        ensure(type(state[key]) is bool, 'animal boolean field')
    ensure(state['source_unchanged'] and type(state['source_cooldown']) is int, 'actual source owner state')
    for key in ('random', 'math_random', 'world_random', 'source_random'):
        ensure(state[key] is None and key == 'random' or random_word(state[key]), 'actual Random field: ' + key)
    ensure(state['world'] in (None, 'PRIMARY') and (state['watcher'] is None or type(state['watcher']) is int), 'animal World/watcher value')
    ensure(type(state['items']) is list and len(state['items']) <= 2, 'finite actual Item list')
    for item in state['items']:
        item_shape(item)
    for key in ('world_loaded', 'chunk_count'):
        ensure(type(state[key]) is int and state[key] == len(state['items']), 'actual World/Chunk membership counts')
    ensure(type(state['sounds']) is list and len(state['sounds']) <= 1, 'finite sound list')
    for sound in state['sounds']:
        ensure(type(sound) is dict and set(sound) == {'name', 'position', 'volume', 'pitch'} and type(sound['name']) is str and
               vector(sound['position'], 3, 16) and word(sound['volume'], 8) and word(sound['pitch'], 8), 'actual sound arguments')
    ensure(type(state['events']) is list and len(state['events']) == len(state['items']) + len(state['sounds']), 'ordered actual callback count')
    for event in state['events']:
        ensure(type(event) is dict and event.get('kind') in ('SOUND', 'ADD') and type(event.get('index')) is int, 'actual callback event')
        if event['kind'] == 'ADD':
            ensure(set(event) == {'kind', 'index', 'world_member', 'chunk_members'} and event['world_member'] is True and
                   type(event['chunk_members']) is int and event['chunk_members'] == 1, 'actual publication callback after memberships')
        else:
            ensure(set(event) == {'kind', 'index'}, 'actual sound callback event')


def rows(text, profiles, native=False):
    found = [json.loads(line[len('ANIMAL_DEATH '):], object_pairs_hook=unique_object)
             for line in text.splitlines() if line.startswith('ANIMAL_DEATH ')]
    inventory = [(i, step, p) for i, p in enumerate(profiles) for step in range(p[9])]
    ensure(len(found) == len(inventory) == 43, 'exact animal death row count')
    keys = {'case', 'profile', 'step', 'input', 'result', 'exception', 'scope', 'math_before', 'uniquifier_before',
            'uniquifier_after', 'constructor', 'state'}
    for index, (row, (profile, step, values)) in enumerate(zip(found, inventory)):
        ensure(type(row) is dict and set(row) == keys and type(row['case']) is int and row['case'] == index and
               type(row['profile']) is int and row['profile'] == profile and type(row['step']) is int and row['step'] == step and
               strict_equal(row['input'], values), 'animal row identity/input schema')
        exception = ('IO' if native else 'CALLBACK') if profile in (33, 34) else 'NPE' if profile in (30, 35, 38, 39, 41) else 'NONE'
        ensure(row['exception'] == exception, 'actual death outcome category')
        ensure(row['scope'] == ('FULL' if exception == 'NONE' else 'SOURCE_FAILURE' if exception == 'NPE' else 'PUBLICATION_CALLBACK'),
               'explicit animal comparison scope')
        ensure(row['result'] is None if exception != 'NONE' or values[1] != 0 else type(row['result']) is bool, 'actual death result type')
        ensure(random_word(row['math_before']) and word(row['uniquifier_before']) and word(row['uniquifier_after']), 'actual entropy context')
        context = row['constructor']
        ensure(type(context) is dict and set(context) == {'actor_private_random', 'source_private_random', 'math_random'} and
               all(random_word(value) for value in context.values()), 'independent actual actor constructor context')
        state_shape(row['state'])
    return found


def next_bits(seed, bits):
    seed = (seed * MULT + 11) & MASK
    return seed, seed >> (48 - bits)


def next_double(seed):
    seed, a = next_bits(seed, 26)
    seed, b = next_bits(seed, 27)
    return seed, ((a << 27) + b) / (1 << 53)


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def fword(value):
    return struct.pack('>f', value).hex()


def dword(value):
    return struct.pack('>d', value).hex()


def audit_entropy(values):
    previous = {}
    total_items = total_math = 0
    for row in values:
        profile, state = row['profile'], row['state']
        old = previous.get(profile, [])
        ensure(state['items'][:len(old)] == old, 'repeated death preserves earlier actual Items')
        new = state['items'][len(old):]
        seed = int(row['math_before'], 16)
        if row['input'][1] == 0 and row['input'][7] == 1:
            seed, draw = next_double(seed)
            ensure(state['animation'][0] == fword(float(int(draw * 2) * 180)), 'actual NULL-source Math direction relation')
            total_math += 1
        for item in new:
            draws = []
            for _ in range(4):
                seed, draw = next_double(seed)
                draws.append(draw)
            ensure(item['rotation'][4] == fword(draws[0] * 3.141592653589793 * 2) and
                   item['rotation'][0] == fword(draws[1] * 360), 'actual Item hover/yaw Math relation')
            ensure(item['velocity'] == [dword(f32(draws[2] * float(f32(.2)) - float(f32(.1)))),
                                        dword(float(f32(.2))), dword(f32(draws[3] * float(f32(.2)) - float(f32(.1))))],
                   'actual Item constructor velocity Math relation')
            total_math += 4
        ensure(state['math_random'] == f'{seed:016x}', 'complete actual shared Math progression')
        u = int(row['uniquifier_before'], 16)
        for _ in new:
            u = (u * UMULT) & ((1 << 64) - 1)
        ensure(row['uniquifier_after'] == f'{u:016x}', 'one actual default Random occurrence per new Item')
        previous[profile] = copy.deepcopy(state['items'])
        total_items += len(new)
    return {'new_items': total_items, 'Math_nextDouble_calls': total_math,
            'default_Random_occurrences': total_items, 'profiles': len(previous), 'calls': len(values)}


def projection(row):
    result = copy.deepcopy(row)
    for key in ('math_before', 'uniquifier_before', 'uniquifier_after', 'constructor'):
        result.pop(key)
    if row['profile'] in (33, 34):
        result['exception'] = 'PUBLICATION_CALLBACK_BOUNDARY'
    state = result['state']
    state.pop('math_random')
    if row['input'][1] == 0 and row['input'][7] == 1:
        state['animation'][0] = 'INDEPENDENT_NULL_SOURCE_DIRECTION'
    for item in state['items']:
        item.pop('private_random')
        item['rotation'][0] = item['rotation'][4] = 'INDEPENDENT_CONSTRUCTOR_MATH'
        item['velocity'] = ['INDEPENDENT_CONSTRUCTOR_MATH', item['velocity'][1], 'INDEPENDENT_CONSTRUCTOR_MATH']
    return result


def audit_jdi(records, observed, archive, inspect_class):
    ensure(len(records) < 200000 and [r['sequence'] for r in records] == list(range(len(records))), 'ordered bounded JDI journal')
    cache = {}
    starts = {r['case']: r for r in records if r['event'] == 'active-entry'}
    ends = {r['case']: r for r in records if r['event'] == 'active-return'}
    ensure(set(starts) == set(ends) == set(range(43)), 'actual complete JDI active lifetimes')
    known = {};last_u = last_nano = None;defaults = {};items = [];pins = 0;draws = 0
    for record in records:
        event = record['event']
        if event == 'code-pin':
            name = record['class']
            if name not in cache:
                cache[name] = inspect_class(archive.read(name + '.class'))
            method = next(m for m in cache[name]['methods'] if (m['name'], m['descriptor']) == (record['method'], record['descriptor']))
            ensure((method['code_bytes'], method['code_sha256']) == (record['bytes'], record['sha256']), 'actual unchanged live original Code')
            pins += 1
        elif event == 'active-entry':
            ref = record['actor_random']
            if ref is not None:
                known[ref['oid']] = int(record['actor_seed'], 16)
        elif event == 'clock':
            if record['kind'] == 'seedUniquifier':
                last_u = int(record['value'], 16);last_nano = None
            else:
                last_nano = int(record['value'], 16)
        elif event == 'default-random':
            ensure(last_u is not None and last_nano is not None and record['seed'] == f'{(last_u ^ last_nano ^ MULT) & MASK:016x}', 'actual default Random U/nano/seed relation')
            ensure(any(f['class'] == 'hl' and f['method'] == '<init>' for f in record['source_frames']), 'actual Item default constructor source frames')
            defaults[record['oid']] = record['seed'];known[record['oid']] = int(record['seed'], 16);last_u = last_nano = None
        elif event == 'random':
            oid = record['oid'];case = record['case']
            if record['role'] == 'math' and oid not in known:
                known[oid] = int(starts[case]['math_seed'], 16)
            if record['role'] == 'math' and not any(r['event'] == 'random' and r['case'] == case and r['role'] == 'math' and r['sequence'] < record['sequence'] for r in records):
                known[oid] = int(starts[case]['math_seed'], 16)
            ensure(oid in known, 'actual Random initial state available')
            seed = known[oid];name = record['method']
            if name == 'nextDouble':
                seed, value = next_double(seed);expected = {'D': dword(value)}
            elif name == 'nextFloat':
                seed, bits = next_bits(seed, 24);expected = {'F': fword(bits / (1 << 24))}
            else:
                bound = record['arguments'][0]
                while True:
                    seed, bits = next_bits(seed, 31);value = (bound * bits) >> 31 if bound & (bound - 1) == 0 else bits % bound
                    if bound & (bound - 1) == 0 or bits - value + bound - 1 < 1 << 31:
                        break
                expected = value
            ensure(strict_equal(record['result'], expected) and record['seed'] == f'{seed:016x}', 'actual Random arguments/result/poststate independently replayed')
            known[oid] = seed;draws += 1
        elif event == 'item-constructor-return':
            ensure(record['private_seed'] == defaults[record['private_random']['oid']] and record['fields']['hl.c'] == 0 and
                   record['fields']['sn.bF'] is False and any(f['class'] == 'sn' and f['method'] == 'a' and f['descriptor'] == '(Liz;F)Lhl;' for f in record['source_frames']), 'actual Item constructor/private Random before delay/publication')
            items.append(record)
    audit = audit_entropy(observed)
    ensure(len(defaults) == len(items) == audit['new_items'], 'complete actual Item/default constructor census')
    ensure(sum(r['event'] == 'random' and r['role'] == 'math' for r in records) == audit['Math_nextDouble_calls'],
           'complete actual Math nextDouble return census')
    for row in observed:
        ensure(starts[row['case']]['math_seed'] == row['math_before'] and ends[row['case']]['math_seed'] == row['state']['math_random'], 'actual suspended Math snapshots agree with target')
    return {**audit, 'live_Code_pins': pins, 'original_class_bodies_reached': len(cache), 'Random_returns': draws,
            'original_entries': sum(r['event'] == 'entry' for r in records),
            'exception_records': sum(r['event'] == 'exception' for r in records)}
