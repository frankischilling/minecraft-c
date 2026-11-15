"""Actual Arrow parent with real Player, application, Inventory and statistic owners."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import tempfile


FAMILY_MODES = ('wake-bed-sapling', 'wake-bed-sapling-source', 'wake-bed-sapling-removal-source',
                'wake-bed-tall-grass', 'wake-bed-tall-grass-source',
                'wake-bed-tall-grass-removal-source', 'wake-bed-tall-grass-no-item')
REMAINING_MODES = ('wake-bed-dead-bush', 'wake-bed-dead-bush-removal-source',
                   'wake-bed-crops', 'wake-bed-crops-source', 'wake-bed-crops-removal-source',
                   'wake-bed-crops-max-items', 'wake-bed-mushroom', 'wake-bed-mushroom-source',
                   'wake-bed-mushroom-removal-source', 'wake-bed-mushroom-high-light',
                   'wake-bed-cactus', 'wake-bed-cactus-source', 'wake-bed-cactus-removal-source',
                   'wake-bed-reeds', 'wake-bed-reeds-source', 'wake-bed-reeds-removal-source')


def remaining_spec(mode):
    if mode not in REMAINING_MODES:
        raise ValueError('Declared remaining plant mode required')
    dead = mode.startswith('wake-bed-dead-bush')
    crops = mode.startswith('wake-bed-crops')
    cactus = mode.startswith('wake-bed-cactus')
    reeds = mode.startswith('wake-bed-reeds')
    high = mode == 'wake-bed-mushroom-high-light'
    source = mode.endswith('-source') and not mode.endswith('-removal-source')
    removal = mode.endswith('-removal-source')
    normal = not source and not removal
    seed = 0x7fffffff if mode == 'wake-bed-crops-max-items' else 1
    state, draws = seed ^ 0x5deece66d, 0
    items = []

    def bits(width):
        nonlocal state, draws
        state = (state * 0x5deece66d + 11) & ((1 << 48) - 1)
        draws += 1
        return state >> (48 - width)

    def f32(value):
        return struct.unpack('>f', struct.pack('>f', value))[0]

    def item(item_id, crop_offset=False):
        position = []
        spread = f32(.7)
        edge = f32(1.0 - spread)
        for coordinate in (7, 64, 8):
            scaled = f32(bits(24) / (1 << 24) * spread)
            if crop_offset:
                offset = f32(scaled + f32(edge * .5))
                value = f32(coordinate + offset)
            else:
                value = coordinate + (scaled + edge * .5)
            position.append(struct.pack('>d', value).hex())
        items.append({'id': str(item_id), 'count': '1', 'damage': '0',
                      **dict(zip(('x', 'y', 'z'), position))})

    bits(24)  # Inherited chance float precedes actual Item selection, even for -1.
    if not dead:
        item(296 if crops else 81 if cactus else 338 if reeds else 39)
    if crops:
        for _ in range(3):
            while True:
                draw = bits(31)
                selected = draw % 15
                if draw - selected + 14 < 1 << 31:
                    break
            if selected <= 7:
                item(295, crop_offset=True)
                if source and len(items) == 2:
                    break
    count = 0 if dead else 4 if mode == 'wake-bed-crops-max-items' else 2 if crops else 1
    if len(items) != count or draws != (1 if dead else 16 if count == 4 else 9 if crops and source else 10 if crops else 4):
        raise ValueError('Declared finite remaining plant LCG outcome')
    return {'plant': '32:0' if dead else '59:7' if crops else '81:0' if cactus else '83:0' if reeds else '39:0',
            'floor': '12:0' if cactus else '2:0' if dead or high else '0:0',
            'sky': '13' if high else '15' if dead or crops or cactus or reeds else '12',
            'normal': normal, 'removed': normal or removal, 'seed': seed,
            'initial': seed ^ 0x5deece66d, 'state': state, 'draws': draws, 'items': items,
            'phases': ['returned'] if normal else ['diagnostic', 'capture', 'hurt-close', 'hurt-cleanup', 'quiescent']}


def remaining_receipts(lines, mode):
    s = remaining_spec(mode)
    prefix = 'ARROW_REMAINING_PLANTS_'

    def fields(line):
        words = line.split()
        result = {}
        for word in words[1:]:
            if '=' not in word:
                raise ValueError('Malformed remaining plant receipt')
            key, value = word.split('=', 1)
            if key in result:
                raise ValueError('Duplicate remaining plant receipt field')
            result[key] = value
        return words[0].removeprefix(prefix), result

    actual = [fields(line) for line in lines if line.startswith(prefix)]
    expected = []
    speed = struct.unpack('>f', struct.pack('>f', .6))[0]
    expected.append(('INPUT', {'plant': s['plant'], 'floor': s['floor'], 'sky': s['sky'],
        'block': '0', 'height': '64', 'world-f': '5', 'sleeping': '1', 'timer': '37',
        'arrow-x': struct.pack('>d', 9.4).hex(), 'arrow-y': struct.pack('>d', 64.85).hex(),
        'arrow-vx': struct.pack('>d', -speed).hex()}))
    if len(actual) < 2 or actual[1][0] != 'RANDOM_INPUT':
        raise ValueError('Actual complete remaining plant Random input required')
    random_input = actual[1][1]
    if (set(random_input) != {'seed-input', 'world-seed', 'world-cache', 'world-gaussian'} or
            random_input['seed-input'] != str(s['seed']) or
            random_input['world-seed'] != f"{s['initial']:016x}" or random_input['world-cache'] != '0' or
            re.fullmatch('[0-9a-f]{16}', random_input['world-gaussian']) is None):
        raise ValueError('Declared remaining plant Random seed and complete cache')
    expected.append(('RANDOM_INPUT', random_input))
    cell = {'index': '0', 'xyz': '8,64,8', 'head': '26:8', 'plant': s['plant'],
            'additions': '0', 'sleeping': '1', 'timer': '37', 'health': '20'}
    expected.append(('CELL', cell))
    for index, item in enumerate(s['items']):
        expected.append(('PUBLICATION', {'index': str(index), 'cells': '1', 'head': '26:8',
            'plant': s['plant'], 'sleeping': '1', 'timer': '37', 'health': '20', **item}))
    if s['removed']:
        expected.append(('CELL', {**cell, 'index': '1', 'xyz': '7,64,8', 'plant': '0:0',
                                 'additions': str(len(s['items']))}))
    prefixes = [value for kind, value in actual if kind == 'PREFIX']
    if len(prefixes) != len(s['phases']):
        raise ValueError('Exact remaining plant unwind phase count required')
    for index, phase in enumerate(s['phases']):
        for number, item in enumerate(s['items']):
            expected.append(('ITEM', {'phase': phase, 'index': str(number), **item,
                                     'inserted': '1', 'removed': '0', 'delay': '10'}))
        row = {'phase': phase, 'cells': '2' if s['removed'] else '1',
            'additions': str(len(s['items'])), 'plant': '0' if s['removed'] else s['plant'].split(':')[0],
            'metadata': '0' if s['removed'] else s['plant'].split(':')[1],
            'sleeping': '0' if s['normal'] else '1', 'timer': '0' if s['normal'] else '37',
            'health': '16' if s['normal'] else '20', 'J': '0' if s['normal'] else '1',
            'pending': '1' if not s['normal'] and index < 3 else '0'}
        for stream in ('world', 'player', 'arrow'):
            for field in ('seed', 'cache', 'gaussian'):
                key = stream + '-' + field
                if key not in prefixes[index]:
                    raise ValueError('Complete remaining plant Random fields required')
                value = prefixes[index][key]
                if ((field == 'cache' and value not in ('0', '1')) or
                        (field != 'cache' and re.fullmatch('[0-9a-f]{16}', value) is None) or
                        (field == 'seed' and int(value, 16) >= 1 << 48)):
                    raise ValueError('Typed remaining plant Random field')
                row[key] = value
        if (row['world-seed'] != f"{s['state']:016x}" or row['world-cache'] != '0' or
                row['world-gaussian'] != random_input['world-gaussian'] or
                index and any(row[k] != prefixes[0][k] for k in row if k not in ('phase', 'pending'))):
            raise ValueError('Remaining plant state or full Random replay during unwind')
        expected.append(('PREFIX', row))
    if actual != expected:
        raise ValueError('Exact ordered remaining plant publication and retained Item prefix required')
    return {'receipts': actual, 'model_draws': s['draws'], 'original_comparison': False}


def plant_receipts(lines, mode):
    family = mode in FAMILY_MODES
    sapling = mode.startswith('wake-bed-sapling')
    no_item = mode == 'wake-bed-tall-grass-no-item'
    plant_id, metadata = (6, 2) if sapling else (31, 1) if family else (37, 0)
    key = 'plant' if family else 'flower'
    prefix = 'ARROW_PLANT_FAMILY_' if family else 'ARROW_FLOWER_'
    def fields(line, prefix):
        result = {}
        for word in line.removeprefix(prefix).split():
            if '=' not in word:
                raise ValueError('Malformed plant receipt field')
            key, value = word.split('=', 1)
            if key in result:
                raise ValueError('Duplicate plant receipt field')
            result[key] = value
        return result
    rows = [line for line in lines if line.startswith(prefix + 'INPUT ')]
    speed = struct.unpack('>f', struct.pack('>f', .6))[0]
    expected = {key: f'{plant_id}:{metadata}', 'floor': '0:0', 'sky': '15', 'block': '0', 'height': '64',
                'world-f': '5', 'sleeping': '1', 'timer': '37',
                'arrow-x': struct.pack('>d', 9.4).hex(), 'arrow-y': struct.pack('>d', 64.85).hex(),
                'arrow-vx': struct.pack('>d', -speed).hex()}
    inputs = fields(rows[0], prefix + 'INPUT ') if len(rows) == 1 else None
    if inputs != expected:
        raise ValueError('Exact real plant/light/sleep/Arrow inputs required')
    rows = [fields(line, prefix + 'PREFIX ') for line in lines if line.startswith(prefix + 'PREFIX ')]
    normal = mode in ('wake-bed-flower', 'wake-bed-sapling', 'wake-bed-tall-grass') or no_item
    phases = ['returned'] if normal else ['diagnostic', 'capture', 'hurt-close', 'hurt-cleanup', 'quiescent']
    if len(rows) != len(phases) or [row.get('phase') for row in rows] != phases:
        raise ValueError('Exact five checked native unwind phases or normal return required')
    removed = normal or mode.endswith('-removal-source')
    random_input = None
    items = []
    if family:
        records = [fields(line, prefix + 'RANDOM_INPUT ') for line in lines
                   if line.startswith(prefix + 'RANDOM_INPUT ')]
        if len(records) != 1:
            raise ValueError('One actual World Random input required')
        random_input = records[0]
        seed = 0x7fffffff if no_item else 1
        state = seed ^ 0x5deece66d
        if (set(random_input) != {'seed-input', 'world-seed', 'world-cache', 'world-gaussian'} or
                random_input['seed-input'] != str(seed) or random_input['world-seed'] != f'{state:016x}' or
                random_input['world-cache'] != '0' or
                re.fullmatch('[0-9a-f]{16}', random_input['world-gaussian']) is None):
            raise ValueError('Actual declared Random reseed and complete cache required')
        for _ in range(4 if sapling else 2 if no_item else 5):
            state = (state * 0x5deece66d + 11) & ((1 << 48) - 1)
        items = [fields(line, prefix + 'ITEM ') for line in lines if line.startswith(prefix + 'ITEM ')]
        if len(items) != len(phases):
            raise ValueError('One actual Item or no-Item receipt per phase required')
    for index, row in enumerate(rows):
        expected = {'phase': phases[index], 'cells': '2' if removed else '1', 'additions': '0' if no_item else '1',
                    key: '0' if removed else str(plant_id), 'metadata': '0' if removed else str(metadata), 'sleeping': '0' if normal else '1',
                    'timer': '0' if normal else '37', 'health': '16' if normal else '20',
                    'J': '0' if normal else '1', 'pending': '1' if not normal and index < 3 else '0'}
        random_keys = {stream + '-' + name for stream in ('world', 'player', 'arrow')
                       for name in ('seed', 'cache', 'gaussian')}
        if set(row) != set(expected) | random_keys or any(row[key] != value for key, value in expected.items()):
            raise ValueError('Committed plant/Player/Item prefix changed during unwind')
        for stream in ('world', 'player', 'arrow'):
            if (not re.fullmatch('[0-9a-f]{16}', row[stream + '-seed']) or
                    int(row[stream + '-seed'], 16) >= 1 << 48 or row[stream + '-cache'] not in ('0', '1') or
                    not re.fullmatch('[0-9a-f]{16}', row[stream + '-gaussian'])):
                raise ValueError('Complete actual Random fields required')
        if family:
            if (row['world-seed'] != f'{state:016x}' or row['world-cache'] != '0' or
                    row['world-gaussian'] != random_input['world-gaussian']):
                raise ValueError('Declared drop Random progress or cache differs')
            expected_item = {'phase': phases[index], 'id': '-1' if no_item else '6' if sapling else '295',
                             'count': '0' if no_item else '1', 'damage': '2' if sapling else '0'}
            if items[index] != expected_item:
                raise ValueError('Actual Sapling metadata, Seed or no-Item result differs')
        if index and any(row[key] != rows[0][key] for key in row if key not in ('phase', 'pending')):
            raise ValueError('Source callback, prefix or Random replay during native unwind')
    return {'inputs': inputs, 'random_input': random_input, 'items': items,
            'prefixes': rows, 'original_comparison': False}


def flower_receipts(lines, mode):
    return plant_receipts(lines, mode)


def plant_parser_controls():
    def record(prefix, values):
        return prefix + ' '.join(key + '=' + value for key, value in values.items())
    def specimen(mode):
        sapling = mode.startswith('wake-bed-sapling')
        no_item = mode.endswith('-no-item')
        normal = mode in ('wake-bed-sapling', 'wake-bed-tall-grass') or no_item
        removed = normal or mode.endswith('-removal-source')
        phases = ['returned'] if normal else ['diagnostic', 'capture', 'hurt-close', 'hurt-cleanup', 'quiescent']
        seed = 0x7fffffff if no_item else 1
        state = seed ^ 0x5deece66d
        speed = struct.unpack('>f', struct.pack('>f', .6))[0]
        lines = [record('ARROW_PLANT_FAMILY_INPUT ', {
            'plant': '6:2' if sapling else '31:1', 'floor': '0:0', 'sky': '15', 'block': '0',
            'height': '64', 'world-f': '5', 'sleeping': '1', 'timer': '37',
            'arrow-x': struct.pack('>d', 9.4).hex(), 'arrow-y': struct.pack('>d', 64.85).hex(),
            'arrow-vx': struct.pack('>d', -speed).hex()}),
            record('ARROW_PLANT_FAMILY_RANDOM_INPUT ', {'seed-input': str(seed), 'world-seed': f'{state:016x}',
                'world-cache': '0', 'world-gaussian': '3ff0000000000000'})]
        for _ in range(4 if sapling else 2 if no_item else 5):
            state = (state * 0x5deece66d + 11) & ((1 << 48) - 1)
        for index, phase in enumerate(phases):
            lines.append(record('ARROW_PLANT_FAMILY_ITEM ', {'phase': phase,
                'id': '-1' if no_item else '6' if sapling else '295', 'count': '0' if no_item else '1',
                'damage': '2' if sapling else '0'}))
            row = {'phase': phase, 'cells': '2' if removed else '1', 'additions': '0' if no_item else '1',
                'plant': '0' if removed else '6' if sapling else '31',
                'metadata': '0' if removed else '2' if sapling else '1',
                'sleeping': '0' if normal else '1', 'timer': '0' if normal else '37',
                'health': '16' if normal else '20', 'J': '0' if normal else '1',
                'pending': '1' if not normal and index < 3 else '0'}
            for stream in ('world', 'player', 'arrow'):
                row.update({stream + '-seed': f'{state:016x}' if stream == 'world' else '0000000000000001',
                            stream + '-cache': '0', stream + '-gaussian': '3ff0000000000000'})
            lines.append(record('ARROW_PLANT_FAMILY_PREFIX ', row))
        return lines
    for mode in FAMILY_MODES:
        plant_receipts(specimen(mode), mode)
    mutations = (
        ('wake-bed-sapling', 0, 'plant=6:2', 'plant=6:0'),
        ('wake-bed-sapling', 1, 'seed-input=1', 'seed-input=2'),
        ('wake-bed-sapling', 1, 'world-cache=0', 'world-cache=1'),
        ('wake-bed-sapling', 2, 'damage=2', 'damage=0'),
        ('wake-bed-sapling', 3, 'cells=2', 'cells=1'),
        ('wake-bed-sapling-source', 3, 'metadata=2', 'metadata=0'),
        ('wake-bed-sapling-source', 5, 'world-cache=0', 'world-cache=1'),
        ('wake-bed-sapling-source', 9, 'pending=0', 'pending=1'),
        ('wake-bed-sapling-removal-source', 3, 'plant=0', 'plant=6'),
        ('wake-bed-tall-grass', 2, 'id=295', 'id=31'),
        ('wake-bed-tall-grass', 3, 'world-seed=', 'world-seed=0'),
        ('wake-bed-tall-grass-source', 5, 'arrow-seed=0000000000000001', 'arrow-seed=0000000000000002'),
        ('wake-bed-tall-grass-removal-source', 3, 'world-gaussian=3ff0000000000000', 'world-gaussian=0000000000000000'),
        ('wake-bed-tall-grass-no-item', 2, 'id=-1', 'id=295'),
        ('wake-bed-tall-grass-no-item', 3, 'additions=0', 'additions=1'),
        ('wake-bed-tall-grass-no-item', 3, 'health=16', 'health=20'),
    )
    refused = 0
    for mode, index, before, after in mutations:
        lines = specimen(mode)
        if before not in lines[index]:
            raise ValueError('Invalid authored receipt mutation')
        lines[index] = lines[index].replace(before, after)
        try:
            plant_receipts(lines, mode)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupt Arrow plant receipt accepted')
    for mode in FAMILY_MODES:
        for mutation in ('duplicate-input', 'duplicate-field', 'missing-phase'):
            lines = specimen(mode)
            if mutation == 'duplicate-input':
                lines.append(lines[0])
            elif mutation == 'duplicate-field':
                lines[0] += ' sky=15'
            else:
                lines.pop()
            try:
                plant_receipts(lines, mode)
            except ValueError:
                refused += 1
            else:
                raise ValueError('Malformed Arrow plant receipt accepted')
    if refused != 37:
        raise ValueError('Every malformed Arrow plant receipt must be refused')
    print('PASS: seven synthetic Arrow plant modes; 37 malformed receipts refused; no native process')


def remaining_parser_controls():
    prefix = 'ARROW_REMAINING_PLANTS_'

    def specimen(mode):
        s = remaining_spec(mode)
        speed = struct.unpack('>f', struct.pack('>f', .6))[0]
        rows = [('INPUT', {'plant': s['plant'], 'floor': s['floor'], 'sky': s['sky'],
            'block': '0', 'height': '64', 'world-f': '5', 'sleeping': '1', 'timer': '37',
            'arrow-x': struct.pack('>d', 9.4).hex(), 'arrow-y': struct.pack('>d', 64.85).hex(),
            'arrow-vx': struct.pack('>d', -speed).hex()}),
            ('RANDOM_INPUT', {'seed-input': str(s['seed']), 'world-seed': f"{s['initial']:016x}",
                             'world-cache': '0', 'world-gaussian': '3ff0000000000000'}),
            ('CELL', {'index': '0', 'xyz': '8,64,8', 'head': '26:8', 'plant': s['plant'],
                      'additions': '0', 'sleeping': '1', 'timer': '37', 'health': '20'})]
        for index, item in enumerate(s['items']):
            rows.append(('PUBLICATION', {'index': str(index), 'cells': '1', 'head': '26:8',
                'plant': s['plant'], 'sleeping': '1', 'timer': '37', 'health': '20', **item}))
        if s['removed']:
            rows.append(('CELL', {'index': '1', 'xyz': '7,64,8', 'head': '26:8', 'plant': '0:0',
                'additions': str(len(s['items'])), 'sleeping': '1', 'timer': '37', 'health': '20'}))
        for index, phase in enumerate(s['phases']):
            for number, item in enumerate(s['items']):
                rows.append(('ITEM', {'phase': phase, 'index': str(number), **item,
                                     'inserted': '1', 'removed': '0', 'delay': '10'}))
            row = {'phase': phase, 'cells': '2' if s['removed'] else '1',
                'additions': str(len(s['items'])), 'plant': '0' if s['removed'] else s['plant'].split(':')[0],
                'metadata': '0' if s['removed'] else s['plant'].split(':')[1],
                'sleeping': '0' if s['normal'] else '1', 'timer': '0' if s['normal'] else '37',
                'health': '16' if s['normal'] else '20', 'J': '0' if s['normal'] else '1',
                'pending': '1' if not s['normal'] and index < 3 else '0'}
            for stream in ('world', 'player', 'arrow'):
                row.update({stream + '-seed': f"{s['state']:016x}" if stream == 'world' else '0000000000000001',
                            stream + '-cache': '0', stream + '-gaussian': '3ff0000000000000'})
            rows.append(('PREFIX', row))
        return rows

    def render(rows):
        return [prefix + kind + ' ' + ' '.join(k + '=' + v for k, v in values.items())
                for kind, values in rows]

    for mode in REMAINING_MODES:
        remaining_receipts(render(specimen(mode)), mode)
    changes = (
        ('wake-bed-dead-bush', 'INPUT', 0, 'floor', '0:0'),
        ('wake-bed-dead-bush', 'RANDOM_INPUT', 0, 'world-cache', '1'),
        ('wake-bed-dead-bush', 'PREFIX', 0, 'additions', '1'),
        ('wake-bed-dead-bush-removal-source', 'CELL', 1, 'additions', '1'),
        ('wake-bed-dead-bush-removal-source', 'PREFIX', 3, 'pending', '1'),
        ('wake-bed-crops', 'INPUT', 0, 'plant', '59:6'),
        ('wake-bed-crops', 'PUBLICATION', 0, 'id', '295'),
        ('wake-bed-crops', 'PUBLICATION', 1, 'index', '0'),
        ('wake-bed-crops', 'PUBLICATION', 1, 'x', '0000000000000000'),
        ('wake-bed-crops', 'PREFIX', 0, 'world-seed', '0000000000000001'),
        ('wake-bed-crops-source', 'PUBLICATION', 1, 'cells', '2'),
        ('wake-bed-crops-source', 'PREFIX', 0, 'health', '16'),
        ('wake-bed-crops-source', 'PREFIX', 1, 'player-cache', '1'),
        ('wake-bed-crops-source', 'PREFIX', 2, 'arrow-gaussian', '0000000000000000'),
        ('wake-bed-crops-source', 'ITEM', 3, 'id', '296'),
        ('wake-bed-crops-source', 'ITEM', 7, 'inserted', '0'),
        ('wake-bed-crops-removal-source', 'CELL', 1, 'plant', '59:7'),
        ('wake-bed-crops-max-items', 'PUBLICATION', 3, 'count', '0'),
        ('wake-bed-crops-max-items', 'ITEM', 2, 'x', '0000000000000000'),
        ('wake-bed-crops-max-items', 'PREFIX', 0, 'additions', '2'),
        ('wake-bed-mushroom', 'INPUT', 0, 'sky', '13'),
        ('wake-bed-mushroom-source', 'PUBLICATION', 0, 'plant', '0:0'),
        ('wake-bed-mushroom-removal-source', 'PREFIX', 4, 'world-gaussian', '0000000000000000'),
        ('wake-bed-mushroom-high-light', 'INPUT', 0, 'floor', '0:0'),
        ('wake-bed-cactus', 'INPUT', 0, 'floor', '0:0'),
        ('wake-bed-cactus-source', 'PUBLICATION', 0, 'id', '338'),
        ('wake-bed-cactus-removal-source', 'PREFIX', 4, 'sleeping', '0'),
        ('wake-bed-reeds', 'PUBLICATION', 0, 'id', '83'),
        ('wake-bed-reeds-source', 'PREFIX', 2, 'plant', '0'),
        ('wake-bed-reeds-removal-source', 'PREFIX', 4, 'world-seed', '0000000000000001'),
    )
    refused = 0
    for mode, kind, occurrence, key, value in changes:
        rows = specimen(mode)
        selected = [row for row in rows if row[0] == kind][occurrence][1]
        if key not in selected or selected[key] == value:
            raise ValueError('Invalid authored remaining plant corruption')
        selected[key] = value
        try:
            remaining_receipts(render(rows), mode)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Corrupt remaining Arrow plant receipt accepted')
    for mode in REMAINING_MODES:
        for mutation in ('duplicate-input', 'duplicate-field', 'missing-phase', 'unknown-kind', 'extra-field'):
            lines = render(specimen(mode))
            if mutation == 'duplicate-input':
                lines.append(lines[0])
            elif mutation == 'duplicate-field':
                lines[0] += ' sky=15'
            elif mutation == 'missing-phase':
                lines.pop()
            elif mutation == 'unknown-kind':
                lines.append(prefix + 'UNKNOWN value=0')
            else:
                lines[0] += ' extra=0'
            try:
                remaining_receipts(lines, mode)
            except ValueError:
                refused += 1
            else:
                raise ValueError('Malformed remaining Arrow plant receipt accepted')
    if refused != len(changes) + 5 * len(REMAINING_MODES):
        raise ValueError('Every malformed remaining Arrow plant receipt must be refused')
    print(f'PASS: {len(REMAINING_MODES)} synthetic remaining Arrow plant modes; '
          f'{refused} malformed receipts refused; no native process')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=('live', 'fatal', 'fatal-source', 'wake',
                                          'wake-source-null-list', 'wake-bed-foot',
                                          'wake-bed-foot-source', 'wake-bed-ladder',
                                          'wake-bed-ladder-source', 'wake-bed-ladder-removal-source', 'wake-bed-torch', 'wake-bed-torch-source', 'wake-bed-torch-removal-source', 'wake-bed-flower',
                                          'wake-bed-flower-source', 'wake-bed-flower-removal-source') + FAMILY_MODES + REMAINING_MODES)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        plant_parser_controls()
        remaining_parser_controls()
        return
    if args.mode is None or args.executable is None or args.work_dir is None:
        parser.error('--mode, --executable and --work-dir are required for native runs')
    repo = Path(__file__).resolve().parents[3]

    def load(name, relative):
        spec = importlib.util.spec_from_file_location(name, repo / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    support = load('arrow_player_capture', 'tests/reference/client/statistics/support.py')
    resources = load('arrow_player_resources', 'tests/reference/client/player_statistics/support.py')
    executable = args.executable.resolve(strict=True)
    before = hashlib.sha256(executable.read_bytes()).hexdigest()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='arrow-player-', dir=args.work_dir)).resolve()
    resource_root, game = out / 'resources', out / 'game'
    resources.resources_at(resource_root)
    game.mkdir()
    child = support.run_bounded(
        [str(executable), args.mode, str(resource_root), str(game)], out, 'native',
        timeout=240, limit=1024**2, env=os.environ.copy())
    lines = child.stdout.decode('utf-8', errors='strict').splitlines()
    footer = (re.fullmatch(r'PASS: ([1-9][0-9]*) actual Arrow Player hurt checks;mode=' +
                           re.escape(args.mode), lines[-1]) if lines else None)
    report = {'mode': args.mode, 'exit': child.returncode, 'capture': child.capture_report,
              'stderr_empty': child.stderr == b'',
              'executable_unchanged': hashlib.sha256(executable.read_bytes()).hexdigest() == before,
              'scope': 'Actual Arrow finite Player hit, fatal work and wake controls'}
    report['passed'] = bool(child.returncode == 0 and footer and not child.stderr and
                            report['executable_unchanged'])
    if args.mode.startswith('wake-bed-flower'):
        try:
            report['flower'] = flower_receipts(lines, args.mode)
        except ValueError as failure:
            report['passed'] = False
            report['flower_failure'] = str(failure)
    if args.mode in FAMILY_MODES:
        try:
            report['plant_family'] = plant_receipts(lines, args.mode)
        except ValueError as failure:
            report['passed'] = False
            report['plant_family_failure'] = str(failure)
    if args.mode in REMAINING_MODES:
        try:
            report['remaining_plants'] = remaining_receipts(lines, args.mode)
        except ValueError as failure:
            report['passed'] = False
            report['remaining_plants_failure'] = str(failure)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(child.stdout.decode('utf-8', errors='strict'), end='')
    if child.stderr:
        print(child.stderr.decode('utf-8', errors='strict'), end='')
    if not report['passed']:
        raise RuntimeError('Arrow Player hurt control failed: ' + str(out))


if __name__ == '__main__':
    main()
