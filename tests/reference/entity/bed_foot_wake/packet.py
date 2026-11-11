"""Validate unchanged-original orphan Bed foot wake observations."""
import importlib.util
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PARENT = HERE.parent / 'player_damage_wake'
SOURCES = ('FootWakeObservation.java', 'capture_reference.py', 'packet.py', 'test_packet.py',
           'README.md', 'hl-pins.json')
SHARED = {
    'setup': PARENT / 'PlayerDamageWakeObservation.java',
    'class_pins': PARENT / 'source-pins.json',
    'packet': PARENT / 'packet.py',
    'item_pins': HERE.parent / 'item_tick/source-pins.json',
    'capture': ROOT / 'tests/reference/client/controller/capture.py',
    'classfile': HERE.parent / 'player/classfile.py',
    'json': HERE.parent / 'player/ConstructorJson.java',
}


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


P = module('bed_foot_wake_support', SHARED['packet'])
MODES = ('success', 'publication-error')
STATE_KEYS = {'position', 'dimensions', 'sleeping', 'timer', 'health', 'idle', 'world_sleeping',
              'head', 'orphan', 'world_random', 'player_random', 'items'}
ITEM_KEYS = {'position', 'motion', 'yaw', 'phase', 'delay', 'stack', 'inserted', 'world_same'}
HEAD_STACK = ['fd.j', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
ORPHAN_STACK = ['fd.j', 'fd.g', 'fd.f', 've.b', 'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
ADDED_STACK = ['fd.c', 'fd.b', 'uu.a', 'uu.a', 've.a', 'uu.g', 've.b',
               'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def class_pins():
    base = P.load(SHARED['class_pins'])
    item = P.load(HERE / 'hl-pins.json')
    earlier = P.load(SHARED['item_pins'])
    P.require(base['archive']['sha256'] == P.JAR and earlier['jar'] == base['archive'] and
              item['name'] == 'hl' and
              item['class_sha256'] == earlier['classes']['hl']['class_sha256'] and
              item['class_bytes'] == earlier['classes']['hl']['class_bytes'], 'original Item pin lineage')
    return {'archive': base['archive'], 'classes': {**base['classes'], 'hl': item}}


def method_pins():
    classes = class_pins()['classes']
    selected = (('gs', 'a', '(Lsn;I)Z'), ('gs', 'a', '(ZZZ)V'),
                ('ve', 'b', '(Lfd;IIII)V'), ('ve', 'a', '(Lfd;IIIIF)V'),
                ('uu', 'g', '(Lfd;IIII)V'), ('uu', 'a', '(Lfd;IIIIF)V'),
                ('uu', 'a', '(Lfd;IIILiz;)V'), ('hl', '<init>', '(Lfd;DDDLiz;)V'),
                ('fd', 'b', '(Lsn;)Z'), ('fd', 'c', '(Lsn;)V'))
    result = {}
    for owner, name, descriptor in selected:
        rows = [m for m in classes[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        P.require(len(rows) == 1, 'one original drop method')
        result[owner + '.' + name + descriptor] = {
            key: rows[0][key] for key in ('code_length', 'code_sha256')}
    return result


def validate(rows):
    P.require(type(rows) is list and len(rows) == 2, 'two ordered foot calls')
    for row, mode in zip(rows, MODES):
        P.require(type(row) is dict and set(row) == {'mode', 'before', 'after', 'result', 'exception',
                  'exception_frames', 'events'} and row['mode'] == mode, 'exact foot row')
        success = mode == 'success'
        P.require(P.exact(row['result'], False if success else None) and
                  row['exception'] == ('NONE' if success else 'java/lang/IllegalStateException'), 'actual foot outcome')
        before, after = row['before'], row['after']
        for state in (before, after):
            P.require(type(state) is dict and set(state) == STATE_KEYS, 'selected foot state')
            P.require(P.vector(state['position'], 3, lambda x: P.word(x, 16)) and
                      P.vector(state['dimensions'], 3, lambda x: P.word(x, 8)), 'raw Player pose')
            P.require(P.exact(state['health'], 20), 'zero-damage health')
            P.require(all(type(state[k]) is bool for k in ('sleeping', 'world_sleeping')) and
                      all(P.integer(state[k]) for k in ('timer', 'idle')) and
                      all(P.vector(state[k], 2, P.integer) for k in ('head', 'orphan')), 'typed Player fields')
            for key in ('world_random', 'player_random'):
                random = state[key]
                P.require(type(random) is list and len(random) == 3 and P.word(random[0], 16) and
                          int(random[0], 16) < (1 << 48) and type(random[1]) is bool and
                          P.word(random[2], 16), 'complete typed Random state')
        P.require(P.exact(before, rows[0]['before']), 'same prepared inputs')
        P.require(P.exact([before[k] for k in ('head', 'orphan', 'sleeping', 'timer', 'world_sleeping', 'items')],
                          [[26, 12], [26, 0], True, 37, True, []]), 'occupied head and orphan foot')
        P.require(P.exact([after[k] for k in ('head', 'orphan', 'sleeping', 'timer', 'world_sleeping', 'idle')],
                          [[26, 8], [0, 0], not success, 0 if success else 37, not success, 0]), 'committed prefix and wake tail')
        P.require(P.exact(after['player_random'], before['player_random']) and
                  not P.exact(after['world_random'], before['world_random']), 'separate Random streams')
        P.require(P.exact(before['idle'], 17) and P.exact(before['items'], []), 'prepared Player prefix')
        P.require(P.exact(row['exception_frames'], [] if success else
                         ['FootWakeObservation$Observer.a', *ADDED_STACK]), 'actual exception stack')
        P.require(type(after['items']) is list and len(after['items']) == 1, 'one actual Item')
        item = after['items'][0]
        P.require(type(item) is dict and set(item) == ITEM_KEYS and
                  P.exact(item['stack'], [355, 1, 0]) and P.exact(item['delay'], 10) and
                  item['inserted'] is True and item['world_same'] is True and
                  P.vector(item['position'], 3, lambda x: P.word(x, 16)) and
                  P.vector(item['motion'], 3, lambda x: P.word(x, 16)) and
                  P.word(item['yaw'], 8) and P.word(item['phase'], 8), 'actual Item fields')
        events = row['events']
        P.require(type(events) is list and len(events) == 3 and
                  all(type(e) is dict for e in events) and
                  [e['kind'] for e in events] == ['cell', 'cell', 'added'] and
                  all(e['sleeping'] is True and e['head'] == [26, 8] and
                      type(e['frames']) is list and e['frames'] for e in events), 'actual ordered callbacks')
        for event, keys, stack in zip(events,
                ({'kind', 'xyz', 'head', 'orphan', 'sleeping', 'frames'},
                 {'kind', 'xyz', 'head', 'orphan', 'sleeping', 'frames'},
                 {'kind', 'head', 'orphan', 'sleeping', 'item', 'frames'}),
                (HEAD_STACK, ORPHAN_STACK, ADDED_STACK)):
            P.require(set(event) == keys and P.exact(event['frames'], stack), 'exact observer fields and stack')
        P.require(P.exact(events[0]['xyz'], [8, 64, 8]) and events[0]['orphan'] == [26, 0] and
                  P.exact(events[1]['xyz'], [7, 64, 8]) and events[1]['orphan'] == [0, 0] and
                  events[2]['orphan'] == [0, 0] and P.exact(events[2]['item'], item), 'observer prefixes')


def validate_finite(finite):
    P.require(type(finite) is dict and set(finite) == {'schema', 'rows'} and
              finite['schema'] == 'BED_FOOT_WAKE_V1', 'exact finite envelope')
    validate(finite['rows'])


def check():
    finite = P.load(HERE / 'finite.json')
    proof = P.load(HERE / 'provenance.json')
    validate_finite(finite)
    P.require(type(proof) is dict and set(proof) == {'status', 'archive_sha256', 'finite_sha256',
              'capture_sha256', 'sources', 'shared', 'methods', 'helpers', 'original'} and
              P.word(proof['capture_sha256'], 64) and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and
              proof['archive_sha256'] == P.JAR and proof['finite_sha256'] == P.digest(HERE / 'finite.json') and
              P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()), 'foot packet source and Code bindings')
    helpers = {'ConstructorJson.class', 'FootWakeObservation.class', 'FootWakeObservation$Observer.class',
               'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class',
               'PlayerDamageWakeObservation$Guard.class'}
    P.require(type(proof['helpers']) is dict and set(proof['helpers']) == helpers and
              all(P.word(value, 64) for value in proof['helpers'].values()), 'authored helper identities')
    receipt_keys = {'exit', 'timed_out', 'pipe_capture_complete', 'output_overflow', 'reader_errors',
                    'stdout_sha256', 'stderr_sha256'}
    P.require(type(proof['original']) is list and len(proof['original']) == 2 and
              all(type(r) is dict and set(r) == receipt_keys and
                  P.exact(r['exit'], 0) and r['pipe_capture_complete'] is True and
                  r['timed_out'] is False and r['output_overflow'] is False and r['reader_errors'] == {}
                  and P.word(r['stdout_sha256'], 64) and P.word(r['stderr_sha256'], 64)
                  for r in proof['original']) and
              P.exact(proof['original'][0], proof['original'][1]), 'two complete identical original calls')
    print('Original orphan-foot wake/drop packet and bindings pass')


if __name__ == '__main__':
    check()
