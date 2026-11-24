"""Closed original stdout/JSON reader; no game methods execute."""
import json
import re

EFFECT_KEYS = set('cohort index name values world_role time result exception center_before center_after arrays_before arrays_after changes worlds_before worlds_after history_before history_after position_set_before position_set_after qy_before qy_after events drops observer_refused'.split())
ENTROPY_KEYS = set('cohort index name mode owners_before owners_after supplied_alias math_before math_after seed_uniquifier_before seed_uniquifier_after events'.split())
EXCEPTION_KEYS = set('cohort index name exception frames declared_callback'.split())

def require(value, message):
    if not value:
        raise AssertionError(message)

def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result

def loads(value):

    def invalid(value):
        raise ValueError('non-JSON constant ' + value)
    return json.loads(value, object_pairs_hook=unique, parse_constant=invalid)

def same(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return set(a) == set(b) and all((same(a[k], b[k]) for k in a))
    if type(a) is list:
        return len(a) == len(b) and all((same(x, y) for x, y in zip(a, b)))
    return a == b

def decode(raw, cohort, expected, world_rows):
    lines = raw.decode('ascii').splitlines()
    require(lines[:2] == ['16 achievements', '151 recipes'], 'actual known Torch constructor envelope; retain mismatch')
    position = 2
    for role, world in enumerate(world_rows):
        require(lines[position].startswith('DB_WORLD\t'), 'ordered constructor receipt')
        row = loads(lines[position].split('\t', 1)[1])
        require(same(row, dict(role=role, name=world['name'], time=world['authored_nbt_time'], loaded=True)), 'actual loaded constructor time/role')
        position += 1
        require(position < len(lines) and lines[position].startswith('DB_WORLD_ENTROPY\t'), 'constructor allocation observation')
        constructor = loads(lines[position].split('\t', 1)[1])
        require(set(constructor) == {'role', 'seed_uniquifier_before', 'seed_uniquifier_after', 'math_before', 'math_after'} and type(constructor['role']) is int and (constructor['role'] == role), 'closed constructor entropy schema')
        for key in ('seed_uniquifier_before', 'seed_uniquifier_after'):
            require(type(constructor[key]) is str and re.fullmatch('[0-9a-f]{16}', constructor[key]), 'actual allocation counter')
        position += 1
    effects, own, exceptions = ([], [], [])
    for index, declared in enumerate(expected):
        parsed = []
        for tag in ('DB', 'DB_ENTROPY'):
            require(position < len(lines) and lines[position].startswith(tag + '\t'), 'ordered complete DB/entropy')
            value = loads(lines[position].split('\t', 1)[1])
            require(type(value) is dict and set(value) == (EFFECT_KEYS if tag == 'DB' else ENTROPY_KEYS), 'exact closed logical/entropy schema')
            require(type(value['index']) is int and value['index'] == index and (value['name'] == declared['name']) and (value['cohort'] == cohort), 'exact row identity')
            parsed.append(value)
            position += 1
        row, entropy = parsed
        require(same(row['values'], declared['values']), 'frozen actual input roster')
        require(all((type(row[key]) is int for key in ('world_role', 'time'))) and type(row['observer_refused']) is bool, 'typed actual Source scalar receipts')
        for key in ('qy_before', 'qy_after'):
            counter = row[key]
            require(type(counter) is dict and set(counter) == {'initialized', 'counter'} and (type(counter['initialized']) is bool) and (type(counter['counter']) is int if counter['initialized'] else counter['counter'] is None), 'passive QY phase/counter; no active cold read')
        require(row['exception'] in ('NONE', 'java.lang.NullPointerException', 'java.lang.IllegalStateException'), 'unplanned exception boundary')
        if row['exception'] != 'NONE':
            require(row['result'] == 'UNRETURNED', 'exception retains unreached helper return store')
            require(position < len(lines) and lines[position].startswith('DB_EXCEPTION\t'), 'complete reached exception frames')
            failure = loads(lines[position].split('\t', 1)[1])
            position += 1
            require(set(failure) == EXCEPTION_KEYS and failure['cohort'] == cohort and (type(failure['index']) is int) and (failure['index'] == index) and (failure['name'] == row['name']) and (failure['exception'] == row['exception']) and failure['frames'], 'actual exception identity/frames')
            if row['exception'] == 'java.lang.IllegalStateException':
                require(declared['values'][14] != 0 and row['observer_refused'] is True and (failure['frames'][0][0] == 'RedstoneTorchObservation$Observer') and (failure['frames'][0][1] == 'refusal'), 'declared callback refusal origin')
            else:
                require((declared['values'][10] == 1 or (declared['values'][1] == 3 and declared['values'][11] == 2)) and failure['frames'][0][0] in ('db', 'vm', 'fd'), 'reached genuine NPE boundary')
            exceptions.append(failure)
        else:
            require(row['result'] in (('true', 'false') if declared['values'][1] in (4, 5) else ('VOID',)), 'actual method descriptor return category')
        effects.append(row)
        own.append(entropy)
    require(position + 3 == len(lines), 'closed stdout and exactly three terminal receipts')
    require(re.fullmatch('DB_GUARD\\tsource\\tnetwork=0\\tnative=0\\texecs=0\\tdenied-writes=0\\tworld-writes=(0|[1-9][0-9]*)', lines[position]) is not None, 'exact Source external denial counters')
    require(lines[position + 1] == 'DB_GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\tdenied-writes=1', 'five denial controls')
    require(lines[position + 2] == 'COMPLETE\t' + cohort + '\t' + str(len(expected)), 'exact full completion')
    return (effects, own, exceptions)
