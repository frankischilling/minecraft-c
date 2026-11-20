"""Audit the public Chest component packet without Java or original assets."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
EXECUTION = ('ChestContainerObservation.java', 'cases.json', 'capture.py')
PUBLICATION = ('projection.py', 'audit.py', 'publish.py', 'test_packet.py', 'README.md')
OUTPUTS = ('finite.json', 'source-pins.json', 'native-observations.h')
CORE = {'cd', 'og', 'lw', 'gp', 'dw', 'iz', 'gm', 'hp', 'e', 'gs', 'dc', 'js', 'ix', 'uu',
        'id', 'da', 'bb', 'bw', 'cv'}
ROW_KEYS = {'profile', 'result', 'rows', 'layout', 'base', 'screen', 'error', 'error_message',
            'calls', 'before', 'after'}
CASE_KEYS = {'profile', 'target', 'operation', 'left', 'right', 'shared', 'name', 'index',
             'amount', 'player_null', 'stacks', 'native_candidate'}


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def unique(pairs):
    result = dict(pairs)
    require(len(result) == len(pairs), 'unique JSON fields')
    return result


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda value: require(False, 'finite JSON numbers'))


def int32(value):
    return type(value) is int and -2147483648 <= value <= 2147483647


def stack(value):
    require(type(value) is dict and set(value) == {'id', 'count', 'damage', 'animation'} and
            all(int32(item) for item in value.values()), 'closed raw Stack fields')


def inputs(case):
    declared = dict(case['stacks'])
    left = None if case['left'] is None else [None] * 64
    right = left if case['shared'] else None if case['right'] is None else [None] * 64
    for field, values in (('left', left), ('right', right)):
        if values is not None and case[field] is not None:
            for index, label in case[field]['slots'].items():
                values[int(index)] = label
    return {'left': left, 'right': right, 'stacks': declared}


def validate(cases, rows):
    require(type(cases) is list and cases and len(cases) <= 256, 'bounded declared cases')
    names = [case.get('profile') for case in cases]
    require(all(type(name) is str and re.fullmatch('[a-z0-9-]+', name) for name in names) and
            len(names) == len(set(names)), 'unique declared profiles')
    require(type(rows) is list and [row.get('profile') for row in rows] == names,
            'all original rows in declared order')
    for case, row in zip(cases, rows):
        require(set(case) == CASE_KEYS and case['target'] in ('cd', 'og', 'hp') and
                type(case['shared']) is bool and type(case['player_null']) is bool and
                type(case['native_candidate']) is bool and int32(case['index']) and
                int32(case['amount']) and (case['name'] is None or type(case['name']) is str),
                'closed declared input')
        require(case['operation'] in (('construct', 'shift', 'usable') if case['target'] != 'og'
                else ('size', 'name', 'get', 'set', 'split', 'limit', 'changed', 'usable')),
                'declared target operation')
        require(type(case['stacks']) is dict and len(case['stacks']) <= 2, 'declared input Stack map')
        for name, value in case['stacks'].items():
            require(re.fullmatch('[A-MO-Z][A-Za-z0-9_]*', name) is not None, 'input identity namespace')
            stack(value)
        for field in ('left', 'right'):
            inventory = case[field]
            if inventory is None:
                continue
            require(type(inventory) is dict and set(inventory) ==
                    {'sizes', 'limit', 'usable', 'failure', 'slots'}, 'closed controlled LW')
            require(type(inventory['sizes']) is list and 1 <= len(inventory['sizes']) <= 4 and
                    all(int32(value) for value in inventory['sizes']) and int32(inventory['limit']) and
                    type(inventory['usable']) is bool and inventory['failure'] in
                    ('none', 'size', 'get', 'set', 'split', 'limit', 'changed', 'usable', 'name'),
                    'declared controlled LW behavior')
            require(type(inventory['slots']) is dict and all(
                type(index) is str and index == str(int(index)) and 0 <= int(index) < 64 and
                name in case['stacks'] for index, name in inventory['slots'].items()),
                'declared physical LW slots')
        require(type(row) is dict and set(row) == ROW_KEYS, 'closed original row')
        require(row['before'] == inputs(case), 'observed pre-call state equals declared arguments')
        for field in ('before', 'after'):
            value = row[field]
            require(type(value) is dict and set(value) == {'left', 'right', 'stacks'} and
                    type(value['stacks']) is dict, 'closed original snapshot')
            for name, item in value['stacks'].items():
                require(type(name) is str and (name in case['stacks'] or
                        re.fullmatch('N[1-9][0-9]*', name)), 'observed identity namespace')
                stack(item)
            for owner in ('left', 'right'):
                values = value[owner]
                require(values is None or (type(values) is list and len(values) == 64 and all(
                    item is None or item in value['stacks'] for item in values)), 'physical observed LW slots')
            if case['shared']:
                require(value['left'] == value['right'], 'same LW child alias remains identical')
        require(set(case['stacks']) <= set(row['after']['stacks']), 'original input identities retained')
        require(type(row['calls']) is list and len(row['calls']) <= 4096 and all(
            type(call) is str and re.fullmatch(
                r'[LR]\.(size|name|limit|changed|usable:null|get:-?[0-9]+|'
                r'split:-?[0-9]+:-?[0-9]+|set:-?[0-9]+:(null|[A-Z][A-Za-z0-9_]*))', call)
            for call in row['calls']), 'bounded controlled LW callback transport')
        require(row['error'] in (None, 'ProbeFailure', 'java.lang.NullPointerException',
                'java.lang.IndexOutOfBoundsException', 'java.lang.ArrayIndexOutOfBoundsException') and
                (row['error_message'] is None or type(row['error_message']) is str), 'raw original exception')
        if row['layout'] is not None:
            require(int32(row['rows']) and type(row['layout']) is list and len(row['layout']) <= 90,
                    'bounded completed CD graph')
            for index, slot in enumerate(row['layout']):
                require(type(slot) is list and len(slot) == 5 and slot[0] == index and
                        slot[1] in (None, 'L', 'R') and all(int32(value) for value in (slot[0], *slot[2:])),
                        'observed source Slot fields and ordering')
            require(row['base'] == {'window': 0, 'transaction': 0, 'snapshots': len(row['layout']),
                    'all_snapshots_null': True, 'listeners': 0, 'blocked_players': 0,
                    'distinct_collections': True}, 'original base Container constructor state')
        else:
            require(row['rows'] is None and row['base'] is None, 'no invented partial constructor graph')
        if row['screen'] is not None:
            require(case['target'] == 'hp' and row['screen'].keys() ==
                    {'width', 'height', 'rows', 'input', 'pauses', 'left_same', 'right_same'} and
                    row['screen']['width'] == 176 and row['screen']['input'] is False and
                    row['screen']['pauses'] is False and row['screen']['left_same'] is True and
                    row['screen']['right_same'] is True and int32(row['screen']['height']) and
                    int32(row['screen']['rows']), 'actual HP constructor state')
        if case['target'] == 'og' and case['operation'] in ('get', 'split') or case['operation'] == 'shift':
            require(row['result'] is None or row['result'] in row['after']['stacks'], 'actual nullable Stack return')


def audit(directory=HERE):
    directory = Path(directory)
    provenance = load(directory / 'provenance.json')
    require(type(provenance['schema']) is int and provenance['schema'] == 1 and provenance['client_jar_sha256'] == JAR and
            provenance['capture']['status'] == 'PASS' and provenance['capture']['processes'] == 2 and
            provenance['capture']['input_continuity'] is True and
            provenance['capture']['compiled_continuity'] is True, 'completed immutable original capture')
    for field, names in (('execution_helpers', EXECUTION), ('publication_helpers', PUBLICATION),
                         ('outputs', OUTPUTS)):
        require(set(provenance[field]) == set(names), 'closed public fingerprint inventory')
        for name in names:
            require(digest(directory / name) == provenance[field][name], 'public fingerprint: ' + name)
    cases = load(directory / 'cases.json')
    require(type(cases['schema']) is int and cases['schema'] == 1 and
            type(cases['physical_slots_per_controlled_inventory']) is int and
            cases['physical_slots_per_controlled_inventory'] == 64,
            'controlled LW declaration version')
    finite = load(directory / 'finite.json')
    require(set(finite) == {'schema', 'client_jar_sha256', 'scope', 'rows'} and
            type(finite['schema']) is int and finite['schema'] == 1 and
            finite['client_jar_sha256'] == JAR, 'closed original observations')
    validate(cases['cases'], finite['rows'])
    require(len(finite['rows']) == provenance['capture']['rows_per_process'], 'exact original row count')
    pins = load(directory / 'source-pins.json')
    require(set(pins) == {'client_jar_sha256', 'classes'} and pins['client_jar_sha256'] == JAR and
            set(pins['classes']) == CORE, 'closed selected original class inventory')
    methods = 0
    for name, value in pins['classes'].items():
        require(set(value) == {'class_bytes', 'class_sha256', 'major', 'minor', 'methods'} and
                type(value['class_bytes']) is int and value['class_bytes'] > 0 and
                re.fullmatch('[0-9a-f]{64}', value['class_sha256']) and value['major'] == 49 and
                value['minor'] == 0 and type(value['methods']) is list, 'selected original class pin')
        identities = []
        for method in value['methods']:
            require(set(method) in ({'name', 'descriptor', 'access'},
                    {'name', 'descriptor', 'access', 'code_bytes', 'code_sha256'}), 'closed method pin')
            identities.append((method['name'], method['descriptor']))
            if 'code_sha256' in method:
                require(type(method['code_bytes']) is int and method['code_bytes'] >= 1 and
                        re.fullmatch('[0-9a-f]{64}', method['code_sha256']), 'selected original Code pin')
                methods += 1
        require(len(identities) == len(set(identities)), 'unique source methods')
    spec = importlib.util.spec_from_file_location('chest_data_projection', directory / 'projection.py')
    projection = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(projection)
    require(projection.tokens((directory / 'native-observations.h').read_text(encoding='utf-8')) ==
            projection.tokens(projection.header(cases['cases'], finite['rows'])),
            'native tables derive only from exact declarations and original observations')
    return {'original_rows': len(finite['rows']), 'classes': len(CORE), 'Code_pins': methods,
            'processes': 2, 'native_qualification': 'PENDING'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packet', type=Path, default=HERE)
    print(audit(parser.parse_args().packet))
