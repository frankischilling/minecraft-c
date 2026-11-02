"""Compare the finite selected Player attack state with unchanged-original rows."""
import argparse
import json
from pathlib import Path
import re

PROFILES = ('cow-normal', 'cow-falling', 'cow-resisted', 'sheep-fatal', 'cow-with-owned-wolf')
FIELDS = ('health', 'previous-health', 'hurt', 'hurt-time', 'max-hurt', 'last-damage',
          'idle', 'dying', 'score', 'mob-kills', 'damage-dealt', 'drops', 'wolf-flags', 'wolf-target')


def rows(text):
    result = []
    for line in text.splitlines():
        if not line.startswith('PLAYER_ATTACK '):
            continue
        pairs = [word.split('=', 1) for word in line.split()[1:]]
        if any(len(pair) != 2 for pair in pairs):
            raise ValueError('Player attack receipt field')
        row = dict(pairs)
        if len(row) != len(pairs) or set(row) != {'profile', *FIELDS}:
            raise ValueError('Unique exact Player attack receipt fields')
        for name in FIELDS:
            if re.fullmatch(r'0|-?[1-9][0-9]*', row[name]) is None:
                raise ValueError('Exact integer Player attack field: ' + name)
            row[name] = int(row[name])
        result.append(row)
    if tuple(row['profile'] for row in result) != PROFILES:
        raise ValueError('Exact five ordered Player attack profiles')
    return result


def compare(actual, expected):
    # Python bool and int compare equal: require actual typed integer state.
    for values in (actual, expected):
        if (type(values) is not list or len(values) != len(PROFILES) or
                any(type(row) is not dict or set(row) != {'profile', *FIELDS} or
                    row['profile'] != name or any(type(row[key]) is not int for key in FIELDS)
                    for name, row in zip(PROFILES, values))):
            raise ValueError('Typed original/native Player attack fixture schema')
    if actual != expected:
        raise ValueError('Selected actual Player attack state differs from the original')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stdout', type=Path, required=True,
                        help='Retained stdout from the actual native controls')
    parser.add_argument('--fixture', type=Path, default=Path(__file__).with_name('finite.json'))
    args = parser.parse_args()
    fixture = json.loads(args.fixture.read_text())
    compare(rows(args.stdout.read_text()), fixture['rows'])
    print('Player attack: 5 exact selected unchanged-original CPU rows; independent entropy and no graphical qualification')


if __name__ == '__main__':
    main()
