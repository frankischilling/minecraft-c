"""Predeclare Furnace inputs only; this file calculates no expected outcomes."""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
cases = []


def stack(item_id, count=1, damage=0, animation=0):
    return dict(id=item_id, count=count, damage=damage, animation=animation)


def add(name, operation, **values):
    case = dict(profile=name, operation=operation, index=0, amount=0,
                fields=[0, 0, 0], xyz=[-8, 64, 19], stacks={}, tile={}, player={}, argument=None,
                nbt=dict(burn=0, cook=0, xyz=[11, -12, 13], items=[], mode='normal'),
                array_null=False, player_null=False, tile_null=False)
    case.update(values)
    cases.append(case)


def supplied(name, operation, item_id=15, count=5, **values):
    inputs = dict(stacks={'A': stack(item_id, count, 0, 7)}, tile={'0': 'A'}, argument='A')
    inputs.update(values)
    add(name, operation, **inputs)


for operation in ('construct', 'size', 'limit', 'name'):
    add(operation, operation)
for index in (-2147483648, -1, 0, 1, 2, 3, 2147483647):
    supplied('get-' + str(index).replace('-', 'm'), 'get', index=index)
add('get-null-array', 'get', array_null=True)
for count in (-3, 0, 1, 64, 65, 2147483647):
    supplied('set-count-' + str(count).replace('-', 'm'), 'set', count=count, index=1)
supplied('set-shared-alias', 'set', count=65, tile={'0': 'A', '2': 'A'}, index=1)
for index in (-1, 3, 2147483647):
    supplied('set-bad-index-' + str(index).replace('-', 'm'), 'set', index=index)
add('set-null', 'set', index=1)
add('set-null-array', 'set', array_null=True)
for count in (-3, 0, 1, 5, 64):
    for amount in (-2, 0, 1, 4, 5, 65, -2147483648, 2147483647):
        supplied(f'remove-{count}-{amount}'.replace('-', 'm'), 'remove', count=count, amount=amount)
supplied('remove-shared-split', 'remove', tile={'0': 'A', '1': 'A'}, amount=2)
supplied('remove-shared-whole', 'remove', tile={'0': 'A', '1': 'A'}, amount=5)
for index in (-1, 3):
    add('remove-empty-bad-' + str(index).replace('-', 'm'), 'remove', index=index, amount=1)
add('remove-empty', 'remove', amount=-1)
for cook, scale in ((0, 24), (199, 24), (200, 24), (-1, 24), (-200, -24),
                    (2147483647, 24), (-2147483648, -1), (2147483647, 2147483647)):
    add(f'cook-scale-{cook}-{scale}'.replace('-', 'm'), 'cook-scale', fields=[0, 0, cook], amount=scale)
for burn, duration, scale in ((0, 0, 12), (1, 0, 12), (199, 200, 12), (200, 200, 12),
                              (-200, 200, 12), (200, -200, 12),
                              (-2147483648, -1, 1), (2147483647, 0, 2147483647)):
    add(f'burn-scale-{burn}-{duration}-{scale}'.replace('-', 'm'), 'burn-scale',
        fields=[burn, duration, 0], amount=scale)
for burn in (-2147483648, -1, 0, 1, 2147483647):
    add('burning-' + str(burn).replace('-', 'm'), 'burning', fields=[burn, 0, 0])
add('fuel-null', 'fuel')
for item_id in (1, 5, 6, 17, 35, 47, 53, 54, 58, 61, 63, 64, 85, 96,
                263, 280, 327, 260, 268, 264, -1, 0, 255, 256, 31999, 32000, 2147483647):
    supplied('fuel-' + str(item_id).replace('-', 'm'), 'fuel', item_id=item_id)
supplied('fuel-charcoal', 'fuel', item_id=263, stacks={'A': stack(263, -2, 1)})
add('can-smelt-empty', 'can-smelt')
supplied('can-smelt-no-recipe', 'can-smelt', item_id=1)
supplied('can-smelt-invalid-item', 'can-smelt', item_id=-1)
for count in (-1, 0, 1, 63, 64, 65, 2147483647):
    supplied('can-smelt-output-' + str(count).replace('-', 'm'), 'can-smelt',
             stacks={'A': stack(15, 3), 'B': stack(265, count)}, tile={'0': 'A', '2': 'B'})
for item_id, damage in ((266, 0), (265, 1), (0, 0), (-1, 0)):
    supplied(f'can-smelt-mismatch-{item_id}-{damage}'.replace('-', 'm'), 'can-smelt',
             stacks={'A': stack(15, 3), 'B': stack(item_id, 1, damage)}, tile={'0': 'A', '2': 'B'})
for item_id in (15, 14, 56, 12, 319, 349, 4, 337, 81, 17):
    supplied('smelt-recipe-' + str(item_id), 'smelt', item_id=item_id)
for count in (-2147483648, -1, 0, 1):
    supplied('smelt-input-' + str(count).replace('-', 'm'), 'smelt', count=count)
for count in (-1, 63, 64, 65):
    supplied('smelt-output-' + str(count).replace('-', 'm'), 'smelt',
             stacks={'A': stack(15, 3), 'B': stack(265, count)}, tile={'0': 'A', '2': 'B'})
supplied('smelt-shared-input-fuel', 'smelt', tile={'0': 'A', '1': 'A'})
add('smelt-empty', 'smelt')
supplied('smelt-invalid-item', 'smelt', item_id=-1)
for name, items, burn, cook, mode in (
    ('empty', [], 123, 199, 'normal'),
    ('coal', [[0, 15, 2, 0], [1, 263, 3, 1]], 1600, 40, 'normal'),
    ('wood', [[1, 17, 1, 2]], -1, -2, 'normal'),
    ('lava', [[1, 327, 1, 0]], 20000, 199, 'normal'),
    ('nonfuel', [[1, 1, 1, 0]], 12, 13, 'normal'),
    ('duplicate', [[1, 263, 3, 0], [1, 280, -2, 1]], 40000, 65535, 'normal'),
    ('ignored-slots', [[-1, -1, -1, -1], [3, 15, 5, 0], [255, 263, 1, 0]], 32768, -32769, 'normal'),
    ('negative-count', [[0, 15, -128, -1], [2, 265, 127, 32767]], -32768, 32767, 'normal'),
    ('invalid-fuel', [[0, 15, 1, 0], [1, -1, 1, 0]], 123, 456, 'normal'),
    ('missing-items', [], 12, 34, 'missing-items'),
    ('wrong-items', [], 12, 34, 'wrong-items'),
    ('wrong-burn', [[0, 15, 1, 0]], 12, 34, 'wrong-burn')):
    supplied('read-' + name, 'read', nbt=dict(burn=burn, cook=cook, xyz=[11, -12, 13], items=items, mode=mode))
for burn, cook in ((0, 0), (32767, 32768), (-32769, 65535), (2147483647, -2147483648)):
    for operation in ('write', 'roundtrip'):
        supplied(f'{operation}-{burn}-{cook}'.replace('-', 'm'), operation,
                 fields=[burn, 999, cook], stacks={'A': stack(15, 257, 65535), 'B': stack(263, -129, 32768)},
                 tile={'0': 'A', '1': 'B', '2': 'A'})
supplied('roundtrip-empty-fuel', 'roundtrip', fields=[1200, 1600, 99])
supplied('write-null-array', 'write', array_null=True)
for burn in (0, 1, -1):
    supplied('tick-null-' + str(burn).replace('-', 'm'), 'tick-null', fields=[burn, 1600, 199])
add('usable-null-world', 'usable-null')
for player_null, tile_null in ((False, False), (False, True), (True, False)):
    add(f'bm-layout-{int(player_null)}-{int(tile_null)}', 'bm-layout',
        player_null=player_null, tile_null=tile_null)
for index in (-1, 0, 1, 2, 3, 29, 30, 38, 39):
    supplied('bm-shift-' + str(index).replace('-', 'm'), 'bm-shift', index=index,
             tile={'0': 'A', '1': 'A', '2': 'A'}, player={'9': 'A', '0': 'A'})
for index in (0, 1, 2, 3, 30):
    supplied('bm-shift-single-' + str(index), 'bm-shift', index=index,
             tile={str(index): 'A'} if index < 3 else {}, player={'9' if index == 3 else '0': 'A'} if index >= 3 else {})
supplied('bm-output-full-unchanged', 'bm-shift', index=2, stacks={'A': stack(265, 5), 'B': stack(1, 64)},
         tile={'2': 'A'}, player={str(i): 'B' for i in range(36)})
add('bm-shift-empty', 'bm-shift', index=0)
for progress in (-1, 0, 1, 2, 3):
    add('bm-progress-' + str(progress).replace('-', 'm'), 'bm-progress', index=progress,
        amount=-2147483648, fields=[12, 34, 56])
add('bm-update-empty', 'bm-update')
add('bm-update-counters', 'bm-update', fields=[12, 34, 56])
add('bm-usable-null-world', 'bm-usable')

if __name__ == '__main__':
    assert len(cases) <= 256 and len({case['profile'] for case in cases}) == len(cases)
    (HERE / 'cases.json').write_text(json.dumps({'schema': 1, 'cases': cases}, indent=2) + '\n',
                                   encoding='utf8', newline='\n')
    print(len(cases), 'declared inputs; no expected results')
