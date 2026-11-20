"""Closed CURRENT203 decoding and observable-state comparison."""
import collections
import copy
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[3]
REFERENCE = ROOT / "tests/reference/world/redstone_torch"
COHORTS = {'lifecycle': 70, 'powered': 32, 'burnout': 78, 'aging': 23}
PRELUDE = b'16 achievements\r\n151 recipes\r\n' if os.name == 'nt' else b'16 achievements\n151 recipes\n'
LENGTHS = (32768, 16384, 16384, 16384, 256)
ROW_KEYS = set('cohort index name values native center_before center_after before after changes events'.split())
OWN_KEYS = set('cohort index name mode owners_before owners_after supplied_alias math_before math_after seed_uniquifier_before seed_uniquifier_after'.split())
NATIVE_KEYS = set('invoke_ok entered returned value observer_refused source_null native_failure error'.split())
ERROR_KEYS = set('status operation hook_class hook_method hook_pc cause_hook_class cause_hook_method cause_hook_pc db_sites db_source_null vm_source_null mod_count_stored collection_status collection_failure info_status world_pc chunk_pc world_access_pc schedule_pc cause registry schedule'.split())

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


def need(value, message):
    if not value:
        raise ValueError(message)


def unique(pairs):
    value = {}
    for key, item in pairs:
        need(key not in value, 'duplicate JSON key: ' + key)
        value[key] = item
    return value


def invalid_constant(value):
    raise ValueError('non-JSON constant: ' + value)


def loads(value):
    return json.loads(value, object_pairs_hook=unique, parse_constant=invalid_constant)


def read(path):
    return loads(Path(path).read_text(encoding='utf-8'))


def same(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(same(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    return a == b


def exact(a, b, label):
    need(same(a, b), label + ': ' + repr(a)[:500] + ' != ' + repr(b)[:500])


def keys(value, expected, label):
    need(type(value) is dict and set(value) == set(expected), label + ' closed keys')


def integer(value, bits=64):
    return type(value) is int and -(1 << (bits - 1)) <= value < (1 << (bits - 1))


def identity(value):
    return value is None or (type(value) is str and re.fullmatch(r'(p:[1-9a-f][0-9a-f]*|hb:[0-9]+|xu:[1-9][0-9]*)', value) is not None)


def random_state(value):
    keys(value, ('seed', 'gaussian', 'have'), 'Random')
    need(type(value['seed']) is str and re.fullmatch('[0-9a-f]{12}', value['seed']) and
         type(value['gaussian']) is str and re.fullmatch('[0-9a-f]{16}', value['gaussian']) and
         type(value['have']) is bool, 'actual Random state bits')


def tree(path):
    return {p.relative_to(path).as_posix(): digest(p) for p in sorted(path.rglob('*')) if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='ascii', newline='\n')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def packet():
    return module('db203_public_reference', REFERENCE / 'packet.py')


def inputs():
    p = packet()
    p.audit()
    declaration = p.declared_inputs()
    for cohort, count in COHORTS.items():
        raw = (REFERENCE / (cohort + '-profiles.tsv')).read_bytes()
        need(raw.endswith(b'\n') and b'\r' not in raw, 'literal LF profiles')
        rows = []
        for line in raw.decode('ascii').splitlines():
            fields = line.split('\t')
            need(len(fields) == 16 and re.fullmatch('[a-z0-9-]+', fields[0]) and
                 all(re.fullmatch('0|-?[1-9][0-9]*', v) for v in fields[1:]), 'closed signed profile fields')
            values = [int(v) for v in fields[1:]]
            need(all(integer(v, 32) for v in values), 'Java input range')
            rows.append({'name': fields[0], 'values': values})
        exact(rows, declaration['cohorts'][cohort], 'complete literal roster')
        need(len(rows) == count and len({r['name'] for r in rows}) == count, 'unique fixed cohort')
    return declaration


def receipt(value, payload, stem, timeout):
    need(type(value['exit']) is int and value['exit'] == 0 and value['timed_out'] is False and
         value['pipe_capture_complete'] is True and value['reader_errors'] == {} and value['output_overflow'] is False and
         type(value['timeout_seconds']) is int and value['timeout_seconds'] == timeout, 'actual bounded complete process')
    for channel, limit in (('stdout', 4194304), ('stderr', 1048576)):
        need(value[channel + '_file'] == stem + '.' + channel and type(value[channel + '_bytes']) is int and
             len(payload[channel]) == value[channel + '_bytes'] and len(payload[channel]) < limit and
             value[channel + '_limit'] == limit and hashlib.sha256(payload[channel]).hexdigest() == value[channel + '_sha256'],
             'actual raw pipe identity')


def parse_current(raw, cohort, profiles):
    need(raw.startswith(PRELUDE), 'exact CURRENT constructor prelude')
    payload = raw[len(PRELUDE):]
    lines = payload.decode('ascii').splitlines()
    need(payload == ('\n'.join(lines) + '\n').encode('ascii'), 'exact CURRENT LF record transport')
    count = 6 if cohort == 'aging' else 1
    need(len(lines) == count + 2 * len(profiles) + 1, 'closed CURRENT full cohort line count')
    worlds = []
    for index in range(count):
        need(lines[index].startswith('DB_CURRENT_WORLD\t'), 'ordered CURRENT constructor')
        value = loads(lines[index].split('\t', 1)[1])
        keys(value, ('role', 'time', 'loaded', 'identity', 'uniquifier_before', 'uniquifier_after', 'math_before', 'math_after'), 'constructor')
        need(type(value['role']) is int and value['role'] == index and integer(value['time']) and value['loaded'] is True and
             identity(value['identity']) and value['identity'] is not None, 'actual CURRENT loaded World')
        need(all(type(value[k]) is str and re.fullmatch('[0-9a-f]{16}', value[k])
                 for k in ('uniquifier_before', 'uniquifier_after')), 'actual constructor allocation counters')
        for key in ('math_before', 'math_after'):
            if value[key] is not None: random_state(value[key])
        worlds.append(value)
    rows, own = [], []
    for index, declared in enumerate(profiles):
        at = count + index * 2
        need(lines[at].startswith('DB_CURRENT\t') and lines[at + 1].startswith('DB_CURRENT_ENTROPY\t'), 'ordered CURRENT row/entropy pair')
        row, state = [loads(lines[n].split('\t', 1)[1]) for n in (at, at + 1)]
        keys(row, ROW_KEYS, 'CURRENT row'); keys(state, OWN_KEYS, 'CURRENT entropy')
        need(type(row['index']) is int and row['index'] == index and row['cohort'] == cohort and row['name'] == declared['name'] and
             same(row['values'], declared['values']) and all(integer(v, 32) for v in row['values']), 'CURRENT frozen row identity')
        need(state['index'] == index and type(state['index']) is int and state['cohort'] == cohort and state['name'] == row['name'] and
             type(state['mode']) is int and state['mode'] == row['values'][11], 'CURRENT entropy row identity')
        keys(row['native'], NATIVE_KEYS, 'CURRENT native outcome')
        need(all(type(row['native'][k]) is bool for k in NATIVE_KEYS - {'error'}), 'typed native outcome booleans')
        need(not row['native']['entered'] or row['native']['error'] is not None, 'entered call retains actual native diagnostic')
        if row['native']['error'] is not None:
            keys(row['native']['error'], ERROR_KEYS, 'CURRENT native diagnostic')
            validate_error(row['native']['error'])
        for snap in (row['before'], row['after']):
            validate_snapshot(snap, count)
        for owners in (state['owners_before'], state['owners_after']):
            keys(owners, ('worlds', 'independent'), 'CURRENT random owners')
            need(type(owners['worlds']) is list and len(owners['worlds']) == count, 'all CURRENT World Randoms')
            for role, owner in enumerate(owners['worlds']):
                keys(owner, ('role', 'alias', 'state'), 'World Random owner')
                need(type(owner['role']) is int and owner['role'] == role and identity(owner['alias']) and owner['alias'] is not None, 'actual World Random identity')
                random_state(owner['state'])
            keys(owners['independent'], ('alias', 'state'), 'independent owner')
            need(identity(owners['independent']['alias']) and owners['independent']['alias'] is not None, 'actual independent owner')
            random_state(owners['independent']['state'])
        for key in ('math_before', 'math_after'):
            if state[key] is not None: random_state(state[key])
        for event in row['events']:
            validate_event(event)
        need(all(type(state[k]) is str and re.fullmatch('[0-9a-f]{16}', state[k]) for k in ('seed_uniquifier_before', 'seed_uniquifier_after')), 'actual global allocation counters')
        rows.append(row); own.append(state)
    need(lines[-1].startswith('DB_CURRENT_FINISH\t'), 'CURRENT finish required')
    finish = loads(lines[-1].split('\t', 1)[1])
    keys(finish, ('cohort', 'rows', 'retired', 'terminal_chunk_counts'), 'CURRENT finish')
    need(finish['cohort'] == cohort and type(finish['rows']) is int and finish['rows'] == len(profiles) and finish['retired'] is True and
         type(finish['terminal_chunk_counts']) is list and len(finish['terminal_chunk_counts']) == count and
         all(type(v) is int and v >= 4 for v in finish['terminal_chunk_counts']), 'actual complete retirement')
    return worlds, rows, own, finish


def validate_error(value):
    for key in ('operation', 'hook_class', 'hook_method', 'cause_hook_class', 'cause_hook_method'):
        need(value[key] is None or type(value[key]) is str, 'native error text')
    for key in ('db_source_null', 'vm_source_null', 'mod_count_stored'):
        need(type(value[key]) is bool, 'native error boolean')
    for key in ERROR_KEYS - {'operation', 'hook_class', 'hook_method', 'cause_hook_class', 'cause_hook_method',
                             'db_source_null', 'vm_source_null', 'mod_count_stored', 'db_sites', 'cause', 'registry', 'schedule'}:
        need(integer(value[key], 32), 'native diagnostic integer')
    need(type(value['db_sites']) is list and len(value['db_sites']) == 6 and all(integer(v, 32) for v in value['db_sites']), 'six DB site fields')
    keys(value['cause'], ('status', 'operation', 'system_error'), 'native cause')
    need(integer(value['cause']['status'], 32) and type(value['cause']['system_error']) is int and
         (value['cause']['operation'] is None or type(value['cause']['operation']) is str), 'native cause fields')
    registry = value['registry']
    keys(registry, ('status', 'message', 'initializer_pc', 'block_id', 'constructor_class', 'constructor_pc', 'profile_status',
                    'active_initializer_class', 'active_initializer_pc', 'item_argument', 'format'), 'Registry cause')
    for key in ('message', 'constructor_class', 'active_initializer_class'):
        need(registry[key] is None or type(registry[key]) is str, 'Registry cause text')
    need(all(integer(registry[k], 32) for k in ('status', 'initializer_pc', 'block_id', 'constructor_pc', 'profile_status', 'active_initializer_pc', 'item_argument')), 'Registry cause integers')
    keys(registry['format'], ('status', 'offset', 'length', 'value', 'flags', 'conversion', 'date_time'), 'format cause')
    need(type(registry['format']['date_time']) is bool and all(type(registry['format'][k]) is int for k in ('status', 'offset', 'length', 'value', 'flags', 'conversion')), 'format cause types')
    keys(value['schedule'], ('status', 'operation', 'pc', 'presence_pc', 'provider_pc', 'record', 'membership', 'ordered'), 'scheduler prefix')
    need((value['schedule']['operation'] is None or type(value['schedule']['operation']) is str) and
         all(integer(value['schedule'][k], 32) for k in ('status', 'pc', 'presence_pc', 'provider_pc', 'record', 'membership', 'ordered')), 'scheduler prefix types')


def validate_event(event):
    common = {'kind', 'world', 'uniquifier', 'states'}
    extra = {'cell': {'xyz', 'cell'}, 'range': {'bounds'}, 'sound': {'name', 'position', 'volume', 'pitch'},
             'particle': {'name', 'position', 'motion'}, 'item': {'identity', 'stack', 'defaults', 'position', 'motion', 'yaw', 'bob', 'entity_random', 'math_post'}}
    need(type(event) is dict and event.get('kind') in extra, 'known native event kind')
    keys(event, common | extra[event['kind']], 'native event')
    need(type(event['world']) is int and 0 <= event['world'] < 6 and type(event['uniquifier']) is str and re.fullmatch('[0-9a-f]{16}', event['uniquifier']), 'native event ordinal owner fields')
    keys(event['states'], ('world', 'independent'), 'event Random states')
    random_state(event['states']['world']); random_state(event['states']['independent'])
    for field in ('position', 'motion'):
        if field in event:
            need(type(event[field]) is list and len(event[field]) == 3 and all(type(v) is str and re.fullmatch('[0-9a-f]{16}', v) for v in event[field]), 'raw event vector bits')
    for field in ('volume', 'pitch', 'yaw', 'bob'):
        if field in event: need(type(event[field]) is str and re.fullmatch('[0-9a-f]{8}', event[field]), 'raw float bits')
    if 'name' in event: need(type(event['name']) is str, 'actual event name')
    for field, width in (('xyz', 3), ('bounds', 6), ('stack', 3), ('defaults', 3)):
        if field in event: need(type(event[field]) is list and len(event[field]) == width and all(integer(v, 32) for v in event[field]), 'typed native event operands')
    if event['kind'] == 'cell':
        need(event['cell'] is None or (type(event['cell']) is list and len(event['cell']) == 2 and all(integer(v, 32) for v in event['cell'])), 'observed cell or explicit unobserved null')
    if event['kind'] == 'item':
        need(identity(event['identity']) and event['identity'] is not None, 'actual Item callback identity')
        random_state(event['entity_random'])
        need(event['math_post'] is not None, 'actual READY Item Math observation')
        random_state(event['math_post'])


def validate_snapshot(snap, world_count):
    keys(snap, ('arrays', 'worlds', 'history', 'position_set', 'sequence'), 'snapshot')
    need(type(snap['arrays']) is list and len(snap['arrays']) == world_count * 20 and
         type(snap['worlds']) is list and len(snap['worlds']) == world_count, 'complete canonical array/World inventory')
    for index, record in enumerate(snap['arrays']):
        need(type(record) is list and len(record) == 6 and record[:3] == [index // 20, index % 20 // 5, index % 5] and
             all(type(v) is int for v in record[:3]) and identity(record[3]) and record[3] is not None and
             type(record[4]) is int and record[4] == LENGTHS[index % 5] and type(record[5]) is str and re.fullmatch('[0-9a-f]{64}', record[5]), 'typed complete array identity/hash')
    for role, world in enumerate(snap['worlds']):
        keys(world, ('role', 'identity', 'time', 'a', 'B', 'o', 's', 'M', 'light', 'ordered', 'membership', 'entities'), 'CURRENT World')
        need(type(world['role']) is int and world['role'] == role and integer(world['time']) and integer(world['M'], 32) and
             all(type(world[k]) is bool for k in ('a', 'B', 'o', 's')) and identity(world['identity']), 'literal typed World fields including M')
        keys(world['light'], ('identity', 'capacity', 'records'), 'CURRENT C')
        need(type(world['light']['capacity']) is int and world['light']['capacity'] >= len(world['light']['records']), 'CURRENT C capacity')
        for value in world['light']['records']:
            keys(value, ('identity', 'region'), 'CURRENT C region')
            need(identity(value['identity']) and type(value['region']) is list and len(value['region']) == 7 and
                 all(integer(v, 32) for v in value['region']) and value['region'][0] in (0, 1), 'actual C ordered values')
        for key in ('ordered', 'membership'):
            keys(world[key], ('identity', 'records'), 'CURRENT schedule collection')
            need(identity(world[key]['identity']), 'CURRENT scheduler owner')
            for value in world[key]['records']:
                keys(value, ('identity', 'xyz', 'block', 'time', 'order'), 'CURRENT QY')
                need(identity(value['identity']) and type(value['xyz']) is list and len(value['xyz']) == 3 and
                     all(integer(v, 32) for v in value['xyz']) and integer(value['block'], 32) and
                     integer(value['time']) and integer(value['order']), 'complete signed scheduled record')
        keys(world['entities'], ('identity', 'capacity', 'records'), 'CURRENT entities')
        need(identity(world['entities']['identity']) and type(world['entities']['capacity']) is int and
             world['entities']['capacity'] >= len(world['entities']['records']), 'CURRENT entity owner/capacity')
        for value in world['entities']['records']:
            keys(value, ('identity', 'source', 'phase', 'stack', 'defaults'), 'CURRENT Item')
            need(identity(value['identity']) and type(value['source']) is int and value['source'] == 0 and
                 type(value['phase']) is int and value['phase'] == 2 and
                 all(type(value[k]) is list and len(value[k]) == 3 and all(integer(v, 32) for v in value[k]) for k in ('stack', 'defaults')), 'CURRENT actual Item fields')
    h = snap['history']
    keys(h, ('owner', 'identity', 'backing', 'backing_kind', 'allocated_records', 'size', 'capacity', 'modCount', 'unused_slots_null', 'records'), 'CURRENT DB List')
    need(identity(h['owner']) and identity(h['backing']) and all(type(h[k]) is int for k in ('identity', 'backing_kind', 'allocated_records', 'size', 'capacity', 'modCount')) and
         0 <= h['size'] <= h['capacity'] <= 1024 and len(h['records']) == h['size'] and h['unused_slots_null'] is True, 'complete DB backing/size/null suffix')
    for value in h['records']:
        if value is None: continue
        keys(value, ('identity', 'constructed', 'xyz', 'time'), 'CURRENT XU')
        need(identity(value['identity']) and value['constructed'] is True and type(value['xyz']) is list and len(value['xyz']) == 3 and
             all(integer(v, 32) for v in value['xyz']) and integer(value['time']), 'actual constructed XU fields')
    p = snap['position_set']
    keys(p, ('owner', 'map', 'table', 'size', 'capacity', 'threshold', 'modCount', 'constructed_keys', 'load_factor', 'output', 'nodes'), 'CURRENT SM Set')
    need(all(identity(p[k]) for k in ('owner', 'map', 'table')) and all(type(p[k]) is int for k in ('size', 'capacity', 'threshold', 'modCount', 'constructed_keys')) and
         len(p['nodes']) == p['size'] and type(p['output']) is bool and type(p['load_factor']) is str and re.fullmatch('[0-9a-f]{8}', p['load_factor']), 'actual SM Set inventory')
    for value in p['nodes']:
        keys(value, ('identity', 'next', 'previous', 'parent', 'left', 'right', 'key_identity', 'xyz', 'hash', 'bucket', 'tree', 'red'), 'CURRENT SM node')
        need(all(type(value[k]) is int for k in ('identity', 'next', 'previous', 'parent', 'left', 'right', 'key_identity', 'hash', 'bucket')) and
             type(value['xyz']) is list and len(value['xyz']) == 3 and all(integer(v, 32) for v in value['xyz']) and
             type(value['tree']) is bool and type(value['red']) is bool, 'actual SM native node fields')
    keys(snap['sequence'], ('initialized', 'next'), 'CURRENT QY sequence')
    need(type(snap['sequence']['initialized']) is bool and integer(snap['sequence']['next']), 'passive QY phase/long counter')


class Relations:
    def __init__(self): self.forward, self.reverse = {}, {}
    def bind(self, domain, source_id, current_id):
        need(source_id is None or type(source_id) is int, domain + ' typed Source identity')
        if source_id in (0, None):
            need(current_id is None, domain + ' null correspondence'); return
        need(type(source_id) is int and source_id > 0 and current_id is not None, domain + ' actual nonnull identities')
        key, reverse = (domain, source_id), (domain, current_id)
        need(key not in self.forward or self.forward[key] == current_id, domain + ' retained source alias relation')
        need(reverse not in self.reverse or self.reverse[reverse] == source_id, domain + ' distinct source owner relation')
        self.forward[key], self.reverse[reverse] = current_id, source_id


def source_records(collection, ordered):
    values = [n['key'] for n in collection['nodes']]
    need(len(values) == collection['size'], 'complete Source scheduler topology retained')
    for value in values:
        need(value['class'] == 'qy', 'genuine Source scheduled record')
    return sorted(values, key=(lambda v: (v['time'], v['order'])) if ordered else
                  (lambda v: (*v['xyz'], v['block'], v['time'], v['order'])))


def compare_snapshot(raw, source_row, suffix, relations):
    arrays = source_row['arrays_' + suffix]
    need(len(arrays) == len(raw['arrays']), 'all arrays observed')
    for old, new in zip(arrays, raw['arrays']):
        exact(old[:3] + old[4:], new[:3] + new[4:], 'literal array coordinates/length/SHA256')
        relations.bind('arrays', old[3], new[3])
    for old, new in zip(source_row['worlds_' + suffix], raw['worlds']):
        relations.bind('worlds', old['alias'], new['identity'])
        exact({k: old[k] for k in ('role', 'time', 'a', 'B', 'o', 's', 'M')},
              {k: new[k] for k in ('role', 'time', 'a', 'B', 'o', 's', 'M')}, 'literal World fields including M')
        light = old['light']; current = new['light']
        relations.bind('C-owner', light['alias'], current['identity'])
        need(light['size'] == len(current['records']), 'ordered C size')
        for a, b in zip(light['slots'][:light['size']], current['records']):
            need(a['class'] == 'st', 'actual Source light region')
            exact(a['region'], b['region'], 'ordered C full region values')
            relations.bind('C-region', a['alias'], b['identity'])
        for source_key, native_key, ordered in (('scheduled_tree', 'ordered', True), ('scheduled_hash', 'membership', False)):
            a, b = old[source_key], new[native_key]
            relations.bind(source_key, a['alias'], b['identity'])
            left, right = source_records(a, ordered), b['records']
            if not ordered: right = sorted(right, key=lambda v: (*v['xyz'], v['block'], v['time'], v['order']))
            need(len(left) == len(right), 'scheduler membership/dedupe size')
            for x, y in zip(left, right):
                exact({k: x[k] for k in ('xyz', 'block', 'time', 'order')},
                      {k: y[k] for k in ('xyz', 'block', 'time', 'order')}, 'scheduler signed fields/deadline/order')
                relations.bind('QY-record', x['alias'], y['identity'])
        entities = old['entities']; values = new['entities']['records']
        relations.bind('entity-list', entities['alias'], new['entities']['identity'])
        need(entities['size'] == len(values), 'ordered World entity list')
        for x, y in zip(entities['slots'][:entities['size']], values):
            need(x['class'] == 'hl', 'Source observed Item class')
            exact(x['stack'], y['stack'], 'persistent Item Stack fields'); exact(x['defaults'], y['defaults'], 'persistent Item defaults')
            relations.bind('entity', x['alias'], y['identity'])
    old, new = source_row['history_' + suffix], raw['history']
    relations.bind('DB-list', old['alias'], new['owner']); relations.bind('DB-backing', old['backing'], new['backing'])
    exact([old[k] for k in ('size', 'capacity', 'modCount')], [new[k] for k in ('size', 'capacity', 'modCount')], 'DB size/capacity/modCount')
    need(all(v is None for v in old['slots'][old['size']:]) and new['unused_slots_null'] is True, 'complete DB unused null slots')
    for x, y in zip(old['slots'][:old['size']], new['records']):
        if x is None or y is None:
            exact(x, y, 'nullable XU slot'); continue
        need(x['class'] == 'xu' and y['constructed'] is True, 'actual XU constructor fields')
        exact(x['xyz'], y['xyz'], 'XU xyz'); exact(x['time'], y['time'], 'XU signed time')
        relations.bind('XU-record', x['alias'], y['identity'])
    old, new = source_row['position_set_' + suffix], raw['position_set']
    relations.bind('SM-owner', old['alias'], new['owner'])
    exact([old[k] for k in ('size', 'modCount')], [new[k] for k in ('size', 'modCount')], 'SM logical size/modCount')
    left = sorted((tuple(n['key']['xyz']) for n in old['nodes']))
    right = sorted(tuple(n['xyz']) for n in new['nodes'])
    exact(left, right, 'SM complete position membership')
    for x, y in zip(sorted(old['nodes'], key=lambda n: tuple(n['key']['xyz'])), sorted(new['nodes'], key=lambda n: tuple(n['xyz']))):
        relations.bind('SM-key', x['key']['alias'], 'wf:' + str(y['key_identity']))
    qy = source_row['qy_' + suffix]
    exact(qy, {'initialized': raw['sequence']['initialized'],
               'counter': raw['sequence']['next'] if raw['sequence']['initialized'] else None}, 'passive QY phase/counter prefix')


def project_events(row):
    logical, raw, drops = [], [], []
    role, mode = row['values'][0], row['values'][11]
    for event in row['events']:
        need(type(event) is dict and event['world'] == role and type(event['world']) is int, 'actual event World/order')
        kind = event['kind']
        if kind == 'cell':
            logical.append({'event': 'cell', 'world': role, 'xyz': event['xyz'], 'cell': event['cell']})
        elif kind == 'range': logical.append(['range', role, *event['bounds']])
        else:
            ordinal = len(raw)
            common = {'event': kind, 'ordinal': ordinal, 'world': role, 'states': event['states']}
            if kind == 'sound':
                raw.append({**common, **{k: event[k] for k in ('name', 'position', 'volume', 'pitch')}})
                logical.append({'event': 'sound', 'world': role, **{k: event[k] for k in ('name', 'position', 'volume')}, 'raw_ordinal': ordinal})
            elif kind == 'particle':
                raw.append({**common, **{k: event[k] for k in ('name', 'position', 'motion')}})
                value = {'event': 'particle', 'world': role, 'name': event['name'], 'motion': event['motion'], 'raw_ordinal': ordinal}
                if mode == 1: value['position'] = event['position']
                logical.append(value)
            elif kind == 'item':
                raw.append({**common, 'alias': event['identity'], **{k: event[k] for k in ('position', 'motion', 'yaw', 'bob', 'entity_random', 'math_post')}})
                logical.append({'event': 'add', 'world': role, 'alias': event['identity'], 'raw_ordinal': ordinal})
                # The genuine native callback checks the allocated Item literal.
                # This is a semantic Item-kind join, not a native Java trace.
                drops.append(['hl', *event['stack'], *event['defaults']])
            else: raise ValueError('unplanned native event kind')
    return logical, raw, drops


def compare_outcome(native, old):
    need(native['invoke_ok'] and native['entered'] and not native['native_failure'], 'unqualified native error: ' + repr(native)[:700])
    need(native['source_null'] == (native['error']['db_source_null'] or native['error']['vm_source_null']), 'actual source-null diagnostic join')
    if old['exception'] == 'NONE':
        need(native['returned'] and not native['source_null'] and not native['observer_refused'] and
             native['error']['status'] == 0, 'actual returning outcome')
        if old['values'][1] in (4, 5): exact(native['value'], old['result'] == 'true', 'actual Boolean return')
        else: need(old['result'] == 'VOID', 'actual void return')
    elif old['exception'] == 'java.lang.NullPointerException':
        need(not native['returned'] and native['source_null'] and not native['observer_refused'] and
             native['error']['status'] != 0, 'actual typed source-null prefix')
    elif old['exception'] == 'java.lang.IllegalStateException':
        need(not native['returned'] and native['observer_refused'] and not native['source_null'] and
             native['error']['status'] == 7, 'actual native LIMIT callback prefix; not Java exception equality')
    else: raise ValueError('unplanned original outcome')


def compare_cohort(cohort, rows, own, original):
    need(len(rows) == len(own) == len(original['rows']) == COHORTS[cohort], 'complete compared cohort')
    relations = Relations()
    entropy_rows, entropy_own = [], []
    for index, (row, state, old) in enumerate(zip(rows, own, original['rows'])):
        compare_outcome(row['native'], old)
        exact(row['center_before'], old['center_before'], 'center before'); exact(row['center_after'], old['center_after'], 'center after')
        exact(row['changes'], old['changes'], 'complete ordered five-array sparse deltas')
        compare_snapshot(row['before'], old, 'before', relations); compare_snapshot(row['after'], old, 'after', relations)
        logical, raw, drops = project_events(row)
        need(len(logical) == len(old['events']), 'complete ordered callback prefix')
        for x, y in zip(old['events'], logical):
            if type(x) is dict and x.get('event') == 'add':
                relations.bind('entity', x['alias'], y['alias'])
                exact({k: v for k, v in x.items() if k != 'alias'}, {k: v for k, v in y.items() if k != 'alias'}, 'published Item callback order')
            else: exact(x, y, 'ordered logical callback (independent31 particles literal)')
        exact(old['drops'], drops, 'actual dropped Stack/defaults')
        entropy_rows.append({'index': index, 'name': row['name'], 'values': row['values'], 'world_role': row['values'][0], 'drops': drops})
        entropy_own.append({**state, 'supplied_alias': state['supplied_alias'] if state['supplied_alias'] is not None else 0, 'events': raw})
    audit = module('db203_native_entropy', REFERENCE / 'entropy.py')
    try: own_audit = {'status': 'PASS', 'value': audit.audit_cohort(entropy_rows, entropy_own)}
    except Exception as error: own_audit = {'status': 'UNQUALIFIED', 'error': type(error).__name__ + ': ' + str(error)}
    allocation = []
    for row, state in zip(rows, own):
        before, after = int(state['seed_uniquifier_before'], 16), int(state['seed_uniquifier_after'], 16)
        steps = None
        for count in range(65):
            if before == after: steps = count; break
            before = before * 1181783497276652981 & ((1 << 64) - 1)
        allocation.append({'index': row['index'], 'observed_default_constructor_steps': steps,
                           'published_items': sum(e['kind'] == 'item' for e in row['events'])})
        if steps is None: own_audit = {'status': 'UNQUALIFIED', 'error': 'unreconciled global default-constructor counter'}
    return {'deterministic_effects': 'PASS', 'rows': len(rows), 'native_own_entropy': own_audit, 'native_allocation_steps': allocation,
            'default_constructor_counts_qualified': False,
            'JDK_C_E_F_physical_topology_compared': False, 'original_ISE_equals_native_LIMIT': False,
            'unqualified': ['exact per-row default-constructor counts and cross-row counter joins',
                            'constructor host-clock seed reconstruction', 'unpublished Entity fields when no retained observation']}


def nbt_time(path):
    data = gzip.decompress(path.read_bytes()); at = 0
    def take(n):
        nonlocal at
        need(n >= 0 and at + n <= len(data), 'copied NBT boundary')
        value = data[at:at+n]; at += n; return value
    def number(fmt): return struct.unpack('>' + fmt, take(struct.calcsize('>' + fmt)))[0]
    def name(): return take(number('H')).decode('utf-8')
    def payload(kind):
        if kind in (1, 2, 3, 4, 5, 6): return number({1:'b', 2:'h', 3:'i', 4:'q', 5:'f', 6:'d'}[kind])
        if kind == 7: return take(number('i'))
        if kind == 8: return name()
        if kind == 9:
            child, length = number('B'), number('i'); need(0 <= length <= 100000, 'NBT list bound')
            return [payload(child) for _ in range(length)]
        if kind == 10:
            value = {}
            while True:
                child = number('B')
                if child == 0: return value
                key = name(); need(key not in value, 'duplicate NBT field'); value[key] = payload(child)
        if kind == 11: return [number('i') for _ in range(number('i'))]
        raise ValueError('unexpected copied NBT type')
    need(number('B') == 10, 'actual copied Compound root'); name(); value = payload(10)
    need(at == len(data) and integer(value['Data']['Time']), 'independently decoded copied Info Time')
    return value['Data']['Time']


