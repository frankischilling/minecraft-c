"""Strict Java-free reader for entered Player wake and ordinary Flower removal."""
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


F = module('flower_wake_foot_support', FOOT / 'packet.py')
P = F.P
SCHEMA = 'FLOWER_WAKE_V1'
SCOPE = 'Three prepared unchanged-original Flower wake calls; separate native controller selects 140 scalars; wider source observations remain unpaired.'
PROVENANCE = 'FLOWER_WAKE_PROVENANCE_V1'
SOURCES = ('FlowerWakeObservation.java', 'capture_reference.py', 'packet.py',
           'test_packet.py', 'README.md', 'flower-pins.json')
SHARED = {**F.SHARED, 'foot_packet': FOOT / 'packet.py', 'foot_item_pins': FOOT / 'hl-pins.json'}
HELPERS = {'ConstructorJson.class', 'FlowerWakeObservation.class', 'FlowerWakeObservation$Observer.class',
           'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class',
           'PlayerDamageWakeObservation$Guard.class'}
BINARIES = ('client', 'java', 'javac', 'javap', 'jvm', 'modules', 'python', *P.LIBRARIES)
EXTRA = {'flower', 'flower_floor', 'light_inputs', 'foot', 'world_random', 'player_random', 'items'}
ITEM_KEYS = {'position', 'motion', 'dimensions', 'bounds', 'yaw', 'phase', 'delay', 'age',
             'health', 'stack', 'inserted', 'removed', 'world_same'}
HEAD_STACK = ['fd.j', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
ADDED_STACK = ['fd.c', 'fd.b', 'uu.a', 'uu.a', 'uu.g', 'wb.h', 'wb.b', 'fd.l', 'fd.i',
               'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
FLOWER_STACK = ['fd.j', 'fd.g', 'fd.f', 'wb.h', 'wb.b', 'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
MODES = ('supported', 'unsupported', 'publication-error')
LIGHT_KEYS = {'position', 'sky_nibble', 'block_light_nibble', 'height', 'world_f_sky_subtraction',
              'fd_m_chunk_subtraction_literal', 'chunk_sky_seen_static'}
SELECTED_METHODS = {
    ('wb', 'b', '(Lfd;IIII)V'): (21, 'b6baad1292a80e407a7f03539725c006b6b9a58cd4ff05729ec7ce78c9418feb'),
    ('wb', 'h', '(Lfd;III)V'): (40, 'a35c065c914aea7bf7b468f9ce791ff1cf8903541a3773928086f676ecd85d9e'),
    ('wb', 'g', '(Lfd;III)Z'): (47, 'a58d474aaeb1259ec2c8c38afbd58ec48bbfeb41d5c6f70a242f1b34097ca11a'),
    ('wb', 'd', '(I)Z'): (36, '30f0544d9c6d19694c826eecb86ac16e88281a8cbe56c1e30fa7ba40d9ce7da2'),
    ('fd', 'm', '(III)I'): (40, 'e8d3c8bc8a8de9c01971db49430fc163a78d2baa22e251f38fd69fb90ab68b75'),
    ('fd', 'l', '(III)Z'): (23, '5eca8ade103fa2e0184bee3f31e4ddebfce2f709f1158181dce9b8ac5fa6ae1d'),
    ('lm', 'c', '(IIII)I'): (54, '49049b63263bbbc9c5b5602dd8dec9c7ada7c3e3bf54358ebedde6c9d31641e5'),
    ('wi', 'a', '(III)I'): (54, '46e3f6fba772909639a20b5ac217a9a860c133c6d9ab006c9b8ed70666a2613c'),
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


def class_record(value, name, size, digest, parent):
    P.require(type(value) is dict and set(value) == {'name', 'super', 'interfaces', 'major', 'minor',
              'access', 'class_sha256', 'class_bytes', 'fields', 'methods'} and value['name'] == name and
              value['super'] == parent and value['interfaces'] == [] and P.exact(value['major'], 49) and
              P.exact(value['minor'], 0) and P.integer(value['access']) and P.exact(value['class_bytes'], size) and
              value['class_sha256'] == digest and type(value['fields']) is list and type(value['methods']) is list,
              'closed original Flower/light class identity')
    for field in value['fields']:
        P.require(type(field) is dict and set(field) == {'access', 'name', 'descriptor'} and
                  P.integer(field['access']) and all(type(field[k]) is str for k in ('name', 'descriptor')),
                  'closed original field')
    base = {'access', 'name', 'descriptor'}
    code = {'code_length', 'code_sha256', 'code_attribute_sha256', 'max_stack', 'max_locals', 'exception_table'}
    for method in value['methods']:
        P.require(type(method) is dict and set(method) in (base, base | code) and P.integer(method['access']) and
                  all(type(method[k]) is str for k in ('name', 'descriptor')), 'closed original method')
        if 'code_length' in method:
            P.require(P.integer(method['code_length']) and 0 < method['code_length'] <= 65535 and
                      P.word(method['code_sha256'], 64) and P.word(method['code_attribute_sha256'], 64) and
                      all(P.integer(method[k]) and 0 <= method[k] <= 65535 for k in ('max_stack', 'max_locals')) and
                      type(method['exception_table']) is list, 'typed original Code')
            for handler in method['exception_table']:
                P.require(type(handler) is dict and set(handler) == {'start', 'end', 'handler', 'catch'} and
                          all(P.integer(handler[k]) for k in ('start', 'end', 'handler')) and
                          0 <= handler['start'] < handler['end'] <= method['code_length'] and
                          0 <= handler['handler'] < method['code_length'] and
                          (handler['catch'] is None or type(handler['catch']) is str), 'typed original handler')


def class_pins():
    pins = F.class_pins()
    extra = P.load(HERE / 'flower-pins.json')
    P.require(type(extra) is dict and set(extra) == {'archive_sha256', 'classes', 'methods'} and
              extra['archive_sha256'] == P.JAR and type(extra['classes']) is dict and
              set(extra['classes']) == {'wb', 'wi'}, 'closed original structural pins')
    identities = {
        'wb': (1313, 'c843c6873acbc51586f64468ab2635341caf44109f7ae4ec410a414dca3543da', 'uu'),
        'wi': (618, 'ef5df5cdcd5ba63d8802f5bc539c8e164681eb173ef74586c369ae483415bb50', 'java/lang/Object')}
    for name, (size, digest, parent) in identities.items():
        P.require(name not in pins['classes'], 'distinct original class inventory')
        class_record(extra['classes'][name], name, size, digest, parent)
    expected = {owner + '.' + name + descriptor: {'code_length': length, 'code_sha256': digest}
                for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items()}
    P.require(P.exact(extra['methods'], expected), 'exact selected original Code authorities')
    return {'archive': pins['archive'], 'classes': {**pins['classes'], **extra['classes']}}


def method_pins():
    classes = class_pins()['classes']
    result = F.method_pins()
    for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items():
        methods = [m for m in classes[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        P.require(len(methods) == 1 and P.exact(methods[0]['code_length'], length) and
                  methods[0]['code_sha256'] == digest, 'unchanged Flower/light method Code')
        result[owner + '.' + name + descriptor] = {key: methods[0][key] for key in ('code_length', 'code_sha256')}
    return result


def trace(value):
    P.require(type(value) is list and 0 < len(value) <= 32, 'bounded actual stack')
    for frame in value:
        P.require(type(frame) is dict and set(frame) == {'class', 'method', 'file', 'line'} and
                  all(type(frame[k]) is str and 0 < len(frame[k]) <= 256 for k in ('class', 'method')) and
                  (frame['file'] is None or type(frame['file']) is str and len(frame['file']) <= 256) and
                  P.integer(frame['line']), 'typed actual stack frame')
    return [f['class'] + '.' + f['method'] for f in value if f['class'] in {'fd', 've', 'uu', 'wb', 'gs'}]


def item(value):
    P.require(type(value) is dict and set(value) == ITEM_KEYS, 'closed actual Item')
    for key, width, size in (('position', 16, 3), ('motion', 16, 3), ('dimensions', 8, 3), ('bounds', 16, 6)):
        P.require(P.vector(value[key], size, lambda v: P.word(v, width)), 'Item raw ' + key)
    P.require(P.word(value['yaw'], 8) and P.word(value['phase'], 8) and
              P.exact([value[k] for k in ('delay', 'age', 'health', 'stack')], [10, 0, 5, [37, 1, 0]]) and
              value['inserted'] is True and value['removed'] is False and value['world_same'] is True,
              'actual inserted Flower Item')


def state(value):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('flower', 'flower_floor', 'foot'):
        P.require(P.vector(value[key], 2, P.integer) and 0 <= value[key][0] <= 255 and
                  0 <= value[key][1] <= 15, 'typed cell ' + key)
    light = value['light_inputs']
    P.require(type(light) is dict and set(light) == LIGHT_KEYS and
              P.exact(light['position'], [7, 64, 8]) and
              P.exact([light[k] for k in ('sky_nibble', 'block_light_nibble', 'height',
                      'world_f_sky_subtraction', 'fd_m_chunk_subtraction_literal')], [15, 0, 64, 5, 0]) and
              type(light['chunk_sky_seen_static']) is bool, 'exact typed raw light inputs, not a forced getter')
    for key, original in (('world_random', value['random'][1]), ('player_random', value['random'][0])):
        P.require(P.exact(value[key], original), 'same complete Random observation ' + key)
    P.require(type(value['items']) is list and len(value['items']) <= 1, 'bounded Item list')
    for observed in value['items']:
        item(observed)


def validate(rows):
    P.require(type(rows) is list and len(rows) == 3, 'three ordered original calls')
    for row, mode in zip(rows, MODES):
        P.require(type(row) is dict and set(row) == {'mode', 'prepared', 'before', 'after', 'result', 'exception',
                  'exception_message', 'exception_stack', 'events'} and row['mode'] == mode, 'closed call row')
        supported, normal = mode == 'supported', mode != 'publication-error'
        floor = [2 if supported else 0, 0]
        P.require(P.exact(row['prepared'], {'shared_setup': [0, 0, 20, 0, 0],
                  'post_setup_cell_assignments': [[7, 64, 8, 37, 0], [7, 63, 8, floor[0], 0]],
                  'light_or_sky_overrides': []}), 'all declared finite input assignments')
        before, after = row['before'], row['after']
        state(before)
        state(after)
        P.require(P.exact([before[k] for k in ('head', 'flower', 'flower_floor', 'foot', 'sleeping',
                          'timer', 'health', 'idle', 'world_sleeping', 'items')],
                          [[26, 12], [37, 0], floor, [26, 0], True, 37, 20, 17, True, []]) and
                  before['light_inputs']['chunk_sky_seen_static'] is (not supported),
                  'prepared actual sleep, Bed, Flower and declared substrate')
        P.require(P.exact(row['result'], False if normal else None) and
                  row['exception'] == ('NONE' if normal else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if normal else 'Flower Item observer interruption'),
                  'actual original outcome')
        P.require(P.exact([after[k] for k in ('head', 'flower', 'flower_floor', 'foot', 'sleeping',
                          'timer', 'world_sleeping', 'health', 'idle')],
                          [[26, 8], [0, 0] if mode == 'unsupported' else [37, 0], floor, [26, 0],
                           not normal, 0 if normal else 37, not normal, 20, 0]) and
                  len(after['items']) == (0 if supported else 1) and
                  after['world_entities'] == before['world_entities'] + (0 if supported else 1) and
                  after['light_inputs']['chunk_sky_seen_static'] is True,
                  'committed survival/drop prefix and actual wake tail')
        P.require(P.exact(after['position'], ['401e000000000000', '40506e1480000000', '401e000000000000']
                          if normal else before['position']) and
                  P.exact(after['dimensions'], ['3f19999a', '3fe66666', '3fcf5c29']), 'actual Player pose/dimensions')
        P.require(P.exact(after['player_random'], before['player_random']) and
                  after['world_random'][0] == (before['world_random'][0] if supported else '00002f08d408505d') and
                  P.exact(after['world_random'][1:], before['world_random'][1:]), 'actual separate Random consumption')
        for plane in ('sky', 'block_light', 'height'):
            P.require(P.exact(before['planes'][plane], after['planes'][plane]), 'unchanged prepared light/height plane')
        events = row['events']
        expected_kinds = ['cell'] if supported else ['cell', 'added', 'cell'] if normal else ['cell', 'added']
        P.require(type(events) is list and len(events) == len(expected_kinds) and
                  all(type(e) is dict and 'kind' in e for e in events) and
                  [e['kind'] for e in events] == expected_kinds, 'actual drop before notifying removal')
        for index, event in enumerate(events):
            added = index == 1
            keys = {'kind', 'head', 'flower', 'floor', 'sleeping', 'stack', 'item' if added else 'xyz'}
            P.require(set(event) == keys and P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                      P.exact(event['flower'], [0, 0] if index == 2 else [37, 0]) and
                      P.exact(event['floor'], floor) and
                      P.exact(trace(event['stack']), (HEAD_STACK, ADDED_STACK, FLOWER_STACK)[index]),
                      'actual callback state and source stack')
            if added:
                P.require(P.exact(event['item'], after['items'][0]), 'published actual Item persists')
            else:
                P.require(P.exact(event['xyz'], [8, 64, 8] if index == 0 else [7, 64, 8]), 'actual cell coordinate')
        if normal:
            P.require(P.exact(row['exception_stack'], []), 'no thrown exception')
        else:
            P.require(P.exact(trace(row['exception_stack']), ADDED_STACK) and
                      row['exception_stack'][0]['class'] == 'FlowerWakeObservation$Observer', 'actual thrown source stack')


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
              P.word(proof['capture_sha256'], 64) and P.exact(proof['class_count'], 32) and
              P.exact(proof['rows_per_vm'], 3) and
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
    print('Original Flower packet and bindings pass; native140-scalar projection checked separately; wider observations unpaired')
    return value


if __name__ == '__main__':
    check()
