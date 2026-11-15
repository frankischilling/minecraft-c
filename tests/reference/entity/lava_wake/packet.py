"""Closed Java-free reader for prepared original Lava mixing during Player wake."""
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


U = module('lava_shared_upright', HERE.parent / 'upright_plants_wake/packet.py')
G, F, P = U.G, U.F, U.P
SCHEMA = 'LAVA_WAKE_V1'
SCOPE = 'Twenty-nine fixed prepared unchanged-original Lava wake calls; original-only packet; native projection, natural encounters, graphics and whole-game parity remain separate.'
PROVENANCE = 'LAVA_WAKE_PROVENANCE_V1'
SOURCES = ('LavaWakeObservation.java', 'capture_reference.py', 'packet.py', 'profiles.json',
           'test_packet.py', 'README.md', 'lava-method-pins.json')
SHARED = {**U.SHARED, 'upright_java': U.HERE / 'UprightPlantsWakeObservation.java',
          'upright_packet': U.HERE / 'packet.py', 'upright_profiles': U.HERE / 'profiles.json',
          'upright_pins': U.HERE / 'upright-plant-pins.json', 'upright_finite': U.HERE / 'finite.json',
          'upright_provenance': U.HERE / 'provenance.json'}
HELPERS = U.HELPERS | {'LavaWakeObservation.class', 'LavaWakeObservation$Observer.class',
                       'LavaWakeObservation$Profile.class'}
BINARIES = U.BINARIES
RECEIPT_KEYS, PROCESS_TIMEOUTS = U.RECEIPT_KEYS, U.PROCESS_TIMEOUTS
public_receipt, class_record, receipt = U.public_receipt, U.class_record, U.receipt
RandomModel, f32 = U.RandomModel, U.f32
PROFILE_KEYS = ('mode', 'id', 'metadata', 'x', 'y', 'z', 'water_direction', 'water_id', 'world_seed',
                'cell_error_at', 'sound_error_at', 'particle_error_at', 'range_error_at')
PROFILE_OPERANDS = (('lava-obsidian', 11, 0, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-cobblestone-meta1', 11, 1, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-cobblestone-meta2', 11, 2, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-cobblestone-meta3', 11, 3, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-cobblestone-meta4', 11, 4, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-hot-meta5', 11, 5, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-hot-meta15', 11, 15, 7, 64, 8, 0, 8, 1, 0, 0, 0, 0), ('lava-no-contact-meta0', 11, 0, 7, 64, 8, -1, 0, 1, 0, 0, 0, 0), ('lava-no-contact-meta5', 11, 5, 7, 64, 8, -1, 0, 1, 0, 0, 0, 0), ('lava-obsidian-east-water', 11, 0, 8, 64, 9, 1, 8, 1, 0, 0, 0, 0), ('lava-obsidian-north-water', 11, 0, 7, 64, 8, 2, 8, 1, 0, 0, 0, 0), ('lava-obsidian-south-water', 11, 0, 7, 64, 8, 3, 8, 1, 0, 0, 0, 0), ('lava-obsidian-above-water', 11, 0, 7, 64, 8, 4, 8, 1, 0, 0, 0, 0), ('lava-obsidian-still-water', 11, 0, 7, 64, 8, 0, 9, 1, 0, 0, 0, 0), ('lava-hot-still-water', 11, 5, 7, 64, 8, 0, 9, 1, 0, 0, 0, 0), ('lava-obsidian-cell-error', 11, 0, 7, 64, 8, 0, 8, 1, 2, 0, 0, 0), ('lava-obsidian-sound-error', 11, 0, 7, 64, 8, 0, 8, 1, 0, 1, 0, 0), ('lava-obsidian-particle-error-first', 11, 0, 7, 64, 8, 0, 8, 1, 0, 0, 1, 0), ('lava-obsidian-particle-error-fourth', 11, 0, 7, 64, 8, 0, 8, 1, 0, 0, 4, 0), ('lava-hot-sound-error-first', 11, 5, 7, 64, 8, 0, 8, 1, 0, 1, 0, 0), ('lava-hot-sound-error-second', 11, 5, 7, 64, 8, 0, 8, 1, 0, 2, 0, 0), ('lava-hot-particle-error-third', 11, 5, 7, 64, 8, 0, 8, 1, 0, 0, 3, 0), ('lava-hot-particle-error-ninth', 11, 5, 7, 64, 8, 0, 8, 1, 0, 0, 9, 0), ('lava-hot-range-error', 11, 5, 7, 64, 8, 0, 8, 1, 0, 0, 0, 1), ('lava-no-contact-range-error', 11, 5, 7, 64, 8, -1, 0, 1, 0, 0, 0, 1), ('lava-obsidian-height-range-error', 11, 0, 7, 64, 8, 0, 8, 1, 0, 0, 0, 1), ('lava-hot-late-range-error', 11, 5, 7, 64, 8, 0, 8, 1, 0, 0, 0, 2), ('lava-no-contact-late-range-error', 11, 5, 7, 64, 8, -1, 0, 1, 0, 0, 0, 2), ('lava-obsidian-still-water-late-range-error', 11, 0, 7, 64, 8, 0, 9, 1, 0, 0, 0, 3))
CLASS_IDENTITIES = {'sg': (356, '224f89c79e507c1b78121acd6a7f8e002f6ea2860b51700d5c89d8e8cc3dc4c6', 'ln', 'b8da9762849e269a3fabc01af3f19bed10a10107789d476ee61fb2704425f517'), 'st': (2380, '35b4d2a3fb39fe52705a05cb80a38c94dc141b8e8394947bf4f7b2c983aa9bf3', 'java/lang/Object', '20feb24e64a87f829625843df446c966309cb83ddf433bcbce122c93c726e81b'), 'eb': (734, 'da8164fc071e412e751668ffa0f79a42bcbb24d2c475e9cf34a7c7f4bae04176', 'java/lang/Enum', '760cd4e1ce6a808cf3d1aa7adc0cc1e857f7b5eb0fd5880af4765df72fa73ae2')}
SELECTED_METHODS = {('rp', 'i', '(Lfd;III)V'): (93, '52b1dce0df75862b3d34d7d7b615c8b1c1fc394f0f5542ca6397ab46fefd2e21'), ('rp', 'j', '(Lfd;III)V'): (218, '3001885c33fc399b98ce801865ea3e537f4569e720b82d6ec04d63572af17700'), ('fd', 'a', '(DDDLjava/lang/String;FF)V'): (53, 'b1355e57eae05b09748d8ab2c5031f163ca5aaa093e5ea0c68d142374b7b1021'), ('fd', 'a', '(Ljava/lang/String;DDDDDD)V'): (55, '7327f1e8a539dfb9bd82998e5f8ba88793a47003cef7e3d3ff5a7d13a685e835'), ('fd', 'f', '(IIII)Z'): (25, 'b5035150f7e871f4f969f27e70730bf1a0323e01cf2d1012fb08badcdfabe501'), ('lm', 'a', '(IIII)Z'): (263, 'd0cbfa80dab1603dacf359c51110f59f90b21f7567df449743b866cf5b072707'), ('lm', 'g', '(III)V'): (465, '0bcb84786164b527fef2dcc64b05064c5e9efe7d5bc6cbc21961b8a6e5c5e015'), ('fd', 'a', '(Leb;IIIIII)V'): (17, '0f682906e572d130df913d80776838b3debb813ced9387d65e43834d92609e7f'), ('st', 'a', '(IIIIII)Z'): (326, 'aea51e4f2041b50b82e20bafc624a6ec19198ec7344abdc6ecacf7994644ad7a')}
MODES = tuple(row[0] for row in PROFILE_OPERANDS)
EXTRA = {'lava', 'water_cell', 'world_o', 'scheduled', 'lighting', 'sky_seen',
         'world_random', 'player_random', 'foot', 'items'}
WATER = ((-1,0,0),(1,0,0),(0,0,-1),(0,0,1),(0,1,0))


def profiles():
    value = P.load(HERE / 'profiles.json')
    P.require(P.exact(value, [dict(zip(PROFILE_KEYS, row)) for row in PROFILE_OPERANDS]),
              'fixed ordered Lava operands')
    return value


def sources():
    return {name: P.digest(HERE / name) for name in SOURCES}


def shared():
    return {name: P.digest(path) for name, path in SHARED.items()}


def class_pins():
    base = U.class_pins()
    extra = P.load(HERE / 'lava-method-pins.json')
    validate_class_pins(extra)
    return {'archive': base['archive'], 'classes': {**base['classes'], **extra['classes']}}


def validate_class_pins(extra):
    P.require(type(extra) is dict and set(extra) == {'archive_sha256', 'classes', 'methods'} and
              extra['archive_sha256'] == P.JAR and type(extra['classes']) is dict and
              set(extra['classes']) == set(CLASS_IDENTITIES), 'closed additional Lava/light classes')
    for name, (size, digest, parent, structure) in CLASS_IDENTITIES.items():
        class_record(extra['classes'][name], name, size, digest, parent)
        P.require(G.structural_hash(extra['classes'][name]) == structure, 'complete additional original class structure')
    expected = {owner + '.' + name + descriptor: {'code_length': size, 'code_sha256': digest}
                for (owner, name, descriptor), (size, digest) in SELECTED_METHODS.items()}
    P.require(P.exact(extra['methods'], expected), 'closed additional original Code inventory')


def method_pins():
    classes, result = class_pins()['classes'], U.method_pins()
    expected = {owner + '.' + name + descriptor: {'code_length': size, 'code_sha256': digest}
                for (owner, name, descriptor), (size, digest) in SELECTED_METHODS.items()}
    value = P.load(HERE / 'lava-method-pins.json')
    P.require(P.exact(value['methods'], expected), 'closed original Lava Code pins')
    for owner, name, descriptor in SELECTED_METHODS:
        selected = [m for m in classes[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
        key = owner + '.' + name + descriptor
        P.require(len(selected) == 1 and P.exact({k: selected[0][k] for k in expected[key]}, expected[key]),
                  'unchanged original Lava method authority')
        result[key] = expected[key]
    return result


def water_position(profile):
    if profile['water_direction'] == -1: return None
    return tuple(profile[k] + d for k, d in zip(('x', 'y', 'z'), WATER[profile['water_direction']]))


def assignments(profile):
    result = [[profile[k] for k in ('x', 'y', 'z')] + [profile['id'], profile['metadata']]]
    water = water_position(profile)
    if water is not None: result.append([*water, profile['water_id'], 0])
    return result


def double(value): return struct.pack('>d', value).hex()
def single(value): return struct.pack('>f', value).hex()


class Interrupted(Exception):
    pass


def model(profile):
    """Finite body-derived order, Java LCG/IEEE rounding and light-region merge.

    This interpreter accepts the declared cells, never observed output. It models
    only the reached neighbor/mixing/quiet setters; no simulation tick runs.
    """
    cells = {(8, 64, 8): [26, 8], (8, 64, 7): [26, 0]}
    cells.update({tuple(row[:3]): row[3:] for row in assignments(profile)})
    heights, skylight, regions, events = {}, {}, [], []
    random, math = RandomModel(profile['world_seed']), RandomModel(0)
    counts = dict(cell=0, range=0, sound=0, particle=0)
    world_o, requests, failure = False, 0, None
    xyz = tuple(profile[k] for k in ('x', 'y', 'z'))
    water = water_position(profile)
    # uu.q values of the declared fixed blocks. Lava is fully sky-opaque;
    # Water attenuates three levels. Other cells are prepared Grass/Air/Bed.
    opacity = {0: 0, 2: 255, 4: 255, 8: 3, 9: 3, 10: 255, 11: 255, 26: 0, 49: 255}
    outer = ['fd.l', 'fd.i', 'fd.g', 'fd.d', 've.a', 'gs.a', 'gs.a']
    mixing = ['rp.j', 'rp.b', 'nx.b', *outer]

    def cell(at): return list(cells.get(tuple(at), [2, 0] if at[1] == 63 else [0, 0]))
    def water_cell(): return [*water, *cell(water)] if water is not None else None
    def emit(kind, stack, **payload):
        event = dict(kind=kind, head=[26, 8], lava=cell(xyz), water_cell=water_cell(),
                     sleeping=True, world_o=world_o, world_random=random.observation(),
                     math_random=math.observation(), **payload)
        events.append(dict(event, source_stack=stack))
        counts[kind] += 1
        if counts[kind] == profile[kind + '_error_at']: raise Interrupted(kind)

    def light(kind, bounds):
        # FD.a tests at most the last five matching requests, newest first.
        for existing in reversed(regions[-5:]):
            if existing['kind'] != kind: continue
            old = existing['bounds']
            if all(bounds[i] >= old[i] for i in range(3)) and all(bounds[i] <= old[i] for i in range(3, 6)):
                return
            if not (all(bounds[i] >= old[i] - 1 for i in range(3)) and
                    all(bounds[i] <= old[i] + 1 for i in range(3, 6))): continue
            merged = [min(bounds[i], old[i]) if i < 3 else max(bounds[i], old[i]) for i in range(6)]
            def volume(b): return (b[3]-b[0]) * (b[4]-b[1]) * (b[5]-b[2])
            if volume(merged) - volume(old) <= 2:
                existing['bounds'] = merged
                return
        regions.append(dict(kind=kind, bounds=list(bounds)))

    def setter(at, block, metadata, tail):
        x, y, z = at
        old_height = heights.get((x, z), 64)
        cells[at] = [block, metadata]
        height = max(y + 1, old_height)
        while height > 0 and opacity[cell((x, height - 1, z))[0]] == 0: height -= 1
        if height != old_height:
            # Original FD.h range callback precedes the height and sky stores.
            emit('range', ['fd.b', 'fd.h', 'lm.g', 'lm.a', *tail],
                 xyz=[x, min(height, old_height), z, x, max(height, old_height), z])
            heights[x, z] = height
            if height >= old_height:
                light('Sky', [x, old_height, z, x, height, z])
                for level in range(old_height, height): skylight[x, level, z] = 0
            else:
                for level in range(height, old_height): skylight[x, level, z] = 15
            value, level = 15, height
            while level > 0 and value > 0:
                level -= 1
                value = max(0, value - max(1, opacity[cell((x, level, z))[0]]))
                skylight[x, level, z] = value
            while level > 0 and opacity[cell((x, level-1, z))[0]] == 0: level -= 1
            if level != height: light('Sky', [x-1, level, z-1, x+1, height, z+1])
        light('Sky', [x, y, z, x, y, z])
        light('Block', [x, y, z, x, y, z])
        # LM.c compares each actual neighboring height with this column.
        for dx, dz in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            adjacent = heights.get((x+dx, z+dz), 64)
            current = heights.get((x, z), 64)
            if adjacent != current:
                light('Sky', [x+dx, min(adjacent, current), z+dz,
                              x+dx, max(adjacent, current), z+dz])

    def fizz(tail):
        first, second = random.floating(), random.floating()
        pitch = f32(f32(2.6) + f32(f32(first - second) * f32(.8)))
        emit('sound', ['fd.a', 'rp.i', *tail], name='random.fizz',
             position=[double(f32(f32(v)+f32(.5))) for v in xyz], volume=single(.5), pitch=single(pitch))
        for _ in range(8):
            px, pz = xyz[0] + math.double(), xyz[2] + math.double()
            emit('particle', ['fd.a', 'rp.i', *tail], name='largesmoke',
                 position=[double(px), double(xyz[1]+1.2), double(pz)], motion=['0'*16]*3)

    def quiet(at, metadata, tail, has_contact):
        nonlocal world_o, requests
        world_o = True
        setter(at, cell(at)[0] - 1, metadata, ['fd.a', 'nx.j', 'nx.b', *tail])
        # FlowingAdded calls common fluid Added/mixing before its schedule.
        if at == xyz and has_contact:
            fizz(['rp.j', 'rp.c', 'om.c', 'lm.a', 'fd.a', 'nx.j', 'nx.b', *tail])
        requests += 1
        emit('range', ['fd.b', 'nx.j', 'nx.b', *tail], xyz=[*at, *at])
        requests += 1
        world_o = False

    try:
        emit('cell', F.HEAD_STACK, xyz=[8, 64, 8])
        contact = water is not None and profile['water_id'] in (8, 9)
        metadata = profile['metadata']
        if contact and metadata <= 4:
            setter(xyz, 49 if metadata == 0 else 4, 0, ['fd.c', 'fd.f', *mixing])
            emit('cell', ['fd.j', 'fd.g', 'fd.f', *mixing], xyz=list(xyz))
            if profile['water_id'] == 9:
                quiet(water, 0, ['fd.l', 'fd.i', 'fd.g', 'fd.f', *mixing], False)
            fizz(mixing)
        else:
            if contact: fizz(mixing)
            quiet(xyz, metadata, outer, contact)
    except Interrupted as error:
        failure = str(error)
    return dict(events=events, cells=cells, heights=heights, skylight=skylight,
                lava=cell(xyz), water_cell=water_cell(), lighting=dict(depth=0, regions=regions),
                sky_seen=False, world_o=world_o, schedule_requests=requests,
                world_random=random.observation(), math_random=math.observation(),
                world_draws=random.draws, math_draws=math.draws, normal=failure is None, failure=failure)


def plane_arrays(profile, after, expected=None):
    """Independent complete bytes, also used to build native reader controls."""
    expected = model(profile) if expected is None else expected
    blocks, metadata, sky = bytearray(32768), bytearray(16384), bytearray(16384)
    def index(x, y, z): return (x * 16 + z) * 128 + y
    def nibble(array, offset, value):
        shift = (offset & 1) * 4
        array[offset >> 1] = (array[offset >> 1] & ~(15 << shift)) | value << shift
    cells = {(8, 64, 8): [26, 8 if after else 12], (8, 64, 7): [26, 0]}
    cells.update({tuple(row[:3]): row[3:] for row in assignments(profile)})
    if after: cells.update(expected['cells'])
    for x in range(16):
        for z in range(16):
            blocks[index(x, 63, z)] = 2
            for y in range(64, 128): nibble(sky, index(x, y, z), 15)
    for (x, y, z), (block, data) in cells.items():
        blocks[index(x, y, z)] = block
        nibble(metadata, index(x, y, z), data)
    height = bytearray([64] * 256)
    if after:
        for (x, y, z), value in expected['skylight'].items(): nibble(sky, index(x, y, z), value)
        for (x, z), value in expected['heights'].items(): height[z * 16 + x] = value
    return dict(blocks=blocks, metadata=metadata, sky=sky, block_light=bytes(16384), height=height)


def planes(profile, after, expected=None):
    return {name: hashlib.sha256(data).hexdigest() for name, data in plane_arrays(profile, after, expected).items()}


def trace(value):
    F.trace(value)
    return [f['class'] + '.' + f['method'] for f in value if f['class'] in
            {'fd', 've', 'uu', 'gs', 'nx', 'rp', 'om', 'lm'}]


def full_trace(value, source, kind, exception=False):
    F.trace(value)
    method = 'b' if kind == 'range' else 'a'
    prefix = [] if exception else ['java.lang.Thread.getStackTrace', 'LavaWakeObservation$Observer.event']
    expected = [*prefix, 'LavaWakeObservation$Observer.' + method,
                *source, 'LavaWakeObservation.run', 'LavaWakeObservation.main']
    P.require(P.exact([frame['class'] + '.' + frame['method'] for frame in value], expected),
              'complete actual Java callback/Throwable frame order')
    for frame in value:
        file = ('Thread.java' if frame['class'] == 'java.lang.Thread' else
                'LavaWakeObservation.java' if frame['class'].startswith('LavaWakeObservation') else 'SourceFile')
        P.require(frame['file'] == file, 'actual authored or unchanged SourceFile frame')


def receiver(value):
    expected = {'class': 'nx', 'material_class': 'sg', 'actual_lava_material': True}
    for key, owner, name, descriptor in (('neighbor', 'nx', 'b', '(Lfd;IIII)V'),
            ('addition', 'rp', 'c', '(Lfd;III)V'), ('tick_rate', 'rp', 'e', '()I')):
        expected[key] = {'class': owner, 'method': name, 'descriptor': descriptor}
    P.require(P.exact(value, expected), 'actual virtual Lava receiver and liquid material')


def state(value):
    P.require(type(value) is dict and set(value) == P.STATE_KEYS | EXTRA, 'closed Lava Player/World state')
    P.state({key: value[key] for key in P.STATE_KEYS})
    for key in ('lava', 'foot'): P.require(P.vector(value[key], 2, P.integer), 'typed actual ' + key)
    P.require(value['water_cell'] is None or P.vector(value['water_cell'], 5, P.integer), 'typed water cell')
    P.require(type(value['world_o']) is bool and type(value['sky_seen']) is bool, 'actual passive World/sky flags')
    P.require(P.exact(value['items'], []) and P.exact(value['foot'], [26, 0]), 'no Item publication; intact Bed foot')
    for key, stream in (('world_random', value['random'][1]), ('player_random', value['random'][0])):
        P.require(P.exact(value[key], stream), 'same complete ' + key)


def exit_position(expected):
    cells = expected['cells']
    def block(x, y, z): return cells.get((x, y, z), [2 if y == 63 else 0, 0])[0]
    for x in range(7, 10):
        for z in range(7, 10):
            if block(x, 63, z) == 2 and block(x, 64, z) == block(x, 65, z) == 0:
                return x + .5, z + .5
    raise ValueError('finite first-pass Bed exit')


def after_base(expected, baseline):
    result = dict(baseline[0 if expected['normal'] else 2]['after'])
    result['world_entities'] = 1
    if expected['normal']:
        x, z = exit_position(expected)
        result['position'] = list(result['position'])
        result['position'][0], result['position'][2] = double(x), double(z)
        width = struct.unpack('>f', bytes.fromhex(result['dimensions'][0]))[0]
        for key in ('bounds', 'original_bounds'):
            result[key] = list(result[key])
            for index, value in ((0, x-width*.5), (3, x+width*.5), (2, z-width*.5), (5, z+width*.5)):
                result[key][index] = double(value)
    return result


def validate(rows):
    P.require(type(rows) is list and len(rows) == 29, 'twenty-nine ordered actual Lava calls')
    baseline = G.read_reference()['rows']
    base_keys = P.STATE_KEYS - {'planes', 'random'}
    sequence, initialized = 0, False
    for row, profile in zip(rows, profiles()):
        expected = model(profile)
        P.require(type(row) is dict and set(row) == {'mode', 'receiver', 'prepared', 'before', 'after',
            'result', 'exception', 'exception_message', 'exception_stack', 'events'} and
            row['mode'] == profile['mode'], 'closed ordered original Lava row')
        receiver(row['receiver'])
        P.require(P.exact(row['prepared'], dict(profile=profile, shared_setup=[0, 0, 20, 0, 0],
            post_setup_cell_assignments=assignments(profile), world_random_reseed=1,
            light_or_sky_getter_overrides=[])), 'fixed prepared cells and no light getter override')
        before, after = row['before'], row['after']
        state(before); state(after)
        P.require(P.exact({k: before[k] for k in base_keys}, {k: baseline[0]['before'][k] for k in base_keys}),
                  'unchanged shared genuine sleep preparation')
        after_expected = after_base(expected, baseline)
        P.require(P.exact({k: after[k] for k in base_keys}, {k: after_expected[k] for k in base_keys}),
                  'complete selected genuine wake or exception prefix')
        P.require(P.exact(before['scheduled'], dict(initialized=initialized, sequence=f'{sequence:016x}', ordered=[], members=[])),
                  'preceding genuine scheduled-class state; no reset')
        sequence += expected['schedule_requests']
        initialized = initialized or expected['schedule_requests'] != 0
        P.require(P.exact(after['scheduled'], dict(initialized=initialized, sequence=f'{sequence:016x}', ordered=[], members=[])),
                  'actual allocated scheduled records and absent-radius sets')
        P.require(P.exact(before['lava'], [profile['id'], profile['metadata']]) and
                  P.exact(before['water_cell'], [*water_position(profile), profile['water_id'], 0] if water_position(profile) else None) and
                  before['world_o'] is False and before['sky_seen'] is False and
                  P.exact(before['lighting'], dict(depth=0, regions=[])), 'actual prepared cells and passive light state')
        P.require(P.exact({k: after[k] for k in ('lava', 'water_cell', 'world_o', 'lighting', 'sky_seen')},
                          {k: expected[k] for k in ('lava', 'water_cell', 'world_o', 'lighting', 'sky_seen')}),
                  'body-derived conversion, retained flag and ordered light-region effects')
        P.require(P.exact(before['planes'], planes(profile, False, expected)) and
                  P.exact(after['planes'], planes(profile, True, expected)), 'all five complete raw Chunk planes')
        P.require(P.exact(before['world_random'], RandomModel(1).observation()) and
                  P.exact(after['world_random'], expected['world_random']) and
                  P.exact(before['player_random'], baseline[0]['before']['player_random']) and
                  P.exact(after['player_random'], before['player_random']) and
                  P.exact(before['random'][2], RandomModel(0).observation()) and
                  P.exact(after['random'][2], expected['math_random']), 'independent World/Player/Math Random streams')
        normal, failure = expected['normal'], expected['failure']
        P.require(P.exact(row['result'], False if normal else None) and
                  row['exception'] == ('NONE' if normal else 'java/lang/IllegalStateException') and
                  row['exception_message'] == (None if normal else 'Lava ' + failure + ' observer interruption'),
                  'first genuine callback Throwable and return')
        P.require(type(row['events']) is list and len(row['events']) == len(expected['events']), 'exact ordered callback count')
        for event, prediction in zip(row['events'], expected['events']):
            P.require(type(event) is dict and set(event) == (set(prediction) - {'source_stack'}) | {'stack'} and
                      P.exact({k: v for k, v in event.items() if k != 'stack'},
                              {k: v for k, v in prediction.items() if k != 'source_stack'}) and
                      P.exact(trace(event['stack']), prediction['source_stack']),
                      'independent exact callback parameters, Random draw prefix and original stack')
            full_trace(event['stack'], prediction['source_stack'], prediction['kind'])
        if normal:
            P.require(P.exact(row['exception_stack'], []), 'no invented Throwable')
        else:
            P.require(P.exact(trace(row['exception_stack']), trace(row['events'][-1]['stack'])) and
                      row['exception_stack'][0]['class'] == 'LavaWakeObservation$Observer' and
                      row['exception_stack'][0]['method'] == ('b' if failure == 'range' else 'a'),
                      'first real observer Throwable stack')
            full_trace(row['exception_stack'], expected['events'][-1]['source_stack'], failure, exception=True)


def validate_finite(value):
    P.require(type(value) is dict and set(value) == {'schema', 'scope', 'rows'} and
              value['schema'] == SCHEMA and value['scope'] == SCOPE, 'closed original-only Lava envelope')
    validate(value['rows'])


def validate_provenance(proof, directory=HERE):
    directory = Path(directory)
    keys = {'schema', 'status', 'archive_sha256', 'finite_sha256', 'capture_sha256', 'sources', 'shared',
            'methods', 'helpers', 'binaries', 'class_count', 'rows_per_vm', 'original', 'processes',
            'inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged'}
    P.require(type(proof) is dict and set(proof) == keys and proof['schema'] == PROVENANCE and
              proof['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY' and proof['archive_sha256'] == P.JAR and
              proof['finite_sha256'] == P.digest(directory / 'finite.json') and P.word(proof['capture_sha256'], 64) and
              P.exact(proof['class_count'], 46) and P.exact(proof['rows_per_vm'], 29) and
              all(proof[k] is True for k in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged', 'class_pins_unchanged')),
              'closed original Lava provenance and complete input preservation')
    for key, names in (('sources', SOURCES), ('shared', SHARED), ('helpers', HELPERS), ('binaries', BINARIES)):
        F.fingerprints(proof[key], names, 'closed ' + key + ' inventory')
    P.require(P.exact(proof['sources'], sources()) and P.exact(proof['shared'], shared()) and
              P.exact(proof['methods'], method_pins()) and proof['binaries']['client'] == P.JAR and
              all(proof['binaries'][name] == digest for name, digest in P.LIBRARIES.items()), 'actual original Code and inputs')
    P.require(type(proof['processes']) is dict and set(proof['processes']) == set(PROCESS_TIMEOUTS), 'five bounded tool processes')
    for label, timeout in PROCESS_TIMEOUTS.items(): receipt(proof['processes'][label], label, timeout)
    P.require(type(proof['original']) is list and len(proof['original']) == 2, 'two fresh original VMs')
    for index, value in enumerate(proof['original']): receipt(value, f'original-{index}', 70, original=True)
    P.require(all(P.exact(proof['original'][0][key], proof['original'][1][key]) for key in
                  ('exit', 'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256')), 'identical unfiltered complete original streams')


def read_reference(directory=HERE):
    directory = Path(directory)
    value = P.load(directory / 'finite.json')
    validate_finite(value)
    validate_provenance(P.load(directory / 'provenance.json'), directory)
    return value


def check(directory=HERE):
    value = read_reference(directory)
    print('Original Lava packet: 29 profiles and bindings pass; native projection remains separate')
    return value


if __name__ == '__main__':
    check()
