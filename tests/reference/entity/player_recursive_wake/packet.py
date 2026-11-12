"""Validate the finite original recursive wake packet without running Java."""
import importlib.util
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
PARENT = HERE.parent / 'player_damage_wake'
SOURCES = ('RecursiveWakeObservation.java', 'capture_reference.py', 'packet.py', 'README.md')
SHARED = {
    'setup': PARENT / 'PlayerDamageWakeObservation.java',
    'class_pins': PARENT / 'source-pins.json',
    'packet': PARENT / 'packet.py',
    'capture': ROOT / 'tests/reference/client/controller/capture.py',
    'classfile': HERE.parent / 'player/classfile.py',
    'json': HERE.parent / 'player/ConstructorJson.java',
    'previous_provenance': PARENT / 'provenance.json',
}


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


P = module('damage_wake_packet_support', SHARED['packet'])
MODES = ('success', 'observer-error', 'null-exit')
METHODS = (
    ('gs', 'a', '(Lsn;I)Z'), ('gs', 'a', '(ZZZ)V'),
    ('ve', 'b', '(Lfd;IIII)V'), ('ve', 'f', '(Lfd;IIII)Lbr;'),
)
STATE_KEYS = {'position', 'dimensions', 'bounds', 'sleeping', 'timer', 'bed', 'spawn',
              'private_coordinate', 'world_same', 'bounds_same', 'health', 'idle', 'world_sleeping',
              'head', 'orphan', 'planes', 'random', 'statistics', 'sounds'}
HEAD_STACK = ['fd.j', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
ORPHAN_STACK = ['fd.j', 'fd.g', 'fd.f', 've.b', 'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    actual = {key: P.digest(path) for key, path in SHARED.items()}
    prior = P.load(SHARED['previous_provenance'])
    for key, record, label in (
        ('setup', 'public_source_sha256', 'PlayerDamageWakeObservation.java'),
        ('class_pins', 'public_source_sha256', 'source-pins.json'),
        ('packet', 'public_source_sha256', 'packet.py'),
        ('capture', 'support_sha256', 'capture.py'),
        ('classfile', 'support_sha256', 'classfile.py'),
        ('json', 'support_sha256', 'ConstructorJson.java'),
    ):
        P.require(actual[key] == prior[record][label], 'retained shared source identity ' + key)
    return actual


def method_pins():
    classes = P.load(SHARED['class_pins'])['classes']
    result = {}
    for owner, name, descriptor in METHODS:
        method = [m for m in classes[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        P.require(len(method) == 1, 'one pinned original method ' + owner + '.' + name)
        result[owner + '.' + name + descriptor] = {key: method[0][key] for key in ('code_length', 'code_sha256')}
    return result


def validate(rows):
    P.require(type(rows) is list and len(rows) == 3, 'three ordered original calls')
    callbacks = [
        {'xyz': [8, 64, 8], 'head': [26, 8], 'orphan': [26, 8], 'sleeping': True, 'frames': HEAD_STACK},
        {'xyz': [7, 64, 8], 'head': [26, 8], 'orphan': [0, 0], 'sleeping': True, 'frames': ORPHAN_STACK},
    ]
    outcomes = (
        (False, 'NONE', []),
        (None, 'java/lang/IllegalStateException', ['RecursiveWakeObservation$Observer.a', *ORPHAN_STACK]),
        (None, 'java/lang/NullPointerException', ['ve.f', 'gs.a', 'gs.a']),
    )
    for row, mode, (result, exception, frames) in zip(rows, MODES, outcomes):
        P.require(type(row) is dict and set(row) == {'mode', 'result', 'exception', 'exception_frames',
                  'before', 'after', 'callbacks'} and row['mode'] == mode, 'exact recursive wake row')
        P.require(P.exact([row['result'], row['exception'], row['exception_frames']],
                          [result, exception, frames]), 'actual typed original outcome')
        P.require(P.exact(row['callbacks'], callbacks), 'ordered callbacks with both committed cells')
        before, after = row['before'], row['after']
        for state in (before, after):
            P.require(type(state) is dict and state.keys() == STATE_KEYS, 'selected wake state keys')
            for key, count, width in (('position', 3, 16), ('dimensions', 3, 8), ('bounds', 6, 16)):
                P.require(P.vector(state[key], count, lambda value: P.word(value, width)), key)
            P.require(state['bounds_same'] is True and P.exact(state['health'], 20) and
                      P.exact(state['sounds'], []) and P.exact(state['statistics'],
                      {'total': [], 'unsent': [], 'dirty': False}), 'damage prefix effects')
        P.require(P.exact(before, rows[0]['before']), 'same prepared inputs')
        P.require(P.exact([before[k] for k in ('head', 'orphan', 'sleeping', 'timer', 'world_same', 'world_sleeping', 'idle')],
                          [[26, 12], [26, 8], True, 37, True, True, 17]), 'occupied Bed and orphan input')
        success = mode == 'success'
        P.require(P.exact([after[k] for k in ('head', 'orphan', 'sleeping', 'timer', 'world_same', 'world_sleeping', 'idle')],
                          [[26, 8], [0, 0], not success, 0 if success else 37, mode != 'null-exit', not success, 0]),
                  'committed mutation, sleeping state and null receiver')
        P.require(P.exact(after['dimensions'], ['3f19999a', '3fe66666', '3fcf5c29']) and
                  (after['position'] != before['position']) == success and
                  (after['bounds'] != before['bounds']) == success, 'standing stores precede exit position')
        for key in ('bed', 'spawn', 'private_coordinate', 'random'):
            P.require(P.exact(after[key], before[key]), 'preserved ' + key)


def check():
    finite = P.load(HERE / 'finite.json')
    proof = P.load(HERE / 'provenance.json')
    validate(finite['rows'])
    P.require(finite['schema'] == 'PLAYER_RECURSIVE_WAKE_V1' and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(HERE / 'finite.json') and
              P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()), 'packet and source binding')
    receipts = proof['original']
    P.require(type(receipts) is list and len(receipts) == 2 and
              all(P.exact(r['exit'], 0) and r['pipe_capture_complete'] is True and
                  r['timed_out'] is False and r['output_overflow'] is False and r['reader_errors'] == {}
                  and P.word(r['stdout_sha256'], 64) for r in receipts), 'two bounded original executions')
    print('Three original recursive wake observations and packet bindings pass')


if __name__ == '__main__':
    check()
