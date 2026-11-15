"""Closed Java-free reader for original Dead Bush, Crops and Mushroom wake calls."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


G = module('remaining_plants_shared_family', HERE.parent / 'sapling_tall_grass_wake/packet.py')
F, P = G.F, G.P
SCHEMA = 'REMAINING_PLANTS_WAKE_V1'
SCOPE = 'Twenty-five fixed prepared unchanged-original Dead Bush/Crops/Mushroom wake calls; original-only packet; native projection and whole-game parity remain separate.'
PROVENANCE = 'REMAINING_PLANTS_WAKE_PROVENANCE_V1'
SOURCES = ('RemainingPlantsWakeObservation.java', 'capture_reference.py', 'packet.py',
           'test_packet.py', 'README.md', 'remaining-plant-pins.json')
SHARED = {**G.SHARED, 'family_packet': G.HERE / 'packet.py',
          'family_pins': G.HERE / 'plant-family-pins.json', 'family_finite': G.HERE / 'finite.json',
          'family_provenance': G.HERE / 'provenance.json'}
HELPERS = {'ConstructorJson.class', 'RemainingPlantsWakeObservation.class',
           'RemainingPlantsWakeObservation$Observer.class', 'RemainingPlantsWakeObservation$Profile.class',
           'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class',
           'PlayerDamageWakeObservation$Guard.class'}
BINARIES = G.BINARIES
EXTRA = G.EXTRA
RECEIPT_KEYS = G.RECEIPT_KEYS
PROCESS_TIMEOUTS = G.PROCESS_TIMEOUTS
CLASS_IDENTITIES = {
    'jb': (481, '5b067bd6444b295199b8ab8a98bdee6b66b6bb66fd29e58ec94bb5e2fb8a4463', 'wb', '3a19380bf5d91be10e6b352261ebd89b34abb3ed3cc1490d003fd2fa5d5d1ab8'),
    'ni': (2100, '846d1a8472b5fe51439272cab4875a344086b4d7cf68981db9e23c3a15f027dc', 'wb', '88d5b4965f0b33027ec9ca5d30f41c861028824f48c84c80cb5ae39abedd9204'),
    'tl': (888, '1838ab3568ff6100c5d3f0a8d8e270cfab5fb4296ca4ba394d700f13321cdc27', 'wb', 'd8323d4232eaff51e77a6a687092d37f84ade1cea6f194ed1dd435a5a0ffb6fe'),
    'gm': (11051, 'f9d971a83efa0c6399f1fc4bc0bc06c6a6249908afc6b46f5889a49365c190c7', 'java/lang/Object', '6f342c2d15a478426d6525a5b2edbc6664318ea49ff3127975bae1ebb6590270')}
SELECTED_METHODS = {
    ('jb', 'd', '(I)Z'): (16, '290c45d1a53f919948b61f8516ac9411719c92ac79942c2b7e8fb25dff79397e'),
    ('jb', 'a', '(ILjava/util/Random;)I'): (2, 'ea3444f7b732d2379c3e81ad7a08e24c072cb9468be2734659ffef7ecaec49ed'),
    ('ni', 'd', '(I)Z'): (16, '2857a91d6823333b499d3abafa96bb63fc58c43a577f6224d5787b1e0bbe99b3'),
    ('ni', 'a', '(Lfd;IIIIF)V'): (171, 'b45bc4efb6f6cbcf81d53ad6483f08404a7af4bebe21534e7b308012adaab8f8'),
    ('ni', 'a', '(ILjava/util/Random;)I'): (15, '0bbb4f14f96fef5ab58fa808e252e1ad201701d82d1cda49b7ea86ba1126594f'),
    ('ni', 'a', '(Ljava/util/Random;)I'): (2, '95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2'),
    ('tl', 'd', '(I)Z'): (6, '00d14110d5487bba1b37a00d8a881c09bb111eefa57f61462c1f528fd08a8c28'),
    ('tl', 'g', '(Lfd;III)Z'): (49, '2445b8dae0de537a1b63978f560dba7ac86e4229d51cf185e390add687b5daec'),
    ('gm', '<clinit>', '()V'): (2668, 'd839820ccc643d594ee77ca3f701d60e4741249305b01b63edcca8a49d58b269')}
PROFILE_OPERANDS = (
    ('dead-bush-supported', 32, 0, 12, 15, 1, 0, False),
    ('dead-bush-unsupported', 32, 0, 2, 15, 1, 0, False),
    ('crop-supported-stage0', 59, 0, 60, 15, 1, 0, False),
    ('crop-supported-stage7', 59, 7, 60, 15, 1, 0, False),
    ('crop-unsupported-stage0-seed1', 59, 0, 0, 15, 1, 0, False),
    ('crop-unsupported-stage0-seed1234', 59, 0, 0, 15, 4660, 0, False),
    ('crop-unsupported-stage0-seed7fffffff', 59, 0, 0, 15, 2147483647, 0, False),
    ('crop-unsupported-stage7-seed1', 59, 7, 0, 15, 1, 0, False),
    ('crop-unsupported-stage7-seed1234', 59, 7, 0, 15, 4660, 0, False),
    ('crop-unsupported-stage7-seed7fffffff', 59, 7, 0, 15, 2147483647, 0, False),
    ('crop-publication-error-first', 59, 7, 0, 15, 1, 1, False),
    ('crop-publication-error-second', 59, 7, 0, 15, 1, 2, False),
    ('crop-removal-error', 59, 7, 0, 15, 1, 0, True),
    ('mushroom39-supported-light0', 39, 0, 2, 0, 1, 0, False),
    ('mushroom39-supported-light12', 39, 0, 2, 12, 1, 0, False),
    ('mushroom39-unsupported', 39, 0, 0, 12, 1, 0, False),
    ('mushroom39-high-light', 39, 0, 2, 13, 1, 0, False),
    ('mushroom39-publication-error', 39, 0, 0, 12, 1, 1, False),
    ('mushroom39-removal-error', 39, 0, 0, 12, 1, 0, True),
    ('mushroom40-supported-light0', 40, 0, 2, 0, 1, 0, False),
    ('mushroom40-supported-light12', 40, 0, 2, 12, 1, 0, False),
    ('mushroom40-unsupported', 40, 0, 0, 12, 1, 0, False),
    ('mushroom40-high-light', 40, 0, 2, 13, 1, 0, False),
    ('mushroom40-publication-error', 40, 0, 0, 12, 1, 1, False),
    ('mushroom40-removal-error', 40, 0, 0, 12, 1, 0, True))
MODES = tuple(row[0] for row in PROFILE_OPERANDS)
public_receipt = G.public_receipt
class_record = G.class_record
receipt = G.receipt


def profiles():
    return [dict(zip(('mode', 'id', 'metadata', 'floor', 'light', 'world_seed',
                      'publication_error_at', 'removal_error'), row)) for row in PROFILE_OPERANDS]


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def validate_class_pins(extra):
    P.require(type(extra) is dict and set(extra) == {'archive_sha256', 'classes', 'methods'} and
              extra['archive_sha256'] == P.JAR and type(extra['classes']) is dict and
              set(extra['classes']) == set(CLASS_IDENTITIES), 'closed remaining-plant class pins')
    for name, (size, digest, parent, structure) in CLASS_IDENTITIES.items():
        class_record(extra['classes'][name], name, size, digest, parent)
        P.require(G.structural_hash(extra['classes'][name]) == structure, 'unchanged full original class structure')
    expected = {owner + '.' + name + descriptor: {'code_length': length, 'code_sha256': digest}
                for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items()}
    P.require(P.exact(extra['methods'], expected), 'exact original method Code authorities')


def class_pins():
    base = G.class_pins()
    extra = P.load(HERE / 'remaining-plant-pins.json')
    validate_class_pins(extra)
    P.require(not set(base['classes']) & set(extra['classes']), 'distinct original classes')
    return {'archive': base['archive'], 'classes': {**base['classes'], **extra['classes']}}


def method_pins():
    classes, result = class_pins()['classes'], G.method_pins()
    for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items():
        found = [m for m in classes[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        P.require(len(found) == 1 and P.exact(found[0]['code_length'], length) and
                  found[0]['code_sha256'] == digest, 'verified unchanged Code')
        result[owner + '.' + name + descriptor] = {k: found[0][k] for k in ('code_length', 'code_sha256')}
    return result


def receiver(value, profile):
    kind = 'jb' if profile['id'] == 32 else 'ni' if profile['id'] == 59 else 'tl'
    expected = {'class': kind}
    for key, owner, name, descriptor in (
            ('neighbor', 'wb', 'b', '(Lfd;IIII)V'),
            ('survival', 'tl' if kind == 'tl' else 'wb', 'g', '(Lfd;III)Z'),
            ('substrate', kind, 'd', '(I)Z'), ('drop_wrapper', 'uu', 'g', '(Lfd;IIII)V'),
            ('drop_chance', 'ni' if kind == 'ni' else 'uu', 'a', '(Lfd;IIIIF)V'),
            ('drop_item', kind if kind != 'tl' else 'uu', 'a', '(ILjava/util/Random;)I'),
            ('drop_metadata', 'uu', 'b_', '(I)I'),
            ('drop_count', 'ni' if kind == 'ni' else 'uu', 'a', '(Ljava/util/Random;)I')):
        expected[key] = {'class': owner, 'method': name, 'descriptor': descriptor}
    P.require(P.exact(value, expected), 'actual receiver and virtual declaring methods')


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


class RandomModel:
    def __init__(self, seed):
        self.state = (seed ^ 0x5deece66d) & ((1 << 48) - 1)
        self.draws = 0

    def bits(self, count):
        self.state = (self.state * 0x5deece66d + 11) & ((1 << 48) - 1)
        self.draws += 1
        return self.state >> (48 - count)

    def floating(self):
        return self.bits(24) / (1 << 24)

    def bounded(self, bound):
        while True:
            bits = self.bits(31)
            value = bits % bound
            if bits - value + bound - 1 < (1 << 31):
                return value

    def double(self):
        return ((self.bits(26) << 27) + self.bits(27)) / (1 << 53)

    def observation(self):
        return [f'{self.state:016x}', False, '0' * 16]


def model(profile):
    """Independent declared bytecode draw/position model, never a seed selector."""
    supported = (profile['floor'] == 12 if profile['id'] == 32 else
                 profile['floor'] == 60 if profile['id'] == 59 else
                 profile['light'] < 13 and profile['floor'] == 2)
    random, math = RandomModel(profile['world_seed']), RandomModel(0)
    items = []

    def spawn(item_id, crop):
        spread = f32(.7)
        offsets = []
        for coordinate in (7, 64, 8):
            scaled, edge = f32(random.floating() * spread), f32(1.0 - spread)
            value = f32(f32(coordinate) + f32(scaled + f32(edge * f32(.5)))) if crop else coordinate + scaled + edge * .5
            offsets.append(struct.pack('>d', value).hex())
        phase = struct.pack('>f', f32(math.double() * 3.141592653589793 * 2.0)).hex()
        yaw = struct.pack('>f', f32(math.double() * 360.0)).hex()
        vx = f32(math.double() * .20000000298023224 - .10000000149011612)
        vz = f32(math.double() * .20000000298023224 - .10000000149011612)
        items.append({'stack': [item_id, 1, 0], 'position': offsets, 'phase': phase, 'yaw': yaw,
                      'motion': [struct.pack('>d', v).hex() for v in (vx, .20000000298023224, vz)]})
        return profile['publication_error_at'] != 0 and len(items) == profile['publication_error_at']

    interrupted = False
    if not supported:
        random.floating()  # Inherited chance draw, even when no Item is returned.
        if profile['id'] in (39, 40):
            interrupted = spawn(profile['id'], False)
        elif profile['id'] == 59:
            if profile['metadata'] == 7:
                interrupted = spawn(296, False)
            if not interrupted:
                for _ in range(3):
                    if random.bounded(15) <= profile['metadata'] and spawn(295, True):
                        interrupted = True
                        break
    removal_error = not supported and not interrupted and profile['removal_error']
    return dict(supported=supported, items=items, publication_error=interrupted,
                removal_error=removal_error, normal=not (interrupted or removal_error),
                removed=not supported and not interrupted, world_random=random.observation(),
                math_random=math.observation(), draws=random.draws)


def planes(profile, after, removed):
    blocks, metadata, sky = bytearray(32768), bytearray(16384), bytearray(16384)
    def index(x, y, z): return (x * 16 + z) * 128 + y
    def nibble(array, offset, value):
        shift = (offset & 1) * 4
        array[offset >> 1] = (array[offset >> 1] & ~(15 << shift)) | value << shift
    def cell(x, y, z, block, data):
        offset = index(x, y, z)
        blocks[offset] = block
        nibble(metadata, offset, data)
    for x in range(16):
        for z in range(16):
            cell(x, 63, z, 2, 0)
            for y in range(64, 128): nibble(sky, index(x, y, z), 15)
    nibble(sky, index(7, 64, 8), profile['light'])
    cell(8, 64, 8, 26, 8 if after else 12)
    cell(8, 64, 7, 26, 0)
    cell(7, 64, 8, 0 if removed else profile['id'], 0 if removed else profile['metadata'])
    cell(7, 63, 8, profile['floor'], 0)
    values = dict(blocks=blocks, metadata=metadata, sky=sky, block_light=bytes(16384), height=bytes([64]) * 256)
    return {name: hashlib.sha256(data).hexdigest() for name, data in values.items()}


def item(value, expected):
    G.item(value, expected['stack'])
    P.require(P.exact({k: value[k] for k in expected}, expected), 'independent actual Item draw/position fields')
    P.require(P.exact(value['dimensions'], ['3e800000', '3e800000', '3e000000']), 'actual Item dimensions')


def state(value, profile, expected_items):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('plant', 'plant_floor', 'foot'):
        P.require(P.vector(value[key], 2, P.integer) and 0 <= value[key][0] <= 255 and
                  0 <= value[key][1] <= 15, 'typed cell ' + key)
    light = value['light_inputs']
    P.require(type(light) is dict and set(light) == F.LIGHT_KEYS and P.exact(light['position'], [7, 64, 8]) and
              P.exact([light[k] for k in ('sky_nibble', 'block_light_nibble', 'height',
                  'world_f_sky_subtraction', 'fd_m_chunk_subtraction_literal')], [profile['light'], 0, 64, 5, 0]) and
              type(light['chunk_sky_seen_static']) is bool, 'actual declared target light/height inputs')
    for key, stream in (('world_random', value['random'][1]), ('player_random', value['random'][0])):
        P.require(P.exact(value[key], stream), 'same complete actual Random ' + key)
    P.require(type(value['items']) is list and len(value['items']) == len(expected_items), 'exact actual Item count')
    for actual, expected in zip(value['items'], expected_items): item(actual, expected)


def trace(value):
    F.trace(value)  # Validate every actual frame, including authored/reflection frames.
    return [f['class'] + '.' + f['method'] for f in value if f['class'] in {'fd', 've', 'uu', 'wb', 'gs', 'ni', 'jb', 'tl'}]


def validate(rows):
    P.require(type(rows) is list and len(rows) == 25, 'twenty-five ordered original calls')
    baseline = G.read_reference()['rows']
    base_keys = P.STATE_KEYS - {'planes', 'random', 'world_entities'}
    for index, (row, profile) in enumerate(zip(rows, profiles())):
        expected = model(profile)
        P.require(type(row) is dict and set(row) == {'mode', 'receiver', 'prepared', 'before', 'after',
            'result', 'exception', 'exception_message', 'exception_stack', 'events'} and
            row['mode'] == profile['mode'], 'closed ordered actual row')
        receiver(row['receiver'], profile)
        plant, floor = [profile['id'], profile['metadata']], [profile['floor'], 0]
        P.require(P.exact(row['prepared'], {'profile': profile, 'shared_setup': [0, 0, 20, 0, 0],
            'post_setup_cell_assignments': [[7, 64, 8, *plant], [7, 63, 8, *floor]],
            'world_random_reseed': profile['world_seed'], 'post_setup_sky_nibble_assignment': [7, 64, 8, profile['light']],
            'light_or_sky_getter_overrides': []}), 'sealed prepared inputs without getter overrides')
        before, after = row['before'], row['after']
        state(before, profile, [])
        state(after, profile, expected['items'])
        P.require(P.exact({k: before[k] for k in base_keys}, {k: baseline[0]['before'][k] for k in base_keys}), 'same shared prepared Player/World state')
        comparison = baseline[0]['after'] if expected['normal'] else baseline[2]['after']
        P.require(P.exact({k: after[k] for k in base_keys}, {k: comparison[k] for k in base_keys}), 'actual shared wake/exception state')
        P.require(P.exact([before[k] for k in ('plant', 'plant_floor', 'foot', 'world_entities')], [plant, floor, [26, 0], 1]) and
                  P.exact([after[k] for k in ('plant', 'plant_floor', 'foot', 'world_entities')], [[0, 0] if expected['removed'] else plant, floor, [26, 0], 1 + len(expected['items'])]), 'committed actual cells and World Entity count')
        P.require(before['light_inputs']['chunk_sky_seen_static'] is (index != 0) and
                  after['light_inputs']['chunk_sky_seen_static'] is True, 'actual shared Chunk flag history, never reset')
        P.require(P.exact(before['planes'], planes(profile, False, False)) and
                  P.exact(after['planes'], planes(profile, True, expected['removed'])), 'complete declared raw Chunk planes')
        P.require(P.exact(before['world_random'], RandomModel(profile['world_seed']).observation()) and
                  P.exact(after['world_random'], expected['world_random']) and
                  P.exact(before['player_random'], baseline[0]['before']['player_random']) and
                  P.exact(after['player_random'], before['player_random']) and
                  P.exact(before['random'][2], RandomModel(0).observation()) and
                  P.exact(after['random'][2], expected['math_random']), 'independent complete World/Player/Math Random paths')
        normal = expected['normal']
        message = 'Remaining plants Item observer interruption' if expected['publication_error'] else 'Remaining plants cell observer interruption'
        P.require(P.exact(row['result'], False if normal else None) and
                  row['exception'] == ('NONE' if normal else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if normal else message), 'actual first return or observer Throwable')
        kinds = ['cell'] + ['added'] * len(expected['items']) + (['cell'] if expected['removed'] else [])
        events = row['events']
        P.require(type(events) is list and len(events) == len(kinds) and all(type(e) is dict for e in events) and
                  [e.get('kind') for e in events] == kinds, 'ordered actual cell/publication prefix')
        item_index = 0
        for event_index, event in enumerate(events):
            added = event['kind'] == 'added'
            removed = event_index != 0 and not added
            keys = {'kind', 'head', 'plant', 'floor', 'sleeping', 'stack', 'item' if added else 'xyz'}
            source_stack = F.FLOWER_STACK if removed else F.HEAD_STACK
            if added:
                if profile['id'] == 59 and expected['items'][item_index]['stack'][0] == 295:
                    source_stack = ['fd.c', 'fd.b', 'ni.a', *F.ADDED_STACK[4:]]
                elif profile['id'] == 59:
                    source_stack = [*F.ADDED_STACK[:4], 'ni.a', *F.ADDED_STACK[4:]]
                else:
                    source_stack = F.ADDED_STACK
            P.require(set(event) == keys and P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                      P.exact(event['plant'], [0, 0] if removed else plant) and P.exact(event['floor'], floor) and
                      P.exact(trace(event['stack']), source_stack), 'actual callback state and Source call order')
            if added:
                P.require(P.exact(event['item'], after['items'][item_index]), 'actual inserted Item survives later work')
                item_index += 1
            else:
                P.require(P.exact(event['xyz'], [7, 64, 8] if removed else [8, 64, 8]), 'actual cell coordinates')
        if normal:
            P.require(P.exact(row['exception_stack'], []), 'no invented Throwable')
        else:
            last = events[-1]
            P.require(P.exact(trace(row['exception_stack']), trace(last['stack'])) and
                      row['exception_stack'][0]['class'] == 'RemainingPlantsWakeObservation$Observer' and
                      row['exception_stack'][0]['method'] == 'a', 'genuine first observer Throwable stack')


def validate_finite(value):
    P.require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and
              value['schema'] == SCHEMA and value['scope'] == SCOPE, 'closed original-only finite envelope')
    validate(value['rows'])


def validate_provenance(proof, directory=HERE):
    directory = Path(directory)
    keys = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'capture_sha256', 'sources', 'shared',
            'methods', 'helpers', 'binaries', 'class_count', 'rows_per_vm', 'original', 'processes',
            'inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof) == keys and proof['schema'] == PROVENANCE and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(directory / 'finite.json') and P.word(proof['capture_sha256'], 64) and
              P.exact(proof['class_count'], 38) and P.exact(proof['rows_per_vm'], 25) and
              all(proof[k] is True for k in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged')),
              'closed original-only provenance and unchanged inputs')
    for key, names in (('sources', SOURCES), ('shared', SHARED), ('helpers', HELPERS), ('binaries', BINARIES)):
        F.fingerprints(proof[key], names, 'closed ' + key + ' inventory')
    P.require(P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()) and proof['binaries']['client'] == P.JAR and
              all(proof['binaries'][name] == digest for name, digest in P.LIBRARIES.items()), 'executed input and original Code bindings')
    P.require(type(proof['processes']) is dict and set(proof['processes']) == set(PROCESS_TIMEOUTS), 'five bounded tool processes')
    for label, timeout in PROCESS_TIMEOUTS.items(): receipt(proof['processes'][label], label, timeout)
    P.require(type(proof['original']) is list and len(proof['original']) == 2, 'two fresh original VMs')
    for index, value in enumerate(proof['original']): receipt(value, f'original-{index}', 70, original=True)
    P.require(all(P.exact(proof['original'][0][key], proof['original'][1][key]) for key in
                  ('exit', 'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256')), 'two identical complete original streams')


def read_reference(directory=HERE):
    directory = Path(directory)
    value = P.load(directory / 'finite.json')
    validate_finite(value)
    validate_provenance(P.load(directory / 'provenance.json'), directory)
    return value


def check(directory=HERE):
    value = read_reference(directory)
    print('Original remaining-plants packet: 25 profiles and bindings pass; native projection remains separate')
    return value


if __name__ == '__main__':
    check()
