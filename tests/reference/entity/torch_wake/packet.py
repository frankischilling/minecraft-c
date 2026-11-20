"""Strict Java-free reader for entered Player wake and ordinary Torch removal."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
FOOT = HERE.parent / 'bed_foot_wake'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


F = module('torch_wake_foot_support', FOOT / 'packet.py')
P = F.P
SCHEMA = 'TORCH_WAKE_V1'
SCOPE = 'Two prepared unchanged-original Player wake calls; no ticks, GUI or native equality qualification.'
PROVENANCE = 'TORCH_WAKE_PROVENANCE_V1'
SOURCES = ('TorchWakeObservation.java', 'capture_reference.py', 'packet.py',
           'test_packet.py', 'README.md', 'vm-pins.json')
SHARED = {**F.SHARED, 'foot_packet': FOOT / 'packet.py', 'foot_item_pins': FOOT / 'hl-pins.json'}
HELPERS = {'ConstructorJson.class', 'TorchWakeObservation.class', 'TorchWakeObservation$Observer.class',
           'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class',
           'PlayerDamageWakeObservation$Guard.class'}
BINARIES = ('client', 'java', 'javac', 'javap', 'jvm', 'modules', 'python', *P.LIBRARIES)
EXTRA = {'torch', 'torch_support', 'torch_floor', 'foot', 'world_random', 'player_random', 'items'}
ITEM_KEYS = {'position', 'motion', 'dimensions', 'bounds', 'yaw', 'phase', 'delay', 'age',
             'health', 'stack', 'inserted', 'removed', 'world_same'}
HEAD_STACK = ['fd.j', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
ADDED_STACK = ['fd.c', 'fd.b', 'uu.a', 'uu.a', 'uu.g', 'vm.b', 'fd.l', 'fd.i',
               'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
TORCH_STACK = ['fd.j', 'fd.g', 'fd.f', 'vm.b', 'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
VM_METHODS = {
    ('b', '(Lfd;IIII)V'): (169, 'bad389f22d5f022071ad02c4ef5911f45563acfca65f289ef04dd353de89da92'),
    ('i', '(Lfd;III)Z'): (43, 'fe7b86d7d2850d8d1a4e100f778e6858ed54cb47a68a74d6c3b222aa9ebfabdc'),
    ('a', '(Lfd;III)Z'): (78, 'b2ee5d510420c33328f9c9ccbf5667285f3ae64fc0ba2ff5599abe94d1f52133'),
    ('h', '(Lfd;III)Z'): (34, 'a35ae41d5832a226fc5b51f12756bcfbe42f060d2e82f3a275b6ddbf4d857bc9'),
}
RECEIPT_KEYS = {'command_sha256', 'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete',
                'output_overflow', 'reader_errors', 'stdout_file', 'stderr_file', 'stdout_bytes',
                'stderr_bytes', 'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit'}


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def fingerprints(value, names, message):
    P.require(type(value) is dict and set(value) == set(names) and
              all(P.word(v, 64) for v in value.values()), message)


def class_pins():
    pins = F.class_pins()
    vm = P.load(HERE / 'vm-pins.json')
    P.require(type(vm) is dict and set(vm) == {'name', 'super', 'interfaces', 'major', 'minor',
              'access', 'class_sha256', 'class_bytes', 'fields', 'methods'} and vm['name'] == 'vm' and
              vm['super'] == 'uu' and vm['interfaces'] == [] and P.exact(vm['major'], 49) and
              P.exact(vm['minor'], 0) and P.integer(vm['access']) and P.exact(vm['class_bytes'], 2645) and
              vm['class_sha256'] == 'de491b271f4a805c89bf0132d8578f1fb22792959d1cd616ea68d9e0ffc1ae5c' and
              type(vm['fields']) is list and type(vm['methods']) is list and
              'vm' not in pins['classes'], 'closed original Torch class identity')
    for field in vm['fields']:
        P.require(type(field) is dict and set(field) == {'access', 'name', 'descriptor'} and
                  P.integer(field['access']) and all(type(field[k]) is str for k in ('name', 'descriptor')),
                  'closed original Torch field')
    base = {'access', 'name', 'descriptor'}
    code = {'code_length', 'code_sha256', 'code_attribute_sha256', 'max_stack', 'max_locals', 'exception_table'}
    for method in vm['methods']:
        P.require(type(method) is dict and set(method) in (base, base | code) and
                  P.integer(method['access']) and
                  all(type(method[k]) is str for k in ('name', 'descriptor')), 'closed original Torch method')
        if 'code_length' in method:
            P.require(P.integer(method['code_length']) and 0 < method['code_length'] <= 65535 and
                      P.word(method['code_sha256'], 64) and P.word(method['code_attribute_sha256'], 64) and
                      all(P.integer(method[k]) and 0 <= method[k] <= 65535 for k in ('max_stack', 'max_locals')) and
                      type(method['exception_table']) is list, 'typed original Torch Code')
            for handler in method['exception_table']:
                P.require(type(handler) is dict and set(handler) == {'start', 'end', 'handler', 'catch'} and
                          all(P.integer(handler[k]) for k in ('start', 'end', 'handler')) and
                          0 <= handler['start'] < handler['end'] <= method['code_length'] and
                          0 <= handler['handler'] < method['code_length'] and
                          (handler['catch'] is None or type(handler['catch']) is str),
                          'typed original Torch handler')
    return {'archive': pins['archive'], 'classes': {**pins['classes'], 'vm': vm}}


def method_pins():
    vm = class_pins()['classes']['vm']
    result = F.method_pins()
    for (name, descriptor), (length, digest) in VM_METHODS.items():
        methods = [m for m in vm['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        P.require(len(methods) == 1 and P.exact(methods[0]['code_length'], length) and
                  methods[0]['code_sha256'] == digest, 'unchanged reached Torch method Code')
        result['vm.' + name + descriptor] = {
            key: methods[0][key] for key in ('code_length', 'code_sha256')}
    return result


def trace(value):
    P.require(type(value) is list and 0 < len(value) <= 32, 'bounded actual stack')
    for frame in value:
        P.require(type(frame) is dict and set(frame) == {'class', 'method', 'file', 'line'} and
                  all(type(frame[k]) is str and 0 < len(frame[k]) <= 256 for k in ('class', 'method')) and
                  (frame['file'] is None or type(frame['file']) is str and len(frame['file']) <= 256) and
                  P.integer(frame['line']), 'typed actual stack frame')
    return [f['class'] + '.' + f['method'] for f in value if f['class'] in {'fd', 've', 'uu', 'vm', 'gs'}]


def item(value):
    P.require(type(value) is dict and set(value) == ITEM_KEYS, 'closed actual Item')
    for key, width, size in (('position', 16, 3), ('motion', 16, 3), ('dimensions', 8, 3), ('bounds', 16, 6)):
        P.require(P.vector(value[key], size, lambda v: P.word(v, width)), 'Item raw ' + key)
    P.require(P.word(value['yaw'], 8) and P.word(value['phase'], 8) and
              P.exact([value[k] for k in ('delay', 'age', 'health', 'stack')], [10, 0, 5, [50, 1, 0]]) and
              value['inserted'] is True and value['removed'] is False and value['world_same'] is True,
              'actual inserted Torch Item')


def state(value):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('torch', 'torch_support', 'torch_floor', 'foot'):
        P.require(P.vector(value[key], 2, P.integer) and 0 <= value[key][0] <= 255 and
                  0 <= value[key][1] <= 15, 'typed cell ' + key)
    for key, original in (('world_random', value['random'][1]), ('player_random', value['random'][0])):
        P.require(P.exact(value[key], original), 'same complete Random observation ' + key)
    P.require(type(value['items']) is list and len(value['items']) <= 1, 'bounded Item list')
    for observed in value['items']:
        item(observed)


def validate(rows):
    P.require(type(rows) is list and len(rows) == 2, 'two ordered original calls')
    for row, mode in zip(rows, ('success', 'publication-error')):
        P.require(type(row) is dict and set(row) == {'mode', 'before', 'after', 'result', 'exception',
                  'exception_message', 'exception_stack', 'events'} and row['mode'] == mode, 'closed call row')
        success = mode == 'success'
        before, after = row['before'], row['after']
        state(before)
        state(after)
        P.require(P.exact(before, rows[0]['before']) and
                  P.exact([before[k] for k in ('head', 'torch', 'torch_support', 'torch_floor', 'foot',
                          'sleeping', 'timer', 'health', 'idle', 'world_sleeping', 'items')],
                          [[26, 12], [50, 1], [0, 0], [1, 0], [26, 0], True, 37, 20, 17, True, []]),
                  'prepared actual sleep, Bed, west air and Stone floor')
        P.require(P.exact(row['result'], False if success else None) and
                  row['exception'] == ('NONE' if success else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if success else 'Torch Item observer interruption'),
                  'actual original outcome')
        P.require(P.exact([after[k] for k in ('head', 'torch', 'torch_support', 'torch_floor', 'foot',
                          'sleeping', 'timer', 'world_sleeping', 'health', 'idle')],
                          [[26, 8], [0, 0] if success else [50, 1], [0, 0], [1, 0], [26, 0],
                           not success, 0 if success else 37, not success, 20, 0]) and
                  len(after['items']) == 1 and after['world_entities'] == before['world_entities'] + 1,
                  'committed prefix and interrupted wake tail')
        P.require(P.exact(after['position'], ['401e000000000000', '40506e1480000000', '401e000000000000']
                          if success else before['position']) and
                  P.exact(after['dimensions'], ['3f19999a', '3fe66666', '3fcf5c29']), 'actual Player pose/dimensions')
        P.require(P.exact(after['player_random'], before['player_random']) and
                  after['world_random'][0] == '00002f08d408505d' and
                  P.exact(after['world_random'][1:], before['world_random'][1:]), 'actual separate Random consumption')
        events = row['events']
        P.require(type(events) is list and len(events) == (3 if success else 2) and
                  all(type(e) is dict and 'kind' in e for e in events) and
                  [e['kind'] for e in events] == (['cell', 'added', 'cell'] if success else ['cell', 'added']),
                  'actual drop before notifying removal')
        for index, event in enumerate(events):
            added = index == 1
            keys = {'kind', 'head', 'torch', 'west', 'floor', 'sleeping', 'stack', 'item' if added else 'xyz'}
            P.require(set(event) == keys and P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                      P.exact(event['torch'], [0, 0] if index == 2 else [50, 1]) and
                      P.exact(event['west'], [0, 0]) and P.exact(event['floor'], [1, 0]) and
                      P.exact(trace(event['stack']), (HEAD_STACK, ADDED_STACK, TORCH_STACK)[index]),
                      'actual callback state and source stack')
            if added:
                P.require(P.exact(event['item'], after['items'][0]), 'published actual Item persists')
            else:
                P.require(P.exact(event['xyz'], [8, 64, 8] if index == 0 else [7, 64, 8]), 'actual cell coordinate')
        if success:
            P.require(P.exact(row['exception_stack'], []), 'no thrown exception')
        else:
            P.require(P.exact(trace(row['exception_stack']), ADDED_STACK) and
                      row['exception_stack'][0]['class'] == 'TorchWakeObservation$Observer', 'actual thrown source stack')


def validate_finite(value):
    P.require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and
              value['schema'] == SCHEMA and value['scope'] == SCOPE, 'closed finite envelope')
    validate(value['rows'])


def public_receipt(receipt):
    result = {key: receipt[key] for key in sorted(RECEIPT_KEYS - {'command_sha256'})}
    result['command_sha256'] = hashlib.sha256(
        json.dumps(receipt['command'], ensure_ascii=True, separators=(',', ':')).encode('ascii')).hexdigest()
    return result


def validate_provenance(proof, directory=HERE):
    directory = Path(directory)
    keys = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'capture_sha256', 'sources', 'shared',
            'methods', 'helpers', 'binaries', 'class_count', 'rows_per_vm', 'original',
            'inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof) == keys and proof['schema'] == PROVENANCE and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(directory / 'finite.json') and
              P.word(proof['capture_sha256'], 64) and P.exact(proof['class_count'], 31) and
              P.exact(proof['rows_per_vm'], 2) and
              all(proof[k] is True for k in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged',
                                            'class_pins_unchanged')),
              'closed original-only provenance')
    fingerprints(proof['sources'], SOURCES, 'closed authored source inventory')
    fingerprints(proof['shared'], SHARED, 'closed shared source inventory')
    fingerprints(proof['helpers'], HELPERS, 'closed compiled helper inventory')
    fingerprints(proof['binaries'], BINARIES, 'closed binary inventory')
    P.require(P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()) and proof['binaries']['client'] == P.JAR and
              all(proof['binaries'][name] == expected for name, expected in P.LIBRARIES.items()),
              'executed sources and original binary/Code identities')
    P.require(type(proof['original']) is list and len(proof['original']) == 2, 'two fresh VM receipts')
    for index, receipt in enumerate(proof['original']):
        P.require(type(receipt) is dict and set(receipt) == RECEIPT_KEYS and
                  P.exact(receipt['exit'], 0) and P.exact(receipt['timeout_seconds'], 70) and
                  receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and
                  receipt['output_overflow'] is False and type(receipt['reader_errors']) is dict and
                  receipt['reader_errors'] == {} and P.word(receipt['command_sha256'], 64),
                  'complete bounded original VM receipt')
        for stream, limit in (('stdout', 4194304), ('stderr', 1048576)):
            P.require(P.exact(receipt[stream + '_limit'], limit) and
                      P.integer(receipt[stream + '_bytes']) and 0 < receipt[stream + '_bytes'] <= limit and
                      receipt[stream + '_file'] == f'original-{index}.{stream}' and
                      P.word(receipt[stream + '_sha256'], 64), 'actual stream extent/hash')
    P.require(all(P.exact(proof['original'][0][key], proof['original'][1][key]) for key in
                  ('exit', 'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256')),
              'two matching complete original streams')


def read_reference(directory=HERE):
    directory = Path(directory)
    value = P.load(directory / 'finite.json')
    validate_finite(value)
    validate_provenance(P.load(directory / 'provenance.json'), directory)
    return value


def check(directory=HERE):
    value = read_reference(directory)
    print('Original Torch wake/drop packet and bindings pass; prepared CPU observations only')
    return value


if __name__ == '__main__':
    check()
