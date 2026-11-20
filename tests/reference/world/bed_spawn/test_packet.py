"""Reject altered Bed results, query order, original inputs and process receipts."""
import copy
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from run_native import compare, finite, native_rows, provenance
from validate import digest, journals, load, queries, profiles, require, rows, unique


def main():
    expected = finite()
    require(compare(copy.deepcopy(expected), expected) == 2014, 'Bed exact comparison cardinality')
    require(native_rows('\n'.join('BED_SPAWN ' + json.dumps(row) for row in expected)) == expected,
            'Bed positive native transport')
    controls = 0
    def reject(function):
        nonlocal controls
        try:
            function()
        except (ValueError, AssertionError, KeyError, TypeError, IndexError):
            controls += 1
        else:
            raise AssertionError('altered Bed evidence accepted')
    def changed(path, value, read=False):
        actual = copy.deepcopy(expected)
        target = actual
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = value
        if read:
            reject(lambda: native_rows('\n'.join('BED_SPAWN ' + json.dumps(row) for row in actual)))
        else:
            reject(lambda: compare(actual, expected))
    for path, value in [((0, 'case'), True), ((0, 'case'), 1), ((0, 'result'), [7, 64, True]),
            ((0, 'journal', 0, 'pc'), True), ((0, 'journal', 0, 'pc'), 5),
            ((0, 'journal', 0, 'completed'), False), ((0, 'journal', 0, 'result'), False),
            ((0, 'journal', 0, 'kind'), 'OTHER'), ((0, 'journal', 1, 'result'), 2),
            ((22, 'journal', 0, 'pc'), 45), ((22, 'journal', 0, 'result'), 1),
            ((22, 'journal', 0, 'y'), 1)]:
        changed(path, value, True)
    for path, value in [((0, 'result'), None), ((0, 'result'), [8, 64, 7]),
            ((0, 'journal', 0, 'result'), 1), ((0, 'journal', 1, 'x'), 8),
            ((0, 'journal', 1, 'result'), 0), ((0, 'journal', 2, 'result'), 0),
            ((17, 'result'), [7, 64, 6]), ((18, 'result'), [7, 64, 7]),
            ((20, 'journal'), expected[19]['journal']), ((23, 'result'), [7, 64, 7]),
            ((24, 'journal', 0, 'x'), 1), ((25, 'journal', 0, 'z'), -1)]:
        changed(path, value)
    changed((0, 'journal'), expected[0]['journal'][:-1])
    reject(lambda: compare(expected[:-1], expected))
    reject(lambda: native_rows(''))
    reject(lambda: json.loads('{"case":0,"case":1}', object_pairs_hook=unique))
    proof = load(HERE / 'provenance.json')
    for path, value in [(('schema',), 'OTHER'), (('status',), 'FAIL'),
            (('archive_sha256',), '0' * 64), (('finite_sha256',), '0' * 64),
            (('profiles_per_vm',), True), (('query_count',), 0), (('code_methods',), 6),
            (('input_unchanged',), False), (('helper_unchanged',), False), (('scope',), 'Whole gameplay'),
            (('source_sha256', 'BedSpawnObservation.java'), '0' * 64),
            (('helper_sha256', 'BedSpawnJdi.class'), 'bad'), (('tool_sha256', 'java.exe'), 'bad'),
            (('original', 0, 'exit'), 1), (('original', 0, 'timed_out'), True),
            (('original', 0, 'pipe_capture_complete'), False), (('original', 0, 'reader_errors'), {'stdout': ['failed']}),
            (('original', 0, 'stdout_file'), '../outside'), (('original', 0, 'stdout_bytes'), 4194305),
            (('original', 1, 'stdout_sha256'), 'bad'), (('jdi', 'exit'), 1),
            (('jdi', 'timeout_seconds'), 90), (('jdi', 'output_overflow'), True),
            (('jdi_target', 'queries.jsonl'), 'bad')]:
        actual = copy.deepcopy(proof); target = actual
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = value
        reject(lambda value=actual: provenance(value, digest(HERE / 'finite.json')))
    packet = load(HERE / 'finite.json')
    for path, value in [((0, 'planes_before'), '0' * 64), ((0, 'random_after'), ['0' * 16, False, '0' * 16]),
            ((0, 'fresh_result'), False), ((0, 'provider_same'), False), ((0, 'input_same'), False),
            ((0, 'exception'), 'NPE'), ((0, 'input', 5), 1)]:
        original = copy.deepcopy(packet['rows']); target = original
        for key in path[:-1]:
            target = target[key]
        target[path[-1]] = value
        reject(lambda value=original: rows('\n'.join('BED_SPAWN ' + json.dumps(row) for row in value)))
    code = copy.deepcopy(packet['code']); code[0]['sha256'] = '0' * 64
    reject(lambda: journals('\n'.join(json.dumps(row) for row in [*code, *[
        {'event': 'journal', 'case': row['case'], 'queries': row['journal']} for row in packet['journals']]])))
    print('Bed spawn packet: ' + str(controls) + ' corruption controls; 2014 scalar comparisons')


if __name__ == '__main__':
    main()
