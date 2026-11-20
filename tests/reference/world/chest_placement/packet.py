"""Asset-independent audit of the original Chest predicate packet."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
EXECUTION = ('ChestPlacementObservation.java', 'ChestPlacementJdi.java', 'capture_reference.py', 'cases.json')
PUBLICATION = ('packet.py', 'publish.py', 'projection.py', 'test_packet.py', 'README.md')
OUTPUTS = ('finite.json', 'traces.json', 'random.json', 'source-pins.json', 'native-observations.h', 'query-observations.h')
METHODS = {'e.a(Lfd;III)Z': (157, '37b296847197814ae31763181d2c1920956d1fa31559d0e3b5bf208a889738c7'),
           'e.h(Lfd;III)Z': (95, '266a4a748b7ae1ebf114cefec5420b341792b2ad7c75f449174f2bdac65e71b4'),
           'fd.a(III)I': (64, 'adf83ac7da3e525eb2cb08d6f7fb1d34683c61efdb1cefc26ee8f3cdc971bf0b')}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    value = dict(pairs)
    require(len(value) == len(pairs), 'unique JSON fields')
    return value


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda value: require(False, 'finite JSON values'))


def int32(value):
    return type(value) is int and -2147483648 <= value <= 2147483647


def validate_rows(cases, rows):
    require(type(cases) is list and type(rows) is list and len(cases) == len(rows) == 64, 'complete finite case count')
    names = [case['name'] for case in cases]
    require(len(set(names)) == len(names) and all(re.fullmatch('[a-z0-9-]+', name) for name in names), 'unique input names')
    for index, (case, row) in enumerate(zip(cases, rows)):
        require(set(case) == {'name', 'operation', 'x', 'y', 'z', 'target_id', 'chests', 'null_world'} and
                type(case['operation']) is int and case['operation'] in (0, 1) and
                all(int32(case[key]) for key in ('x', 'y', 'z', 'target_id')) and
                0 <= case['target_id'] <= 255 and type(case['null_world']) is bool, 'closed declared predicate')
        require(type(case['chests']) is list and len(case['chests']) <= 9 and all(
            type(cell) is list and len(cell) == 3 and all(int32(value) for value in cell) and
            0 <= cell[0] <= 31 and 0 <= cell[1] <= 127 and 0 <= cell[2] <= 31 for cell in case['chests']) and
            len({tuple(cell) for cell in case['chests']}) == len(case['chests']), 'canonical saved-Chunk input cells')
        require(set(row) == {'index', 'name', 'operation', 'x', 'y', 'z', 'target_id', 'null_world', 'cells',
                'target_cell', 'result', 'exception', 'array_changes', 'world_fields_unchanged',
                'chunk_fields_unchanged', 'queues_before', 'queues_after', 'events', 'rng_unchanged'}, 'closed original row')
        require(row['index'] == index and all(row[key] == case[key] for key in
                ('name', 'operation', 'x', 'y', 'z', 'target_id', 'null_world')) and
                row['cells'] == [cell + [54] for cell in case['chests']], 'original inputs match predeclaration')
        target = case['target_id'] if 0 <= case['x'] <= 31 and 0 <= case['y'] <= 127 and 0 <= case['z'] <= 31 else 0
        if [case['x'], case['y'], case['z']] in case['chests']:
            target = 54
        require(row['target_cell'] == target, 'actual directly authored target cell')
        require(row['array_changes'] == 0 and row['world_fields_unchanged'] is True and
                row['chunk_fields_unchanged'] is True and row['rng_unchanged'] is True and
                type(row['queues_before']) is list and row['queues_before'] == row['queues_after'] and
                row['events'] == [], 'read-only observed state')
        require((row['result'] in ('true', 'false') and row['exception'] == 'NONE') or
                (row['result'] == 'UNRETURNED' and row['exception'] == 'java.lang.NullPointerException'),
                'unaltered original return/failure')


def validate_random(cases, observations):
    require(set(observations) == {'interpreter', 'default', 'trace-interpreter', 'trace-default'}, 'all four actual streams')
    for rows in observations.values():
        require(type(rows) is list and len(rows) == len(cases), 'all raw Random rows')
        previous = None
        for index, (case, row) in enumerate(zip(cases, rows)):
            require(set(row) == {'index', 'name', 'before', 'after'} and row['index'] == index and
                    row['name'] == case['name'] and row['before'] == row['after'], 'actual unchanged RNG state')
            value = row['before']
            require(type(value) is list and len(value) == 3 and re.fullmatch('[0-9a-f]{12}', value[0]) and
                    type(value[1]) is bool and re.fullmatch('[0-9a-f]{16}', value[2]), 'raw Java Random words')
            require(previous is None or previous == value, 'actual sequential cohort RNG joins')
            previous = row['after']


def validate_traces(cases, rows, code):
    require(code == {name: {'code_bytes': size, 'code_sha256': pin} for name, (size, pin) in METHODS.items()},
            'actual entered live Code identities')
    require(type(rows) is list and len(rows) == len(cases), 'all actual JDI boundaries')
    for index, (case, row) in enumerate(zip(cases, rows)):
        require(set(row) == {'index', 'queries', 'entered_E'} and row['index'] == index and
                type(row['queries']) is list and len(row['queries']) <= 32 and
                type(row['entered_E']) is list and 1 <= len(row['entered_E']) <= 5, 'bounded selected live trace')
        require(row['entered_E'][0] == {'method': 'e.' + ('a' if case['operation'] == 0 else 'h') + '(Lfd;III)Z',
                'world_null': case['null_world'], 'xyz': [case['x'], case['y'], case['z']]}, 'actual entered original arguments')
        for entry in row['entered_E']:
            require(set(entry) == {'method', 'world_null', 'xyz'} and entry['method'] in METHODS and
                    entry['method'].startswith('e.') and type(entry['world_null']) is bool and
                    type(entry['xyz']) is list and len(entry['xyz']) == 3 and all(int32(value) for value in entry['xyz']),
                    'selected E method-entry transport')
        for query in row['queries']:
            require(set(query) == {'xyz', 'source_frames', 'result'} and type(query['xyz']) is list and
                    len(query['xyz']) == 3 and all(int32(value) for value in query['xyz']) and
                    type(query['result']) is int and 0 <= query['result'] <= 255 and
                    type(query['source_frames']) is list and 1 <= len(query['source_frames']) <= 2,
                    'actual World method arguments/return')
            for frame in query['source_frames']:
                require(set(frame) == {'method', 'pc'} and frame['method'].startswith('e.') and
                        frame['method'] in METHODS and type(frame['pc']) is int and
                        0 <= frame['pc'] < METHODS[frame['method']][0], 'live original caller location')


def audit(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(type(provenance['schema']) is int and provenance['schema'] == 1 and provenance['client_jar_sha256'] == JAR and
            provenance['capture']['status'] == 'PASS' and provenance['capture']['ordinary_targets'] == 2 and
            provenance['capture']['traced_targets'] == 2, 'complete original capture')
    for group, names in (('execution_helpers', EXECUTION), ('publication_helpers', PUBLICATION), ('outputs', OUTPUTS)):
        require(set(provenance[group]) == set(names), 'closed public fingerprint table')
        for name in names:
            require(digest(directory / name) == provenance[group][name], 'public fingerprint: ' + name)
    declared, finite = load(directory / 'cases.json'), load(directory / 'finite.json')
    require(type(declared['schema']) is int and type(finite['schema']) is int and
            declared['schema'] == finite['schema'] == 1 and finite['client_jar_sha256'] == JAR and
            declared['canonical_chunks'] == [[0, 0], [0, 1], [1, 0], [1, 1]], 'saved-World declaration identity')
    validate_rows(declared['cases'], finite['rows'])
    validate_random(declared['cases'], load(directory / 'random.json')['streams'])
    traces = load(directory / 'traces.json')
    validate_traces(declared['cases'], traces['rows'], traces['live_Code'])
    pins = load(directory / 'source-pins.json')
    require(pins['client_jar_sha256'] == JAR, 'verified original archive')
    seen = set()
    for owner in ('e', 'fd'):
        for method in pins['classes'][owner]['methods']:
            key = owner + '.' + method['name'] + method['descriptor']
            if key in METHODS:
                require((method['code_bytes'], method['code_sha256']) == METHODS[key], 'archive and live source method identity')
                seen.add(key)
    require(seen == set(METHODS), 'all selected archive method identities present')
    spec = importlib.util.spec_from_file_location('chest_placement_table', directory / 'projection.py')
    projection = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(projection)
    require(projection.tokens((directory / 'native-observations.h').read_text()) ==
            projection.tokens(projection.header(declared['cases'], finite['rows'])), 'C data derives from actual outcomes')
    require(projection.tokens((directory / 'query-observations.h').read_text()) ==
            projection.tokens(projection.query_header(declared['cases'], traces['rows'])), 'C query data derives from actual events')
    return {'original_cases': len(finite['rows']), 'public_predicates': sum(case['operation'] == 0 for case in declared['cases']),
            'private_helpers': sum(case['operation'] == 1 for case in declared['cases']),
            'actual_World_queries': sum(len(row['queries']) for row in traces['rows']), 'live_Code_pins': len(METHODS),
            'scope': 'Finite original predicate/JDI evidence; native replay separate'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packet', type=Path, default=HERE)
    print(audit(parser.parse_args().packet))
