"""Strict Java-free reader for entered Player wake and Ladder removal."""
import importlib.util
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


F = module('ladder_wake_foot_support', FOOT / 'packet.py')
P = F.P
SCHEMA = 'LADDER_WAKE_V1'
SCOPE = 'Two prepared unchanged-original Player wake calls; no ticks, GUI or whole-game qualification.'
SOURCES = ('LadderWakeObservation.java', 'capture_reference.py', 'packet.py',
           'test_packet.py', 'README.md', 'dp-pins.json')
SHARED = {**F.SHARED, 'foot_packet': FOOT / 'packet.py', 'foot_item_pins': FOOT / 'hl-pins.json'}
EXTRA = {'ladder', 'ladder_support', 'foot', 'world_random', 'player_random', 'items'}
ITEM_KEYS = {'position', 'motion', 'dimensions', 'bounds', 'yaw', 'phase', 'delay', 'age',
             'health', 'stack', 'inserted', 'removed', 'world_same'}
HEAD_STACK = ['fd.j', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
ADDED_STACK = ['fd.c', 'fd.b', 'uu.a', 'uu.a', 'uu.g', 'dp.b', 'fd.l', 'fd.i',
               'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
LADDER_STACK = ['fd.j', 'fd.g', 'fd.f', 'dp.b', 'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def class_pins():
    pins = F.class_pins()
    dp = P.load(HERE / 'dp-pins.json')
    P.require(dp['name'] == 'dp' and 'dp' not in pins['classes'], 'separate original Ladder class')
    return {'archive': pins['archive'], 'classes': {**pins['classes'], 'dp': dp}}


def method_pins():
    dp = class_pins()['classes']['dp']
    methods = [m for m in dp['methods'] if m['name'] == 'b' and m['descriptor'] == '(Lfd;IIII)V']
    P.require(len(methods) == 1 and methods[0]['code_length'] == 139 and
              methods[0]['code_sha256'] == 'd861b7c1ad99cb7816bd130ae59ce56aa6a0894439504e923dcf0ff25ac2f443',
              'unchanged Ladder neighbor Code')
    return {**F.method_pins(), 'dp.b(Lfd;IIII)V': {
        key: methods[0][key] for key in ('code_length', 'code_sha256')}}


def trace(value):
    P.require(type(value) is list and 0 < len(value) <= 32, 'bounded actual stack')
    for frame in value:
        P.require(type(frame) is dict and set(frame) == {'class', 'method', 'file', 'line'} and
                  all(type(frame[k]) is str and 0 < len(frame[k]) <= 256 for k in ('class', 'method')) and
                  (frame['file'] is None or type(frame['file']) is str) and P.integer(frame['line']),
                  'typed actual stack frame')
    return [f['class'] + '.' + f['method'] for f in value if f['class'] in {'fd', 've', 'uu', 'dp', 'gs'}]


def item(value):
    P.require(type(value) is dict and set(value) == ITEM_KEYS, 'closed Item observation')
    for key, width, size in (('position', 16, 3), ('motion', 16, 3), ('dimensions', 8, 3), ('bounds', 16, 6)):
        P.require(P.vector(value[key], size, lambda v: P.word(v, width)), 'Item raw ' + key)
    P.require(P.word(value['yaw'], 8) and P.word(value['phase'], 8) and
              P.exact([value[k] for k in ('delay', 'age', 'health', 'stack')], [10, 0, 5, [65, 1, 0]]) and
              value['inserted'] is True and value['removed'] is False and value['world_same'] is True,
              'actual inserted Ladder Item')


def state(value):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('ladder', 'ladder_support', 'foot'):
        P.require(P.vector(value[key], 2, P.integer), 'typed cell ' + key)
    for key in ('world_random', 'player_random'):
        random = value[key]
        P.require(type(random) is list and len(random) == 3 and P.word(random[0], 16) and
                  int(random[0], 16) < 1 << 48 and type(random[1]) is bool and P.word(random[2], 16),
                  'complete Random ' + key)
    P.require(type(value['items']) is list and len(value['items']) <= 1, 'bounded Item list')
    for value in value['items']:
        item(value)


def validate(rows):
    P.require(type(rows) is list and len(rows) == 2, 'two ordered actual calls')
    for row, mode in zip(rows, ('success', 'publication-error')):
        P.require(type(row) is dict and set(row) == {'mode', 'before', 'after', 'result', 'exception',
                  'exception_message', 'exception_stack', 'events'} and row['mode'] == mode, 'closed call row')
        success = mode == 'success'
        before, after = row['before'], row['after']
        state(before)
        state(after)
        P.require(P.exact(before, rows[0]['before']) and
                  P.exact([before[k] for k in ('head', 'ladder', 'ladder_support', 'foot', 'sleeping', 'timer', 'items')],
                          [[26, 12], [65, 2], [0, 0], [26, 0], True, 37, []]), 'prepared Bed and unsupported Ladder')
        P.require(P.exact(row['result'], False if success else None) and
                  row['exception'] == ('NONE' if success else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if success else 'Ladder Item observer interruption'), 'actual outcome')
        P.require(P.exact([after[k] for k in ('head', 'ladder', 'sleeping', 'timer', 'world_sleeping', 'health', 'idle')],
                          [[26, 8], [0, 0] if success else [65, 2], not success, 0 if success else 37,
                           not success, 20, 0]) and len(after['items']) == 1, 'committed prefix and stopped tail')
        P.require(P.exact(after['player_random'], before['player_random']) and
                  after['world_random'][0] == '00002f08d408505d', 'separate Random consumption')
        events = row['events']
        P.require(type(events) is list and len(events) == (3 if success else 2) and
                  all(type(e) is dict for e in events) and
                  [e['kind'] for e in events] == (['cell', 'added', 'cell'] if success else ['cell', 'added']),
                  'drop before notifying removal')
        for index, event in enumerate(events):
            added = index == 1
            keys = {'kind', 'head', 'ladder', 'sleeping', 'stack', 'item' if added else 'xyz'}
            P.require(set(event) == keys and P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                      P.exact(event['ladder'], [0, 0] if index == 2 else [65, 2]) and
                      P.exact(trace(event['stack']), (HEAD_STACK, ADDED_STACK, LADDER_STACK)[index]),
                      'actual callback state and stack')
            if added:
                P.require(P.exact(event['item'], after['items'][0]), 'published Item persists')
            else:
                P.require(P.exact(event['xyz'], [8, 64, 8] if index == 0 else [7, 64, 8]), 'actual cell coordinates')
        if success:
            P.require(P.exact(row['exception_stack'], []), 'no thrown exception')
        else:
            P.require(P.exact(trace(row['exception_stack']), ADDED_STACK) and
                      row['exception_stack'][0]['class'] == 'LadderWakeObservation$Observer', 'actual thrown stack')


def validate_finite(value):
    P.require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and
              value['schema'] == SCHEMA and value['scope'] == SCOPE, 'closed finite envelope')
    validate(value['rows'])


def check():
    finite, proof = P.load(HERE / 'finite.json'), P.load(HERE / 'provenance.json')
    validate_finite(finite)
    P.require(type(proof) is dict and set(proof) == {'status', 'archive_sha256', 'finite_sha256',
              'capture_sha256', 'sources', 'shared', 'methods', 'helpers', 'original'} and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(HERE / 'finite.json') and P.word(proof['capture_sha256'], 64) and
              P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()), 'source, archive and Code bindings')
    helpers = {'ConstructorJson.class', 'LadderWakeObservation.class', 'LadderWakeObservation$Observer.class',
               'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class', 'PlayerDamageWakeObservation$Guard.class'}
    P.require(type(proof['helpers']) is dict and set(proof['helpers']) == helpers and
              all(P.word(v, 64) for v in proof['helpers'].values()), 'only declared authored helpers')
    keys = {'exit', 'timed_out', 'pipe_capture_complete', 'output_overflow', 'reader_errors', 'stdout_sha256', 'stderr_sha256'}
    P.require(type(proof['original']) is list and len(proof['original']) == 2 and
              all(type(r) is dict and set(r) == keys and P.exact(r['exit'], 0) and r['timed_out'] is False and
                  r['pipe_capture_complete'] is True and r['output_overflow'] is False and r['reader_errors'] == {} and
                  P.word(r['stdout_sha256'], 64) and P.word(r['stderr_sha256'], 64) for r in proof['original']) and
              P.exact(proof['original'][0], proof['original'][1]), 'two complete matching original calls')
    print('Original Ladder wake/drop packet and bindings pass')


if __name__ == '__main__':
    check()
