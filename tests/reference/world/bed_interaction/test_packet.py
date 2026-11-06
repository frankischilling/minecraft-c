"""Optimized-safe controls for the retained packet and native World projection readers."""
import copy
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from validate import compare, digest, load, loads, packet, planes, projection, require
from run_native import _native_rows_validated, finite, native_rows, provenance


def main():
    original = packet(load(HERE / 'finite.json'))
    original_before = copy.deepcopy(original)
    expected = finite()
    proof = load(HERE / 'provenance.json')
    comparisons = compare(expected, projection(original))
    text = '\n'.join('BED_INTERACTION ' + json.dumps(row) for row in expected)
    compare(native_rows(text, original), expected)
    rejected = 0

    def refusal(action, label):
        nonlocal rejected
        try:
            action()
        except (ValueError, KeyError, TypeError, IndexError):
            rejected += 1
            return
        raise ValueError('Bed corruption accepted: ' + label)

    def replace(value, path, item):
        result = copy.deepcopy(value)
        at = result
        for key in path[:-1]:
            at = at[key]
        at[path[-1]] = item
        return result

    def native(path, item, label):
        value = replace(expected, path, item)
        encoded = '\n'.join('BED_INTERACTION ' + json.dumps(row) for row in value)
        refusal(lambda: compare(_native_rows_validated(encoded, original), expected), label)

    for index, row in enumerate(expected):
        for key, value in (('case', True), ('metadata', False), ('partner', int(row['partner'])),
                           ('remote', int(row['remote'])), ('kind', 'unknown')):
            native([index, key], value, 'native scalar type/value ' + str(index) + '/' + key)
        bad = copy.deepcopy(row); bad['extra'] = 0
        native([index], bad, 'native unknown key ' + str(index))
        bad = copy.deepcopy(row); del bad['random_before']
        native([index], bad, 'native missing key ' + str(index))
        native([index, 'input_cells', 0, 3], 25, 'native full input plane ' + str(index))
        native([index, 'input_cells', 0, 0], True, 'native bool cell coordinate ' + str(index))
        native([index, 'output_cells'], [[0, 0, 0, 0, 1]], 'native nonzero air metadata ' + str(index))
        native([index, 'random_after', 0], '0000000000000000', 'native Random ' + str(index))
        if row['kind'] == 'activate':
            native([index, 'returned'], 1, 'native bool returned ' + str(index))
            native([index, 'player_unchanged'], False, 'native Player preservation ' + str(index))
            native([index, 'sleep_result'], 'cw.a', 'native actual CW identity ' + str(index))
            native([index, 'message_keys'], ['0045'], 'native actual message key ' + str(index))
        else:
            native([index, 'id'], False, 'native bool block ID ' + str(index))
            if row['items']:
                native([index, 'items', 0, 'world_member'], 1, 'native Item World membership')
                native([index, 'items', 0, 'position', 0], '0000000000000000', 'native Item position')
                native([index, 'items', 0, 'stack', 0], 354, 'native Item ID')
    refusal(lambda: native_rows('\n'.join(text.splitlines()[:-1]), original), 'missing native row')
    refusal(lambda: native_rows(text + '\n' + text.splitlines()[-1], original), 'extra native row')
    refusal(lambda: native_rows('\n'.join(reversed(text.splitlines())), original), 'reordered native rows')

    def source(path, item, label):
        value = replace(original, path, item)
        refusal(lambda: packet(value), label)

    source(['rows'], original['rows'][:-1], 'missing original row')
    source(['rows', 0, 'case'], True, 'source bool case')
    source(['rows', 0, 'changed_id'], False, 'source bool neighborID')
    source(['rows', 0, 'items', 0, 'pickup_delay'], True, 'source Item delay bool')
    source(['rows', 64, 'player_before', 0], True, 'source Player health bool')
    source(['rows', 64, 'notice', 0, 1], False, 'source notice age bool')
    source(['rows', 64, 'notice'], [], 'source notice retained')
    source(['rows', 0, 'input_planes'], '0' * 64, 'source input plane hash')
    source(['rows', 0, 'output_planes'], '0' * 64, 'source output plane hash')
    source(['journals', 0, 'calls', 0, 'arguments', 0], True, 'source bool int operand')
    source(['journals', 0, 'calls', 0, 'result'], False, 'source bool int result')
    source(['journals', 0, 'calls', 0, 'entered'], True, 'source bool entry tick')
    source(['journals', 0, 'calls', 0, 'returned'], 0, 'source entry/return order')
    source(['journals', 0, 'calls', 0, 'completed'], 1, 'source completed bool')
    source(['journals', 0, 'calls', 0, 'pc'], 6, 'source reached PC')
    source(['journals', 64, 'calls', 4, 'result'], 'cw.a', 'source current CW identity')
    source(['code', 0, 'sha256'], '0' * 64, 'source game live Code')
    source(['code', 6, 'sha256'], '0' * 64, 'source JDK live Code')
    source(['code', 0, 'bytes'], True, 'source Code byte extent bool')
    source(['code'], original['code'][:-1], 'source missing live Code')
    source(['code', 1], original['code'][0], 'source duplicate live Code')
    source(['journals'], original['journals'][:-1], 'source missing journal')
    bad = copy.deepcopy(original['rows'][0]); bad['inferred'] = True
    source(['rows', 0], bad, 'source unknown field')
    for path, value in ((['status'], 'FAIL'), (['schema'], 'unknown'), (['archive_sha256'], '0' * 64),
                        (['finite_sha256'], '0' * 64), (['input_unchanged'], 1), (['helper_unchanged'], False),
                        (['reviewed_observations_equal'], 1), (['profiles_per_vm'], True),
                        (['scope'], 'full GUI/night/native parity'), (['query_count'], 407),
                        (['code_methods'], 19), (['game_code_methods'], 20), (['class_count'], 23),
                        (['original', 0, 'exit'], False), (['original', 0, 'timed_out'], True),
                        (['original', 0, 'pipe_capture_complete'], 1), (['original', 0, 'reader_errors'], []),
                        (['original', 0, 'output_overflow'], True), (['jdi', 'stdout_bytes'], True),
                        (['jdi', 'timeout_seconds'], 180), (['tool_sha256', 'java.exe'], 'bad'),
                        (['source_sha256', 'validate.py'], '0' * 64),
                        (['helper_sha256', 'BedInteractionObservation.class'], 'bad'),
                        (['jdi_target', 'calls.jsonl'], 'bad')):
        bad = replace(proof, path, value)
        refusal(lambda bad=bad: provenance(bad, digest(HERE / 'finite.json')), 'provenance ' + str(path))
    for sample in ('{"a":1,"a":2}', '{"a":{"z":0,"z":1}}', '{"a":NaN}', '{"a":Infinity}', '{"a":-Infinity}'):
        refusal(lambda sample=sample: loads(sample), 'duplicate key/non-JSON constant')
    for cells in ([[0, 0, 0, 0, 0]], [[0, 0, 0, 1, 0], [0, 0, 0, 2, 0]],
                  [[1, 0, 0, 1, 0], [0, 0, 0, 1, 0]], [[0, 0, 0, 1, 16]], [[16, 0, 0, 1, 0]]):
        refusal(lambda cells=cells: planes(cells), 'lossless sparse-plane shape/domain/order')
    compare(original, original_before, 'Bed mutation controls preserve frozen original')
    require(comparisons == 2569, 'Bed selected comparison inventory')
    print('Bed interaction packet: ' + str(rejected) + ' rejected corruptions; ' +
          str(comparisons) + ' projection scalars; optimized=' + str(not __debug__))


if __name__ == '__main__':
    main()
