"""Strict observation/process admission; never produce game answers."""
import copy
import hashlib
import json
from pathlib import Path
import re

LIMIT = 8 * 1024 * 1024
PREFIX = b"LOOKUP_RECORD "
PROFILES = {
    "color": {
        "default": ["water", "cache", "grass", "foliage", "missing", "clamp", "blur"],
        "zip": ["water", "cache", "grass", "foliage", "missing", "clamp", "blur"],
        "frontiers": ["nonimage", "strip", "corrupt", "null-key"],
        "priority": ["water", "null-list-cache", "null-pack-cache", "null-pack-miss"],
        "helpers": ["new", "fill-6", "fill-8", "fill-0", "fill-1", "fill-5", "fill-null", "null-image-new", "null-image-fill"],
        "statics": ["hw-initial", "hw-set", "ia-initial", "ia-set", "jh-initial", "jh-set", "hw-mutated", "ia-mutated", "jh-mutated", "cache-mutated"],
        "first-setter": ["hw-set", "ia-set", "jh-set", "hw-mutated", "ia-mutated", "jh-mutated", "cache-mutated"],
    },
    "texture": {"default": ["ordinary", "cache", "missing"], "zip": ["ordinary", "cache", "missing"],
                "frontiers": ["strip", "clamp", "corrupt", "null-key"],
                "priority": ["ordinary", "null-list-cache", "null-pack-cache", "null-pack-miss"]},
}
FRONTIERS = {("color", "frontiers", "strip"), ("color", "frontiers", "corrupt"),
             ("texture", "frontiers", "strip"), ("texture", "frontiers", "clamp"),
             ("texture", "frontiers", "corrupt"),
             *(('color', 'helpers', label) for label in ('fill-0', 'fill-1', 'fill-5', 'fill-null'))}


def need(value, message):
    if not value:
        raise ValueError(message)


def fingerprint(path):
    data = Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def pairs(items):
    result = {}
    for key, value in items:
        need(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def parse(data):
    need(type(data) is bytes and 0 < len(data) <= LIMIT and data.endswith(b"\n"), "complete bounded stdout")
    lines = data.split(b"\n")
    rows = [(i, row) for i, row in enumerate(lines) if row.startswith(PREFIX)]
    need(len(rows) == 1 and rows[0][0] == len(lines) - 2, "one unique final authored record")
    raw = rows[0][1][len(PREFIX):]
    if raw.endswith(b"\r"):
        raw = raw[:-1]
    need(raw and all(32 <= byte <= 126 for byte in raw), "ASCII complete JSON transport")
    record = json.loads(raw.decode('ascii'), object_pairs_hook=pairs,
                        parse_constant=lambda _: (_ for _ in ()).throw(ValueError('nonfinite JSON')))
    prefix = b"\n".join(lines[:rows[0][0]]) + (b"\n" if rows[0][0] else b"")
    return record, prefix


def integer(value, low, high, name):
    need(type(value) is int and low <= value <= high, name)


def exact(value, fields, name):
    need(type(value) is dict and set(value) == set(fields), name)


def site(value):
    exact(value, ('class','method','descriptor','pc'), 'complete native source frame')
    for field in ('class','method','descriptor'):
        need(value[field] is None or (type(value[field]) is str and 0 < len(value[field]) <= 512),
             'typed nullable source-frame text')
    integer(value['pc'], -1, (1 << 31)-1, 'actual source PC')


def diagnostic(value, helper, outcome):
    fields = {'status','site'}
    if not helper:
        fields |= {'dependency','operation','native_policy','caught_source_io','frames',
                   'pack_status','cleanup_pack_status','array_status','array_site',
                   'texture_status','texture_pc','buffer_status','owner_status'}
    exact(value, fields, 'complete actual native diagnostic; no dropped child cause')
    integer(value['status'], 0, 8 if helper else 14, 'integer native status')
    expected = {'return':0, 'throw':6, 'frontier':8 if helper else 11}[outcome]
    need(value['status'] == expected, 'native outcome/status relation')
    site(value['site'])
    if helper:
        return
    site(value['dependency']); site(value['array_site'])
    need(value['operation'] is None or (type(value['operation']) is str and 0 < len(value['operation']) <= 512), 'typed native operation')
    for flag in ('native_policy','caught_source_io'):
        need(type(value[flag]) is bool, 'actual native Boolean diagnostic')
    need(type(value['frames']) is list and len(value['frames']) <= 3, 'complete bounded native call frames')
    for frame in value['frames']: site(frame)
    for field, maximum in (('pack_status',12),('cleanup_pack_status',12),('array_status',8),
                           ('texture_status',9),('buffer_status',14),('owner_status',7)):
        integer(value[field], 0, maximum, 'integer child status ' + field)
    integer(value['texture_pc'], 0, (1 << 32)-1, 'actual texture child PC')


def validate(record, mode, profile, runtime):
    need(runtime in ('original','native'), 'finite actual runtime')
    exact(record, ('schema','runtime','mode','profile','setup','device','events'), 'exact record fields')
    need(type(record['schema']) is int and record['schema'] == 1 and record['runtime'] == runtime and
         record['mode'] == mode and record['profile'] == profile, 'exact runtime/mode/profile binding')
    need(mode in PROFILES and profile in PROFILES[mode], 'finite profile')
    setup = record['setup']
    exact(setup, ('actual_app','app_z_alias','pack','engine_i_null'), 'complete setup')
    need(setup['actual_app'] is True and setup['app_z_alias'] is True and
         setup['engine_i_null'] is (mode == 'color') and setup['pack'] in ('pa','vp'), 'genuine setup relations')
    if profile in ('default','zip'):
        need(setup['pack'] == ('vp' if profile == 'zip' else 'pa'), 'actual selected Pack; no fallback success')
    if mode == 'color':
        need(record['device'] is None, 'CPU profile has no GL context record')
    else:
        exact(record['device'], ('vendor','renderer','version'), 'complete actual device record')
        need(all(type(v) is str and 0 < len(v) <= 4096 for v in record['device'].values()), 'actual bounded device strings')
    events = record['events']
    need(type(events) is list and all(type(e) is dict for e in events) and
         [e.get('label') for e in events] == PROFILES[mode][profile], 'exact ordered event inventory')
    identities = {}
    def array(value):
        if value is None: return
        exact(value, ('id','length','hex'), 'actual array record')
        integer(value['id'], 1, 128, 'actual bounded array identity')
        integer(value['length'], 0, 1048576, 'whole array length')
        need(type(value['hex']) is str and len(value['hex']) == value['length'] * 8 and
             re.fullmatch(r'[0-9a-f]*', value['hex']) is not None, 'complete actual int32 payload')
        need(identities.setdefault(value['id'], value['length']) == value['length'], 'same real array identity keeps length')
    def state(value):
        exact(value, ('b','c','d_count','j','k','ledger','e','e0','f','l_null','selected_null'), 'full selected ji/ge state')
        for flag in ('j','k','l_null','selected_null'): need(type(value[flag]) is bool, 'Boolean source field')
        for field in ('e','f'):
            cur = value[field]
            need(type(cur) is list and len(cur) == 3, 'full buffer cursor')
            for n in cur: integer(n, 0, 1048576, 'bounded direct buffer')
            need(cur[0] <= cur[1] <= cur[2], 'direct buffer cursor ordering')
        integer(value['e0'], -(1 << 31), (1 << 31)-1, 'raw actual e0 int')
        integer(value['d_count'], 0, 1024, 'actual d count')
        need(type(value['ledger']) is list and len(value['ledger']) <= 1024, 'actual ge ledger')
        for n in value['ledger']: integer(n, -(1 << 31), (1 << 31)-1, 'source ge Integer')
        for name in ('b','c'):
            need(type(value[name]) is list and len(value[name]) <= 1024, 'bounded actual Map')
            keys = set()
            for entry in value[name]:
                exact(entry, ('key','id'), 'actual Map entry')
                key = entry['key']
                need(key is None or (type(key) is str and len(key) <= 512 and len(key) % 4 == 0 and
                     re.fullmatch('[0-9a-f]*',key) is not None), 'counted UTF16 key')
                need(key not in keys, 'no duplicate actual Map key'); keys.add(key)
                integer(entry['id'], 1 if name == 'c' else -(1 << 31), 128 if name == 'c' else (1 << 32)-1, 'actual Map value')
        if mode == 'color':
            need(value['b'] == [] and value['d_count'] == 0 and value['ledger'] == [], 'color has no GPU/ge-ID effects')
    for event in events:
        label, outcome = event['label'], event.get('outcome')
        need(type(outcome) is str and outcome in ('return','throw','frontier'), 'typed source outcome')
        static = profile in ('statics','first-setter') and label != 'cache-mutated'
        helper = profile == 'helpers'
        fields = {'label','outcome','array'} if static else {'label','outcome','exception'}
        if static:
            if label.endswith('-set'): fields.add('same')
        elif helper:
            fields |= {'array','destination'}
            if label == 'new' and outcome == 'return': fields.add('image_plane_independent')
        elif mode == 'color': fields |= {'array','state'}
        else: fields |= {'id','state','bound_before','bound_after','width','height','gpu','transfer','gl_error'}
        if runtime == 'native' and not static: fields.add('native')
        exact(event, fields, 'exact emitter fields for ' + mode + '/' + profile + '/' + label + '/' + outcome)
        if static:
            need(outcome == 'return', 'static observation completed')
        else:
            exception = event['exception']
            if outcome == 'throw':
                need(type(exception) is str and len(exception) <= 512 and
                     re.fullmatch(r'[A-Za-z_$][A-Za-z0-9_$]*(?:\.[A-Za-z_$][A-Za-z0-9_$]*)*',exception) is not None,
                     'actual thrown Java exception class')
                if runtime == 'native': need(exception == 'java.lang.NullPointerException', 'native source exception relation')
            else: need(exception is None, 'nonthrow has NULL exception')
            if runtime == 'original': need(outcome != 'frontier', 'whole original outcome, not native frontier')
            else:
                diagnostic(event['native'], helper, outcome)
                if outcome == 'frontier':
                    need((mode,profile,label) in FRONTIERS, 'unexpected native frontier is a failure')
                    expected_pc = 20 if helper else 52 if mode == 'color' and label == 'strip' else 193 if mode == 'color' else 77 if label == 'strip' else 221 if label == 'corrupt' else 52
                    if label != 'clamp': need(event['native']['site']['pc'] == expected_pc, 'actual frontier source site')
        if 'array' in event:
            array(event['array'])
            if static:
                need(event['array'] is not None or (runtime == 'original' and label.endswith('-initial')), 'required actual static array')
            elif outcome != 'return': need(event['array'] is None, 'failed call has no returned array')
            elif not (helper and label == 'fill-null'): need(event['array'] is not None, 'successful call has actual array payload')
            else: need(event['array'] is None, 'actual null destination helper return')
        if helper:
            destination = event['destination']; array(destination)
            length = {'fill-6':6,'fill-8':8,'fill-0':0,'fill-1':1,'fill-5':5,'null-image-fill':1}.get(label)
            need((destination is None) == (length is None), 'actual helper destination presence')
            if length is not None: need(destination['length'] == length, 'actual declared helper destination length')
            if outcome == 'return' and destination is not None:
                need(event['array'] == destination, 'helper returns same actual destination occurrence')
            if label == 'new' and outcome == 'return':
                need(event['image_plane_independent'] is True, 'returned-array mutation leaves image plane unchanged')
        if label.endswith('-initial') and runtime == 'native':
            value = event['array']
            need(value['length'] == 65536 and value['hex'] == '00000000' * 65536, 'genuine clinit zero65536')
        if static and label.endswith('-set'): need(event['same'] is True, 'same incoming/static array')
        if 'state' in event: state(event['state'])
        if mode == 'texture':
            for field in ('bound_before','bound_after'): integer(event[field], -(1 << 31), (1 << 31)-1, 'actual integer GL binding')
            need(event['bound_before'] == event['bound_after'], 'GPU query preserves actual binding')
            if outcome == 'return':
                integer(event['id'], -(1 << 31), (1 << 32)-1, 'actual raw texture ID')
                need(type(event['transfer']) is str, 'successful lookup carries actual transfer bytes')
            else:
                need(event['id'] is None and event['gpu'] is None and event['transfer'] is None, 'failed lookup has no returned ID/GPU/transfer')
            for field in ('gpu','transfer'):
                value = event[field]
                need(value is None or (type(value) is str and len(value) <= 2097152 and len(value) % 2 == 0 and
                     re.fullmatch('[0-9a-f]*',value) is not None), 'whole bounded GPU/transfer bytes')
            if event['gpu'] is None: need(event['width'] is None and event['height'] is None, 'no queried image has NULL extents')
            else:
                # The Java emitter permits an actual zero-sized level; the native
                # probe may refuse it. Do not fabricate a successful image payload.
                for field in ('width','height'): integer(event[field], 0, 1048576, 'actual integer texture extent')
                need(len(event['gpu']) == event['width'] * event['height'] * 8, 'whole real GPU image')
            integer(event['gl_error'], 0, (1 << 32)-1, 'actual integer GL diagnostic')
    by_label = {event['label']:event for event in events}
    if mode == 'color' and 'cache' in by_label:
        need(by_label['water']['array']['id'] == by_label['cache']['array']['id'], 'same c-cache array')
    if profile in ('statics','first-setter'):
        ids = {event['array']['id'] for event in events if event['label'].endswith(('-set','-mutated')) or event['label'] == 'cache-mutated'}
        need(len(ids) == 1, 'static/cache/caller same-array identity')
        if runtime == 'native':
            initial = [event['array']['id'] for event in events if event['label'].endswith('-initial')]
            need(len(initial) == len(set(initial)) and not (set(initial) & ids), 'distinct initial class arrays and supplied array')
    return record

def completed(capture, stdout, stderr, executable):
    need(type(capture) is dict and type(capture.get('exit')) is int and capture['exit'] == 0, 'actual child completed exit0;77 is refusal')
    need(capture.get('reaped') is True and capture.get('pipe_capture_complete') is True and
         capture.get('reader_errors') == {} and capture.get('timed_out') is False and
         capture.get('output_overflow') is False, 'real process/reader/timeout/overflow completion')
    need(type(capture.get('timeout_seconds')) is int and capture['timeout_seconds'] == 180 and
         type(capture.get('limit')) is int and capture['limit'] == LIMIT, 'fixed actual capture policy')
    need(type(stdout) is bytes and type(stderr) is bytes and len(stdout) <= LIMIT and len(stderr) <= LIMIT and
         type(capture.get('stdout_bytes')) is int and type(capture.get('stderr_bytes')) is int and
         capture['stdout_bytes'] == len(stdout) and capture['stderr_bytes'] == len(stderr), 'complete actual pipe extents')
    need(type(capture.get('reader_errors')) is dict, 'actual reader-error mapping')
    need(type(capture.get('command')) is list and capture['command'] and
         all(type(argument) is str and argument and '\x00' not in argument for argument in capture['command']) and
         Path(capture['command'][0]).resolve(strict=True) == Path(executable).resolve(strict=True), 'direct actual program identity')


def compare(original, native):
    mode, profile = original['mode'], original['profile']
    validate(original, mode, profile, 'original'); validate(native, mode, profile, 'native')
    need(original['setup'] == native['setup'], 'same genuine setup projection')
    matched, frontiers, mismatches, continuations = [], [], [], []
    tainted = False
    for actual, current in zip(original['events'], native['events']):
        label = actual['label']
        if current['outcome'] == 'frontier':
            frontiers.append({'label': label, 'original_whole_outcome': actual, 'native_prefix': current})
            tainted = profile != 'helpers'
            continue
        if tainted:
            continuations.append({'label':label, 'original_whole_outcome':actual, 'native_actual_outcome':current})
            continue
        a, b = copy.deepcopy(actual), copy.deepcopy(current)
        b.pop('native', None)
        # HashMap iteration order is unspecified; preserve raw rows above and
        # compare only its actual key/value relation. No IDs are renamed.
        for row in (a, b):
            if 'state' in row:
                for name in ('b','c'):
                    row['state'][name] = sorted(row['state'][name], key=lambda e: (e['key'] is not None, e['key'] or ''))
        if a != b:
            mismatches.append({'label': label, 'original': actual, 'native': current,
                               'raw_context_ID_difference_possible': mode == 'texture'})
        else:
            matched.append(label)
    return {'status': 'MISMATCH' if mismatches else 'SUPPORTED_ROWS_MATCH_WITH_SEPARATE_FRONTIERS' if frontiers else 'MATCH',
            'matched': matched, 'frontiers': frontiers, 'mismatches': mismatches,
            'continuations_uncompared_after_frontier': continuations,
            'raw_device_difference': None if original['device'] == native['device'] else {'original':original['device'], 'native':native['device']},
            'whole_call_parity_for_frontiers': False, 'numeric_ID_mapping_performed': False}
