"""Project qualified private Sign captures without executing either runtime."""
import copy
import hashlib
import json
from pathlib import Path
import re

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
HELPERS = {
    'SignDrawObservation.java': 'a87849531e4f278bf9f02f1f0acc6b55d0ba139a1ca97751e4c1a201666b00dc',
    'SignDrawJdiV2.java': 'dae3b670db534d754bafe78bbb6973711ec7a39b9980056737b6a5115dfec554',
    'JdiStringUnits.java': 'd0f6eb112e64010ced85fbc23d2546118a688633c897e962cac206b8d7b4ca40',
    'ConstructorJson.java': '9ed002149ed9782c5deb4b833be9ed97ad2319ed6291af526b02baf58f62ceb3',
}
SCOPE = {
    'boundary': 'Actual saved Sign factory followed by direct unchanged PO.a(YK,DDDF)V',
    'environment': 'Owned hidden32x32 LWJGL Pbuffer, original World/LL/RF/PS/Engine/Font',
    'font_payload': 'Actual public GL11.glCallLists(IntBuffer) entry, stopped active bytes and wrapper return',
    'geometry': 'Complete active NW.g[0:o] words at actual NW.a() entry, including quad conversion',
    'strings': 'Actual boot String backing bytes/coder and observed StringUTF16 shifts; no StringReference.value projection',
    'excluded': ['private JNI entry', 'GPU execution or pixels', 'visible window or physical monitor proof',
                 'ordinary World tick', 'editor UI', 'placement/removal gameplay', 'whole renderer parity',
                 'native active payload trace', 'native parity', 'FPS', 'sustained play'],
    'native_active_payload_trace': False,
}

def need(value, message):
    if not value:
        raise ValueError(message)

def sha(raw):
    return hashlib.sha256(raw).hexdigest()

def pin(path):
    raw = Path(path).read_bytes()
    return {'bytes': len(raw), 'sha256': sha(raw)}

def unique(pairs):
    out = dict(pairs)
    need(len(out) == len(pairs), 'duplicate JSON key')
    return out

def parse(raw):
    def invalid(value):
        raise ValueError('nonfinite JSON: ' + value)
    return json.loads(raw, object_pairs_hook=unique, parse_constant=invalid)

def read(path):
    return parse(Path(path).read_bytes())

def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, ensure_ascii=True) + '\n', encoding='utf8', newline='\n')

def private_free(value):
    if isinstance(value, dict):
        for key, item in value.items():
            private_free(key)
            private_free(item)
    elif isinstance(value, list):
        if value and all(type(v) is str and re.fullmatch('[0-9a-f]{4}', v) for v in value):
            private_free(''.join(chr(int(v, 16)) for v in value))
        for item in value:
            private_free(item)
    elif isinstance(value, str):
        need(not re.search(r'[A-Za-z]:[\\/]|\\\\[^ ]|/Users/|/home/|\\Users\\|\.local[\\/]', value),
             'private path in public projection')

def entry(e, cls, method, descriptor=None):
    return (e['event'] == 'entry' and e['site']['class'] == cls and e['site']['method'] == method
            and (descriptor is None or e['site']['descriptor'] == descriptor))

def exit_event(e, cls, method, descriptor=None):
    return (e['event'] == 'exit' and e['class'] == cls and e['method'] == method
            and (descriptor is None or e['descriptor'] == descriptor))

def string_units(value):
    need(type(value) is dict and set(value) == {'utf16_units', 'length'}, 'exact observed String projection')
    need(type(value['length']) is int and len(value['utf16_units']) == value['length'] <= 4096,
         'complete bounded String units')
    need(all(type(x) is str and re.fullmatch('[0-9a-f]{4}', x) for x in value['utf16_units']), 'raw UTF16 units')
    return ''.join(value['utf16_units'])

def raw_strings(journal):
    numbered = {e['sequence']: e for e in journal if 'sequence' in e}
    need(list(numbered) == list(range(len(numbered))), 'complete journal sequence')
    by_owner, count = {}, 0
    for event in journal:
        if event['event'] != 'string-backing':
            continue
        owner = numbered[event['for_record_sequence']]
        need(owner['profile'] == event['profile'] and owner['sequence'] < event['sequence'], 'String evidence owner')
        projections = []
        for value in event['observations']:
            need(set(value) == {'is_null', 'length', 'utf16_units', 'storage'} and value['is_null'] is False,
                 'actual nonnull backing result')
            s = value['storage']
            need(s['string_class'] == 'java.lang.String' and s['string_boot_loader'] is True
                 and s['utf16_class'] == 'java.lang.StringUTF16' and s['utf16_boot_loader'] is True
                 and s['backing_class'] == 'byte[]', 'actual boot String/array namespace')
            shapes = [('value_field', 'java.lang.String', 'value', '[B', False),
                      ('coder_field', 'java.lang.String', 'coder', 'B', False),
                      ('hi_shift_field', 'java.lang.StringUTF16', 'HI_BYTE_SHIFT', 'I', True),
                      ('lo_shift_field', 'java.lang.StringUTF16', 'LO_BYTE_SHIFT', 'I', True)]
            for field, owner_type, name, descriptor, statics in shapes:
                need(s[field] == dict(declaring_type=owner_type, name=name, descriptor=descriptor,
                                     static=statics, final=True), 'actual String field shape')
            need(s['before'] == s['after'] and s['byte_reads_equal'] is True, 'stable String brackets')
            raw = bytes.fromhex(s['bytes_hex'])
            need(len(raw) == s['backing_length'] <= 8192 and s['coder'] in (0, 1), 'bounded actual String storage')
            hi, lo = s['hi_byte_shift'], s['lo_byte_shift']
            need((hi, lo) in ((0, 8), (8, 0)), 'actual complementary shifts')
            if s['coder'] == 0:
                units = ['%04x' % b for b in raw]
            else:
                need(len(raw) % 2 == 0, 'even UTF16 backing extent')
                units = ['%04x' % ((raw[i] << hi) | (raw[i + 1] << lo)) for i in range(0, len(raw), 2)]
            need(units == value['utf16_units'] and len(units) == value['length'], 'exact backing unit decode')
            projections.append({'utf16_units': units, 'length': len(units)})
            count += 1
        if owner['event'] == 'entry':
            expected = [v for v in owner['arguments'] if type(v) is dict and set(v) == {'utf16_units', 'length'}]
        elif owner['event'] == 'exception':
            expected = [owner['message']]
        elif owner['event'] == 'font-list-buffer':
            order = owner['observation']['order']
            expected = [dict(utf16_units=['%04x' % ord(c) for c in order], length=len(order))] * 2
        else:
            expected = [owner['return']]
        need(projections == expected, 'backing bytes drive actual selected value')
        by_owner[owner['sequence']] = event['observations']
    need(len(by_owner) == 635 and count == 775, 'all qualified V2 String records')
    return by_owner

def ordinary(raw):
    need(raw.endswith(b'\r\n') and raw.count(b'\n') == raw.count(b'\r\n'), 'original Windows stdout framing')
    lines = raw.decode('ascii').splitlines()
    need(lines[:2] in (['16 achievements', '151 recipes'], ['151 recipes', '16 achievements']) and len(lines) == 42,
         'complete original bootstrap and Target records')
    def row(index, prefix):
        need(lines[index].startswith(prefix + ' '), 'ordered Target family')
        return parse(lines[index][len(prefix) + 1:])
    out = {'origins': row(2, 'SIGN_DRAW_ORIGINS'), 'device': row(3, 'SIGN_DRAW_DEVICE'),
           'font_constructor': row(4, 'SIGN_DRAW_FONT_CONSTRUCTED'),
           'rows': [row(i + 5, 'SIGN_DRAW_ROW') for i in range(36)],
           'complete': row(41, 'SIGN_DRAW_COMPLETE')}
    private_free(out)
    return out

def comparison_without_yaw(value):
    value = copy.deepcopy(value)
    for row in value['rows']:
        for phase in ('before', 'after'):
            row[phase]['manager']['primitives'].pop('h')
    return value

class Labels:
    def __init__(self):
        self.ids = {}
    def __call__(self, value):
        if isinstance(value, dict):
            if set(value) == {'class', 'identity'}:
                names = self.ids.setdefault(value['class'], {})
                key = str(value['identity'])
                if key not in names:
                    names[key] = 'R' + str(len(names))
                return {'class': value['class'], 'label': names[key]}
            return {k: self(v) for k, v in value.items()}
        if isinstance(value, list):
            return [self(v) for v in value]
        return value

def pack_traces(value):
    traces, lookup = [], {}
    def visit(node):
        if isinstance(node, dict):
            result = {}
            for key, item in node.items():
                if key in ('source_frames', 'payload_source_frames'):
                    encoded = json.dumps(item, sort_keys=True, separators=(',', ':'))
                    if encoded not in lookup:
                        lookup[encoded] = len(traces)
                        traces.append(item)
                    result[key] = {'trace': lookup[encoded]}
                else:
                    result[key] = visit(item)
            return result
        if isinstance(node, list):
            return [visit(item) for item in node]
        return node
    result = visit(value)
    result['traces'] = traces
    return result

def stopped_payload(directory, event, receipt, ordinal):
    o = event['observation']
    need(o['before'] == o['after'] and o['reader_exit'] == 0 and not o['reader_errors'], 'actual stopped Buffer bracket')
    request_path, response_path = directory / o['request'], directory / o['response']
    request, response = read(request_path), read(response_path)
    need(pin(request_path)['sha256'] == o['request_sha256'] == response['request_sha256']
         and pin(response_path)['sha256'] == o['response_sha256'], 'physical native observation pins')
    b = o['before']
    need(response['status'] == 'PASS' and response['handle_closed'] is True
         and response['double_read_equal'] is True and response['capacity_mapping_equal'] is True
         and response['rights'] == 0x410, 'actual native query/read admission and closure')
    need(request['pid'] == response['pid'] == receipt['target_pid'] and request['buffer_id'] == b['identity']
         and request['buffer_class'] == response['buffer_class'] == b['class'], 'actual Buffer and owned PID joins')
    need(request['address_hex'] == response['base_address_hex'] == b['address_hex'], 'actual address provenance')
    need(request['stop']['event_sequence'] == ordinal and request['stop']['suspend_policy'] == 'ALL', 'actual stopped call ordinal')
    for field in ('position', 'limit', 'capacity'):
        need(type(response[field]) is int and response[field] == request[field] == b[field], 'actual Buffer cursor')
    raw = bytes.fromhex(response['active_bytes_hex'])
    need(len(raw) == response['active_bytes'] == 4 * (b['limit'] - b['position']) and b['capacity'] == 1024,
         'complete actual active byte extent')
    order = response['byte_order']
    need(order == request['byte_order'] == o['order'] == 'LITTLE_ENDIAN', 'actual native Font byte order')
    words = ['%08x' % int.from_bytes(raw[i:i + 4], 'little') for i in range(0, len(raw), 4)]
    need(words == response['words_hex'] == response['native_words_hex'] == o['words_hex'] == o['native_words_hex'],
         'independent native byte/word decode')
    stem = o['request'][:-len('.request.json')]
    for stream in ('stdout', 'stderr'):
        need(pin(directory / (stem + '.' + stream))['sha256'] == o['reader_' + stream + '_sha256'], 'complete reader pipe')
    public = {'class': b['class'], 'position': b['position'], 'limit': b['limit'], 'capacity': b['capacity'],
              'order': order, 'active_bytes_hex': raw.hex(), 'words': words, 'field_bracket_stable': True,
              'native_double_read_equal': True, 'native_handle_closed': True}
    evidence = {'ordinal': ordinal, 'request': pin(request_path), 'response': pin(response_path),
                'stdout': pin(directory / (stem + '.stdout')), 'stderr': pin(directory / (stem + '.stderr'))}
    return public, evidence

def journal_projection(directory):
    receipt = read(directory / 'target.capture.json')
    need(receipt['target_exit'] == 0 and receipt['target_complete'] is True and receipt['timed_out'] is False
         and receipt['pipe_capture_complete'] is True and not receipt['reader_errors'] and receipt['failure'] is None,
         'complete actual joined child')
    for name in ('target.stdout', 'target.stderr', 'journal.ndjson'):
        need(pin(directory / name) == receipt[name], 'complete private child stream hash')
    journal = [parse(line) for line in (directory / 'journal.ndjson').read_bytes().splitlines()]
    backing = raw_strings(journal)
    codes = [{k: e[k] for k in ('class', 'method', 'descriptor', 'native', 'bytes', 'sha256')}
             for e in journal if e['event'] == 'live-code']
    need(len(codes) == 44 and receipt['events'] == 11247 and receipt['entries'] == 36
         and receipt['returns'] == 35 and receipt['font_buffers'] == 140 and receipt['draws'] == 268,
         'qualified finite actual counts')
    labels, batches, definitions = Labels(), [], []
    current, part, pending = None, None, None
    for e in journal:
        if entry(e, 'ps', 'd', '(F)V'):
            part = labels(e['receiver'])
        if entry(e, 'org.lwjgl.opengl.GL11', 'glNewList'):
            need(current is None, 'non-nested actual display-list definition')
            current = {'id': e['arguments'][0], 'mode': e['arguments'][1], 'phase': e['profile'],
                       'part': part if e['profile'] >= 0 else None, 'batches': [], 'operations': []}
        if e['event'] == 'nw-draw':
            need(current is not None, 'actual NW emission in real list definition')
            n = len(e['words'])
            need(n == e['state']['nw.o'] == 8 * e['state']['nw.h'] == 48, 'complete actual quad-converted vertex words')
            pending = {'index': len(batches), 'phase': e['profile'], 'list': current['id'],
                       'receiver': labels(e['receiver']), 'state': e['state'], 'source_frames': e['source_frames'],
                       'words': e['words'], 'draw_arrays': None}
            current['batches'].append(len(batches)); batches.append(pending)
        if entry(e, 'org.lwjgl.opengl.GL11', 'glDrawArrays'):
            need(pending is not None and pending['draw_arrays'] is None, 'actual retained NW draw leaf')
            pending['draw_arrays'] = {'arguments': e['arguments'], 'source_frames': e['source_frames']}
        if current is not None and e['event'] == 'entry' and e['site']['class'] == 'org.lwjgl.opengl.GL11' \
                and e['site']['method'] in ('glTranslatef', 'glColor3f'):
            current['operations'].append({'method': e['site']['method'], 'arguments': e['arguments'], 'source_frames': e['source_frames']})
        if entry(e, 'org.lwjgl.opengl.GL11', 'glEndList'):
            need(current is not None, 'actual display-list closure')
            definitions.append(current); current = None
        if exit_event(e, 'nw', 'a', '()V'):
            pending = None
        if exit_event(e, 'ps', 'd', '(F)V'):
            part = None
    need(current is None and len(batches) == 268 and all(b['draw_arrays'] is not None for b in batches), 'all complete actual batches')
    for b in batches:
        need(b['draw_arrays']['arguments'] == [4, 0, 6], 'actual GL triangles after NW quad conversion')
    draws, buffers, buffer_ordinal = [], [], 0
    for index in range(36):
        group = [e for e in journal if e.get('profile') == index]
        po = [e for e in group if entry(e, 'po', 'a', '(Lyk;DDDF)V')]
        rf = [e for e in group if entry(e, 'rf', 'a', '()V')]
        parts = [e for e in group if entry(e, 'ps', 'a', '(F)V')]
        need(len(po) == len(rf) == 1 and len(parts) == 2, 'actual typed PO/RF/PS calls')
        def call(e):
            return labels({k: e[k] for k in ('site', 'receiver', 'arguments', 'source_frames')})
        measures = [e for e in group if entry(e, 'sj', 'a', '(Ljava/lang/String;)I')]
        widths = [e['return'] for e in group if exit_event(e, 'sj', 'a', '(Ljava/lang/String;)I')]
        text_draws = [e for e in group if entry(e, 'sj', 'b', '(Ljava/lang/String;III)V')]
        cores = [e for e in group if entry(e, 'sj', 'a', '(Ljava/lang/String;IIIZ)V')]
        payloads = [e for e in group if e['event'] == 'font-list-buffer']
        wrapper_returns = [e for e in group if exit_event(e, 'org.lwjgl.opengl.GL11', 'glCallLists', '(Ljava/nio/IntBuffer;)V')]
        expected = 4 if index < 35 else 0
        need(len(measures) == len(widths) == len(text_draws) == len(cores) == len(payloads) == len(wrapper_returns) == expected,
             'actual four-row Font measure/draw/public-wrapper returns')
        rows = []
        for row, (measure, width, text_draw, core, payload) in enumerate(zip(measures, widths, text_draws, cores, payloads)):
            need(string_units(measure['arguments'][0]) == string_units(text_draw['arguments'][0])
                 == string_units(core['arguments'][0]), 'same actual text through Font calls')
            m, d = backing[measure['sequence']][0], backing[text_draw['sequence']][0]
            observed, evidence = stopped_payload(directory, payload, receipt, buffer_ordinal)
            evidence.update(profile=index, row=row); buffers.append(evidence); buffer_ordinal += 1
            rows.append({'row': row, 'measure': call(measure), 'width': width, 'draw': call(text_draw),
                         'core': call(core), 'same_string_alias': m['storage']['string_id'] == d['storage']['string_id'],
                         'string_storage': {k: m['storage'][k] for k in ('coder', 'bytes_hex', 'hi_byte_shift', 'lo_byte_shift')},
                         'payload': observed, 'payload_source_frames': payload['source_frames'], 'public_wrapper_returned': True})
        gl = [call(e) for e in group if e['event'] == 'entry' and e['site']['class'] == 'org.lwjgl.opengl.GL11']
        failures = [labels({k: e[k] for k in ('site', 'catch', 'exception', 'message', 'source_frames')})
                    for e in group if e['event'] == 'exception']
        draws.append({'index': index, 'entry': call(po[0]), 'model': {**call(rf[0]), 'parts': labels(rf[0]['parts'])},
                      'part_calls': [dict(**call(e), state=e['receiver_primitives']) for e in parts], 'gl': gl,
                      'rows': rows, 'returned': len([e for e in group if exit_event(e, 'po', 'a')]) == 1,
                      'failures': failures})
    font = {'definitions': [d for d in definitions if d['phase'] == -2],
            'model_definitions': [d for d in definitions if d['phase'] >= 0],
            'list_allocations': []}
    allocations = [e for e in journal if entry(e, 'org.lwjgl.opengl.GL11', 'glGenLists')]
    allocated = [e for e in journal if exit_event(e, 'org.lwjgl.opengl.GL11', 'glGenLists')]
    need(len(allocations) == len(allocated) == 3, 'real Font and two model list producers')
    for begin, end in zip(allocations, allocated):
        font['list_allocations'].append({'phase': begin['profile'], 'count': begin['arguments'][0], 'base': end['return'], 'source_frames': begin['source_frames']})
    out = pack_traces({'live_code': codes, 'draws': draws, 'batches': batches, 'font': font})
    private_free(out)
    provenance = {'target_receipt': pin(directory / 'target.capture.json'),
                  'streams': {n: pin(directory / n) for n in ('target.stdout', 'target.stderr', 'journal.ndjson')},
                  'counts': {'events': 11247, 'live_code': 44, 'entries': 36, 'returns': 35, 'nw_batches': 268,
                             'nw_words': 12864, 'font_buffers': 140, 'string_records': 635, 'string_reads': 775, 'exceptions': 3},
                  'buffer_evidence': buffers}
    return out, provenance, ordinary((directory / 'target.stdout').read_bytes())
