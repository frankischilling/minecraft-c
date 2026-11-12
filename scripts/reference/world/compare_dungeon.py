#!/usr/bin/env python3
"""Compare one retained original Dungeon call with one native observation.

Inputs are actual capture files, never terrain or loot predictions. This checks
the selected acquired Chunk, its Tile graph, ordered World Tile/light queues and
five consuming Random continuation calls. It does not compare arbitrary World
graphs, constructors, fallback Chunks, persistence or natural population.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

MAX_BYTES = 64 * 1024 * 1024
ARRAYS = {'blocks_hex': ('blocks', 32768), 'metadata_hex': ('e', 16384),
          'skylight_hex': ('f', 16384), 'block_light_hex': ('g', 16384),
          'heightmap_hex': ('heightmap', 256)}


class Failure(ValueError):
    """Incomplete, malformed or unsupported observation."""


def require(value, message):
    if not value:
        raise Failure(message)


def pairs(rows):
    result = {}
    for key, value in rows:
        require(key not in result, 'duplicate case-sensitive JSON key: ' + key)
        result[key] = value
    return result


def read(path):
    with path.open('rb') as stream:
        raw = stream.read(MAX_BYTES + 1)
    require(0 < len(raw) <= MAX_BYTES, 'complete bounded input: ' + str(path))
    value = json.loads(raw.decode('utf-8-sig'), object_pairs_hook=pairs,
                       parse_constant=lambda value: (_ for _ in ()).throw(Failure(value)))
    require(isinstance(value, dict), 'one JSON object')
    return value, {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def hex_value(value, length):
    require(isinstance(value, str) and len(value) == length * 2 and
            re.fullmatch('[0-9a-f]*', value) is not None, 'complete lowercase byte hex')
    return value


def scalar(value, kind):
    require(isinstance(value, dict) and value.get('type') == kind, 'typed scalar ' + kind)
    return value['raw_bits'] if kind in ('double', 'float', 'long') else value['value']


def fields(value, owner):
    rows = [row for row in value['fields'] if row['class'] == owner]
    require(len(rows) == len({row['name'] for row in rows}), 'unique declared fields')
    return {row['name']: row['value'] for row in rows}


def position_text(position):
    require(len(position) == 3 and all(type(value) is int for value in position),
            'three integral coordinates')
    return ','.join(map(str, position))


def alias(value, identities):
    require(value is not None, 'live identity')
    if value not in identities:
        identities[value] = len(identities)
    return identities[value]


def original_image(context, side):
    tile_base, chest, spawner, stack = ('ow', 'js', 'cy', 'iz') if side == 'client' else (
        'jh', 'gk', 'bx', 'fy')
    world = context['established_world_context']['world']
    # Both the ordinary loaded Chunk and fallback can have coordinates(0,0).
    entries = world['map']['entries']
    require(len(entries) == 1 and entries[0]['key'] == 0, 'single acquired prepared Chunk')
    reference = entries[0]['value']
    require(reference != world['fallback'], 'acquired ordinary Chunk is not fallback')
    selected = [chunk for chunk in context['complete_cache_chunks']
                if chunk['object']['ref'] == reference]
    require(len(selected) == 1, 'actual cache-held Chunk identity selects one full image')
    chunk = selected[0]
    require(chunk['coordinates'] == [0, 0] and chunk['world'] == world['world'],
            'acquired Chunk coordinates and actual World binding')
    result = {'minimum_height': chunk['minimum_height'], 'flags': chunk['flags'],
              'last_save_time_hex': chunk['last_save_time_hex'],
              'world_time_hex': context['established_world_context']['world_time'],
              'world_binding': True}
    world_fields = fields(context['World_instance_fields'], 'fd' if side == 'client' else 'dj')
    result['World_flags'] = {name: scalar(world_fields[name], 'boolean') for name in ('a', 'o', 'L')}
    for output, (name, length) in ARRAYS.items():
        blob = chunk['data'][name]
        if name in ('e', 'f', 'g'):
            blob = blob['bytes']
        require(blob['length'] == length, 'complete original Chunk array')
        result[output] = hex_value(blob['values'], length)
    tile_positions, tile_payloads, stack_ids = {}, {}, {}
    result['Tiles'], result['Tile_Map'] = {}, []
    tile_map = chunk['Tile_Map']
    require(tile_map['size'] == len(tile_map['entries']), 'complete actual Tile Map')
    for entry in tile_map['entries']:
        tile = entry['tile']
        source_class = tile['object']['class']
        require(source_class in (chest, spawner), 'known concrete Dungeon Tile')
        base = fields(tile, tile_base)
        require(set(base) == {'d', 'e', 'f', 'g', 'h'}, 'complete Tile base fields')
        position = [scalar(base[name], 'Integer') for name in ('e', 'f', 'g')]
        key = position_text(position)
        key_fields = {row['name']: row['value'] for row in entry['key']['fields']}
        require([scalar(key_fields[name], 'Integer') for name in ('a', 'b', 'c')] == position,
                'actual Map key equals held Tile coordinates')
        require(key not in result['Tiles'], 'unique Tile position')
        tile_positions[tile['object']['ref']] = key
        tile_payloads[tile['object']['ref']] = tile
        result['Tile_Map'].append(key)  # Keep actual HashMap iteration order.
        record = {'kind': 'chest' if source_class == chest else 'spawner',
                  'position': position, 'invalid': scalar(base['h'], 'boolean'),
                  'world_binding': base['d'] == world['world']}
        subtype = fields(tile, source_class)
        if source_class == chest:
            require(set(subtype) == {'a'}, 'complete Chest fields')
            slots = subtype['a']
            require(slots['length'] == len(slots['items']) == 36, 'all physical fresh Chest slots')
            record['chest_capacity'], record['slots'] = slots['length'], []
            for value in slots['items']:
                if value is None:
                    record['slots'].append(None)
                    continue
                require(value['object']['class'] == stack, 'actual original Stack')
                source = fields(value, stack)
                require(set(source) == {'a', 'b', 'c', 'd'}, 'complete four-int Stack')
                record['slots'].append({'alias': alias(value['object']['ref'], stack_ids),
                    'id': scalar(source['c'], 'Integer'), 'count': scalar(source['a'], 'Integer'),
                    'damage': scalar(source['d'], 'Integer'), 'animation': scalar(source['b'], 'Integer')})
        else:
            require(set(subtype) == {'a', 'b', 'c', 'i'}, 'complete Spawner fields')
            name = subtype['i']
            require(name is None or name['type'] == 'String', 'nullable original String')
            record.update(spawner_name_utf16=None if name is None else name['utf16'],
                          spawner_delay=scalar(subtype['a'], 'Integer'),
                          rotation_bits=scalar(subtype['b'], 'double'),
                          previous_rotation_bits=scalar(subtype['c'], 'double'))
        result['Tiles'][key] = record
    result['World_Tile_lists'] = {}
    for name in ('c', 'G'):
        source = context['World_lists_by_actual_field'][name]
        require(source['size'] == len(source['entries']), 'complete World Tile List')
        require(all(value == tile_payloads.get(value['object']['ref']) for value in source['entries']),
                'same Tile identity has identical Map/List payloads in this prepared image')
        result['World_Tile_lists'][name] = [tile_positions[value['object']['ref']]
                                          for value in source['entries']]
    result['Entity_slices'] = []
    require(len(chunk['Entity_slices']) == 8, 'all eight Entity slices')
    for source in chunk['Entity_slices']:
        require(source['size'] == len(source['entries']) == 0, 'prepared empty Entity slice')
        result['Entity_slices'].append(0)
    result['Light_queue'] = [{'kind': row['kind']['ordinal'], 'default': row['kind']['default'],
                              'bounds': row['bounds']} for row in world['queue']['entries']]
    result['enqueue_depth'], result['drain_depth'] = world['enqueue_depth'], world['drain_depth']
    result['chunk_static_a'] = world['chunk_static_a']
    result['scheduled'] = {}
    for name in ('E', 'F'):
        source = context['World_scheduled_by_actual_field'][name]
        require(source['size'] == len(source['native_iteration']) == 0, 'prepared empty scheduling')
        result['scheduled'][name] = 0
    return result


def native_image(image):
    require(image['coordinates'] == [0, 0] and image['Tile_Map_valid'] is True,
            'actual native ordinary Chunk and valid Map')
    result = {key: image[key] for key in ('minimum_height', 'flags', 'last_save_time_hex',
              'world_binding', 'World_flags', 'Entity_slices', 'enqueue_depth', 'drain_depth', 'chunk_static_a')}
    require(image['World_Tile_lists']['L'] is image['World_flags']['L'],
            'same native deferred-list flag cell')
    result['world_time_hex'] = f"{image['world_time'] & ((1 << 64) - 1):016x}"
    result['Light_queue'] = [{'kind': row['kind']['ordinal'], 'default': row['kind']['default'],
                              'bounds': row['bounds']} for row in image['Light_queue']]
    scheduled = image['World_scheduling']
    require(scheduled['E_count'] == len(scheduled['E']) == 0 and
            scheduled['F_count'] == len(scheduled['F']) == 0, 'complete native empty scheduling')
    result['scheduled'] = {'E': scheduled['E_count'], 'F': scheduled['F_count']}
    for name, (_, length) in ARRAYS.items():
        result[name] = hex_value(image[name], length)
    positions, stack_ids = {}, {}
    result['Tiles'], result['Tile_Map'], result['World_Tile_lists'] = {}, [], {}
    for tile in image['Tiles']:
        key = position_text(tile['position'])
        require(key not in result['Tiles'], 'unique native Tile position')
        positions[tile['tile_id']] = key
        require(tile['phase'] == 1, 'actual READY native Tile')
        record = {name: tile[name] for name in ('kind', 'position', 'invalid', 'world_binding')}
        if tile['kind'] == 'chest':
            require(tile['chest_capacity'] == len(tile['slots']) == 36, 'all physical native Chest slots')
            record['chest_capacity'], record['slots'] = tile['chest_capacity'], []
            for value in tile['slots']:
                if value is None:
                    record['slots'].append(None)
                else:
                    record['slots'].append({'alias': alias(value['stack_id'], stack_ids),
                        **{name: value[name] for name in ('id', 'count', 'damage', 'animation')}})
        else:
            require(tile['kind'] == 'spawner', 'concrete native Dungeon Tile')
            record.update({name: tile[name] for name in ('spawner_name_utf16', 'spawner_delay',
                                                         'rotation_bits', 'previous_rotation_bits')})
        result['Tiles'][key] = record
    for row in image['Tile_Map']:
        key = position_text(row['position'])
        require(positions[row['tile_id']] == key, 'native Map/Tile identity join')
        require(row['key'] == row['position'], 'actual native key equals Tile position in chunk0,0')
        result['Tile_Map'].append(key)
    for name in ('c', 'G'):
        result['World_Tile_lists'][name] = [positions[value] for value in image['World_Tile_lists'][name]]
    require(image['World_Tile_lists']['valid'], 'actual native map/list invariants')
    return result


def original_identities(value):
    images = []
    for stage in ('before_placement', 'after_placement'):
        context = value[stage]
        world = context['established_world_context']['world']
        reference = world['map']['entries'][0]['value']
        chunk = next(chunk for chunk in context['complete_cache_chunks']
                     if chunk['object']['ref'] == reference)
        data = chunk['data']
        wrappers = [data[name]['wrapper']['ref'] for name in ('e', 'f', 'g')]
        backings = [data['blocks_reference']['ref'], data['heightmap_reference']['ref'],
                    *[data[name]['backing']['ref'] for name in ('e', 'f', 'g')]]
        require(len(set(wrappers)) == 3 and len(set(backings)) == 5,
                'distinct original Chunk child owners')
        images.append((chunk['object'], chunk['world'], wrappers, backings))
    require(images[0] == images[1], 'same held original Chunk/World/arrays before and after')


def original(value):
    require(value['terminal'] == 'captured_actual_return; comparison external', 'complete actual original return')
    require(not any(name in value for name in ('fatal', 'close_failure', 'thread_capture_failure')),
            'original capture has no setup/close/thread failure')
    case, side = value['selected_case'], value['side']
    require(side in ('client', 'server') and value['placement']['status'] == 'returned', 'actual side and return')
    require(type(value['placement']['boolean']) is bool, 'actual Boolean return')
    original_identities(value)
    operations = value['continuation']['operations']
    require([(row['method'], row['signature']) for row in operations] == [
        ('nextGaussian', '()D'), ('nextInt', '(I)I'), ('nextInt', '()I'),
        ('nextLong', '()J'), ('nextDouble', '()D')], 'exact consuming continuation schedule')
    require(all(row['outcome']['status'] == 'returned' and
                row['receiver'] == value['supplied_Random']['identity'] for row in operations),
            'same actual supplied Random and complete continuation')
    draws = [row['outcome']['value'] for row in operations]
    priming = value['priming']['operation']
    require(priming['receiver'] == value['supplied_Random']['identity'] and
            priming['class'] == 'java.util.Random' and priming['method'] == 'nextGaussian' and
            priming['signature'] == '()D' and priming['outcome']['status'] == 'returned',
            'actual same-Random Gaussian priming')
    require(operations[1]['bound'] == 11, 'actual continuation bound11 input')
    calls = [row for row in value['operations'] if row['label'] == 'actual_notifying_whole_Dungeon_place']
    owner, world_class = ('er', 'fd') if side == 'client' else ('da', 'dj')
    require(len(calls) == 1, 'one actual whole-Dungeon call')
    call = calls[0]
    world = value['before_placement']['established_world_context']['world']['world']
    require(call['class'] == owner and call['method'] == 'a' and
            call['signature'] == '(L' + world_class + ';Ljava/util/Random;III)Z' and
            call['receiver'] == value['feature_before']['object'] == value['feature_after']['object'] and
            call['arguments'] == [world, value['supplied_Random']['identity'],
                {'type': 'Integer', 'value': 8}, {'type': 'Integer', 'value': 64},
                {'type': 'Integer', 'value': 8}] and
            call['outcome'] == {'status': 'returned', 'value': {'type': 'boolean', 'value': value['placement']['boolean']}},
            'actual source entry, receiver, inputs and returned outcome')
    return {'side': side, 'case_index': case['index'], 'seed_hex': case['seed_hex'],
            'radius_x': case['radius_x'], 'radius_z': case['radius_z'],
            'openings': case['openings'], 'reject': case['reject'],
            'priming_gaussian_bits': scalar(priming['outcome']['value'], 'double'),
            'placement': value['placement']['boolean'],
            'before': original_image(value['before_placement'], side),
            'after': original_image(value['after_placement'], side),
            'continuation': {'gaussian_bits': scalar(draws[0], 'double'),
                'bounded11': scalar(draws[1], 'Integer'), 'integer': scalar(draws[2], 'Integer'),
                'long_bits': scalar(draws[3], 'long'), 'double_bits': scalar(draws[4], 'double')}}


def native(value):
    require(value['schema'] == 'dungeon-placement-native-a' and value['cleanup_passed'] is True,
            'complete native observation with checked cleanup')
    require(value['placement']['status'] == 0, 'normal native return rather than native failure')
    require(type(value['placement']['boolean']) is bool, 'native returned Boolean')
    expected_identities = {'chunk_same', 'map_descriptor_same', 'nibble_wrappers_same',
        'blocks_backing_same', 'heightmap_backing_same', 'World_c_descriptor_same',
        'World_G_descriptor_same', 'repeated_Map_value_same', 'repeated_Stack_get_same'}
    require(set(value['identity_checks']) == expected_identities and
            all(result is True for result in value['identity_checks'].values()),
            'native within-process identity checks')
    result = {name: value[name] for name in ('side', 'case_index', 'seed_hex', 'radius_x',
              'radius_z', 'openings', 'reject', 'priming_gaussian_bits', 'continuation')}
    result.update(placement=value['placement']['boolean'], before=native_image(value['before']),
                  after=native_image(value['after']))
    return result


def differences(expected, actual, path=''):
    if type(expected) is not type(actual):
        return [{'path': path, 'reason': 'different JSON types'}]
    if isinstance(expected, dict):
        if set(expected) != set(actual):
            return [{'path': path, 'reason': 'different fields',
                     'original': sorted(expected), 'native': sorted(actual)}]
        return [row for key in expected for row in differences(expected[key], actual[key], path + '/' + key)]
    if isinstance(expected, list):
        if len(expected) != len(actual):
            return [{'path': path, 'reason': 'different lengths', 'original': len(expected), 'native': len(actual)}]
        return [row for index, (left, right) in enumerate(zip(expected, actual))
                for row in differences(left, right, path + '/' + str(index))]
    if expected == actual:
        return []
    if isinstance(expected, str) and len(expected) > 256:
        return [{'path': path, 'reason': 'different complete bytes',
                 'first_different_hex_character': next((i for i, (a, b) in enumerate(zip(expected, actual)) if a != b), min(len(expected), len(actual))),
                 'original_sha256': hashlib.sha256(expected.encode('ascii')).hexdigest(),
                 'native_sha256': hashlib.sha256(actual.encode('ascii')).hexdigest()}]
    return [{'path': path, 'reason': 'different values', 'original': expected, 'native': actual}]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'fresh comparison output')
    report = {'status': 'failed', 'scope': __doc__.strip(),
              'limits': 'Selected acquired Chunk and exact declared projection only. Local identity tokens are joined within each capture, then compared as position/slot alias graphs. Actual Map, Tile List and light queue order is preserved. No full World, fallback, process-tree, cold-context, save/reload or natural population claim.'}
    try:
        left, report['original_input'] = read(args.original)
        right, report['native_input'] = read(args.native)
        expected, actual = original(left), native(right)
        report.update(side=expected['side'], case_index=expected['case_index'], seed_hex=expected['seed_hex'])
        report['differences'] = differences(expected, actual)
        if not report['differences']:
            report['status'] = 'passed'
    except (Failure, KeyError, TypeError, ValueError, OSError) as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'status': report['status'], 'differences': len(report.get('differences', [])),
                      'output': str(args.output)}))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
