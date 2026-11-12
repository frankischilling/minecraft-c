"""Compare all actual original/native NW backing words for six held Item icons."""
import argparse
import json
from pathlib import Path
import re

ICONS = (64, 65, 66, 67, 68, 21)
ORDER = tuple((icon, batch) for icon in ICONS for batch in range(6))


def unique(pairs):
    value = dict(pairs)
    if len(value) != len(pairs):
        raise ValueError('Duplicate held geometry JSON field')
    return value


def validate(values):
    if type(values) is not list or len(values) != 36:
        raise ValueError('Exactly36 ordered held geometry groups')
    for row, (icon, batch) in zip(values, ORDER):
        if (type(row) is not dict or set(row) != {'icon', 'batch', 'words'} or
            type(row['icon']) is not int or row['icon'] != icon or
            type(row['batch']) is not int or row['batch'] != batch or
            type(row['words']) is not list or len(row['words']) != (48 if batch < 2 else 768) or
            any(type(word) is not str or re.fullmatch('[0-9a-f]{8}', word) is None for word in row['words'])):
            raise ValueError('Exact typed original held geometry schema/order/extent')
    return values


def rows(text):
    return validate([json.loads(line.removeprefix('HELD_ITEM_GEOMETRY '), object_pairs_hook=unique)
                     for line in text.splitlines() if line.startswith('HELD_ITEM_GEOMETRY ')])


def compare(actual, expected):
    validate(actual)
    validate(expected)
    if actual != expected:
        for a, b in zip(actual, expected):
            for index, (x, y) in enumerate(zip(a['words'], b['words'])):
                if x != y:
                    raise ValueError(f"Original held geometry mismatch icon{a['icon']} batch{a['batch']} word{index}: {x} != {y}")
        raise ValueError('Original held geometry mismatch')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stdout', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, default=Path(__file__).with_name('finite.json'))
    args = parser.parse_args()
    fixture = json.loads(args.fixture.read_text(), object_pairs_hook=unique)
    compare(rows(args.stdout.read_text()), fixture['rows'])
    print('36 direct-original NW groups;19008 exact backing words; finite known inactive slots')
