"""Explicit original readback projection and independent private-stream checks."""
import copy
import struct

MASK = (1 << 48) - 1
MULTIPLIER = 0x5DEECE66D
ENTITY_FIELDS = ('sn.aD', 'sn.aM', 'sn.aN', 'sn.aO', 'sn.aP', 'sn.aQ', 'sn.aR',
                 'sn.aJ', 'sn.aK', 'sn.aL', 'sn.bl', 'sn.bm', 'sn.bn',
                 'sn.aS', 'sn.aT', 'sn.aU', 'sn.aV', 'sn.bg', 'sn.bh', 'sn.bf',
                 'sn.bb', 'sn.be', 'ls.E', 'ls.Y', 'ls.Z', 'ls.by', 'ls.aa', 'ls.ab',
                 'ls.ac', 'ls.al', 'ls.au', 'ls.av', 'ls.ah', 'gs.bO', 'gs.u', 'gs.g')
# by is declared by SN, whereas several surrounding hurt fields belong to LS.
ENTITY_FIELDS = tuple('sn.by' if value == 'ls.by' else value for value in ENTITY_FIELDS)
CONSTRUCTOR_FIELDS = ('ls.F', 'ls.G', 'ls.aj')


def need(value, message):
    if not value:
        raise ValueError(message)


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def stream_checks(row):
    before, after = row['before'], row['after']
    need(before['world_random'] == after['world_random'] and
         before['math_random'] == after['math_random'], 'source call preserves World and shared Math streams')
    sounds = [event for event in after['events'] if event['event'] == 'sound']
    need(len(sounds) <= 1 and len(before['events']) == 0, 'bounded actual source sound')
    for entry, exit in zip(before['entities'], after['entities']):
        need(entry['id'] == exit['id'] and entry['class'] == exit['class'], 'retained actual Entity inventory')
        state = int(entry['random'][0], 16)
        need(0 <= state <= MASK and entry['random'][1:] == [False, '0000000000000000'], 'actual constructor private Random input')
        draws = []
        for _ in range(2 if entry['id'] == 0 and sounds else 0):
            state = (state * MULTIPLIER + 11) & MASK
            draws.append((state >> 24) / 16777216.0)
        need(exit['random'] == [f'{state:016x}', False, '0000000000000000'], 'exact independent private Random consumption')
        if draws:
            expected = struct.pack('>f', f32(f32(f32(draws[0] - draws[1]) * f32(.2)) + f32(1))).hex()
            need(sounds[0]['pitch'] == expected, 'actual hurt pitch from this run private Random')
        for field in CONSTRUCTOR_FIELDS:
            if field in entry['fields']:
                need(entry['fields'][field] == exit['fields'][field], 'unwritten constructor entropy field preserved')


def selected_state(raw):
    result = {key: copy.deepcopy(raw[key]) for key in ('actor_name', 'actor_world_null', 'actor_app_null',
        'actor_inventory_null', 'world_entities', 'world_players', 'chunks', 'statistics', 'box_pool_used', 'events')}
    result['entities'] = [{'role': entity['identity'], 'class': entity['class'], 'id': entity['id'],
                           'fields': {name: entity['fields'][name] for name in ENTITY_FIELDS if name in entity['fields']},
                           'bounds': entity['bounds'], 'watcher': entity['watcher'], 'target': entity['target']}
                          for entity in raw['entities']]
    for event in result['events']:
        if event['event'] == 'sound':
            del event['pitch']
    return result


def project(raw):
    stream_checks(raw)
    return {'case': raw['case'], 'input': raw['input'], 'result': raw['result'], 'exception': raw['exception'],
            'exception_message': raw['exception_message'], 'exception_frames': raw['exception_frames'],
            'before': selected_state(raw['before']), 'after': selected_state(raw['after'])}


def entropy(raw):
    return {'case': raw['case'], 'world_before': raw['before']['world_random'], 'world_after': raw['after']['world_random'],
            'math_before': raw['before']['math_random'], 'math_after': raw['after']['math_random'],
            'entities': [{'id': before['id'], 'class': before['class'], 'before': before['random'], 'after': after['random'],
                          'constructor_fields': {name: before['fields'][name] for name in CONSTRUCTOR_FIELDS if name in before['fields']}}
                         for before, after in zip(raw['before']['entities'], raw['after']['entities'])],
            'sounds': [event for event in raw['after']['events'] if event['event'] == 'sound']}
