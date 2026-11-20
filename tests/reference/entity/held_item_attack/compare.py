"""Exact typed selected fields from genuine original held-Item attacks."""
import argparse
import json
from pathlib import Path
import re

STRINGS = ('profile', 'mode', 'item-class', 'attack-owner', 'hit-owner', 'use-owner', 'target-kind')
BOOLEANS = ('selected-same', 'selected-cleared', 'inventory-changed', 'dying')
INTEGERS = ('id-before', 'count-before', 'damage-before', 'animation-before', 'id-after',
    'count-after', 'damage-after', 'animation-after', 'health', 'previous-health', 'hurt',
    'hurt-time', 'max-hurt', 'last-damage', 'idle', 'score', 'mob-kills', 'damage-dealt',
    'used-before', 'used-after', 'drops', 'max-damage', 'attack-power', 'saddled-before',
    'saddled-after', 'broken-before', 'broken-after', 'sound-events', 'particle-events', 'break-sounds')
FIELDS = {*STRINGS, *BOOLEANS, *INTEGERS}


def unique(pairs):
    result = dict(pairs)
    if len(result) != len(pairs):
        raise ValueError('Duplicate held Item JSON field')
    return result


def validate(values):
    if type(values) is not list or not values:
        raise ValueError('Nonempty ordered held Item rows')
    seen = set()
    for row in values:
        if (type(row) is not dict or set(row) != FIELDS or
            any(type(row[key]) is not str or not row[key] for key in STRINGS) or
            any(type(row[key]) is not bool for key in BOOLEANS) or
            any(type(row[key]) is not int or not -(1 << 31) <= row[key] < (1 << 31)
                for key in INTEGERS) or row['profile'] in seen):
            raise ValueError('Unique exact typed held Item schema')
        seen.add(row['profile'])
    return values


def rows(text):
    return validate([json.loads(line.removeprefix('HELD_ITEM '), object_pairs_hook=unique)
                     for line in text.splitlines() if line.startswith('HELD_ITEM ')])


def inventory_rows(text):
    result = [json.loads(line.removeprefix('HELD_ITEM_CLASS '), object_pairs_hook=unique)
              for line in text.splitlines() if line.startswith('HELD_ITEM_CLASS ')]
    for row in result:
        if (set(row) != {'id', 'class', 'super-class', 'attack-owner', 'hit-owner', 'use-owner', 'max-damage',
                        'icon-owner', 'color-owner', 'full3d-owner', 'rotate-owner', 'full3d', 'rotate', 'icon0', 'color0'} or
            any(type(row[key]) is not str for key in ('class', 'super-class', 'attack-owner', 'hit-owner', 'use-owner',
                'icon-owner', 'color-owner', 'full3d-owner', 'rotate-owner')) or
            any(type(row[key]) is not bool for key in ('full3d', 'rotate')) or
            any(type(row[key]) is not int for key in ('icon0', 'color0')) or
            type(row['id']) is not int or not 0 <= row['id'] < 32000 or
            type(row['max-damage']) is not int or row['max-damage'] < 0):
            raise ValueError('Actual original Item inventory schema')
    if not result or [r['id'] for r in result] != sorted({r['id'] for r in result}):
        raise ValueError('Unique ordered actual Item inventory')
    return result


def compare(actual, expected):
    validate(actual)
    validate(expected)
    if actual != expected:
        for index, (a, b) in enumerate(zip(actual, expected)):
            if a != b:
                fields = sorted(key for key in FIELDS if a[key] != b[key])
                raise ValueError('Held Item original mismatch row ' + str(index) + ': ' + ','.join(fields))
        raise ValueError('Held Item original row count mismatch')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, default=Path(__file__).with_name('finite.json'))
    args = parser.parse_args()
    fixture = json.loads(args.fixture.read_text(), object_pairs_hook=unique)
    compare(rows(args.stdout.read_text()), fixture['rows'])
    print('Held Item attack: ' + str(len(fixture['rows'])) + ' exact selected original rows')
