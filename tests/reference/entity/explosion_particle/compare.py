"""Strict finite comparisons for original explosion ticks and XW sprite words."""
import argparse
import json
from pathlib import Path
import re

TICKS = ('age0','age7','age8','negative-age','wrap-age','zero-life','ground-age0')
TICK_FIELDS = {'profile','exception','age','lifetime','icon','dead','ground','position','previous','velocity','bounds'}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Repeated explosion observation field: ' + key)
        result[key] = value
    return result


def tick_rows(text):
    values = [json.loads(line[len('EXPLOSION_TICK '):], object_pairs_hook=unique_object) for line in text.splitlines()
              if line.startswith('EXPLOSION_TICK ')]
    if len(values) != len(TICKS):
        raise ValueError('Exactly seven original explosion tick profiles')
    for profile, row in zip(TICKS, values):
        if type(row) is not dict or set(row) != TICK_FIELDS or row['profile'] != profile:
            raise ValueError('Exact ordered explosion tick schema')
        if row['exception'] != ('ArithmeticException' if profile == 'zero-life' else 'none'):
            raise ValueError('Actual explosion tick return/exception category')
        if any(type(row[key]) is not int or not -(1<<31)<=row[key]<(1<<31)
               for key in ('age','lifetime','icon')) or any(
                type(row[key]) is not bool for key in ('dead','ground')):
            raise ValueError('Typed explosion tick primitive fields')
        for key, count in (('position',3),('previous',3),('velocity',3),('bounds',6)):
            if (type(row[key]) is not list or len(row[key]) != count or
                    any(type(word) is not str or re.fullmatch('[0-9a-f]{16}',word) is None for word in row[key])):
                raise ValueError('Exact explosion tick binary64 words')
    return values


def draw_rows(text):
    values = []
    for line in text.splitlines():
        if not line.startswith('EXPLOSION_DRAW '):
            continue
        match = re.fullmatch(r'EXPLOSION_DRAW profile=([0-5]) brightness=([0-9a-f]{8}) words=([0-9a-f,]+)',line)
        if match is None:
            raise ValueError('Exact original explosion sprite receipt')
        words = match[3].split(',')
        if len(words) != 48 or any(re.fullmatch('[0-9a-f]{8}',word) is None for word in words):
            raise ValueError('All 48 actual original NW words required')
        values.append({'profile':int(match[1]),'brightness':match[2],'words':words})
    if [row['profile'] for row in values] != list(range(6)):
        raise ValueError('Exactly six ordered original XW sprite profiles')
    return values


def compare(actual, expected):
    # Compare serialized types as well as values; bool must not stand in for int.
    if json.dumps(actual,sort_keys=True) != json.dumps(expected,sort_keys=True):
        raise ValueError('Selected explosion particle observation differs from unchanged original')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stdout',type=Path,required=True)
    parser.add_argument('--mode',choices=('ticks','draws'),required=True)
    parser.add_argument('--fixture',type=Path,default=Path(__file__).with_name('finite.json'))
    args=parser.parse_args()
    expected=json.loads(args.fixture.read_text())[args.mode]
    actual=(tick_rows if args.mode=='ticks' else draw_rows)(args.stdout.read_text())
    compare(actual,expected)
    print('Explosion particle: exact selected unchanged-original '+args.mode)


if __name__=='__main__':
    main()
