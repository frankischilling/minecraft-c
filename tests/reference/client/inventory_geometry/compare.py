"""Compare active source words from specialized Block inventory batches."""
import json
from pathlib import Path
import re

IDS = (81, 53, 67, 85) * 4
COUNTS = (6, 12, 12, 24) * 4
ORDER = tuple((profile, block, batch) for profile, (block, count) in enumerate(zip(IDS, COUNTS))
              for batch in range(count))
PCS = {81: (420, 451, 502, 552, 603, 653),
       53: (844, 875, 907, 938, 970, 1001),
       67: (844, 875, 907, 938, 970, 1001),
       85: (1225, 1256, 1288, 1319, 1351, 1382)}


def unique(pairs):
    result = dict(pairs)
    if len(result) != len(pairs):
        raise ValueError('Duplicate inventory geometry field')
    return result


def active_indices(colored):
    # Original quad conversion copies XYZ/UV and enabled color to inserted
    # vertices3/4. It leaves their normal words and every reserved word alone.
    return [i for i in range(48) if i % 8 < 5 or
            i % 8 == 6 and i // 8 in (0, 1, 2, 5) or i % 8 == 5 and colored]


def validate(rows):
    if type(rows) is not list or len(rows) != len(ORDER):
        raise ValueError('Exactly216 source inventory draw rows')
    for row, (profile, block, batch) in zip(rows, ORDER):
        if (type(row) is not dict or set(row) != {'profile', 'id', 'batch', 'words', 'active_indices',
                                                 'bounds', 'nw_state', 'cv_pc'} or
                type(row['profile']) is not int or row['profile'] != profile or
                type(row['id']) is not int or row['id'] != block or
                type(row['batch']) is not int or row['batch'] != batch or
                type(row['cv_pc']) is not int or row['cv_pc'] != PCS[block][batch % 6] or
                type(row['words']) is not list or len(row['words']) != 48 or
                any(type(word) is not str or re.fullmatch('[0-9a-f]{8}', word) is None
                    for word in row['words']) or
                row['active_indices'] != active_indices(profile in (11, 12, 13)) or
                type(row['bounds']) is not list or len(row['bounds']) != 6 or
                any(type(word) is not str or re.fullmatch('[0-9a-f]{16}', word) is None
                    for word in row['bounds']) or type(row['nw_state']) is not dict):
            raise ValueError('Exact source inventory row schema/order/extent')
    return rows


def compare(actual, expected):
    validate(actual)
    validate(expected)
    for observed, reference in zip(actual, expected):
        for key in ('profile', 'id', 'batch', 'active_indices', 'bounds', 'nw_state', 'cv_pc'):
            if observed[key] != reference[key]:
                raise ValueError(f'Inventory source state mismatch profile{reference["profile"]} '
                                 f'batch{reference["batch"]} field{key}')
        for index in reference['active_indices']:
            if observed['words'][index] != reference['words'][index]:
                raise ValueError(f'Inventory active word mismatch profile{reference["profile"]} '
                                 f'batch{reference["batch"]} word{index}')
    return {'groups': len(actual), 'active_words': sum(len(r['active_indices']) for r in actual)}


def compare_native_geometry(actual, expected):
    """Compare passive submitted batches, without inventing draw-entry observations."""
    validate(expected)
    if type(actual) is not list or len(actual) != len(expected):
        raise ValueError('Exactly216 actual passive native submitted batches')
    compared = 0
    for observed, reference in zip(actual, expected):
        if (type(observed) is not dict or set(observed) !=
                {'profile', 'id', 'batch', 'words', 'vertices', 'mode', 'arrays'} or
                any(type(observed[k]) is not int or observed[k] != reference[k]
                    for k in ('profile', 'id', 'batch')) or
                type(observed['vertices']) is not int or observed['vertices'] != 6 or
                type(observed['mode']) is not int or observed['mode'] != 4 or
                type(observed['arrays']) is not int or observed['arrays'] !=
                    (15 if reference['nw_state']['l'] else 11) or
                type(observed['words']) is not list or len(observed['words']) != 48 or
                any(type(word) is not str or re.fullmatch('[0-9a-f]{8}', word) is None
                    for word in observed['words'])):
            raise ValueError('Exact passive native batch schema/order/vertices/mode/arrays')
        for index in reference['active_indices']:
            if observed['words'][index] != reference['words'][index]:
                raise ValueError(f'Inventory submitted active word mismatch profile{reference["profile"]} '
                                 f'batch{reference["batch"]} word{index}')
            compared += 1
    return {'groups': len(actual), 'active_words': compared,
            'comparison': 'actual submitted native batches against original active CPU backing words'}


def read(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=unique)
