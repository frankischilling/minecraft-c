"""Reject corrupted Player hurt packet identities, fields and comparison scopes."""
import copy
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from run_native import compare, finite, validate_provenance
from capture_reference import digest, ensure, read_json, rows, unique_object


def main():
    expected = finite()
    actual = copy.deepcopy(expected)
    for row in actual:
        if row['profile'] == 18:
            row['exception'] = 'IO'
        if row['profile'] == 21:
            row.update(exception='DEPENDENCY', result=None, completion=None)
    ensure(compare(actual, expected) == 20872, 'Player comparison cardinality')
    controls = 0
    def reject(function):
        nonlocal controls
        try:
            function()
        except (AssertionError, ValueError, KeyError, TypeError):
            controls += 1
        else:
            raise AssertionError('corruption accepted')
    def corrupt(path, value, reader=False):
        data = copy.deepcopy(actual)
        selected = data
        for key in path[:-1]:
            selected = selected[key]
        selected[path[-1]] = value
        if reader:
            reject(lambda: rows('\n'.join('PLAYER_HURT ' + json.dumps(row) for row in data)))
        else:
            reject(lambda: compare(data, expected))
    for path, value in [
        ((0, 'case'), True), ((0, 'case'), 1), ((0, 'profile'), 1), ((0, 'step'), 1),
        ((0, 'schema'), 'OTHER'), ((0, 'input', 1), 19), ((0, 'amount'), 3),
        ((0, 'state', 'fields', 'ls.Y'), True), ((0, 'state', 'fields', 'sn.aS'), {'F': 'bad'}),
        ((0, 'state', 'fields', 'dc.b'), {'nonnull': False}),
        ((0, 'state', 'bounds', 0), {'D': '000000000000000'}),
        ((0, 'state', 'random', 0, 0), 'ffffffffffffffff'),
        ((0, 'state', 'random', 0, 1), 0), ((0, 'state', 'filters'), []),
        ((0, 'state', 'inventory', 'main'), []), ((0, 'state', 'inventory', 'armor'), []),
        ((0, 'state', 'writer', 'total'), [[1, 2], [1, 3]]),
        ((0, 'state', 'aliases', 'world'), 1), ((0, 'state', 'sounds', 0, 'pitch'), 'bad'),
    ]:
        corrupt(path, value, True)
    for path, value in [
        ((0, 'state', 'fields', 'ls.Y'), 15), ((0, 'before', 'fields', 'ls.Y'), 19),
        ((0, 'state', 'fields', 'ls.F'), {'F': '00000000'}),
        ((0, 'state', 'world', 'fields', 'fd.Q'), 0),
        ((0, 'state', 'sounds', 0, 'pitch'), '3f800000'),
        ((0, 'state', 'aliases', 'world'), False),
        ((0, 'state', 'inventory', 'selected'), 1),
        ((9, 'state', 'inventory', 'stack', 'damage'), 0),
        ((0, 'state', 'writer', 'dirty'), False),
        ((0, 'state', 'random', 0, 0), '0000000000000000'),
        ((0, 'result'), False), ((0, 'scope'), 'EXCEPTION'),
        ((24, 'exception'), 'CALLBACK'), ((27, 'exception'), 'NONE'),
        ((27, 'result'), True), ((27, 'completion'), expected[27]['completion']),
        ((0, 'completion'), expected[27]['completion']),
    ]:
        corrupt(path, value)
    reject(lambda: rows(''))
    reject(lambda: compare(actual[:-1], expected))
    reject(lambda: json.loads('{"case":0,"case":1}', object_pairs_hook=unique_object))
    for path, value in [
        (('schema',), 'OTHER'), (('status',), 'FAIL'), (('archive_sha256',), '0' * 64),
        (('finite_sha256',), '0' * 64), (('input_unchanged',), False), (('helper_unchanged',), False),
        (('rows_per_vm',), True), (('profiles_per_vm',), 21), (('independent_player_fields',), []),
        (('independent_world_fields',), []), (('source_sha256', 'PlayerHurtObservation.java'), '0' * 64),
        (('tool_sha256', 'java.exe'), 'bad'), (('helper_sha256', 'PlayerHurtObservation.class'), 'bad'),
        (('original', 0, 'exit'), 1), (('original', 0, 'timed_out'), True),
        (('original', 0, 'pipe_capture_complete'), False), (('original', 0, 'reader_errors'), {'stdout': ['error']}),
        (('original', 0, 'output_overflow'), True), (('original', 0, 'stdout_file'), '../other'),
        (('original', 0, 'stdout_bytes'), 4194305), (('original', 1, 'stderr_sha256'), 'bad'),
    ]:
        provenance = read_json(HERE / 'provenance.json')
        selected = provenance
        for key in path[:-1]:
            selected = selected[key]
        selected[path[-1]] = value
        reject(lambda: validate_provenance(provenance, digest(HERE / 'finite.json')))
    print('Player hurt packet: ' + str(controls) + ' corruption controls; 20872 scalar comparisons')


if __name__ == '__main__':
    main()
