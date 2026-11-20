"""Observe actual native PO/RF/SJ over a supplied frozen original Sign cohort.

The original ordinary rows compare Source owner/model/Font/NW and represented
GL core/facade cells. Cold legacy state, texture matrices/stacks and display-list
mode/index have explicit native observation frontiers. Without --jdi-reference,
native per-row Font receipts are observations only; the optional qualified V2
cohort joins their argument digests and measured results.
"""
import argparse
import copy
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'src/client').is_dir())


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


P = module('sign_native_observer_shared_reader', ROOT / 'tests/tools/client/test_piston_geometry.py')
require, pin, snapshot, shape = P.require, P.pin, P.snapshot, P.shape
MAX_JSON = 16 * 1024 * 1024
INPUT_PINS = {
    'prepared/profiles.tsv': {'bytes': 3701, 'sha256': '90dad775df69811d0d0e151aef1cc7a2c6a83494f82519e39c9aaebf84d3fc95'},
    'prepared/cases.json': {'bytes': 23559, 'sha256': '9258955e9e782939500e0434130526005a56144bc94040110a7bd8e6604a4f9a'},
    '0-interpreter.stdout': {'bytes': 298890, 'sha256': '637609987c21589d31505f5d96d3563582efc00706417746889c59c6e42dd5d7'},
    '1-default.stdout': {'bytes': 298890, 'sha256': '1613d975da71d366a437224bd2554db724602c1d2ddcb0efd2968ae9e2db94b2'},
}
FOOTER = (rb'16 achievements(?P<nl>\r?\n)151 recipes(?P=nl)'
          rb'PASS: 36 Source Sign direct PO observation rows; '
          rb'checks=(?P<checks>[1-9][0-9]{0,9}); cleanup=1(?P=nl)')
ROW_KEYS = {'index', 'before', 'returned', 'failure', 'receipt', 'after',
            'constructed_font_after', 'retired', 'retire_idempotent', 'after_retire_gl'}
ROW_RECEIPT_KEYS = {'captured', 'null', 'selected', 'units', 'digest', 'measure_attempted',
                    'measure_returned', 'draw_attempted', 'draw_returned', 'width', 'x', 'y'}


def read_json(raw):
    return json.loads(raw.decode('utf8'), object_pairs_hook=P.unique, parse_constant=P.nonfinite)


def bounded(path, bound=MAX_JSON):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= bound, 'bounded regular ' + path.name)
    raw = path.read_bytes()
    require(len(raw) <= bound, 'bounded complete ' + path.name)
    return raw


def normalized_pins(value):
    return {key.replace('\\', '/'): item for key, item in value.items()}


def guard_worlds(before, after):
    locks = {f'case{i:02d}/session.lock' for i in range(36)}
    require(set(before) == set(after) and {n for n in before if n.endswith('/session.lock')} == locks,
            'exact complete36 World wire inventory')
    require(all(before[n]['bytes'] == after[n]['bytes'] == 8 for n in locks), 'all SessionLocks remain eight bytes')
    require(all(after[n] == value for n, value in before.items() if n not in locks), 'all non-lock saved input bytes remain exact')


def reference(directory):
    directory = directory.resolve(strict=True)
    for name, expected in INPUT_PINS.items():
        require(pin(bounded(directory / name)) == expected, 'frozen original input/raw pin ' + name)
    run = read_json(bounded(directory / 'run.json'))
    require(run['inputs_before'] == run['inputs_after'], 'original declared input bracket')
    manifest = read_json(bounded(directory / 'prepared/source-manifest.json'))
    cases = read_json(bounded(directory / 'prepared/cases.json'))['cases']
    require(len(cases) == 36 and [c['index'] for c in cases] == list(range(36)), 'ordered36 original declarations')
    files = snapshot(directory / 'prepared/fixtures')
    require(files == normalized_pins(manifest['fixtures']) == normalized_pins(run['prepared_fixtures']), 'physical original authored input inventory')
    require(manifest['profiles_sha256'] == INPUT_PINS['prepared/profiles.tsv']['sha256'] and
            manifest['cases_sha256'] == INPUT_PINS['prepared/cases.json']['sha256'], 'prepared declaration joins')
    products = {p.relative_to(directory / 'classes').as_posix(): pin(p.read_bytes())
                for p in (directory / 'classes').glob('*.class')}
    require(products == run['products'], 'physical original observer products')
    observations = []
    for index, mode in enumerate(('interpreter', 'default')):
        stem = f'{index}-{mode}'
        receipt = read_json(bounded(directory / (stem + '.capture.json')))
        process = run['processes'][index]
        require({k: process[k] for k in receipt} == receipt and process['mode'] == mode,
                'original raw process receipt joins')
        require(type(receipt['exit']) is int and receipt['exit'] == 0 and receipt['timed_out'] is False and
                receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {} and
                receipt['output_overflow'] is False, 'complete successful original fresh process')
        command = receipt['command']
        require(('-Xint' in command) == (mode == 'interpreter') and 'SignDrawObservation' in command,
                'actual original mode/observer command')
        streams = {}
        for stream in ('stdout', 'stderr'):
            require(receipt[stream + '_file'] == stem + '.' + stream, 'closed raw stream filename')
            raw = bounded(directory / receipt[stream + '_file'])
            require(pin(raw) == {'bytes': receipt[stream + '_bytes'], 'sha256': receipt[stream + '_sha256']},
                    'physical complete raw ' + stream)
            streams[stream] = raw
        require(pin(streams['stderr']) == {'bytes':407,'sha256':'4209084a8f38e7e35e537ef3d246b3a272d1fc9288e8fea3f3c161ef67f3e391'},
                'exact four retained JDK SecurityManager deprecation warnings')
        lines = streams['stdout'].decode('utf8').splitlines()
        require(lines[:2] == ['16 achievements', '151 recipes'] and len(lines) == 42,
                'closed original startup+origins+device+font+36rows+complete inventory')
        records = []
        for line in lines[2:]:
            prefix, raw = line.split(' ', 1)
            records.append((prefix, read_json(raw.encode())))
        require([p for p, _ in records] == ['SIGN_DRAW_ORIGINS', 'SIGN_DRAW_DEVICE', 'SIGN_DRAW_FONT_CONSTRUCTED'] +
                ['SIGN_DRAW_ROW'] * 36 + ['SIGN_DRAW_COMPLETE'], 'closed original record names/order')
        origins, device, font = (records[i][1] for i in range(3))
        rows = [r for _, r in records[3:-1]]
        require(origins['jar_sha256'] == 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c' and
                len(origins['classes']) == 35 and all(v['jar'] is True for v in origins['resources'].values()),
                'observed original class/resource origins')
        for i, (case, row) in enumerate(zip(cases, rows)):
            require(row['index'] == i and row['returned'] is (i != 35), 'ordered original return/failure inventory')
            require(row['profile'] == (directory / 'prepared/profiles.tsv').read_text().splitlines()[i].split('\t'),
                    'original profile-to-declaration join')
            require((row['failure'] is None) == (i != 35), 'one actual terminal original failure')
        complete = records[-1][1]
        require(complete['rows'] == 36 and complete['network'] == complete['execs'] == complete['denied_writes'] == 0 and
                complete['raw_allocations'] == complete['private_source_writes'] == complete['world_tick_calls'] == 0 and
                complete['terminal_gl_repair'] is False, 'actual original completion/scope')
        guard_worlds(normalized_pins(process['saves_before']), normalized_pins(process['saves_after']))
        observations.append({'font': font, 'rows': rows, 'device': device})
    return {'directory': directory, 'cases': cases, 'observations': observations,
            'pins': {name: pin(bounded(directory / name)) for name in INPUT_PINS}}


JDI_JOURNAL_PINS = (
    {'bytes':4940452,'sha256':'0223875d8aa296511f56edc6ae0ee325802f1850c647625c92faae942da171a3'},
    {'bytes':4940452,'sha256':'0f214770ae90bf1631378469d5eb7485a00abe3f77493b699fd152f966f2ac67'},
)
JDI_TARGET_PINS = (
    {'bytes':298890,'sha256':'102719e441a0a5fb98e5411120a41cf3f3e23a2ebc910aa1da24f5402903bdf9'},
    {'bytes':298890,'sha256':'1e1941e07fdf9df949eecba987bc2e9cb73d0f270dd8ee505a79b9978d07b433'},
)
MEASURE_DESCRIPTOR = '(Ljava/lang/String;)I'
DRAW_DESCRIPTOR = '(Ljava/lang/String;III)V'


def qualified_string_units(argument, observation):
    require(type(argument) is dict and set(argument) == {'utf16_units','length'} and type(argument['length']) is int and
            type(observation) is dict and observation['is_null'] is False, 'actual nonnull Font String argument')
    units = observation['utf16_units']
    length = observation['length']
    require(type(length) is int and length >= 0 and type(units) is list and len(units) == length and
            all(type(u) is str and re.fullmatch('[0-9a-f]{4}',u) for u in units) and
            argument == {'utf16_units':units,'length':length}, 'exact entry-to-backing UTF16 join')
    storage = observation['storage']
    require(storage['string_class'] == 'java.lang.String' and storage['string_boot_loader'] is True and
            storage['backing_class'] == 'byte[]' and storage['utf16_class'] == 'java.lang.StringUTF16' and
            storage['utf16_boot_loader'] is True and storage['byte_reads_equal'] is True and
            storage['before'] == storage['after'], 'qualified immutable actual String backing observation')
    require(storage['value_field'] == {'declaring_type':'java.lang.String','name':'value','descriptor':'[B','static':False,'final':True} and
            storage['coder_field'] == {'declaring_type':'java.lang.String','name':'coder','descriptor':'B','static':False,'final':True},
            'actual String value/coder fields')
    for key, name in (('hi_shift_field','HI_BYTE_SHIFT'),('lo_shift_field','LO_BYTE_SHIFT')):
        require(storage[key] == {'declaring_type':'java.lang.StringUTF16','name':name,'descriptor':'I','static':True,'final':True},
                'actual immutable bootstrap UTF16 byte-order fields')
    coder, high, low = storage['coder'],storage['hi_byte_shift'],storage['lo_byte_shift']
    require(type(coder) is int and coder in (0,1) and type(high) is int and type(low) is int and
            {high,low} == {0,8} and type(storage['bytes_hex']) is str and
            re.fullmatch('(?:[0-9a-f]{2})*',storage['bytes_hex']), 'qualified String coder and byte shifts')
    raw = bytes.fromhex(storage['bytes_hex'])
    require(type(storage['backing_length']) is int and len(raw) == storage['backing_length'] == length * (coder + 1) and
            storage['before'] == {'value_id':storage['backing_id'],'coder':coder,'backing_length':len(raw),
                                  'hi_byte_shift':high,'lo_byte_shift':low}, 'same physical backing/coder/extent before and after')
    decoded = [f'{b:04x}' for b in raw] if coder == 0 else [f'{(raw[i] << high) | (raw[i+1] << low):04x}' for i in range(0,len(raw),2)]
    require(decoded == units, 'exact Java UTF16 units independently decoded from qualified backing bytes')
    return ''.join(units)


def jdi_font_calls(events, cases):
    backing = {}
    for event in events:
        if event['event'] == 'string-backing':
            key = event['for_record_sequence']
            require(key not in backing, 'one immutable String backing record per method entry')
            backing[key] = event
    stacks, pairs, receivers = {}, [], set()
    descriptors = {MEASURE_DESCRIPTOR,DRAW_DESCRIPTOR}
    for event in events:
        profile = event.get('profile',-1)
        if event['event'] == 'entry':
            site = event['site']
            selected = site['class'] == 'sj' and site['descriptor'] in descriptors and profile >= 0
            if not selected: continue
            require(type(profile) is int and 0 <= profile < 35 and site['pc'] == 0 and
                    site['method'] == ('a' if site['descriptor'] == MEASURE_DESCRIPTOR else 'b'),
                    'actual qualified nonterminal Font method entry')
            receiver = event['receiver']
            require(receiver['class'] == 'sj' and type(receiver['identity']) is int and receiver['identity'] > 0,
                    'genuine actual Font receiver')
            receivers.add(receiver['identity'])
            proof = backing.get(event['sequence'])
            require(proof is not None and proof['profile'] == profile and len(proof['observations']) == 1,
                    'Font method-to-qualified String backing receipt')
            units = qualified_string_units(event['arguments'][0],proof['observations'][0])
            thread = event['thread']
            stacks.setdefault(thread,[]).append((event,units))
        elif event['event'] == 'exit' and event['class'] == 'sj' and event['descriptor'] in descriptors and profile >= 0:
            thread = event['thread']
            require(stacks.get(thread), 'actual Font exit has its reached entry')
            entry, units = stacks[thread].pop()
            require(entry['profile'] == profile and entry['site']['descriptor'] == event['descriptor'] and
                    entry['site']['method'] == event['method'] and entry['sequence'] < event['sequence'],
                    'actual same-thread Font entry/exit nesting')
            pairs.append((entry,event,units))
    require(all(not stack for stack in stacks.values()) and len(receivers) == 1 and len(pairs) == 280,
            'exact140 measure and140 draw completions on the same actual Font')
    pairs.sort(key=lambda value:value[0]['sequence'])
    result = []
    for index in range(140):
        measure, draw = pairs[index*2:index*2+2]
        case, row = index // 4, index % 4
        first, returned, units = measure
        drawing, drawn, draw_units = draw
        selected = cases[case]['edit_row'] == row
        require(first['profile'] == drawing['profile'] == case and
                first['site']['descriptor'] == MEASURE_DESCRIPTOR and drawing['site']['descriptor'] == DRAW_DESCRIPTOR and
                first['sequence'] < returned['sequence'] < drawing['sequence'] < drawn['sequence'],
                'closed case/row/measure/draw Source call order')
        for entry, pc in ((first,321 if selected else 356),(drawing,342 if selected else 377)):
            frames = entry['source_frames']
            require(len(frames) >= 2 and frames[1] == {'class':'po','method':'a','descriptor':'(Lyk;DDDF)V','pc':pc},
                    'actual selected/ordinary PO Font invocation caller')
        require(type(returned['return']) is int and -(1 << 31) <= returned['return'] < 1 << 31 and
                drawn['return'] == {'void':True} and units == draw_units, 'actual Font result and same immutable argument content')
        args = drawing['arguments']
        require(len(first['arguments']) == 1 and len(args) == 4 and
                all(type(v) is int and -(1 << 31) <= v < 1 << 31 for v in args[1:]) and args[3] == 0,
                'actual unshadowed black Font draw arguments')
        declared = cases[case]['loaded_units'][row]
        if selected: declared = '003e0020' + declared + '0020003c'
        require(units == declared, 'qualified original argument agrees with loaded text/public selected row')
        result.append({'case':case,'row':row,'units':units,'length':len(units)//4,'digest':digest(row,units),
                       'width':returned['return'],'x':args[1],'y':args[2],'selected':selected})
    return result


def jdi_reference(directory, packet):
    directory = directory.resolve(strict=True)
    run = read_json(bounded(directory/'run.json'))
    require(run['inputs_before'] == run['inputs_after'] and
            run['profiles'] == INPUT_PINS['prepared/profiles.tsv'] and
            run['string_qualification_sha256'] == 'a3188e301f7644ed8bf10cc5a98c530ddb454bd5163d92b0e9c21cc86fe7b457' and
            run['bridge_qualification_sha256'] == 'd42d8eed91c7fd9896233dbc99774da2e8eb7db1184f3ed7cb4a7514979dc049',
            'qualified V2 input/String/native-reader joins')
    require(read_json(bounded(directory/'prepared/cases.json'))['cases'] == packet['cases'] and
            pin(bounded(directory/'prepared/profiles.tsv')) == INPUT_PINS['prepared/profiles.tsv'] and
            snapshot(directory/'prepared/fixtures') == normalized_pins(run['prepared_fixtures']),
            'same declared saved inputs, loaded UTF16 and editor profiles')
    actual_products = {p.relative_to(directory/'classes').as_posix():pin(p.read_bytes()) for p in (directory/'classes').glob('*.class')}
    require(actual_products == run['products'], 'physical qualified JDI observer/helper products')
    observations = []
    for index, mode in enumerate(('interpreter','default')):
        stem=f'{index}-{mode}';observer=directory/(stem+'-observer')
        process = run['processes'][index]
        controller=read_json(bounded(directory/(stem+'.capture.json')))
        require(process['mode'] == mode and process['controller'] == controller and type(controller['exit']) is int and controller['exit'] == 0 and
                controller['timed_out'] is False and controller['pipe_capture_complete'] is True and
                controller['reader_errors'] == {} and controller['output_overflow'] is False,
                'complete successful physical JDI controller process')
        for stream in ('stdout','stderr'):
            raw=bounded(directory/(stem+'.'+stream))
            require(controller[stream+'_file'] == stem+'.'+stream and
                    pin(raw) == {'bytes':controller[stream+'_bytes'],'sha256':controller[stream+'_sha256']}, 'controller physical raw stream join')
        target=read_json(bounded(observer/'target.capture.json'))
        require(process['target'] == target and type(target['target_exit']) is int and target['target_exit'] == 0 and target['target_complete'] is True and
                target['timed_out'] is False and target['pipe_capture_complete'] is True and target['reader_errors'] == [] and
                target['entries'] == 36 and target['returns'] == 35 and target['font_buffers'] == 140 and target['failure'] is None,
                'complete actual36-case JDI target terminal state')
        require(all(type(target[key]) is int for key in ('entries','returns','font_buffers')) and
                type(target['options']) is str and
                (re.search(r'(?<!\S)-Xint(?!\S)',target['options']) is not None) == (mode == 'interpreter'), 'actual original VM mode/count types')
        raw=bounded(observer/'journal.ndjson')
        require(pin(raw) == JDI_JOURNAL_PINS[index] == target['journal.ndjson'], 'physical frozen V2 journal')
        for stream in ('stdout','stderr'):
            data=bounded(observer/('target.'+stream))
            require(pin(data) == target['target.'+stream], 'target physical raw stream join')
            if stream == 'stdout': require(pin(data) == JDI_TARGET_PINS[index], 'frozen actual JDI target stdout')
        guard_worlds(normalized_pins(process['saves_before']),normalized_pins(process['saves_after']))
        require(raw.endswith(b'\n'), 'complete actual journal last record')
        events=[read_json(line) for line in raw.splitlines()]
        calls=jdi_font_calls(events,packet['cases'])
        observations.append({'mode':mode,'calls':calls,'journal':pin(raw)})
    shape(observations[0]['calls'],observations[1]['calls'],'exact qualified Font arguments/results in both VMs',values=True)
    return {'directory':directory,'observations':observations}


def compare_jdi_font_calls(actual, jdi):
    require(len(jdi['observations']) == 2, 'both qualified original Font-call processes required')
    for observation in jdi['observations']:
        calls=observation['calls']
        require(len(calls) == 140, 'all140 original measure/draw pairs required')
        for index, call in enumerate(calls):
            case,row=divmod(index,4)
            require(call['case'] == case and call['row'] == row, 'strict complete original Font call ordering')
            native=actual['rows'][case]['receipt']['rows'][row]
            require(native['captured'] is True and native['measure_attempted'] is True and native['measure_returned'] is True and
                    native['draw_attempted'] is True and native['draw_returned'] is True and native['null'] is False and
                    native['selected'] is call['selected'] and native['units'] == call['length'] and
                    native['digest'] == call['digest'] == digest(row,call['units']) and
                    native['width'] == call['width'] and native['x'] == call['x'] and native['y'] == call['y'],
                    f"native Font argument length/digest/width/x/y differs from qualified {observation['mode']} case{case} row{row}")
    terminal=actual['rows'][35]['receipt']['rows']
    require(terminal[0]['captured'] is True and terminal[0]['measure_attempted'] is True and terminal[0]['measure_returned'] is False and
            all(row['draw_attempted'] is False and row['draw_returned'] is False for row in terminal),
            'terminal null-Font has no fabricated entered/completed SJ call')
    return {'vms':2,'measure_calls_per_vm':140,'draw_calls_per_vm':140,'rows_per_vm':35,
            'comparison':'Qualified original raw UTF16 backing units joined to native argument length/FNV digest and exact measured width/x/y.',
            'scope':'No native active glCallLists payload was observed or compared; original140 buffers per VM remain separate qualified evidence.',
            'journals':[v['journal'] for v in jdi['observations']]}


def ids(observation):
    font = observation['font']['font']
    model = observation['rows'][1]['after']['model']
    sign = observation['rows'][0]['after']['textures']['/item/sign.png']
    result = {'font': font['texture'], 'font_base': font['list_base'], 'sign': sign,
              'board': model['board']['o'], 'pole': model['pole']['o']}
    require(all(type(v) is int and v > 0 for v in result.values()) and result['font'] != result['sign'] and
            len({result['font_base'], result['board'], result['pole']}) == 3,
            'actual distinct texture/model/list allocation relationships')
    return result


GL_REPRESENTATION = 'core-driver-and-source-facade-v2'
GL_UNREPRESENTED = ['texture_matrix', 'texture_depth', 'list_index', 'list_mode']
GL_CORE = {'depth_func','blend_src','blend_dst','cull_mode','front_face','bound_texture',
           'depth_test','blend','cull','depth_write','error'}
GL_SOURCE = {'matrix_mode','modelview_depth','projection_depth','lighting','light0','light1',
             'color_material','shade','normalize','rescale_normal','texture2d',
             'modelview','projection','current_color','current_normal'}


def original_gl_projection(value, initialized=True):
    # This projects the original comparison cells. It never supplies input or
    # missing defaults to the native facade: a cold native facade stays cold.
    source = {'initialized': initialized}
    if initialized:
        source.update({key:copy.deepcopy(value[key]) for key in GL_SOURCE})
        source.update(color_known=True,normal_known=True)
    return {'representation':GL_REPRESENTATION,'unrepresented':GL_UNREPRESENTED.copy(),
            'core':{key:copy.deepcopy(value[key]) for key in GL_CORE},'source':source}


def gl_normalized(value, allocation, native, initialized=True):
    if native:
        require(type(value) is dict and set(value) == {'representation','unrepresented','core','source'} and
                value['representation'] == GL_REPRESENTATION and value['unrepresented'] == GL_UNREPRESENTED,
                'exact native driver/facade representation and explicit frontiers')
        value = copy.deepcopy(value)
        require(type(value['core']) is dict and set(value['core']) == GL_CORE and
                type(value['source']) is dict and value['source'].get('initialized') is initialized,
                'complete core state and actual facade initialization stage')
        expected_keys = {'initialized'} | (GL_SOURCE | {'color_known','normal_known'} if initialized else set())
        require(set(value['source']) == expected_keys, 'exact passive facade cells; no cold defaults')
        if initialized:
            require(value['source']['color_known'] is True and value['source']['normal_known'] is True,
                    'actual source attribute values must be known')
    else:
        value = original_gl_projection(value,initialized)
    texture = value['core']['bound_texture']
    require(type(texture) is int and texture in (0, allocation['font'], allocation['sign']), 'known actual bound texture occurrence')
    value['core']['bound_texture'] = 0 if texture == 0 else 'font' if texture == allocation['font'] else 'sign'
    return value


def font_normalized(value, allocation, native):
    if value is None:
        return None
    value = copy.deepcopy(value)
    require(value['texture'] == allocation['font'] and value['list_base'] == allocation['font_base'], 'same actual constructed Font resources')
    value['texture'], value['list_base'] = 'font', 'font_base'
    buffer = value['buffer']
    require(buffer['class'] == ('native.direct-int-buffer' if native else 'java.nio.DirectIntBufferU'), 'explicit native/JVM buffer representation')
    buffer['class'] = 'writable-direct-native-order-int32'
    normalized = []
    for word in buffer['active_words']:
        require(type(word) is str and re.fullmatch('[0-9a-f]{8}', word), 'raw actual Font active word')
        number = int(word, 16)
        # The constructor's unfilled full buffer is Source zero-filled storage.
        if number == 0:
            normalized.append('00000000')
        else:
            offset = number - allocation['font_base']
            require(0 <= offset < 288, 'actual Font command remains within its own range')
            normalized.append('font+' + str(offset))
    buffer['active_words'] = normalized
    return value


def state_normalized(value, allocation, native):
    value = copy.deepcopy(value)
    for name in ('board', 'pole'):
        part = value['model'][name]
        require(type(part['o']) is int and part['o'] == (allocation[name] if part['n'] else 0), 'same lazy actual PS list occurrence')
        if part['n']:
            part['o'] = name
    value['font'] = font_normalized(value['font'], allocation, native)
    for name, number in value['textures'].items():
        require(name == '/item/sign.png' and type(number) is int and number == allocation['sign'], 'actual cached Sign texture occurrence')
        value['textures'][name] = 'sign'
    yaw = value['manager']['primitives']['h']
    require(type(yaw) is str and re.fullmatch('[0-9a-f]{8}', yaw) and
            math.isfinite(struct.unpack('>f', bytes.fromhex(yaw))[0]), 'fresh viewer finite yaw raw bits')
    value['manager']['primitives']['h'] = 'fresh-viewer-yaw-not-cross-process-input'
    value['gl'] = gl_normalized(value['gl'], allocation, native)
    return value


def digest(row, units):
    # Reverse bytes within each Java unit, preserving surrogate order exactly.
    data = b''.join(bytes.fromhex(units[i:i+4])[::-1] for i in range(0, len(units), 4))
    value = 14695981039346656037
    for byte in struct.pack('<IBQ', row, 0, len(data) // 2) + data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f'{value:016x}'


def admit_receipt(receipt, case):
    expected_keys = {'source_pc', 'model_parts', 'compiled_parts', 'faces', 'vertices', 'rows_measured', 'rows_drawn',
                     'text_units', 'text_digest', 'angle', 'text_scale', 'model_returned', 'model_popped', 'font_returned',
                     'depth_disabled', 'depth_restored', 'color_restored', 'matrix_popped', 'rows'}
    require(type(receipt) is dict and set(receipt) == expected_keys, 'complete native Source call receipt')
    terminal = case['null_font']
    angle = case['metadata'] * 360 / 16 if case['block'] == 63 else {2:180,4:90,5:-90}.get(case['metadata'],0)
    require(receipt['angle'] == struct.pack('>f',angle).hex(), 'actual PO angle from declared source metadata')
    for key in ('source_pc','model_parts','compiled_parts','faces','vertices','rows_measured','rows_drawn','text_units'):
        require(type(receipt[key]) is int and 0 <= receipt[key] <= (1 << 31), 'native receipt integer ' + key)
    require(receipt['source_pc'] == (356 if terminal else 25) and receipt['model_parts'] == 2 and
            receipt['compiled_parts'] == int(case['index'] < 2) and receipt['faces'] == receipt['compiled_parts'] * 6 and
            receipt['vertices'] == receipt['faces'] * 4 and receipt['rows_measured'] == receipt['rows_drawn'] == (0 if terminal else 4),
            'actual return/compilation and Font call counts')
    for field in ('model_returned', 'model_popped', 'font_returned', 'depth_disabled'):
        require(receipt[field] is True, 'completed actual PO prefix ' + field)
    for field in ('depth_restored', 'color_restored', 'matrix_popped'):
        require(receipt[field] is (not terminal), 'actual PO suffix ' + field)
    require(type(receipt['rows']) is list and len(receipt['rows']) == 4 and receipt['text_scale'] == '3c360b62', 'fixed admitted row receipts/text scale')
    expected_units, total = 0, 14695981039346656037
    for index, row in enumerate(receipt['rows']):
        require(type(row) is dict and set(row) == ROW_RECEIPT_KEYS, 'exact bounded native row receipt')
        units = case['loaded_units'][index]
        selected = index == case['edit_row']
        if selected:
            units = '003e0020' + units + '0020003c'
        captured = not terminal or index == 0
        if not terminal:
            expected_units += len(units) // 4
            data = b''.join(bytes.fromhex(units[j:j+4])[::-1] for j in range(0,len(units),4))
            for byte in struct.pack('<IBQ',index,0,len(data)//2) + data:
                total = ((total ^ byte) * 1099511628211) & ((1 << 64) - 1)
        for key in ('captured', 'measure_attempted'):
            require(row[key] is captured, 'actual reached row/measure invocation')
        for key in ('measure_returned', 'draw_attempted', 'draw_returned'):
            require(row[key] is (not terminal), 'actual Font invocation returns')
        require(row['null'] is False and row['selected'] is (selected and captured), 'actual submitted null/editor argument')
        require(type(row['units']) is int and row['units'] == (len(units) // 4 if captured else 0) and
                row['digest'] == (digest(index, units) if captured else '0000000000000000'), 'exact native Font UTF16 argument digest')
        require(all(type(row[key]) is int and -(1 << 31) <= row[key] < 1 << 31 for key in ('width', 'x', 'y')), 'actual Font int32 results')
        if terminal:
            require(row['width'] == row['x'] == row['y'] == 0, 'no fabricated late Font result/arguments')
        if not terminal:
            neg = (-row['width']) & 0xffffffff
            if neg >= 1 << 31: neg -= 1 << 32
            require(row['x'] == (abs(neg) // 2) * (-1 if neg < 0 else 1) and row['y'] == index * 10 - 20,
                    'actual PO Java centering and row positioning')
    require(receipt['text_units'] == expected_units and receipt['text_digest'] == f'{total:016x}', 'complete successful native text argument aggregate')
    # Per-row widths are actual native observations here. An ordinary original
    # snapshot does not expose each sj.a(String) return: JDI qualification is separate.


def admit_native(raw, stdout, stderr, code, packet):
    require(type(code) is int and code == 0 and type(stderr) is bytes and not stderr and type(stdout) is bytes,
            'complete clean native terminal transport')
    match = re.fullmatch(FOOTER, stdout)
    require(match is not None and int(match['checks']) <= 2147483647, 'exact native startup/cleanup footer')
    require(type(raw) is bytes and 0 < len(raw) <= MAX_JSON and raw.startswith(b'{') and
            (raw.endswith(b'}\n') or raw.endswith(b'}\r\n')) and not raw.endswith(b'\n\n'), 'bounded single complete native JSON')
    actual = read_json(raw)
    require(type(actual) is dict and set(actual) == {'schema', 'device', 'font_constructed', 'rows', 'cleanup', 'rows_complete'} and
            actual['schema'] == 'NATIVE_SIGN_DIRECT_DRAW_V1' and actual['cleanup'] is True and
            type(actual['rows_complete']) is int and actual['rows_complete'] == 36 and
            type(actual['rows']) is list and len(actual['rows']) == 36, 'complete native Sign cohort')
    device = actual['device']
    require(type(device) is dict and set(device) == {'vendor', 'renderer', 'version', 'hidden', 'monitor', 'display_id'} and
            all(type(device[k]) is str and device[k] for k in ('vendor', 'renderer', 'version')) and
            device['hidden'] is True and type(device['monitor']) is int and device['monitor'] == 2 and
            type(device['display_id']) is int and device['display_id'] > 0, 'actual hidden monitor2 native context')
    require(type(actual['font_constructed']) is dict and set(actual['font_constructed']) == {'before_gl','after_gl','font','model'}, 'exact constructed Font observation')
    allocation = ids({'font': actual['font_constructed'], 'rows': actual['rows']})
    for original in packet['observations']:
        original_ids = ids(original)
        for name in ('before_gl', 'after_gl'):
            shape(gl_normalized(original['font'][name], original_ids, False, name != 'before_gl'),
                  gl_normalized(actual['font_constructed'][name], allocation, True, name != 'before_gl'),
                  'font_constructor.' + name, values=True)
        shape(font_normalized(original['font']['font'], original_ids, False),
              font_normalized(actual['font_constructed']['font'], allocation, True), 'actual Font constructor cells', values=True)
        shape(original['font']['model'], actual['font_constructed']['model'], 'model before first draw', values=True)
        for i, (case, expected, row) in enumerate(zip(packet['cases'], original['rows'], actual['rows'])):
            require(type(row) is dict and set(row) == ROW_KEYS and type(row['index']) is int and row['index'] == i,
                    'closed ordered native row inventory')
            require(row['returned'] is (i != 35) and row['retired'] is True and row['retire_idempotent'] is True,
                    'actual Source return and checked retirement')
            if i == 35:
                failure = row['failure']
                require(type(failure) is dict and set(failure) == {'status', 'class', 'source_class', 'pc', 'native_policy', 'source_exception'} and
                        type(failure['status']) is int and failure['class'] == 'java/lang/NullPointerException' and
                        failure['source_class'] == 'po' and type(failure['pc']) is int and failure['pc'] == 356 and
                        failure['native_policy'] is False and failure['source_exception'] is True,
                        'actual terminal null-Font invocation failure')
            else:
                require(row['failure'] is None, 'no fabricated positive Source failure')
            admit_receipt(row['receipt'], case)
            require(row['before']['manager']['primitives']['h'] == row['after']['manager']['primitives']['h'],
                    'same actual viewer-derived yaw retained through PO')
            for phase in ('before', 'after'):
                shape(state_normalized(expected[phase], original_ids, False), state_normalized(row[phase], allocation, True),
                      f'row{i}.{phase}', values=True)
            shape(font_normalized(expected['constructed_font_after'], original_ids, False),
                  font_normalized(row['constructed_font_after'], allocation, True), f'row{i}.constructed_font_after', values=True)
            shape(row['after']['gl'], row['after_retire_gl'], f'row{i}.native_retirement_preserves_source_GL', values=True)
    return {'checks': int(match['checks']), 'cases': 36, 'device': device,
            'comparison': 'Exact ordinary owner/model/Font/NW cells and represented GL core/facade cells; allocation IDs compared as actual producer/consumer relationships. Fresh viewer yaw retained but excluded across processes.',
            'gl_representation': GL_REPRESENTATION, 'gl_unrepresented_frontiers': GL_UNREPRESENTED.copy(),
            'gl_cold_scope': 'Before Font construction the native facade is uninitialized; no legacy initial matrix/attribute values are claimed. Core raster/texture state is compared.',
            'font_call_scope': 'Native four-row exact arguments/digests, calls and measured widths; original per-call JDI equality remains separate.'}


def compiled_inputs():
    result = P.compiled_inputs()
    for name in ('tests/client/render/tile_entity_renderer/test_sign_geometry.c',
                 'tests/tools/client/test_sign_geometry.py', 'tests/tools/client/test_piston_geometry.py'):
        file = ROOT / name
        if file.exists(): result[name] = pin(file.read_bytes())
    # Include the selected actual driver even while privately reviewed before publication.
    result[str(Path(__file__).resolve())] = pin(Path(__file__).read_bytes())
    return result


def synthetic(packet):
    original = packet['observations'][0]
    actual = {'schema': 'NATIVE_SIGN_DIRECT_DRAW_V1', 'cleanup': True, 'rows_complete': 36,
              'device': {'vendor': 'synthetic reader fixture', 'renderer': 'no GL execution',
                         'version': 'protocol control', 'hidden': True, 'monitor': 2, 'display_id': 2},
              'font_constructed': copy.deepcopy(original['font']), 'rows': []}
    actual['font_constructed']['font']['buffer']['class'] = 'native.direct-int-buffer'
    for phase in ('before_gl','after_gl'):
        actual['font_constructed'][phase] = original_gl_projection(actual['font_constructed'][phase],phase != 'before_gl')
    for case, observed in zip(packet['cases'], original['rows']):
        terminal = case['null_font']
        record = {'index': case['index'], 'before': copy.deepcopy(observed['before']),
                  'after': copy.deepcopy(observed['after']), 'returned': not terminal,
                  'failure': None, 'retired': True, 'retire_idempotent': True,
                  'after_retire_gl': copy.deepcopy(observed['after']['gl']),
                  'constructed_font_after': copy.deepcopy(observed['constructed_font_after'])}
        if terminal:
            record['failure'] = {'status': 1, 'class': 'java/lang/NullPointerException', 'source_class': 'po',
                                 'pc': 356, 'native_policy': False, 'source_exception': True}
        for value in (record['before']['font'], record['after']['font'], record['constructed_font_after']):
            if value is not None: value['buffer']['class'] = 'native.direct-int-buffer'
        angle = case['metadata'] * 360 / 16 if case['block'] == 63 else {2:180,4:90,5:-90}.get(case['metadata'],0)
        receipt = {'source_pc': 356 if terminal else 25, 'model_parts': 2,
                   'compiled_parts': int(case['index'] < 2), 'faces': 6 * int(case['index'] < 2),
                   'vertices': 24 * int(case['index'] < 2), 'rows_measured': 0 if terminal else 4,
                   'rows_drawn': 0 if terminal else 4, 'text_units': 0, 'text_digest': '',
                   'angle': struct.pack('>f',angle).hex(), 'text_scale':'3c360b62',
                   'model_returned': True, 'model_popped': True, 'font_returned': True, 'depth_disabled': True,
                   'depth_restored': not terminal, 'color_restored': not terminal, 'matrix_popped': not terminal, 'rows': []}
        total = 14695981039346656037
        for i, units in enumerate(case['loaded_units']):
            selected = i == case['edit_row']
            if selected: units = '003e0020' + units + '0020003c'
            captured = not terminal or i == 0
            receipt['rows'].append({'captured': captured, 'null': False, 'selected': selected and captured,
                'units': len(units)//4 if captured else 0, 'digest': digest(i,units) if captured else '0000000000000000',
                'measure_attempted': captured, 'measure_returned': not terminal, 'draw_attempted': not terminal,
                'draw_returned': not terminal, 'width': 0, 'x': 0, 'y': 0 if terminal else i*10-20})
            if not terminal:
                receipt['text_units'] += len(units)//4
                data = b''.join(bytes.fromhex(units[j:j+4])[::-1] for j in range(0,len(units),4))
                for byte in struct.pack('<IBQ',i,0,len(data)//2)+data:
                    total = ((total ^ byte) * 1099511628211) & ((1 << 64) - 1)
        receipt['text_digest'] = f'{total:016x}'
        record['receipt'] = receipt
        for phase in ('before','after'):
            record[phase]['gl'] = original_gl_projection(record[phase]['gl'])
        record['after_retire_gl'] = original_gl_projection(record['after_retire_gl'])
        actual['rows'].append(record)
    return actual


def controls(packet,jdi=None):
    value = synthetic(packet)
    encode = lambda v: json.dumps(v, separators=(',',':'), allow_nan=False).encode() + b'\n'
    footer = b'16 achievements\n151 recipes\nPASS: 36 Source Sign direct PO observation rows; checks=100; cleanup=1\n'
    raw = encode(value)
    admit_native(raw,footer,b'',0,packet)
    count = 1
    # Runtime GL allocations need producer/consumer equality, not the original
    # process's numeric names. Exercise a wholly different consistent namespace.
    shifted = copy.deepcopy(value)
    old = ids({'font':shifted['font_constructed'],'rows':shifted['rows']})
    def shift_gl(gl):
        if gl['core']['bound_texture'] == old['font']: gl['core']['bound_texture'] += 20
        elif gl['core']['bound_texture'] == old['sign']: gl['core']['bound_texture'] += 30
    def shift_font(font):
        if font is None: return
        font['texture'] += 20; font['list_base'] += 1000
        font['buffer']['active_words'] = [f'{int(word,16)+1000:08x}' if int(word,16) else word
                                          for word in font['buffer']['active_words']]
    for name in ('before_gl','after_gl'): shift_gl(shifted['font_constructed'][name])
    shift_font(shifted['font_constructed']['font'])
    for row in shifted['rows']:
        for phase in ('before','after'):
            state = row[phase]
            shift_gl(state['gl']); shift_font(state['font'])
            if state['textures']: state['textures']['/item/sign.png'] += 30
            for name in ('board','pole'):
                if state['model'][name]['n']: state['model'][name]['o'] += 1000
        shift_gl(row['after_retire_gl']); shift_font(row['constructed_font_after'])
    admit_native(encode(shifted),footer,b'',0,packet);count += 1

    def reject(label, data=raw, stdout=footer, stderr=b'', code=0):
        nonlocal count
        try: admit_native(data,stdout,stderr,code,packet)
        except (ValueError, KeyError, TypeError, IndexError, UnicodeError, json.JSONDecodeError): count += 1
        else: raise ValueError('reader admitted corruption: ' + label)

    def changed(path, item):
        altered = copy.deepcopy(value)
        at = altered
        for key in path[:-1]: at = at[key]
        at[path[-1]] = item
        return encode(altered)

    for label,path,item in (
        ('rows count',('rows_complete',),35),('row reorder',('rows',1,'index'),0),
        ('row index bool',('rows',1,'index'),True),('extra root',('extra',),True),
        ('missing success',('rows',0,'returned'),False),('retirement omitted',('rows',0,'retired'),False),
        ('cleanup replay',('rows',35,'after_retire_gl','source','modelview_depth'),1),
        ('depth repair',('rows',35,'after_retire_gl','core','depth_write'),True),
        ('model compilation',('rows',0,'after','model','board','n'),False),
        ('pole first wall',('rows',0,'after','model','pole','h'),True),
        ('Source Tile text',('rows',24,'after','tile','lines',0,'units',0),'0054'),
        ('Source map extra',('rows',0,'after','world','mapped_tiles'),[]),
        ('Source buffer lost',('rows',24,'after','font','buffer','active_words'),[]),
        ('Source NW UV',('rows',1,'after','nw','i'),'0000000000000000'),
        ('unexpected GL error',('rows',0,'after','gl','core','error'),1280),
        ('source matrix word',('rows',35,'after','gl','source','modelview',0),'00000000'),
        ('source matrix mode',('rows',0,'after','gl','source','matrix_mode'),0),
        ('source projection word',('rows',0,'after','gl','source','projection',0),'00000000'),
        ('source normal word',('rows',0,'after','gl','source','current_normal',2),'00000000'),
        ('source normal unknown',('rows',0,'after','gl','source','normal_known'),False),
        ('source color unknown',('rows',0,'after','gl','source','color_known'),False),
        ('source matrix depth',('rows',35,'after','gl','source','modelview_depth'),1),
        ('source facade cold later',('rows',0,'after','gl','source','initialized'),False),
        ('source facade invented cold state',('font_constructed','before_gl','source','matrix_mode'),5888),
        ('unsupported field hidden',('rows',0,'after','gl','unrepresented'),[]),
        ('false source fault',('rows',35,'failure','native_policy'),True),
        ('early Source fault',('rows',35,'failure','pc'),202),
        ('Font row text',('rows',24,'receipt','rows',0,'digest'),'0000000000000000'),
        ('Font skipped return',('rows',24,'receipt','rows',0,'draw_returned'),False),
        ('Font late call',('rows',35,'receipt','rows',0,'measure_returned'),True),
        ('Font row position',('rows',24,'receipt','rows',0,'y'),-19),
        ('Font count bool',('rows',24,'receipt','rows_drawn'),True),
        ('Font aggregate',('rows',24,'receipt','text_digest'),'0000000000000000'),
        ('wrong monitor',('device','monitor'),1),('visible context',('device','hidden'),False)):
        reject(label, changed(path,item))
    for label,data in (('truncated',raw[:-10]),('newline missing',raw[:-1]),('extra document',raw+b'{}\n'),
                       ('duplicate',raw.replace(b'"cleanup":true',b'"cleanup":true,"cleanup":true',1))):
        reject(label,data)
    reject('footer prefix',stdout=footer.split(b'\n',2)[2]);reject('extra footer',stdout=footer+b'extra\n')
    reject('stderr',stderr=b'failure\n');reject('failed exit',code=1);reject('bool exit',code=False)
    if jdi is not None:
        # Copy actual qualified values only into a synthetic native protocol
        # fixture. These controls carry no new execution or Source credit.
        joined = copy.deepcopy(value)
        for call in jdi['observations'][0]['calls']:
            row = joined['rows'][call['case']]['receipt']['rows'][call['row']]
            row.update(width=call['width'],x=call['x'],y=call['y'])
        compare_jdi_font_calls(joined,jdi);count += 1
        for key in ('width','x','y','digest','units','draw_returned'):
            changed = copy.deepcopy(joined)
            row = changed['rows'][28]['receipt']['rows'][1]
            row[key] = ('0000000000000000' if key == 'digest' else False if key == 'draw_returned' else row[key]+1)
            try: compare_jdi_font_calls(changed,jdi)
            except ValueError: count += 1
            else: raise ValueError('JDI join admitted native mismatch ' + key)
        for corrupt in ('missing','duplicate','wrong_width','transport_replacement'):
            altered = copy.deepcopy(jdi)
            calls = altered['observations'][1]['calls']
            if corrupt == 'missing': calls.pop()
            elif corrupt == 'duplicate': calls[1] = calls[0]
            elif corrupt == 'wrong_width': calls[28*4+1]['width'] += 1
            else: calls[28*4+1]['units'] = calls[28*4+1]['units'].replace('d83d','fffd')
            try: compare_jdi_font_calls(joined,altered)
            except ValueError: count += 1
            else: raise ValueError('JDI join admitted original mismatch ' + corrupt)
        # Validate the decoder independently of JDI StringReference.value().
        # The unpaired surrogate must survive exact backing-byte reconstruction.
        storage = {'string_class':'java.lang.String','string_boot_loader':True,'backing_class':'byte[]',
                   'utf16_class':'java.lang.StringUTF16','utf16_boot_loader':True,'backing_id':'fixture',
                   'value_field':{'declaring_type':'java.lang.String','name':'value','descriptor':'[B','static':False,'final':True},
                   'coder_field':{'declaring_type':'java.lang.String','name':'coder','descriptor':'B','static':False,'final':True},
                   'hi_shift_field':{'declaring_type':'java.lang.StringUTF16','name':'HI_BYTE_SHIFT','descriptor':'I','static':True,'final':True},
                   'lo_shift_field':{'declaring_type':'java.lang.StringUTF16','name':'LO_BYTE_SHIFT','descriptor':'I','static':True,'final':True},
                   'coder':1,'hi_byte_shift':0,'lo_byte_shift':8,'bytes_hex':'00d8','backing_length':2,'byte_reads_equal':True}
        storage['before'] = {'value_id':'fixture','coder':1,'backing_length':2,'hi_byte_shift':0,'lo_byte_shift':8}
        storage['after'] = copy.deepcopy(storage['before'])
        argument = {'utf16_units':['d800'],'length':1}
        observation = {'is_null':False,'length':1,'utf16_units':['d800'],'storage':storage}
        require(qualified_string_units(argument,observation) == 'd800','exact independent surrogate backing control');count += 1
        lossy = copy.deepcopy(observation);lossy['utf16_units'] = ['fffd']
        try: qualified_string_units({'utf16_units':['fffd'],'length':1},lossy)
        except ValueError: count += 1
        else: raise ValueError('qualified String decoder admitted JDWP replacement loss')
    print(json.dumps({'status':'PASS','controls':count,
                      'scope':'Synthetic reader controls only; no native/JVM/GL/gameplay execution'}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True, help='frozen qualified ordinary original capture directory')
    parser.add_argument('--controls', action='store_true')
    parser.add_argument('--jdi-reference', type=Path, help='optional qualified V2 original Font-call cohort')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--artifacts', type=Path)
    args = parser.parse_args()
    packet = reference(args.reference)
    jdi = jdi_reference(args.jdi_reference,packet) if args.jdi_reference is not None else None
    if args.controls:
        require(args.executable is None and args.assets is None and args.artifacts is None, 'controls accept no native launch arguments')
        controls(packet,jdi);return
    require(os.name == 'nt' and all((args.executable,args.assets,args.artifacts)), 'actual Windows native product/assets/artifacts required')
    products = module('sign_native_products',ROOT/'tests/tools/client/native_product_capture.py')
    resources = module('sign_native_resources',ROOT/'tests/tools/client/test_picking.py')
    args.artifacts.mkdir(parents=True,exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='sign-direct-',dir=args.artifacts.resolve()))
    worlds = output/'worlds'
    # Copy only authored saved declarations. Original expected output is never
    # passed to the native process, resource loader or World constructor.
    shutil.copytree(packet['directory']/'prepared/fixtures',worlds)
    for name,data in resources.RESOURCES.items():
        file=output/'resources'/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
    profile = output/'profiles.tsv';profile.write_bytes((packet['directory']/'prepared/profiles.tsv').read_bytes())
    executable=args.executable.resolve(strict=True);assets=args.assets.resolve(strict=True)
    textures=(assets/'jar').resolve(strict=True)
    require(all((textures/name).is_file() for name in ('font.txt','font/default.png','item/sign.png','pack.png')), 'actual imported font/sign class resources')
    before_assets,before_worlds,before_sources=snapshot(assets),snapshot(worlds),compiled_inputs()
    before_reference=snapshot(packet['directory'])
    before_jdi=snapshot(jdi['directory']) if jdi is not None else None
    native_json=output/'native.json'
    command=[str(executable),str(output/'resources'),str(textures),str(worlds),str(profile),str(native_json)]
    report={'status':'FAIL','scope':__doc__,'command':command,'reference_pins':packet['pins'],
            'assets_before':before_assets,'worlds_before':before_worlds,'compiled_inputs_before':before_sources,
            'timed_out':False,'pipe_capture_complete':False}
    env=os.environ.copy();home=output/'appdata';home.mkdir()
    env.update(APPDATA=str(home),LOCALAPPDATA=str(home),USERPROFILE=str(home),ALSOFT_DRIVERS='null')
    error=None
    try:
        report['native_products']=products.capture_native_products(command,output)
        started=time.perf_counter()
        process=subprocess.run(command,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,timeout=300,check=False)
        report.update(returncode=process.returncode,elapsed_seconds=time.perf_counter()-started,pipe_capture_complete=True)
        (output/'stdout.bin').write_bytes(process.stdout);(output/'stderr.bin').write_bytes(process.stderr)
        require(process.returncode==0 and not process.stderr, 'actual native terminal exit=' + str(process.returncode)+': '+process.stderr.decode(errors='replace')[:4096])
        raw=bounded(native_json);report['native_json']=pin(raw)
        report.update(admit_native(raw,process.stdout,process.stderr,process.returncode,packet))
        if jdi is not None:
            report['jdi_font_calls'] = compare_jdi_font_calls(read_json(raw),jdi)
            report['font_call_scope'] = report['jdi_font_calls']['comparison']
        report['status']='PASS'
    except BaseException as failure:
        error=failure;report['error']=type(failure).__name__+': '+str(failure)
        if hasattr(failure,'receipt'): report['native_products']=failure.receipt
        if isinstance(failure,subprocess.TimeoutExpired):
            report.update(timed_out=True,pipe_capture_complete=False)
            (output/'stdout.bin').write_bytes(failure.stdout or b'');(output/'stderr.bin').write_bytes(failure.stderr or b'')
    try:
        report['assets_after']=snapshot(assets);report['worlds_after']=snapshot(worlds);report['compiled_inputs_after']=compiled_inputs()
        require(report['assets_after']==before_assets and report['compiled_inputs_after']==before_sources, 'unchanged actual assets/compiled source bracket')
        require(snapshot(packet['directory'])==before_reference and pin(profile.read_bytes())==INPUT_PINS['prepared/profiles.tsv'], 'unchanged full original reference cohort/native declaration transport')
        if jdi is not None: require(snapshot(jdi['directory']) == before_jdi,'unchanged complete JDI cohort bracket')
        guard_worlds(before_worlds,report['worlds_after'])
        report['native_products_after']={item['source']:pin(Path(item['source']).read_bytes()) for item in report.get('native_products',{}).get('products',[])}
        require(all(report['native_products_after'][item['source']]==item['before'] for item in report.get('native_products',{}).get('products',[])), 'unchanged every captured native product')
    except BaseException as failure:
        report['guard_errors']=[type(failure).__name__+': '+str(failure)];report['status']='FAIL'
        if error is None: error=failure;report['error']=report['guard_errors'][0]
    for stream in ('stdout','stderr'):
        file=output/(stream+'.bin')
        if file.exists(): report[stream]=pin(file.read_bytes())
    (output/'run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print('Sign direct artifacts: '+str(output))
    if error is not None: raise error


if __name__=='__main__': main()
