"""Closed Java-free reader for eight original Sapling/TallGrass wake profiles."""
import hashlib
import json
from pathlib import Path

# The shared reader disables bytecode writes before loading its own dependencies.
import importlib.util
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
FLOWER = HERE.parent / 'flower_wake'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


F = module('plant_family_wake_flower_support', FLOWER / 'packet.py')
P = F.P
SCHEMA = 'SAPLING_TALL_GRASS_WAKE_V1'
SCOPE = 'Eight fixed prepared unchanged-original Sapling/TallGrass wake calls; native comparisons recorded separately; no ticks or whole-game parity claim.'
PROVENANCE = 'SAPLING_TALL_GRASS_WAKE_PROVENANCE_V1'
SOURCES = ('PlantFamilyWakeObservation.java', 'capture_reference.py', 'packet.py',
           'test_packet.py', 'README.md', 'plant-family-pins.json')
SHARED = {**F.SHARED, 'flower_packet': FLOWER / 'packet.py', 'flower_pins': FLOWER / 'flower-pins.json'}
HELPERS = {'ConstructorJson.class', 'PlantFamilyWakeObservation.class',
           'PlantFamilyWakeObservation$Observer.class', 'PlantFamilyWakeObservation$Profile.class',
           'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class',
           'PlayerDamageWakeObservation$Guard.class'}
BINARIES = ('client', 'java', 'javac', 'javap', 'jvm', 'modules', 'python', *P.LIBRARIES)
EXTRA = {'plant', 'plant_floor', 'light_inputs', 'foot', 'world_random', 'player_random', 'items'}
RECEIPT_KEYS = F.RECEIPT_KEYS
PROCESS_TIMEOUTS = {'java': 15, 'javac': 15, 'javap': 15, 'bytecode': 20, 'compile': 45}
CLASS_IDENTITIES = {
    'he': (1111, '61ee6a39ac8534f7a62388aa74c64ec495ce762dbde0177d2513a452b8ad39a4', 'wb',
           '8fc3f6fd59ff54cb941252b8f738ccac3e8d7d4ae52e5fa1b14c561b5ace1829'),
    'ru': (954, '798fb8e40028cb1a9d031690c5ee44adb4fb2af14b75778d857290dcfb7371eb', 'wb',
           '583a37ece09ae4e1e9fe40c8ef2c6185645d7e7e8356888b55b9d1db77eff22c')}
SELECTED_METHODS = {
    ('he', 'b_', '(I)I'): (4, 'bbe0922278563486c9ace91488753bbef5a64d445195dc2e906e5a7c8ced8668'),
    ('ru', 'a', '(ILjava/util/Random;)I'): (18, '741a2edb01dfb345007402e335fc993e4931a675826708ef97b6cd69b634d6f7'),
    ('uu', 'a', '(ILjava/util/Random;)I'): (5, 'f22dc1078ce8ea05ef3c7db975cd3c15573688066375d789970b811405627904'),
    ('uu', 'b_', '(I)I'): (2, '42ca1f85243342bd41983ab86f2235408fa46ded7466d4525196f66b9dbb5f3e'),
    ('uu', 'a', '(Ljava/util/Random;)I'): (2, '95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2')}
PROFILE_OPERANDS = (
    ('sapling-supported', 6, 2, 2, 1, False),
    ('sapling-unsupported', 6, 2, 0, 1, False),
    ('sapling-publication-error', 6, 2, 0, 1, True),
    ('tallgrass-supported', 31, 1, 2, 1, False),
    ('tallgrass-unsupported', 31, 1, 0, 1, False),
    ('tallgrass-publication-error', 31, 1, 0, 1, True),
    ('tallgrass-unsupported-seed1234', 31, 1, 0, 4660, False),
    ('tallgrass-unsupported-seed7fffffff', 31, 1, 0, 2147483647, False))
MODES = tuple(row[0] for row in PROFILE_OPERANDS)
RANDOM_AFTER = ('00000005deece66c', '0000684df9922e30', '0000684df9922e30',
                '00000005deece66c', '0000352cccfcca7b', '0000352cccfcca7b',
                '000018efaef6dbf4', '0000e83ea2c3179c')
public_receipt = F.public_receipt
class_record = F.class_record


def profiles():
    return [dict(zip(('mode', 'id', 'metadata', 'floor', 'world_seed', 'observer_error'), row))
            for row in PROFILE_OPERANDS]


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def structural_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()


def validate_class_pins(extra):
    P.require(type(extra) is dict and set(extra) == {'archive_sha256', 'classes', 'methods'} and
              extra['archive_sha256'] == P.JAR and type(extra['classes']) is dict and
              set(extra['classes']) == set(CLASS_IDENTITIES), 'closed Sapling/TallGrass class pins')
    for name, (size, digest, parent, structure) in CLASS_IDENTITIES.items():
        class_record(extra['classes'][name], name, size, digest, parent)
        P.require(structural_hash(extra['classes'][name]) == structure, 'unchanged full original class structure')
    expected = {owner + '.' + name + descriptor: {'code_length': length, 'code_sha256': digest}
                for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items()}
    P.require(P.exact(extra['methods'], expected), 'exact virtual original Code authorities')


def class_pins():
    base = F.class_pins()
    extra = P.load(HERE / 'plant-family-pins.json')
    validate_class_pins(extra)
    P.require(not set(base['classes']) & set(extra['classes']), 'distinct original classes')
    return {'archive': base['archive'], 'classes': {**base['classes'], **extra['classes']}}


def method_pins():
    classes = class_pins()['classes']
    result = F.method_pins()
    for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items():
        found = [m for m in classes[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        P.require(len(found) == 1 and P.exact(found[0]['code_length'], length) and
                  found[0]['code_sha256'] == digest, 'verified virtual drop Code')
        result[owner + '.' + name + descriptor] = {k: found[0][k] for k in ('code_length', 'code_sha256')}
    return result


def receiver(value, sapling):
    expected = {'class': 'he' if sapling else 'ru'}
    for key, owner, name, descriptor in (
            ('neighbor', 'wb', 'b', '(Lfd;IIII)V'), ('survival', 'wb', 'g', '(Lfd;III)Z'),
            ('substrate', 'wb', 'd', '(I)Z'), ('drop_wrapper', 'uu', 'g', '(Lfd;IIII)V'),
            ('drop_chance', 'uu', 'a', '(Lfd;IIIIF)V'),
            ('drop_item', 'uu' if sapling else 'ru', 'a', '(ILjava/util/Random;)I'),
            ('drop_metadata', 'he' if sapling else 'uu', 'b_', '(I)I'),
            ('drop_count', 'uu', 'a', '(Ljava/util/Random;)I')):
        expected[key] = {'class': owner, 'method': name, 'descriptor': descriptor}
    P.require(P.exact(value, expected), 'actual receiver and virtual declaring methods')


def item(value, stack):
    P.require(type(value) is dict and set(value) == F.ITEM_KEYS, 'closed actual Item')
    for key, width, size in (('position', 16, 3), ('motion', 16, 3), ('dimensions', 8, 3), ('bounds', 16, 6)):
        P.require(P.vector(value[key], size, lambda v: P.word(v, width)), 'Item raw ' + key)
    P.require(P.word(value['yaw'], 8) and P.word(value['phase'], 8) and
              P.exact([value[k] for k in ('delay', 'age', 'health', 'stack')], [10, 0, 5, stack]) and
              value['inserted'] is True and value['removed'] is False and value['world_same'] is True,
              'actual inserted Sapling or Seed Item')


def state(value, stack):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('plant', 'plant_floor', 'foot'):
        P.require(P.vector(value[key], 2, P.integer) and 0 <= value[key][0] <= 255 and
                  0 <= value[key][1] <= 15, 'typed cell ' + key)
    light = value['light_inputs']
    P.require(type(light) is dict and set(light) == F.LIGHT_KEYS and
              P.exact(light['position'], [7, 64, 8]) and
              P.exact([light[k] for k in ('sky_nibble', 'block_light_nibble', 'height',
                      'world_f_sky_subtraction', 'fd_m_chunk_subtraction_literal')], [15, 0, 64, 5, 0]) and
              type(light['chunk_sky_seen_static']) is bool, 'real cached light inputs')
    for key, original in (('world_random', value['random'][1]), ('player_random', value['random'][0])):
        P.require(P.exact(value[key], original), 'same complete Random observation ' + key)
    P.require(type(value['items']) is list and len(value['items']) <= 1, 'bounded actual Items')
    for observed in value['items']:
        item(observed, stack)


def validate(rows):
    P.require(type(rows) is list and len(rows) == 8, 'eight ordered original calls')
    for index, (row, profile) in enumerate(zip(rows, profiles())):
        P.require(type(row) is dict and set(row) == {'mode', 'receiver', 'prepared', 'before', 'after',
                  'result', 'exception', 'exception_message', 'exception_stack', 'events'} and
                  row['mode'] == profile['mode'], 'closed ordered call row')
        sapling = profile['id'] == 6
        supported, normal = profile['floor'] == 2, not profile['observer_error']
        has_item = not supported and index != 7
        plant, floor = [profile['id'], profile['metadata']], [profile['floor'], 0]
        stack = [6, 1, 2] if sapling else [295, 1, 0]
        receiver(row['receiver'], sapling)
        P.require(P.exact(row['prepared'], {'profile': profile, 'shared_setup': [0, 0, 20, 0, 0],
                  'post_setup_cell_assignments': [[7, 64, 8, *plant], [7, 63, 8, *floor]],
                  'world_random_reseed': profile['world_seed'], 'light_or_sky_overrides': []}),
                  'all sealed prepared operands and actual Random reseed')
        before, after = row['before'], row['after']
        state(before, stack)
        state(after, stack)
        P.require(P.exact([before[k] for k in ('head', 'plant', 'plant_floor', 'foot', 'sleeping',
                          'timer', 'health', 'idle', 'world_sleeping', 'items')],
                          [[26, 12], plant, floor, [26, 0], True, 37, 20, 17, True, []]) and
                  before['light_inputs']['chunk_sky_seen_static'] is (index != 0), 'prepared entered sleep')
        P.require(P.exact(row['result'], False if normal else None) and
                  row['exception'] == ('NONE' if normal else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if normal else 'Plant family Item observer interruption'),
                  'actual first outcome or observer Throwable')
        P.require(P.exact([after[k] for k in ('head', 'plant', 'plant_floor', 'foot', 'sleeping',
                          'timer', 'world_sleeping', 'health', 'idle')],
                          [[26, 8], plant if supported or not normal else [0, 0], floor, [26, 0],
                           not normal, 0 if normal else 37, not normal, 20, 0]) and
                  len(after['items']) == int(has_item) and
                  after['world_entities'] == before['world_entities'] + int(has_item) and
                  after['light_inputs']['chunk_sky_seen_static'] is True, 'committed drop/removal/wake prefix')
        P.require(P.exact(after['position'], ['401e000000000000', '40506e1480000000', '401e000000000000']
                          if normal else before['position']) and
                  P.exact(after['dimensions'], ['3f19999a', '3fe66666', '3fcf5c29']), 'actual pose/dimensions')
        seed_before = f"{(profile['world_seed'] ^ 0x5deece66d):016x}"
        P.require(P.exact(before['world_random'], [seed_before, False, '0' * 16]) and
                  P.exact(after['world_random'], [RANDOM_AFTER[index], False, '0' * 16]) and
                  P.exact(after['player_random'], before['player_random']) and
                  P.exact(before['random'][2], ['00000005deece66d', False, '0' * 16]) and
                  P.exact(after['random'][2], ['00001df425034d55' if has_item else '00000005deece66d',
                                              False, '0' * 16]), 'actual separate Random paths')
        for plane in ('sky', 'block_light', 'height'):
            P.require(P.exact(before['planes'][plane], after['planes'][plane]), 'unchanged real light/height plane')
        events = row['events']
        kinds = ['cell'] if supported else ['cell', 'added', 'cell'] if has_item and normal else \
                ['cell', 'added'] if has_item else ['cell', 'cell']
        P.require(type(events) is list and len(events) == len(kinds) and
                  all(type(e) is dict and 'kind' in e for e in events) and
                  [e['kind'] for e in events] == kinds, 'actual callback publication prefix')
        for event_index, event in enumerate(events):
            added = event['kind'] == 'added'
            removed = event_index != 0 and not added
            keys = {'kind', 'head', 'plant', 'floor', 'sleeping', 'stack', 'item' if added else 'xyz'}
            expected_stack = F.ADDED_STACK if added else F.FLOWER_STACK if removed else F.HEAD_STACK
            P.require(set(event) == keys and P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                      P.exact(event['plant'], [0, 0] if removed else plant) and
                      P.exact(event['floor'], floor) and P.exact(F.trace(event['stack']), expected_stack),
                      'actual callback state and source order')
            if added:
                P.require(P.exact(event['item'], after['items'][0]), 'published actual Item persists')
            else:
                P.require(P.exact(event['xyz'], [7, 64, 8] if removed else [8, 64, 8]), 'actual cell coordinate')
        if normal:
            P.require(P.exact(row['exception_stack'], []), 'no thrown exception')
        else:
            P.require(P.exact(F.trace(row['exception_stack']), F.ADDED_STACK) and
                      row['exception_stack'][0]['class'] == 'PlantFamilyWakeObservation$Observer',
                      'genuine Item observer first Throwable stack')


def validate_finite(value):
    P.require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and
              value['schema'] == SCHEMA and value['scope'] == SCOPE, 'closed finite envelope')
    validate(value['rows'])


def receipt(value, label, timeout, original=False):
    P.require(type(value) is dict and set(value) == RECEIPT_KEYS and
              P.exact(value['exit'], 0) and P.exact(value['timeout_seconds'], timeout) and
              value['timed_out'] is False and value['pipe_capture_complete'] is True and
              value['output_overflow'] is False and type(value['reader_errors']) is dict and
              value['reader_errors'] == {} and P.word(value['command_sha256'], 64), 'closed bounded process receipt')
    for stream, limit in (('stdout', 4194304), ('stderr', 1048576)):
        P.require(P.exact(value[stream + '_limit'], limit) and P.integer(value[stream + '_bytes']) and
                  (0 < value[stream + '_bytes'] <= limit if original else 0 <= value[stream + '_bytes'] <= limit) and
                  value[stream + '_file'] == label + '.' + stream and P.word(value[stream + '_sha256'], 64),
                  'complete typed stream extent/hash')
        if value[stream + '_bytes'] == 0:
            P.require(value[stream + '_sha256'] == hashlib.sha256(b'').hexdigest(), 'empty stream hash')
    if label == 'compile':
        P.require(value['stdout_bytes'] == value['stderr_bytes'] == 0, 'clean authored helper compile')
    elif label == 'bytecode':
        P.require(value['stdout_bytes'] > 0 and value['stderr_bytes'] == 0, 'complete original javap output')
    else:
        P.require(value['stdout_bytes'] + value['stderr_bytes'] > 0, 'nonempty actual process output')


def validate_provenance(proof, directory=HERE):
    directory = Path(directory)
    keys = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'capture_sha256', 'sources', 'shared',
            'methods', 'helpers', 'binaries', 'class_count', 'rows_per_vm', 'original', 'processes',
            'inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof) == keys and proof['schema'] == PROVENANCE and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(directory / 'finite.json') and P.word(proof['capture_sha256'], 64) and
              P.exact(proof['class_count'], 34) and P.exact(proof['rows_per_vm'], 8) and
              all(proof[k] is True for k in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged',
                                            'class_pins_unchanged')), 'closed original-only provenance')
    for key, names in (('sources', SOURCES), ('shared', SHARED), ('helpers', HELPERS), ('binaries', BINARIES)):
        F.fingerprints(proof[key], names, 'closed ' + key + ' inventory')
    P.require(P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()) and proof['binaries']['client'] == P.JAR and
              all(proof['binaries'][name] == expected for name, expected in P.LIBRARIES.items()),
              'executed sources and original binary/Code identities')
    P.require(type(proof['processes']) is dict and set(proof['processes']) == set(PROCESS_TIMEOUTS),
              'five actual bounded tool processes')
    for label, timeout in PROCESS_TIMEOUTS.items():
        receipt(proof['processes'][label], label, timeout)
    P.require(type(proof['original']) is list and len(proof['original']) == 2, 'two fresh original VM receipts')
    for index, value in enumerate(proof['original']):
        receipt(value, f'original-{index}', 70, original=True)
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
    print('Original Sapling/TallGrass packet: 8 profiles and bindings pass; native comparisons run separately')
    return value


if __name__ == '__main__':
    check()
