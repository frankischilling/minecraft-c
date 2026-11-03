"""Audit unchanged original calls and each VM's own inactive NW backing."""
import importlib.util
import json
from pathlib import Path
import struct

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('inventory_geometry_compare', HERE / 'compare.py')
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)
METADATA = (0, 1, 3, 15, -1, -2147483648, 2147483647, 3, 8, 2, 4, 0, 5, 6, -7, 9, 0)
BRIGHTNESS = ('3f800000', '3ec00000', '3f400000', '3f000000', '3f200000', '3fa00000',
              '00000000', 'be800000', '3f000000', '3f600000', '3e000000', '3f400000',
              '3f800000', '80000000', '3ec00000', '3f800000', '3f800000')
COLOR = (True, False, True, False, True, True, False, True, False, True, False, True,
         False, False, True, True, False)
FACE_PCS = {81: (415, 446, 487, 538, 588, 639),
            53: (839, 870, 902, 933, 965, 996),
            67: (839, 870, 902, 933, 965, 996),
            85: (1220, 1251, 1283, 1314, 1346, 1377)}
BEGIN_PCS = {81: (393, 425, 456, 507, 557, 608),
             53: (817, 849, 880, 912, 943, 975),
             67: (817, 849, 880, 912, 943, 975),
             85: (1198, 1230, 1261, 1293, 1324, 1356)}
STAIRS = ((0, 0, 0, 1, 1, .5), (0, 0, .5, 1, .5, 1))
FENCE = ((.375, 0, 0, .625, 1, .25), (.375, 0, .75, .625, 1, 1),
         (.4375, .8125, -.125, .5625, .9375, 1.125),
         (.4375, .3125, -.125, .5625, .4375, 1.125))


def require(value, message):
    if not value:
        raise AssertionError('Original inventory geometry: ' + message)


def tagged(text, name):
    prefix = 'INVENTORY_GEOMETRY_' + name + ' '
    return [json.loads(line[len(prefix):], object_pairs_hook=P.unique)
            for line in text.splitlines() if line.startswith(prefix)]


def doubles(values):
    return [struct.pack('>d', value).hex() for value in values]


def floats(values):
    return [struct.pack('>f', value).hex() for value in values]


def normal(side):
    x, y, z = ((0, -1, 0), (0, 1, 0), (0, 0, -1),
               (0, 0, 1), (-1, 0, 0), (1, 0, 0))[side]
    def byte(value):
        return ((value + 128) & 255) - 128
    result = (byte(x * 128) | byte(y * 127) << 8 | byte(z * 127) << 16) & 0xffffffff
    return result if result < 0x80000000 else result - 0x100000000


def source_pc(record, method='a', descriptor='(Luu;IF)V'):
    frames = [frame for frame in record['source-frames'] if frame['class'] == 'cv' and
              frame['method'] == method and frame['descriptor'] == descriptor]
    require(len(frames) == 1, 'one actual entered CV source frame')
    return frames[0]['pc']


def expected_events(profile):
    block = (P.IDS + (53,))[profile]
    if profile == 16:
        return ['cv-entry', 'block-bounds', 'gl-translate', 'nw-begin',
                'stair-texture', 'source-exception']
    result = ['cv-entry']
    for part in range(P.COUNTS[profile] // 6):
        if block != 81:
            result.append('block-bounds')
        result.append('gl-translate')
        for side in range(6):
            result.append('nw-begin')
            if block in (53, 67):
                result.append('stair-texture')
            if block == 81 and side >= 2:
                result.append('nw-add-offset')
            result.append('cv-face')
            if block == 81 and side >= 2:
                result.append('nw-add-offset')
            result.append('nw-draw')
        result.append('gl-translate')
    if block == 85:
        result.append('block-bounds')
    result.append('cv-return')
    return result


def audit(report, ordinary, target, events):
    owners = {'app': 'kq', 'settings': 'kv', 'packs': 'ik', 'engine': 'ji', 'font': 'sj', 'nw': 'nw'}
    for text in (ordinary, target):
        graph = tagged(text, 'OWNERS')
        require(len(graph) == 1 and all(graph[0][k] == v for k, v in owners.items()) and
                graph[0]['canonical_app'] is True and graph[0]['raw_allocations'] == 0 and
                graph[0]['world_constructed'] is False, 'genuine original constructor graph')
        device = tagged(text, 'DEVICE')
        require(len(device) == 1 and device[0]['hidden_pbuffer'] is True and
                device[0]['mode'] in ('immediate', 'compile'), 'real hidden Pbuffer and declared observer mode')
        complete = tagged(text, 'COMPLETE')
        require(len(complete) == 1 and all(complete[0][k] == v for k, v in {
            'profiles': 17, 'returned': 16, 'failed': 1, 'network': 0, 'game_substitution': False,
            'random_rebinding': False, 'raw_allocations': 0, 'nw_backing_replaced': False,
            'nw_backing_cleared': False, 'source_failure_retried': False}.items()),
            'complete calls and source/external guards')
        initial = tagged(text, 'INITIAL')
        require(len(initial) == 1 and initial[0]['static'] == {'b': True, 'c': False} and
                len(initial[0]['words']) == 48 and initial[0]['instance']['o'] == 0,
                'unmodified actual Font-used NW singleton')
        inputs, returns = tagged(text, 'INPUT'), tagged(text, 'RETURN')
        require(len(inputs) == 17 and len(returns) == 17, 'all actual input/completion receipts')
        previous = initial[0]['words']
        for profile, (entry, done) in enumerate(zip(inputs, returns)):
            block = (P.IDS + (53,))[profile]
            require(entry['profile'] == done['profile'] == profile and entry['id'] == block and
                    entry['class'] == {81: 'or', 53: 'ss', 67: 'ss', 85: 'jw'}[block] and
                    entry['metadata'] == METADATA[profile] and entry['brightness'] == BRIGHTNESS[profile] and
                    entry['cv']['b'] is COLOR[profile], 'declared exact actual input profile')
            require(entry['prior_words'] == previous, 'authored preparation never changes NW backing')
            require(entry['nw_before']['w'] is False and entry['nw_before']['o'] == 0,
                    'ordinary previously closed NW batches')
            require(done['returned'] is (profile != 16) and done['exception'] ==
                    ('java.lang.NullPointerException' if profile == 16 else None) and
                    done['gl_error'] == 0 and done['nw_after']['w'] is (profile == 16) and
                    done['nw_after']['o'] == 0, 'actual return or unrepaired first failure')
            if block in (53, 67):
                require(done['bounds'] == doubles(STAIRS[0] if profile == 16 else STAIRS[1]),
                        'actual final/reached Stair part bounds')
            if block == 85:
                require(done['bounds'] == doubles((0, 0, 0, 1, 1, 1)), 'actual Fence final bounds reset')
            if block == 81:
                require(done['bounds'] == entry['bounds'], 'Cactus inherited shape preserves input bounds')
            require(entry['gl_before']['projection'] == done['gl_after']['projection'],
                    'inventory preserves actual projection')
            expected = list(entry['gl_before']['modelview'])
            if profile == 16:
                require(done['source_block']['id'] is None, 'actual nullable Stair source retained')
            if profile == 16 and device[0]['mode'] == 'immediate':
                for at in (12, 13, 14):
                    expected[at] = struct.pack('>f', struct.unpack('>f', bytes.fromhex(expected[at]))[0] - .5).hex()
            require(done['gl_after']['modelview'] == expected, 'actual complete/partial GL translation')
            previous = done['last_words']
    for label in ('DEVICE', 'OWNERS', 'INITIAL', 'INPUT', 'RETURN'):
        require(tagged(ordinary, label) == tagged(target, label), 'ordinary/JDI exact state agreement ' + label)
    inputs, returns = tagged(target, 'INPUT'), tagged(target, 'RETURN')
    meaningful = [record for record in events if record['event'] != 'live-code']
    require([record['profile'] for record in meaningful] ==
            [profile for profile in range(17) for _ in expected_events(profile)], 'actual ordered profile groups')
    require([record['event'] for record in meaningful] ==
            [event for profile in range(17) for event in expected_events(profile)], 'complete actual source call sequence')
    entries = [r for r in meaningful if r['event'] == 'cv-entry']
    require(len({r['receiver'] for r in entries}) == 17, 'seventeen genuine fresh CV owners')
    blocks = {}
    rows, inactive_checks = [], 0
    for profile in range(17):
        block = (P.IDS + (53,))[profile]
        group = [record for record in meaningful if record['profile'] == profile]
        entry = group[0]
        require(entry['arguments'][1:] == [METADATA[profile], BRIGHTNESS[profile]] and
                entry['cv'] == inputs[profile]['cv'] and entry['bounds'] == inputs[profile]['bounds'],
                'actual entry arguments/source state')
        if block in blocks:
            require(blocks[block] == entry['arguments'][0], 'same registered Block singleton')
        blocks[block] = entry['arguments'][0]
        prior, begin, face = inputs[profile]['prior_words'], None, None
        batch, translates, setters, offsets = 0, 0, 0, 0
        for record in group[1:]:
            kind = record['event']
            if kind == 'gl-translate':
                pcs = {81: (384, 662), 53: (812, 1010), 67: (812, 1010), 85: (1193, 1391)}[block]
                require(source_pc(record) == pcs[translates % 2] and
                        record['arguments'] == floats((-.5 if translates % 2 == 0 else .5,) * 3),
                        'actual GL translation arguments and source callsite')
                translates += 1
            elif kind == 'block-bounds':
                shapes = STAIRS if block in (53, 67) else (*FENCE, (0, 0, 0, 1, 1, 1))
                pcs = (785, 803) if block in (53, 67) else (1065, 1094, 1140, 1184, 1407)
                require(record['receiver'] == blocks[block] and source_pc(record) == pcs[setters] and
                        record['arguments'] == floats(shapes[setters]),
                        'actual virtual part/reset bounds setter arguments')
                setters += 1
            elif kind == 'stair-texture':
                require(record['receiver'] == blocks[block] and record['arguments'] == [batch % 6] and
                        source_pc(record) == (836, 867, 899, 930, 962, 993)[batch % 6] and
                        record['source-null'] is (profile == 16) and
                        record['source-id'] == inputs[profile]['source_block']['id'],
                        'actual nullable live Stair source one-argument side dispatch')
            elif kind == 'nw-add-offset':
                pc = (474, 497, 525, 547, 575, 598, 626, 648)[offsets]
                values = ((0, 0, .0625), (0, 0, -.0625), (0, 0, -.0625), (0, 0, .0625),
                          (.0625, 0, 0), (-.0625, 0, 0), (-.0625, 0, 0), (.0625, 0, 0))[offsets]
                require(block == 81 and source_pc(record) == pc and record['arguments'] == floats(values),
                        'actual additive Cactus inset/undo source calls')
                offsets += 1
            elif kind == 'nw-begin':
                require(record['prior_words'] == prior and source_pc(record) == BEGIN_PCS[block][batch % 6],
                        'actual begin callsite and preserved prior backing')
                begin = record
            elif kind == 'cv-face':
                require(begin is not None and record['face'] == 'abcdef'[batch % 6] and
                        source_pc(record) == FACE_PCS[block][batch % 6] and
                        record['arguments'][0] == blocks[block] and
                        record['arguments'][1:4] == ['0000000000000000'] * 3 and
                        record['prior_words'] == prior and record['cv'] == inputs[profile]['cv'],
                        'actual face source call, original origin and own prior backing')
                if block in (53, 67):
                    require(record['bounds'] == doubles(STAIRS[batch // 6]), 'actual Stair part setter effects')
                elif block == 85:
                    require(record['bounds'] == doubles(FENCE[batch // 6]), 'actual Fence part setter effects')
                else:
                    require(record['bounds'] == inputs[profile]['bounds'], 'actual Cactus bounds at face')
                face = record
            elif kind == 'nw-draw':
                require(face is not None and record['batch'] == batch and source_pc(record) == P.PCS[block][batch % 6] and
                        record['bounds'] == face['bounds'], 'actual draw source callsite/order/bounds')
                state = record['state']
                require(all(state[k] == v for k, v in {'h': 6, 'p': 4, 'o': 48, 'r': 7, 'w': True,
                        'm': True, 'n': True, 'l': profile in (11, 12, 13), 'q': False}.items()),
                        'actual NW source flags/counts/quad conversion')
                require(state['v'] == normal(batch % 6) and all(
                    state[key] == inputs[profile]['nw_before'][key] for key in ('s', 't', 'u')),
                    'actual source normal and restored persistent NW offset')
                active = P.active_indices(state['l'])
                for vertex in (0, 1, 2, 5):
                    require(record['words'][vertex * 8 + 6] == f'{normal(batch % 6) & 0xffffffff:08x}',
                            'actual explicitly written source normal words')
                for index, word in enumerate(record['words']):
                    if index not in active:
                        require(word == prior[index], 'own-VM inactive backing preserved')
                        inactive_checks += 1
                rows.append({'profile': profile, 'id': block, 'batch': batch, 'words': record['words'],
                             'active_indices': active, 'bounds': record['bounds'],
                             'nw_state': {k: state[k] for k in ('h', 'p', 'o', 'r', 'w', 'm', 'n', 'l', 'q', 's', 't', 'u', 'v')},
                             'cv_pc': source_pc(record)})
                prior, begin, face = record['words'], None, None
                batch += 1
            elif kind == 'source-exception':
                require(profile == 16 and record['exception'] == 'java.lang.NullPointerException' and
                        record['site'] == {'class': 'ss', 'method': 'a', 'descriptor': '(I)I', 'pc': 5} and
                        source_pc(record) == 836 and begin is not None and face is None,
                        'actual first source exception after begin/normal, before face/draw')
        require(batch == (0 if profile == 16 else P.COUNTS[profile]) and
                returns[profile]['last_words'] == prior, 'all buffers and complete/failed final backing')
        require(translates == (1 if profile == 16 else P.COUNTS[profile] // 3) and
                setters == (1 if profile == 16 else 0 if block == 81 else 2 if block in (53, 67) else 5) and
                offsets == (8 if block == 81 else 0), 'all actual transforms, setters and inset operations')
    require(len({r['receiver'] for r in meaningful if r['event'] in ('nw-begin', 'nw-draw', 'nw-add-offset')}) == 1,
            'same actual original NW singleton through all work')
    pins = [record for record in events if record['event'] == 'live-code']
    expected_pins = {('cv', 'a', '(Luu;IF)V'), ('nw', 'a', '()V'), ('nw', 'b', '()V'),
                     ('nw', 'c', '(FFF)V'), ('uu', 'a', '(FFFFFF)V'), ('ss', 'a', '(I)I'),
                     *(('cv', method, '(Luu;DDDI)V') for method in 'abcdef')}
    require({(r['class'], r['method'], r['descriptor']) for r in pins} == expected_pins and
            len(pins) == len(expected_pins), 'all twelve reached original Code pins')
    for pin in pins:
        source = next(method for method in report['pins']['classes'][pin['class']]['methods']
                      if method['name'] == pin['method'] and method['descriptor'] == pin['descriptor'])
        require(pin['sha256'] == source['code_sha256'] and pin['bytes'] == source['code_bytes'],
                'live unchanged original Code identity')
    P.validate(rows)
    return {'rows': rows, 'draws': len(rows), 'raw_words': 48 * len(rows),
            'active_words': sum(len(r['active_indices']) for r in rows),
            'inactive_word_checks': inactive_checks, 'source_vertices': 4 * len(rows),
            'packed_vertices': 6 * len(rows), 'live_code': pins}
