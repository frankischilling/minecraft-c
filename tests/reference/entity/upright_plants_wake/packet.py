"""Closed Java-free reader for prepared original Cactus/Reeds wake calls."""
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


R = module('upright_shared_remaining', HERE.parent / 'remaining_plants_wake/packet.py')
G, F, P = R.G, R.F, R.P
SCHEMA = 'UPRIGHT_PLANTS_WAKE_V1'
SCOPE = 'Twenty-one fixed prepared unchanged-original Cactus/Reeds wake calls; original-only packet; native projection, natural encounters and whole-game parity remain separate.'
PROVENANCE = 'UPRIGHT_PLANTS_WAKE_PROVENANCE_V1'
SOURCES = ('UprightPlantsWakeObservation.java', 'capture_reference.py', 'packet.py',
           'test_packet.py', 'README.md', 'profiles.json', 'upright-plant-pins.json')
SHARED = {**G.SHARED, 'family_packet': G.HERE / 'packet.py',
          'family_pins': G.HERE / 'plant-family-pins.json', 'family_finite': G.HERE / 'finite.json',
          'family_provenance': G.HERE / 'provenance.json',
          'remaining_packet': R.HERE / 'packet.py'}
HELPERS = {'ConstructorJson.class', 'UprightPlantsWakeObservation.class',
           'UprightPlantsWakeObservation$Observer.class', 'UprightPlantsWakeObservation$Profile.class',
           'PlayerDamageWakeObservation.class', 'PlayerDamageWakeObservation$Observer.class',
           'PlayerDamageWakeObservation$Guard.class'}
BINARIES = G.BINARIES
EXTRA = G.EXTRA | {'column', 'world_o', 'water_cell', 'scheduled'}
RECEIPT_KEYS, PROCESS_TIMEOUTS = G.RECEIPT_KEYS, G.PROCESS_TIMEOUTS
public_receipt, class_record, receipt = G.public_receipt, G.class_record, G.receipt
CLASS_IDENTITIES = {'or': (1603, 'cc3e2a3c8f38ef3204f09aeccc985ee1438910426fd061dc6ea397fe87678296', 'uu', '3044467651a96d3015325c8cbf2ca06ae805ea2d733da6b26e6c3efcfb1e44ad'), 'ri': (1542, '6ecb444cbeb380afeea0f562272ff39d2aad434e97f4a4ca6e53b21a1d7bbf0b', 'uu', '2a6493920b82f489c8399a83f4e3a4f0bf7a614d50eba5788d75ec12a44857db'), 'ln': (2585, '2fc4925a07555360820ccc20648a7026ac13e5acafc643b3e0b039051c555d24', 'java/lang/Object', '842ec3d38cfcdaff2849e20a500cef2f086645150bf270ef5722de75f861f925'), 'gm': (11051, 'f9d971a83efa0c6399f1fc4bc0bc06c6a6249908afc6b46f5889a49365c190c7', 'java/lang/Object', '6f342c2d15a478426d6525a5b2edbc6664318ea49ff3127975bae1ebb6590270'), 'gk': (1277, '37e191ced3871708dc7b7d886419f29efd32d3fdd6a820b387538dff1b93ed57', 'uu', 'accfe88b4098c2b1cd4ead88facd2908d0f2c6eb0dab7c7bb5880a65ff087280'), 'nx': (1279, '0a2191eee933fa9ae5207d9f6d085f5f93e006ec35459b0fa073ef859104d213', 'rp', '747f23bb910a7e48ea9a10b19c7e1aa82b792911732c04def66a6007e2225b2d'), 'rp': (4326, '659aba3f004ae066c05560529480f3f03db59a3dc1412325847ac01d2f1bbec6', 'uu', '5cb6fed0d9b3bae503d979966ef61b5be1ca6829859d0d2075cc2169625ce9c5'), 'om': (3398, '189fe9f8e3a363be62eefbbdd60052b811fb1d25a011f42df265f9a436e49fb2', 'rp', '3d3462c20dc244c0c85d99db9eb9ecbb45d4492879b155a06962692ba858af59'), 'qy': (992, '500a13a82592d4aa09a9b36de409138962f7a43e31f5bd6906976d4d01630c83', 'java/lang/Object', 'c410a2336ac452c1b14ea68b1e956ee2caec1276616e8e05165c58edca2928b2')}
SELECTED_METHODS = {('or', 'b', '(Lfd;IIII)V'): (40, '1db3219cc815137d8f0f04e6ac8f0acf3c2636a7f93c50b6ea8a2df650b47d24'), ('or', 'g', '(Lfd;III)Z'): (112, '8f11e9cc7f2625dfa008a189552cca435c0ce47012e843b7876a9294191eeaf3'), ('ri', 'b', '(Lfd;IIII)V'): (10, 'ebd38fafae362134d0cec09ca6136f5dd7bfe460d6e7d8542c4354a49f3e607b'), ('ri', 'h', '(Lfd;III)V'): (40, 'd04807fac159522e6c6e2ecc6a44ac2d8d3054d53fd5c36823df98b7c3123b58'), ('ri', 'g', '(Lfd;III)Z'): (10, '0d9b8c22bcfd352647c1211b84075711abe12bda339daebef497476be6d685fc'), ('ri', 'a', '(Lfd;III)Z'): (129, '02c7c25b36087f15ba8ccd9a9d72cd9999cb14281acf5bc588255b04602c89d6'), ('ri', 'a', '(ILjava/util/Random;)I'): (7, '8e27457eb4770c5b8a4e1f8f6c3ed3c9ec4e72eff73d3959446ca87657d0f42e'), ('ln', 'a', '()Z'): (2, '95ef737cd8a13db65e3eb3c18e8816a5179dcbdf341309522ab00e95d5aae6d2'), ('fd', 'f', '(III)Lln;'): (28, '77f4fc68a1aab798c713c4367f49202d55d7f1a6fa471e6998249089c64d1c48'), ('gm', '<clinit>', '()V'): (2668, 'd839820ccc643d594ee77ca3f701d60e4741249305b01b63edcca8a49d58b269'), ('gk', 'b', '(Lfd;IIII)V'): (17, 'f8f645365b48180d81d998f71445c0bf6742079be84ddcb80ec8ebe640801fd7'), ('gk', 'e', '()I'): (2, '2aa3b8c641f9fb2da52054da18aeb7ce8524fd71d21f4ce9d042f43f527cc675'), ('nx', 'b', '(Lfd;IIII)V'): (36, '4dda9947fe89b24ca7c1bda9b3ab97bcdd5836d548a0b6ddf4e8cb4a9bbc1536'), ('nx', 'j', '(Lfd;III)V'): (68, '2a5ce753dc87edd4d2e08cf9565615b38d4e2b4fe7e6bdea398cf1c40abc07fa'), ('rp', 'b', '(Lfd;IIII)V'): (10, '99dec0be4c7f6fbc6f06c0e1799384688c9daa03314428318e907de741ebdca6'), ('rp', 'c', '(Lfd;III)V'): (10, '99dec0be4c7f6fbc6f06c0e1799384688c9daa03314428318e907de741ebdca6'), ('rp', 'j', '(Lfd;III)V'): (218, '3001885c33fc399b98ce801865ea3e537f4569e720b82d6ec04d63572af17700'), ('rp', 'e', '()I'): (27, 'd560033d178cfe8bee0c27a1a35b4c7da571df39b441c8635aadf1e98a351b42'), ('om', 'c', '(Lfd;III)V'): (41, '9ff8d75dde6eb9d5e501e2d73b9c63fa722cc5a27f079b7ccfbad880ad7c76be'), ('qy', '<init>', '(IIII)V'): (39, 'ddfa908a1c5063d81f4d1ba916878741507a1acaa786d9418c6abfd8fac78d12'), ('qy', '<clinit>', '()V'): (5, '295221cd03e94a561995d173f4c95d33c1b08075bb6a6582afd178bc40019ae1'), ('fd', 'c', '(IIIII)V'): (236, '6f37f2fa8030e0a959fb27095502550b5b9ed94a8ed2583a4c65fe51d69a1899'), ('ve', 'f', '(Lfd;IIII)Lbr;'): (177, '73d8e8bf553c2ff112e2ec45c26941790fc8712020f5228954ce15e757cb3327'), ('fd', 'h', '(III)Z'): (45, '9159cea1434cb4ef1f90e98a19398660c319d0876cab388766cdd40b18e9dd11'), ('fd', 'b', '(IIIIII)V'): (52, 'ebdc8942d1e45b1fd8c8aec55210a8ccb1dca9b907df26fdb55de08b03a71238'), ('fd', 'a', '(IIIIII)Z'): (91, '813486d41e9bee099568d81c26f281020b62f80a0dbf3da682902669d2ebd62d')}
PROFILE_OPERANDS = (('cactus-bed', 81, 0, 12, 15, 1, 1, -1, 0, 0, 0, False), ('cactus-publication-error', 81, 0, 12, 15, 1, 1, -1, 0, 1, 0, False), ('cactus-removal-error', 81, 0, 12, 15, 1, 1, -1, 0, 0, 64, False), ('cactus-column3', 81, 0, 12, 15, 1, 3, -1, 0, 0, 0, False), ('cactus-column3-publication-error-second', 81, 0, 12, 15, 1, 3, -1, 0, 2, 0, False), ('cactus-column3-removal-error-middle', 81, 0, 12, 15, 1, 3, -1, 0, 0, 65, False), ('reeds-sand', 83, 0, 12, 15, 1, 1, -1, 0, 0, 0, False), ('reeds-air', 83, 0, 0, 15, 1, 1, -1, 0, 0, 0, False), ('reeds-dry-grass', 83, 0, 2, 15, 1, 1, -1, 0, 0, 0, False), ('reeds-west-water', 83, 0, 2, 15, 1, 1, 0, 9, 0, 0, False), ('reeds-east-water', 83, 0, 2, 15, 1, 1, 1, 9, 0, 0, False), ('reeds-north-water', 83, 0, 2, 15, 1, 1, 2, 9, 0, 0, False), ('reeds-south-water', 83, 0, 2, 15, 1, 1, 3, 9, 0, 0, False), ('reeds-flowing-water', 83, 0, 3, 15, 1, 1, 0, 8, 0, 0, False), ('reeds-stacked', 83, 0, 83, 15, 1, 1, -1, 0, 0, 0, False), ('reeds-publication-error', 83, 0, 2, 15, 1, 1, -1, 0, 1, 0, False), ('reeds-removal-error', 83, 0, 2, 15, 1, 1, -1, 0, 0, 64, False), ('reeds-column3', 83, 0, 2, 15, 1, 3, -1, 0, 0, 0, False), ('reeds-column3-publication-error-second', 83, 0, 2, 15, 1, 3, -1, 0, 2, 0, False), ('reeds-column3-removal-error-middle', 83, 0, 2, 15, 1, 3, -1, 0, 0, 65, False), ('reeds-east-water-range-error', 83, 0, 2, 15, 1, 1, 1, 9, 0, 0, True))
PROFILE_KEYS = ('mode', 'id', 'metadata', 'floor', 'light', 'world_seed', 'columns',
                'water_direction', 'water_id', 'publication_error_at', 'removal_error_y', 'range_error')
WATER = ((-1, 0), (1, 0), (0, -1), (0, 1))


def profiles():
    expected = [dict(zip(PROFILE_KEYS, values)) for values in PROFILE_OPERANDS]
    value = P.load(HERE / 'profiles.json')
    P.require(P.exact(value, expected), 'predeclared ordered upright operands')
    return value


MODES = tuple(values[0] for values in PROFILE_OPERANDS)
f32, RandomModel = R.f32, R.RandomModel


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def validate_class_pins(extra):
    P.require(type(extra) is dict and set(extra) == {'archive_sha256', 'classes', 'methods'} and
              extra['archive_sha256'] == P.JAR and type(extra['classes']) is dict and
              set(extra['classes']) == set(CLASS_IDENTITIES), 'closed upright class pins')
    for name, (size, digest, parent, structure) in CLASS_IDENTITIES.items():
        if name != 'qy':
            class_record(extra['classes'][name], name, size, digest, parent)
        else:
            record = extra['classes'][name]
            P.require(type(record) is dict and record.get('name') == name and
                      record.get('super') == parent and record.get('interfaces') == ['java/lang/Comparable'] and
                      P.exact(record.get('class_bytes'), size) and record.get('class_sha256') == digest,
                      'actual Comparable scheduled-record class')
        P.require(G.structural_hash(extra['classes'][name]) == structure, 'unchanged original class structure')
    expected = {owner + '.' + name + descriptor: {'code_length': length, 'code_sha256': digest}
                for (owner, name, descriptor), (length, digest) in SELECTED_METHODS.items()}
    P.require(P.exact(extra['methods'], expected), 'exact original upright/material Code authorities')


def class_pins():
    base = G.class_pins()
    extra = P.load(HERE / 'upright-plant-pins.json')
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
    kind = 'or' if profile['id'] == 81 else 'ri'
    expected = {'class': kind}
    for key, owner, name, descriptor in (
            ('neighbor', kind, 'b', '(Lfd;IIII)V'), ('survival', kind, 'g', '(Lfd;III)Z'),
            ('placement', kind, 'a', '(Lfd;III)Z'), ('drop_wrapper', 'uu', 'g', '(Lfd;IIII)V'),
            ('drop_chance', 'uu', 'a', '(Lfd;IIIIF)V'),
            ('drop_item', 'uu' if kind == 'or' else 'ri', 'a', '(ILjava/util/Random;)I'),
            ('drop_metadata', 'uu', 'b_', '(I)I'), ('drop_count', 'uu', 'a', '(Ljava/util/Random;)I')):
        expected[key] = {'class': owner, 'method': name, 'descriptor': descriptor}
    P.require(P.exact(value, expected), 'actual receiver and virtual declaring methods')


def column(profile, removed_count):
    return [[profile['id'], profile['metadata']] if removed_count <= i < profile['columns'] else [0, 0]
            for i in range(3)]


def assignments(profile):
    result = [[7, 64 + i, 8, profile['id'], profile['metadata']] for i in range(profile['columns'])]
    result.append([7, 63, 8, profile['floor'], 0])
    if profile['water_direction'] != -1:
        dx, dz = WATER[profile['water_direction']]
        result.append([7 + dx, 63, 8 + dz, profile['water_id'], 0])
    return result


def water_cell(profile, converted=False):
    if profile['water_direction'] == -1:
        return None
    dx, dz = WATER[profile['water_direction']]
    return [7 + dx, 63, 8 + dz, 8 if converted else profile['water_id'], 0]


def model(profile):
    """Declared straight-column source order and unsigned Java LCG; no runtime selectors."""
    supported = profile['id'] == 83 and (profile['floor'] == 83 or
        profile['floor'] in (2, 3) and profile['water_direction'] != -1 and profile['water_id'] in (8, 9))
    random, math = RandomModel(profile['world_seed']), RandomModel(0)
    items, events = [], [{'kind': 'cell', 'xyz': [8, 64, 8], 'removed_count': 0, 'items': 0}]
    removed = 0
    publication_error = removal_error = False
    if not supported:
        for level in range(profile['columns']):
            random.floating()  # Inherited probability comparison, even for these certain drops.
            offsets = []
            for coordinate in (7, 64 + level, 8):
                spread = f32(.7)
                scaled, edge = f32(random.floating() * spread), f32(1.0 - spread)
                offsets.append(struct.pack('>d', coordinate + scaled + edge * .5).hex())
            phase = struct.pack('>f', f32(math.double() * 3.141592653589793 * 2.0)).hex()
            yaw = struct.pack('>f', f32(math.double() * 360.0)).hex()
            vx = f32(math.double() * .20000000298023224 - .10000000149011612)
            vz = f32(math.double() * .20000000298023224 - .10000000149011612)
            item = {'stack': [81 if profile['id'] == 81 else 338, 1, 0], 'position': offsets,
                    'phase': phase, 'yaw': yaw,
                    'motion': [struct.pack('>d', value).hex() for value in (vx, .20000000298023224, vz)]}
            items.append(item)
            events.append({'kind': 'added', 'removed_count': removed, 'items': len(items),
                           'level': level, 'world_random': random.observation()})
            if profile['publication_error_at'] == len(items):
                publication_error = True
                break
            removed += 1
            events.append({'kind': 'cell', 'xyz': [7, 64 + level, 8], 'removed_count': removed,
                           'items': len(items), 'level': level})
            if profile['removal_error_y'] == 64 + level:
                removal_error = True
                break
    converted = profile['water_direction'] == 1 and profile['water_id'] == 9
    if converted:
        events.append({'kind': 'range', 'xyz': [8, 63, 8, 8, 63, 8], 'removed_count': 0, 'items': 0})
    # FD.i visits the Sand beneath the removed bottom before the upper column.
    # A cell observer failure stops before that notification; later failures
    # retain the already allocated-but-unqueued scheduled record.
    sand = profile['floor'] == 12 and removed > 0 and not (
        removal_error and profile['removal_error_y'] == 64)
    range_error = converted and profile['range_error']
    return dict(supported=supported, items=items, events=events, removed_count=removed,
                publication_error=publication_error, removal_error=removal_error,
                range_error=range_error, world_o=range_error,
                normal=not (publication_error or removal_error or range_error), world_random=random.observation(),
                math_random=math.observation(), draws=random.draws,
                water_converted=converted, schedule_requests=int(sand) + (1 if range_error else 2) * int(converted))


def planes(profile, after, removed_count):
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
    cell(8, 64, 8, 26, 8 if after else 12)
    cell(8, 64, 7, 26, 0)
    for x, y, z, block, data in assignments(profile): cell(x, y, z, block, data)
    if after:
        for level in range(removed_count): cell(7, 64 + level, 8, 0, 0)
        if profile['water_direction'] == 1 and profile['water_id'] == 9:
            cell(8, 63, 8, 8, 0)
    values = dict(blocks=blocks, metadata=metadata, sky=sky, block_light=bytes(16384), height=bytes([64]) * 256)
    return {name: hashlib.sha256(data).hexdigest() for name, data in values.items()}


def item(value, expected):
    G.item(value, expected['stack'])
    P.require(P.exact({key: value[key] for key in expected}, expected), 'independent Item positions and Math draws')
    P.require(P.exact(value['dimensions'], ['3e800000', '3e800000', '3e000000']), 'actual Item dimensions')


def state(value, profile, expected_items):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('plant', 'plant_floor', 'foot'):
        P.require(P.vector(value[key], 2, P.integer) and 0 <= value[key][0] <= 255 and
                  0 <= value[key][1] <= 15, 'typed cell ' + key)
    P.require(P.vector(value['column'], 3, lambda c: P.vector(c, 2, P.integer)), 'complete three-cell column')
    P.require(type(value['world_o']) is bool, 'actual World notification flag')
    water = value['water_cell']
    P.require(water is None or P.vector(water, 5, P.integer), 'actual declared water cell')
    scheduled = value['scheduled']
    P.require(type(scheduled) is dict and set(scheduled) == {'initialized', 'sequence', 'ordered', 'members'} and
              type(scheduled['initialized']) is bool and P.word(scheduled['sequence'], 16) and
              type(scheduled['ordered']) is list and type(scheduled['members']) is list,
              'complete passive scheduled-class and World queue observations')
    light = value['light_inputs']
    P.require(type(light) is dict and set(light) == F.LIGHT_KEYS and P.exact(light['position'], [7, 64, 8]) and
              P.exact([light[k] for k in ('sky_nibble', 'block_light_nibble', 'height',
                  'world_f_sky_subtraction', 'fd_m_chunk_subtraction_literal')], [15, 0, 64, 5, 0]) and
              light['chunk_sky_seen_static'] is False, 'actual unchanged light inputs; no plant light call')
    for key, stream in (('world_random', value['random'][1]), ('player_random', value['random'][0])):
        P.require(P.exact(value[key], stream), 'same complete Random ' + key)
    P.require(type(value['items']) is list and len(value['items']) == len(expected_items), 'exact Item count')
    for actual, expected in zip(value['items'], expected_items): item(actual, expected)


def trace(value):
    F.trace(value)
    return [f['class'] + '.' + f['method'] for f in value if f['class'] in {'fd', 've', 'uu', 'gs', 'or', 'ri', 'nx', 'rp', 'om', 'gk', 'qy'}]


def source_stack(profile, kind, level):
    plant = ['or.b'] if profile['id'] == 81 else ['ri.h', 'ri.b']
    prefix = ['fd.c', 'fd.b', 'uu.a', 'uu.a', 'uu.g'] if kind == 'added' else ['fd.j', 'fd.g', 'fd.f']
    nested = ['fd.l', 'fd.i', 'fd.g', 'fd.f', *plant]
    return prefix + plant + nested * level + ['fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']


def exit_position(profile, removed_count):
    """First proved ve.f candidate over these declared Grass/Water/plant cells."""
    cells = {(x, y, z): block for x, y, z, block, metadata in assignments(profile)}
    cells.update({(8, 64, 8): 26, (8, 64, 7): 26})
    for level in range(removed_count): cells[(7, 64 + level, 8)] = 0
    def block(x, y, z):
        return cells.get((x, y, z), 2 if y == 63 else 0)
    # Orientation0's first pass scans X7..9 then Z7..9. These finite
    # profiles always return during that pass. Grass/Dirt/Sand supply support;
    # Water and Reeds do not. Both cells above the support must be actual Air.
    for x in range(7, 10):
        for z in range(7, 10):
            if block(x, 63, z) in (2, 3, 12) and block(x, 64, z) == block(x, 65, z) == 0:
                return x + .5, z + .5
    raise ValueError('Declared fixture must reach first-pass Bed exit')


def after_base(profile, expected, baseline):
    reference = baseline[0]['after'] if expected['normal'] else baseline[2]['after']
    result = {key: value for key, value in reference.items()}
    if expected['normal']:
        x, z = exit_position(profile, expected['removed_count'])
        result['position'] = list(reference['position'])
        result['position'][0], result['position'][2] = (struct.pack('>d', value).hex() for value in (x, z))
        width = struct.unpack('>f', bytes.fromhex(reference['dimensions'][0]))[0]
        for key in ('bounds', 'original_bounds'):
            result[key] = list(reference[key])
            for index, value in ((0, x - width * .5), (3, x + width * .5),
                                 (2, z - width * .5), (5, z + width * .5)):
                result[key][index] = struct.pack('>d', value).hex()
    return result


def validate(rows):
    P.require(type(rows) is list and len(rows) == 21, 'twenty-one ordered original calls')
    baseline = G.read_reference()['rows']
    base_keys = P.STATE_KEYS - {'planes', 'random', 'world_entities'}
    sequence = 0
    initialized = False
    for row, profile in zip(rows, profiles()):
        expected = model(profile)
        P.require(type(row) is dict and set(row) == {'mode', 'receiver', 'prepared', 'before', 'after',
            'result', 'exception', 'exception_message', 'exception_stack', 'events'} and
            row['mode'] == profile['mode'], 'closed ordered original row')
        receiver(row['receiver'], profile)
        P.require(P.exact(row['prepared'], {'profile': profile, 'shared_setup': [0, 0, 20, 0, 0],
            'post_setup_cell_assignments': assignments(profile), 'world_random_reseed': 1,
            'light_or_sky_getter_overrides': []}), 'sealed prepared cells and supplied Random seed')
        before, after = row['before'], row['after']
        state(before, profile, [])
        state(after, profile, expected['items'])
        P.require(P.exact(before['scheduled'], {'initialized': initialized, 'sequence': f'{sequence:016x}',
                    'ordered': [], 'members': []}), 'actual preceding global scheduled-class state')
        sequence += expected['schedule_requests']
        initialized = initialized or expected['schedule_requests'] != 0
        P.require(P.exact(after['scheduled'], {'initialized': initialized, 'sequence': f'{sequence:016x}',
                    'ordered': [], 'members': []}) and before['world_o'] is False and after['world_o'] is expected['world_o'],
                    'exact allocated record sequence; both actual sets empty and reached World flag prefix')
        P.require(P.exact(before['water_cell'], water_cell(profile)) and
                  P.exact(after['water_cell'], water_cell(profile, expected['water_converted'])),
                  'actual below-Bed water conversion')
        P.require(P.exact({k: before[k] for k in base_keys}, {k: baseline[0]['before'][k] for k in base_keys}), 'shared prepared Player/World state')
        comparison = after_base(profile, expected, baseline)
        P.require(P.exact({k: after[k] for k in base_keys}, {k: comparison[k] for k in base_keys}), 'shared actual wake/exception prefix')
        P.require(P.exact(before['column'], column(profile, 0)) and
                  P.exact(after['column'], column(profile, expected['removed_count'])) and
                  P.exact(before['plant'], before['column'][0]) and P.exact(after['plant'], after['column'][0]) and
                  P.exact([before[k] for k in ('plant_floor', 'foot', 'world_entities')], [[profile['floor'], 0], [26, 0], 1]) and
                  P.exact([after[k] for k in ('plant_floor', 'foot', 'world_entities')], [[profile['floor'], 0], [26, 0], 1 + len(expected['items'])]),
                  'committed column cells and actual World Entity count')
        P.require(P.exact(before['planes'], planes(profile, False, 0)) and
                  P.exact(after['planes'], planes(profile, True, expected['removed_count'])), 'complete raw Chunk planes')
        P.require(P.exact(before['world_random'], RandomModel(1).observation()) and
                  P.exact(after['world_random'], expected['world_random']) and
                  P.exact(before['player_random'], baseline[0]['before']['player_random']) and
                  P.exact(after['player_random'], before['player_random']) and
                  P.exact(before['random'][2], RandomModel(0).observation()) and
                  P.exact(after['random'][2], expected['math_random']), 'independent complete World/Player/Math streams')
        normal = expected['normal']
        message = ('Upright plants Item observer interruption' if expected['publication_error'] else
                   'Upright plants range observer interruption' if expected['range_error'] else
                   'Upright plants cell observer interruption')
        P.require(P.exact(row['result'], False if normal else None) and
                  row['exception'] == ('NONE' if normal else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if normal else message), 'actual first return or observer Throwable')
        events = row['events']
        P.require(type(events) is list and len(events) == len(expected['events']), 'exact ordered callback count')
        for index, (event, prediction) in enumerate(zip(events, expected['events'])):
            added = prediction['kind'] == 'added'
            keys = {'kind', 'head', 'plant', 'column', 'floor', 'sleeping', 'stack',
                    'world_o', 'water_cell', 'item' if added else 'xyz'}
            expected_column = column(profile, prediction['removed_count'])
            ranged = prediction['kind'] == 'range'
            expected_stack = (F.HEAD_STACK if index == 0 else
                ['fd.b', 'nx.j', 'nx.b', 'fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a'] if ranged else
                source_stack(profile, prediction['kind'], prediction['level']))
            P.require(type(event) is dict and set(event) == keys and event['kind'] == prediction['kind'] and
                      P.exact(event['head'], [26, 8]) and event['sleeping'] is True and
                      P.exact(event['column'], expected_column) and P.exact(event['plant'], expected_column[0]) and
                      event['world_o'] is ranged and P.exact(event['water_cell'], water_cell(profile, ranged)) and
                      P.exact(event['floor'], [profile['floor'], 0]) and P.exact(trace(event['stack']), expected_stack),
                      'actual ordered callback cells and genuine original call stack')
            if added:
                P.require(P.exact(event['item'], after['items'][prediction['items'] - 1]), 'actual Item survives later effects')
            else:
                P.require(P.exact(event['xyz'], prediction['xyz']), 'actual callback coordinates')
        if normal:
            P.require(P.exact(row['exception_stack'], []), 'no invented Throwable')
        else:
            P.require(P.exact(trace(row['exception_stack']), trace(events[-1]['stack'])) and
                      row['exception_stack'][0]['class'] == 'UprightPlantsWakeObservation$Observer' and
                      row['exception_stack'][0]['method'] == ('b' if expected['range_error'] else 'a'),
                      'first genuine observer Throwable stack')


def validate_finite(value):
    P.require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and
              value['schema'] == SCHEMA and value['scope'] == SCOPE, 'closed original finite envelope')
    validate(value['rows'])


def validate_provenance(proof, directory=HERE):
    directory = Path(directory)
    keys = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'capture_sha256', 'sources', 'shared',
            'methods', 'helpers', 'binaries', 'class_count', 'rows_per_vm', 'original', 'processes',
            'inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof) == keys and proof['schema'] == PROVENANCE and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(directory / 'finite.json') and P.word(proof['capture_sha256'], 64) and
              P.exact(proof['class_count'], 43) and P.exact(proof['rows_per_vm'], 21) and
              all(proof[k] is True for k in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged')),
              'closed original provenance with unchanged inputs')
    for key, names in (('sources', SOURCES), ('shared', SHARED), ('helpers', HELPERS), ('binaries', BINARIES)):
        F.fingerprints(proof[key], names, 'closed ' + key + ' inventory')
    P.require(P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()) and proof['binaries']['client'] == P.JAR and
              all(proof['binaries'][name] == digest for name, digest in P.LIBRARIES.items()), 'executed original input and Code bindings')
    P.require(type(proof['processes']) is dict and set(proof['processes']) == set(PROCESS_TIMEOUTS), 'five bounded tool processes')
    for label, timeout in PROCESS_TIMEOUTS.items(): receipt(proof['processes'][label], label, timeout)
    P.require(type(proof['original']) is list and len(proof['original']) == 2, 'two fresh original VMs')
    for index, value in enumerate(proof['original']): receipt(value, f'original-{index}', 70, original=True)
    P.require(all(P.exact(proof['original'][0][key], proof['original'][1][key]) for key in
                  ('exit', 'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256')), 'identical complete fresh-VM streams')


def read_reference(directory=HERE):
    directory = Path(directory)
    value = P.load(directory / 'finite.json')
    validate_finite(value)
    validate_provenance(P.load(directory / 'provenance.json'), directory)
    return value


def check(directory=HERE):
    value = read_reference(directory)
    print('Original upright-plants packet: 21 profiles and bindings pass; native projection remains separate')
    return value


if __name__ == '__main__':
    check()
