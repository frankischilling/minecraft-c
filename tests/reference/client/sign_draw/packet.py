"""Strict offline admission of the finite original Sign draw packet."""
import sys
sys.dont_write_bytecode = True
import copy
from pathlib import Path
import re
import struct

import project as P

HERE = Path(__file__).resolve().parent
# DATA_ROOTS_BEGIN
ROOTS = {'.gitattributes': {'bytes': 15, 'sha256': '70a4361a1eed91143e7de91d6eebf82f98990ec0dd9df197f2b64050a5f851f8'}, 'ConstructorJson.java': {'bytes': 1571, 'sha256': '9ed002149ed9782c5deb4b833be9ed97ad2319ed6291af526b02baf58f62ceb3'}, 'JdiStringUnits.java': {'bytes': 5285, 'sha256': 'd0f6eb112e64010ced85fbc23d2546118a688633c897e962cac206b8d7b4ca40'}, 'README.md': {'bytes': 4486, 'sha256': 'ff6c984d1f2353ec6bbc774369e65af5366514768935cabdf8f92f9aa7db6d0c'}, 'SignDrawJdiV2.java': {'bytes': 24551, 'sha256': 'dae3b670db534d754bafe78bbb6973711ec7a39b9980056737b6a5115dfec554'}, 'SignDrawObservation.java': {'bytes': 15583, 'sha256': 'a87849531e4f278bf9f02f1f0acc6b55d0ba139a1ca97751e4c1a201666b00dc'}, 'batches.json': {'bytes': 499939, 'sha256': 'b53091d0b9c73960346bc32679decee8544f72eac13f44ad961b74512a185312'}, 'cases.json': {'bytes': 17427, 'sha256': '91d207d32afb668342892b37ac00dc0b2b98a5f1962eb5cb011f60fd3c9f5e90'}, 'draws.json': {'bytes': 1026155, 'sha256': 'c76d45b43dbc23033ce06295960332ef8f757b1cf1d6a77fc008de9c495ad65b'}, 'font.json': {'bytes': 112104, 'sha256': 'ab9b01bf6fda56f68421c0993466e0dfb3a542dc7086cb00c6d642ea3eff060e'}, 'ordinary.stdout': {'bytes': 298890, 'sha256': '637609987c21589d31505f5d96d3563582efc00706417746889c59c6e42dd5d7'}, 'project.py': {'bytes': 20097, 'sha256': 'f6af0f53e1adc9100669d5ee8ed89d942246408e2f18fd6bb1713f6d6b7576c9'}, 'provenance.json': {'bytes': 223752, 'sha256': '696d414bdeb9a6adc052a508bab2c592755335fbf1a0572e5ac0c755598adb27'}, 'publish.py': {'bytes': 15671, 'sha256': 'b67cf0f152a8c9ae26794d7d38930f728ed5c247b36d887cc97303696e41c1ca'}, 'source-pins.json': {'bytes': 16853, 'sha256': '9cdbc8f2f58273e68d1c05aee35301114d3076d50e5c4cf6b18128def9722163'}, 'test_packet.py': {'bytes': 7408, 'sha256': '3cb42de06d8f7ca29ad0e700551df2f54ebe99a1b703554b225e974077efa929'}, 'traces.json': {'bytes': 32691, 'sha256': '2edbe60e4612c2065a9170be123900d107288abc0d80eea919905dcd71fc9c4a'}, 'verify_projection.py': {'bytes': 11421, 'sha256': 'c79a286f9cb9637ad439060cf1508e3d7c3e22f8a40044f51f771ad5844657b1'}}
# DATA_ROOTS_END
DATA = {'cases.json', 'source-pins.json', 'draws.json', 'batches.json', 'font.json', 'traces.json', 'provenance.json'}
CODE = {'project.py', 'packet.py', 'publish.py', 'test_packet.py', 'verify_projection.py', 'README.md', '.gitattributes'}
FILES = DATA | CODE | set(P.HELPERS) | {'ordinary.stdout', 'publication.json'}
SELF = P.pin(Path(__file__))

def keys(value, expected):
    P.need(type(value) is dict and set(value) == set(expected), 'closed object fields')

def integer(value, low=0, high=2147483647):
    P.need(type(value) is int and low <= value <= high, 'exact bounded integer')

def hexword(value, width=8):
    P.need(type(value) is str and re.fullmatch('[0-9a-f]{' + str(width) + '}', value), 'complete raw hexadecimal word')

def float_bits(value):
    return struct.pack('>f', value).hex()

def validate_parts(data, target):
    """Semantic validation is separately callable by corruption controls."""
    declaration, source, draws = data['cases.json'], data['source-pins.json'], data['draws.json']['draws']
    batches, font, traces, provenance = (data['batches.json']['batches'], data['font.json'],
                                        data['traces.json']['traces'], data['provenance.json'])
    P.private_free(data)
    P.need(provenance['scope'] == P.SCOPE, 'exact bounded scope and missing native/JNI/pixel credit')
    P.need(provenance['modes'] == ['ordinary-interpreter', 'ordinary-default', 'jdi-interpreter', 'jdi-default'], 'all4 actual modes')
    P.need(provenance['summary'] == {'processes': 4, 'cases_per_process': 36, 'po_entries_per_jdi': 36,
                                    'po_returns_per_jdi': 35, 'font_payloads_per_jdi': 140,
                                    'nw_batches_per_jdi': 268, 'nw_words_per_jdi': 12864,
                                    'font_glyph_batches': 256, 'model_face_batches': 12,
                                    'live_code_methods': 44, 'string_records_per_jdi': 635, 'string_reads_per_jdi': 775},
           'actual finite observed counts')
    for mode, stream in provenance['streams'].items():
        P.need(len(stream['raw_viewer_yaw_bits']) == 36, 'all actual viewer-yaw leaves retained')
        for yaw in stream['raw_viewer_yaw_bits']:
            hexword(yaw)
        transport = stream.get('transport', stream.get('controller'))
        P.need(transport['exit'] == 0 and type(transport['exit']) is int and transport['timed_out'] is False
               and transport['pipe_capture_complete'] is True and not transport['reader_errors']
               and transport['output_overflow'] is False, 'complete actual outer transport')
        P.need(len(stream['warnings']) == 4 and '<owned-helper-directory>' in stream['warnings'][1], 'complete scoped warning projection')
        P.need(stream['saves']['files'] == 108 and stream['saves']['changed_eight_byte_session_locks'] == 36, 'complete owned saved-input bracket')
        if mode.startswith('jdi-'):
            P.need(stream['target']['counts'] == {'events': 11247, 'live_code': 44, 'entries': 36, 'returns': 35,
                                                 'nw_batches': 268, 'nw_words': 12864, 'font_buffers': 140,
                                                 'string_records': 635, 'string_reads': 775, 'exceptions': 3}, 'actual joined counts')
            P.need(len(stream['target']['buffer_evidence']) == 140, 'all physical per-call payload receipts')
            for i, evidence in enumerate(stream['target']['buffer_evidence']):
                P.need(evidence['ordinal'] == i and evidence['profile'] == i // 4 and evidence['row'] == i % 4, 'actual buffer receipt order')
    P.need(source['client_jar_sha256'] == P.JAR and len(source['classes']) == 36
           and len(source['live_methods']) == 44, 'original Class and actual live-Code census')
    P.need(target['origins']['jar_sha256'] == P.JAR
           and target['origins']['classes'] == {n: c['class_sha256'] for n, c in source['classes'].items() if n != 'org.lwjgl.opengl.GL11'},
           'all35 loaded original Class origins')
    P.need(target['origins']['resources'] == {'/' + n: {'sha256': v['sha256'], 'jar': True}
                                             for n, v in source['resources'].items()}, 'all4 original resource origins')
    live = {}
    for method in source['live_methods']:
        key = method['class'], method['method'], method['descriptor']
        P.need(key not in live and method['native'] is False and method['bytes'] > 0, 'unique actual Java live method Code')
        live[key] = method
        hexword(method['sha256'], 64)
    P.need(live[('po', 'a', '(Lyk;DDDF)V')]['sha256'] == '0d3d6271aa33088a2368db3d1c5ae0c4c9bdbc7314e434e1f22d915761b32bd5'
           and live[('rf', 'a', '()V')]['sha256'] == 'c01498b5da272f1bc1194f7d12b209b05b6fb0408fc684037596d07a4a884e40',
           'typed PO and actual RF Code authority')
    P.need(len(traces) == 58 and len({repr(t) for t in traces}) == 58, 'deduplicated complete observed Source traces')
    for trace in traces:
        P.need(type(trace) is list and trace, 'nonempty observed Source trace')
        for frame in trace:
            keys(frame, ('class', 'method', 'descriptor', 'pc')); integer(frame['pc'], 0, 50000)
    def frames(reference):
        keys(reference, ('trace',)); integer(reference['trace'], 0, len(traces) - 1)
        return traces[reference['trace']]
    def direct(call, pc):
        f = frames(call['source_frames'])
        return len(f) > 1 and f[1]['class'] == 'po' and f[1]['pc'] == pc
    def call_shape(call, extra=()):
        keys(call, ('site', 'receiver', 'arguments', 'source_frames', *extra))
        keys(call['site'], ('class', 'method', 'descriptor', 'pc')); frames(call['source_frames'])
        if call['receiver'] is not None:
            keys(call['receiver'], ('class', 'label'))
            P.need(re.fullmatch(r'R[0-9]{1,3}', call['receiver']['label']), 'output-only bounded receiver label')
    cases = declaration['cases']
    P.need(len(cases) == len(draws) == len(target['rows']) == 36
           and declaration['time'] == 8 and declaration['seed'] == 123
           and declaration['tile_xyz'] == [8, 64, 8], 'complete declared finite input schedule')
    P.need([(c['block'], c['metadata']) for c in cases[:24]] ==
           [(68, 2)] + [(63, v) for v in range(16)] + [(68, v) for v in (3, 4, 5, 0, 1, 6, 15)], 'standing/wall/fallback order')
    P.need(cases[28]['loaded_units'][1].endswith('d83d') and len(cases[28]['loaded_units'][1]) == 60, 'actual split high-surrogate boundary remains raw')
    P.need([c['edit_row'] for c in cases[30:35]] == [0, 1, 2, 3, 4] and cases[-1]['null_font'] is True, 'explicit public editor prestates and terminal null Font')
    P.need(len(batches) == 268 and sum(len(b['words']) for b in batches) == 12864, 'complete active NW vertex record count')
    for index, batch in enumerate(batches):
        keys(batch, ('index', 'phase', 'list', 'receiver', 'state', 'source_frames', 'words', 'draw_arrays'))
        keys(batch['draw_arrays'], ('arguments', 'source_frames'))
        P.need(batch['index'] == index and batch['phase'] in (-2, 0, 1) and len(batch['words']) == 48
               and batch['state']['nw.o'] == 48 and batch['state']['nw.h'] == 6, 'actual complete active prefix, not stale retained suffix')
        for word in batch['words']:
            hexword(word)
        P.need(frames(batch['source_frames'])[0] == {'class': 'nw', 'method': 'a', 'descriptor': '()V', 'pc': 0}, 'real NW draw boundary')
        P.need(batch['draw_arrays']['arguments'] == [4, 0, 6], 'real GL triangles after original quad conversion')
        frames(batch['draw_arrays']['source_frames'])
    P.need(sum(b['phase'] == -2 for b in batches) == 256
           and sum(b['phase'] == 0 for b in batches) == 6 and sum(b['phase'] == 1 for b in batches) == 6,
           'real Font glyph batches and two lazy model parts')
    definitions = font['definitions'] + font['model_definitions']
    P.need(len(font['definitions']) == 288 and len(font['model_definitions']) == 2
           and [d['id'] for d in definitions] == list(range(1, 291)), 'actual complete Font/model list producers')
    assigned = []
    for definition in definitions:
        keys(definition, ('id', 'mode', 'phase', 'part', 'batches', 'operations'))
        P.need(definition['mode'] == 4864, 'actual GL_COMPILE list mode')
        for i in definition['batches']:
            integer(i, 0, 267); P.need(batches[i]['list'] == definition['id'], 'actual vertex/list producer join')
            assigned.append(i)
        for operation in definition['operations']:
            keys(operation, ('method', 'arguments', 'source_frames'))
            P.need(operation['method'] in ('glTranslatef', 'glColor3f'), 'observed list operation'); frames(operation['source_frames'])
    P.need(sorted(assigned) == list(range(268)), 'every actual batch assigned exactly once')
    P.need([len(d['batches']) for d in font['definitions']] == [1] * 256 + [0] * 32
           and [len(d['batches']) for d in font['model_definitions']] == [6, 6], 'actual glyph/palette/board/pole definitions')
    P.need([(d['count'], d['base'], d['phase']) for d in font['list_allocations']] == [(288, 1, -2), (1, 289, 0), (1, 290, 1)], 'actual list allocation outputs')
    all_list_ids = {d['id'] for d in font['definitions']}
    odd, payload_count, model_ids = [], 0, set()
    for index, (case, draw, observed) in enumerate(zip(cases, draws, target['rows'])):
        keys(draw, ('index', 'entry', 'model', 'part_calls', 'gl', 'rows', 'returned', 'failures'))
        call_shape(draw['entry']); call_shape(draw['model'], ('parts',))
        for call in draw['part_calls']:
            call_shape(call, ('state',))
        for call in draw['gl']:
            call_shape(call)
        integer(case['index'], 0, 35); P.need(case['index'] == draw['index'] == observed['index'] == index, 'ordered exact cases')
        P.need(len(case['input_units']) == len(case['loaded_units']) == 4
               and [v[:60] for v in case['input_units']] == case['loaded_units'], 'declared reader truncation in exact UTF16 units')
        for units in case['input_units'] + case['loaded_units']:
            P.need(type(units) is str and re.fullmatch('(?:[0-9a-f]{4})*', units), 'authored UTF16 unit string')
        P.need(draw['entry']['site'] == {'class': 'po', 'method': 'a', 'descriptor': '(Lyk;DDDF)V', 'pc': 0}
               and draw['entry']['arguments'][1:] == ['3fd8000000000000', '3fe4000000000000', 'bfc0000000000000', '3f000000'], 'actual typed PO operands')
        frames(draw['entry']['source_frames']); model_ids.add(repr(draw['model']['receiver']))
        P.need(draw['model']['site'] == {'class': 'rf', 'method': 'a', 'descriptor': '()V', 'pc': 0}
               and len(draw['part_calls']) == 2 and draw['model']['parts']['a']['state']['ps.h'] is True
               and draw['model']['parts']['b']['state']['ps.h'] is (case['block'] == 63), 'actual RF model and visible/hidden pole')
        rotations = [g for g in draw['gl'] if g['site']['method'] == 'glRotatef' and direct(g, 64 if case['block'] == 63 else 149)]
        angle = case['metadata'] * 360 / 16 if case['block'] == 63 else {2: 180, 4: 90, 5: -90}.get(case['metadata'], 0)
        P.need(len(rotations) == 1 and rotations[0]['arguments'] == [float_bits(-float(angle)), '00000000', '3f800000', '00000000'], 'actual rotation operands including negative zero')
        depths = [g['arguments'] for g in draw['gl'] if g['site']['method'] == 'glDepthMask']
        P.need(depths == ([[False], [True]] if index < 35 else [[False]]), 'actual normal/failure depth-mask prefix')
        P.need(draw['returned'] is (index < 35) and observed['returned'] is (index < 35), 'actual35 returns and terminal Source failure')
        P.need(len(draw['rows']) == (4 if index < 35 else 0), 'all actual Font rows')
        before, after = observed['before'], observed['after']
        P.need(before['tile'] == after['tile'] and before['world'] == after['world'], 'selected draw preserves actual loaded Tile/World')
        for state in (before, after):
            tile, world = state['tile'], state['world']
            P.need(tile['class'] == 'yk' and tile['world_same'] is True and tile['xyz'] == [8, 64, 8]
                   and tile['text_array_same'] is True and tile['yk'] == {'b': case['edit_row'], 'c': False}, 'real loaded Sign and declared public editor state')
            P.need([''.join(v['units']) for v in tile['lines']] == case['loaded_units'], 'exact actual loaded units')
            P.need(world['time'] == 8 and world['block'] == case['block'] and world['metadata'] == case['metadata']
                   and world['entities'] == world['players'] == 0 and world['iterating'] is False, 'real unticked authored World')
            P.need(state['gl']['error'] == 0 and state['nw_active_words'] == [], 'ordinary GL errors and no stale words relabeled active')
        for row_index, row in enumerate(draw['rows']):
            keys(row, ('row', 'measure', 'width', 'draw', 'core', 'same_string_alias', 'string_storage',
                       'payload', 'payload_source_frames', 'public_wrapper_returned'))
            for field in ('measure', 'draw', 'core'):
                call_shape(row[field])
            keys(row['string_storage'], ('coder', 'bytes_hex', 'hi_byte_shift', 'lo_byte_shift'))
            P.need(row['row'] == row_index and row['same_string_alias'] is True and row['public_wrapper_returned'] is True, 'actual row String alias and public wrapper return')
            expected = case['loaded_units'][row_index]
            if case['edit_row'] == row_index:
                expected = '003e0020' + expected + '0020003c'
            P.need(P.string_units(row['measure']['arguments'][0]) == P.string_units(row['draw']['arguments'][0])
                   == P.string_units(row['core']['arguments'][0]) == expected, 'actual exact Font units and editor markers')
            width = row['width']; integer(width, 0, 4096)
            P.need(row['draw']['arguments'][1:] == [-(width // 2), row_index * 10 - 20, 0]
                   and row['core']['arguments'][1:] == [-(width // 2), row_index * 10 - 20, 0, False], 'actual width-driven integer centering and Font core operands')
            P.need(any(f['class'] == 'po' and f['pc'] == (321 if case['edit_row'] == row_index else 356)
                       for f in frames(row['measure']['source_frames'])), 'actual PO measure caller')
            P.need(any(f['class'] == 'po' and f['pc'] == (342 if case['edit_row'] == row_index else 377)
                       for f in frames(row['draw']['source_frames'])), 'actual PO text draw caller')
            s = row['string_storage']; raw = bytes.fromhex(s['bytes_hex']); hi, lo = s['hi_byte_shift'], s['lo_byte_shift']
            P.need((hi, lo) in ((0, 8), (8, 0)) and s['coder'] in (0, 1), 'actual String representation')
            units = ''.join('%04x' % x for x in raw) if s['coder'] == 0 else ''.join('%04x' % ((raw[i] << hi) | (raw[i + 1] << lo)) for i in range(0, len(raw), 2))
            P.need(units == expected, 'published actual backing bytes agree with exact String units')
            payload = row['payload']; raw = bytes.fromhex(payload['active_bytes_hex'])
            keys(payload, ('class', 'position', 'limit', 'capacity', 'order', 'active_bytes_hex', 'words',
                           'field_bracket_stable', 'native_double_read_equal', 'native_handle_closed'))
            P.need(payload['class'] == 'java.nio.DirectIntBufferU' and payload['position'] == 0
                   and type(payload['limit']) is int and 0 <= payload['limit'] <= payload['capacity'] == 1024
                   and payload['order'] == 'LITTLE_ENDIAN' and len(raw) == 4 * payload['limit'], 'complete bounded actual public Font payload')
            words = ['%08x' % int.from_bytes(raw[i:i + 4], 'little') for i in range(0, len(raw), 4)]
            P.need(words == payload['words'] and all(int(w, 16) in all_list_ids for w in words), 'actual payload words consume observed Font list producers')
            P.need(payload['field_bracket_stable'] is payload['native_double_read_equal'] is payload['native_handle_closed'] is True, 'actual stopped native read admission')
            frames(row['payload_source_frames']); payload_count += 1
            if width % 2:
                odd.append([index, row_index, width, -(width // 2)])
        if index < 35:
            P.need(not draw['failures'] and observed['failure'] is None, 'successful actual Source call')
            for field in ('modelview', 'projection', 'texture_matrix', 'modelview_depth', 'projection_depth', 'texture_depth', 'depth_write'):
                P.need(before['gl'][field] == after['gl'][field], 'normal GL epilogue restores selected state')
            P.need(after['gl']['depth_write'] is True and after['gl']['current_color'] == ['3f800000'] * 4, 'normal depth/color epilogue')
        else:
            P.need(len(draw['failures']) == 1 and draw['failures'][0]['site'] == {'class': 'po', 'method': 'a', 'descriptor': '(Lyk;DDDF)V', 'pc': 356}, 'actual terminal null-font throw site')
            P.need(after['gl']['depth_write'] is False and after['gl']['modelview_depth'] == before['gl']['modelview_depth'] + 1, 'actual unrepaired terminal GL prefix')
            P.need(observed['failure']['nodes'][0]['class'] == 'java.lang.NullPointerException', 'complete Target Throwable retained')
    P.need(len(model_ids) == 1 and payload_count == 140 and odd, 'same RF receiver, complete payloads and actual odd-width centering')
    P.need(target['rows'][0]['after']['model']['board']['o'] == 289 and target['rows'][0]['after']['model']['pole']['n'] is False
           and target['rows'][1]['after']['model']['pole']['o'] == 290, 'actual first-wall/next-standing cache history')
    for row in target['rows'][1:]:
        P.need(row['after']['model']['board']['o'] == 289 and row['after']['model']['pole']['o'] == 290, 'model list outputs reused')
    P.need(target['complete']['rows'] == 36 and all(target['complete'][k] == 0 for k in
                                                  ('network', 'execs', 'denied_writes', 'raw_allocations', 'private_source_writes', 'world_tick_calls'))
           and target['complete']['terminal_gl_repair'] is False, 'complete external guard/no-repair footer')
    return {'cases': 36, 'original_processes': 4, 'po_calls_per_jdi': 36, 'font_payloads': payload_count,
            'active_nw_batches': 268, 'active_nw_words': 12864, 'live_code_methods': 44,
            'odd_width_rows': len(odd), 'scope': 'Offline original packet only; no native/JVM execution'}

def read_packet(root=HERE):
    root = Path(root)
    P.need({p.name for p in root.iterdir()} == FILES and all(p.is_file() and not p.is_symlink() for p in root.iterdir()), 'exact flat public file inventory')
    publication = P.read(root / 'publication.json')
    P.need(publication == {'schema': 1, 'files': {n: P.pin(root / n) for n in sorted(FILES - {'publication.json'})}}, 'complete publication byte manifest')
    P.need(P.pin(root / 'packet.py') == SELF, 'trusted validator bytes')
    P.need(ROOTS and all(P.pin(root / n) == value for n, value in ROOTS.items()), 'immutable observed data/helper roots')
    data = {name: P.read(root / name) for name in DATA}
    target = P.ordinary((root / 'ordinary.stdout').read_bytes())
    result = validate_parts(data, target)
    return {'declaration': data['cases.json'], 'expected': data, 'ordinary': target, 'summary': result}

if __name__ == '__main__':
    print(__import__('json').dumps(read_packet()['summary'], sort_keys=True))
