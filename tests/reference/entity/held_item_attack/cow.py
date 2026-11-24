"""Audit each process's real Cow-private entropy, then compare declared fixed fields."""
import importlib.util
import json
from pathlib import Path
import re
import struct

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('held_cow_selected_compare', HERE / 'compare.py')
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)
PROFILES = ('cow10-diamond-ordinary', 'cow10-diamond-falling', 'cow10-diamond-break')
RANDOMS = ('cow-before', 'cow-after', 'player-before', 'player-after', 'world-before', 'world-after')
MASK = (1 << 48) - 1


def require(value, text):
    if not value:
        raise ValueError('Held Cow death: ' + text)


def state(word):
    require(type(word) is str and re.fullmatch('[0-9a-f]{16}', word) and int(word, 16) <= MASK,
            'actual48-bit Random state')
    return int(word, 16)


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


def bits(value):
    return struct.pack('>f', value).hex()


def entropy(before):
    seed = state(before)
    def draw(width):
        nonlocal seed
        seed = (seed * 0x5deece66d + 11) & MASK
        return seed >> (48 - width)
    first, second = draw(24) / (1 << 24), draw(24) / (1 << 24)
    pitch = bits(f32(f32(f32(first - second) * f32(.2)) + 1.))
    attempts = 0
    while True:
        attempts += 1
        require(attempts <= 100, 'bounded observed nextInt(3) rejection')
        value = draw(31)
        quantity = value % 3
        # Java's signed32 expression bits-value+(bound-1) must be nonnegative.
        if value - quantity + 2 <= (1 << 31) - 1:
            return {'after': f'{seed:016x}', 'pitch': pitch, 'quantity': quantity,
                    'float_draws': 2, 'next_int_attempts': attempts}


def audit(values):
    require(type(values) is list and [v.get('profile') for v in values] == list(PROFILES),
            'three ordered genuine Cow10 lethal calls')
    P.validate([v['selected'] for v in values])
    results = []
    for index, row in enumerate(values):
        require(set(row) == {'profile', 'selected', *RANDOMS, 'sounds', 'items', 'events'},
                'exact raw death/entropy schema')
        for key in RANDOMS:
            state(row[key])
        require(row['player-before'] == row['player-after'] and row['world-before'] == row['world-after'],
                'actual source Player/World private streams unchanged')
        actual = entropy(row['cow-before'])
        require(row['cow-after'] == actual['after'], 'independent Cow private RNG progression')
        require(row['sounds'] == [{'name': 'mob.cowhurt', 'volume': '3ecccccd', 'pitch': actual['pitch']}],
                'actual death sound and independently reconstructed source pitch')
        require(type(row['items']) is list and len(row['items']) == actual['quantity'] and
                row['events'] == ['SOUND'] + ['ADD'] * actual['quantity'],
                'independent LS.q nextInt3 quantity and ordered publication prefix')
        for item in row['items']:
            require(set(item) == {'stack', 'pickup-delay', 'world-member', 'chunk-members', 'distinct', 'private-random'} and
                    item['stack'] == [334, 1, 0, 0] and
                    all(type(value) is int for value in item['stack']) and
                    type(item['pickup-delay']) is int and item['pickup-delay'] == 10 and
                    item['world-member'] is True and item['distinct'] is True and
                    type(item['chunk-members']) is int and item['chunk-members'] == 1,
                    'genuine Leather Item/Stack values and actual membership/identity receipts')
            state(item['private-random'])
        selected = row['selected']
        damage, broken = 11 if index == 1 else 10, index == 2
        expected = {'profile': PROFILES[index], 'mode': 'falling' if index == 1 else 'ordinary',
            'id-before': 276, 'count-before': 1, 'damage-before': 1561 if broken else 0,
            'animation-before': 5, 'id-after': 276, 'count-after': 0 if broken else 1,
            'damage-after': 0 if broken else 1, 'animation-after': 5,
            'selected-same': not broken, 'selected-cleared': broken, 'inventory-changed': False,
            'health': 10 - damage, 'previous-health': 10, 'hurt': 20, 'hurt-time': 10,
            'max-hurt': 10, 'last-damage': damage, 'idle': 0, 'dying': True,
            'score': 3, 'mob-kills': 1, 'damage-dealt': damage, 'used-before': 11, 'used-after': 12,
            'drops': actual['quantity'], 'item-class': 'qd', 'attack-owner': 'qd', 'hit-owner': 'qd',
            'use-owner': 'gm', 'target-kind': 'Cow', 'max-damage': 1561, 'attack-power': 10,
            'saddled-before': -1, 'saddled-after': -1, 'broken-before': 17,
            'broken-after': 18 if broken else 17, 'sound-events': 1, 'particle-events': 0, 'break-sounds': 0}
        require(all(type(selected[key]) is type(value) and selected[key] == value
                    for key, value in expected.items()), 'independent Player/Stack/death result formula')
        results.append({'profile': row['profile'], **actual})
    return results


def rows(text):
    result = [json.loads(line.removeprefix('HELD_ITEM_COW '), object_pairs_hook=P.unique)
              for line in text.splitlines() if line.startswith('HELD_ITEM_COW ')]
    audit(result)
    return result


def projection(values):
    # Explicit selection, not a rewritten/raw-row normalization. Actual drops,
    # RNG states, sound pitch and published Items remain in every raw receipt
    # and are fully audited against that process's own real before-state.
    audit(values)
    return [{key: value for key, value in row['selected'].items() if key != 'drops'}
            for row in values]


def compare(actual, expected):
    if projection(actual) != projection(expected):
        raise ValueError('Declared deterministic Cow-death fields differ from original')
