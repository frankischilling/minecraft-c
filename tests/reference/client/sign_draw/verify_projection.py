"""Independent second-pass joins from public records to both retained raw journals."""
import sys
sys.dont_write_bytecode = True
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

def need(value, message):
    if not value:
        raise ValueError(message)

def read(path):
    return json.loads(Path(path).read_bytes())

def pin(path):
    raw = Path(path).read_bytes()
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}

def is_entry(e, cls, method, descriptor=None):
    return e['event'] == 'entry' and e['site']['class'] == cls and e['site']['method'] == method and (descriptor is None or e['site']['descriptor'] == descriptor)

def is_exit(e, cls, method, descriptor=None):
    return e['event'] == 'exit' and e['class'] == cls and e['method'] == method and (descriptor is None or e['descriptor'] == descriptor)

def target(path):
    lines = Path(path).read_bytes().decode('ascii').splitlines()
    return [json.loads(line.split(' ', 1)[1]) for line in lines[2:]]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('ordinary', 'joined', 'packet'):
        p.add_argument('--' + name, required=True, type=Path)
    args = p.parse_args(); public = args.packet.resolve()
    sys.path.insert(0, str(public))
    spec = importlib.util.spec_from_file_location('independent_sign_public_admission', public / 'packet.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    packet = module.read_packet(public); data = packet['expected']; provenance = data['provenance.json']
    traces = data['traces.json']['traces']; draws = data['draws.json']['draws']; batches = data['batches.json']['batches']
    font = data['font.json']; definitions = font['definitions'] + font['model_definitions']
    ordinary_run, joined_run = read(args.ordinary / 'run.json'), read(args.joined / 'run.json')
    need(pin(args.ordinary / 'run.json') == provenance['raw_capture_receipts']['ordinary']
         and pin(args.joined / 'run.json') == provenance['raw_capture_receipts']['joined'], 'frozen raw capture receipts')
    need((args.ordinary / '0-interpreter.stdout').read_bytes() == (public / 'ordinary.stdout').read_bytes(), 'unmodified ordinary stdout bytes')
    targets = [target(args.ordinary / (str(i) + '-' + mode + '.stdout')) for i, mode in enumerate(('interpreter', 'default'))]
    checks = 0
    for mode_index, mode in enumerate(('interpreter', 'default')):
        directory = args.joined / (str(mode_index) + '-' + mode + '-observer')
        raw = (directory / 'journal.ndjson').read_bytes(); journal = [json.loads(line) for line in raw.splitlines()]
        need(pin(directory / 'journal.ndjson') == provenance['streams']['jdi-' + mode]['target']['streams']['journal.ndjson'], 'raw journal hash')
        codes = [{k: e[k] for k in ('class', 'method', 'descriptor', 'native', 'bytes', 'sha256')} for e in journal if e['event'] == 'live-code']
        need(codes == data['source-pins.json']['live_methods'], 'all44 actual live Code rows')
        aliases, inverse = {}, {}
        def match(actual, published):
            nonlocal checks
            checks += 1
            if isinstance(published, dict) and set(published) == {'trace'}:
                need(actual == traces[published['trace']], 'complete observed Source trace')
            elif isinstance(actual, dict) and set(actual) == {'class', 'identity'}:
                need(set(published) == {'class', 'label'} and actual['class'] == published['class'], 'actual receiver type')
                key, label = (actual['class'], str(actual['identity'])), (published['class'], published['label'])
                need(aliases.setdefault(key, label) == label and inverse.setdefault(label, key) == key, 'bijective output-only receiver labels')
            elif isinstance(actual, dict):
                need(type(published) is dict and set(actual) == set(published), 'complete selected record fields')
                for key in actual:
                    match(actual[key], published[key])
            elif isinstance(actual, list):
                need(type(published) is list and len(actual) == len(published), 'complete selected array')
                for x, y in zip(actual, published):
                    match(x, y)
            else:
                need(type(actual) is type(published) and actual == published, 'exact observed scalar')
        def call(e):
            return {k: e[k] for k in ('site', 'receiver', 'arguments', 'source_frames')}
        raw_batches = [e for e in journal if e['event'] == 'nw-draw']
        arrays = [e for e in journal if is_entry(e, 'org.lwjgl.opengl.GL11', 'glDrawArrays')]
        need(len(raw_batches) == len(arrays) == len(batches) == 268, 'all actual active batches/draw leaves')
        for observed, array, published in zip(raw_batches, arrays, batches):
            match(observed['receiver'], published['receiver']); match(observed['state'], published['state'])
            match(observed['words'], published['words']); match(observed['source_frames'], published['source_frames'])
            match(array['arguments'], published['draw_arrays']['arguments']); match(array['source_frames'], published['draw_arrays']['source_frames'])
        starts = [e for e in journal if is_entry(e, 'org.lwjgl.opengl.GL11', 'glNewList')]
        ends = [e for e in journal if is_entry(e, 'org.lwjgl.opengl.GL11', 'glEndList')]
        need(len(starts) == len(ends) == len(definitions) == 290, 'all actual list definition boundaries')
        for start, end, definition in zip(starts, ends, definitions):
            need([definition['id'], definition['mode']] == start['arguments'] and definition['phase'] == start['profile'], 'actual list producer operands')
            selected = [e for e in journal if 'sequence' in e and start['sequence'] < e['sequence'] < end['sequence']]
            indexes = [i for i, batch in enumerate(raw_batches) if start['sequence'] < batch['sequence'] < end['sequence']]
            need(indexes == definition['batches'], 'independent interval join for every complete list/batch')
            operations = [{'method': e['site']['method'], 'arguments': e['arguments'], 'source_frames': e['source_frames']}
                          for e in selected if e['event'] == 'entry' and e['site']['class'] == 'org.lwjgl.opengl.GL11'
                          and e['site']['method'] in ('glColor3f', 'glTranslatef')]
            match(operations, definition['operations'])
            if definition['part'] is not None:
                earlier = [e for e in journal if is_entry(e, 'ps', 'd', '(F)V') and e['sequence'] < start['sequence']]
                match(earlier[-1]['receiver'], definition['part'])
        backing = {e['for_record_sequence']: e['observations'] for e in journal if e['event'] == 'string-backing'}
        for values in backing.values():
            for observed in values:
                s = observed['storage']; raw_bytes = bytes.fromhex(s['bytes_hex'])
                order = 'little' if s['hi_byte_shift'] == 0 else 'big'
                units = list(raw_bytes) if s['coder'] == 0 else [int.from_bytes(raw_bytes[i:i + 2], order) for i in range(0, len(raw_bytes), 2)]
                need(observed['utf16_units'] == ['%04x' % value for value in units], 'independent byte-order UTF16 decode')
        ordinal = 0
        for index, published in enumerate(draws):
            group = [e for e in journal if e.get('profile') == index]
            po = [e for e in group if is_entry(e, 'po', 'a', '(Lyk;DDDF)V')]
            rf = [e for e in group if is_entry(e, 'rf', 'a', '()V')]
            parts = [e for e in group if is_entry(e, 'ps', 'a', '(F)V')]
            match(call(po[0]), published['entry']); match(call(rf[0]), {k: published['model'][k] for k in call(rf[0])})
            match(rf[0]['parts'], published['model']['parts'])
            match([dict(**call(e), state=e['receiver_primitives']) for e in parts], published['part_calls'])
            match([call(e) for e in group if e['event'] == 'entry' and e['site']['class'] == 'org.lwjgl.opengl.GL11'], published['gl'])
            measure = [e for e in group if is_entry(e, 'sj', 'a', '(Ljava/lang/String;)I')]
            widths = [e['return'] for e in group if is_exit(e, 'sj', 'a', '(Ljava/lang/String;)I')]
            text = [e for e in group if is_entry(e, 'sj', 'b', '(Ljava/lang/String;III)V')]
            core = [e for e in group if is_entry(e, 'sj', 'a', '(Ljava/lang/String;IIIZ)V')]
            payloads = [e for e in group if e['event'] == 'font-list-buffer']
            for row, m, width, d, c, payload in zip(published['rows'], measure, widths, text, core, payloads):
                match(call(m), row['measure']); match(call(d), row['draw']); match(call(c), row['core']); match(width, row['width'])
                s = backing[m['sequence']][0]['storage']; ds = backing[d['sequence']][0]['storage']
                match({k: s[k] for k in ('coder', 'bytes_hex', 'hi_byte_shift', 'lo_byte_shift')}, row['string_storage'])
                need(row['same_string_alias'] is (s['string_id'] == ds['string_id']), 'actual String alias join')
                raw_observation = payload['observation']; response = read(directory / raw_observation['response']); p = row['payload']
                need(response['active_bytes_hex'] == p['active_bytes_hex'] and response['words_hex'] == response['native_words_hex'] == p['words'], 'all actual native active bytes/words')
                evidence = provenance['streams']['jdi-' + mode]['target']['buffer_evidence'][ordinal]
                need(pin(directory / raw_observation['request']) == evidence['request'] and pin(directory / raw_observation['response']) == evidence['response'], 'actual private request/response hashes')
                match(payload['source_frames'], row['payload_source_frames']); ordinal += 1
            need(published['returned'] is bool([e for e in group if is_exit(e, 'po', 'a', '(Lyk;DDDF)V')]), 'actual PO return ledger')
            failures = [{k: e[k] for k in ('site', 'catch', 'exception', 'message', 'source_frames')} for e in group if e['event'] == 'exception']
            match(failures, published['failures'])
        need(ordinal == 140, 'all actual public Font payloads independently joined')
        targets.append(target(directory / 'target.stdout'))
    baseline = copy.deepcopy(targets[0])
    for mode, value in zip(provenance['modes'], targets):
        value = copy.deepcopy(value); yaw = []
        for row in value[3:39]:
            before = row['before']['manager']['primitives'].pop('h'); after = row['after']['manager']['primitives'].pop('h')
            need(before == after, 'raw viewer yaw unchanged by call'); yaw.append(before)
        need(yaw == provenance['streams'][mode]['raw_viewer_yaw_bits'], 'all raw viewer-yaw outputs retained')
        if mode == provenance['modes'][0]:
            baseline = value
        else:
            need(value == baseline, 'exact remaining Target state across all4 actual VMs')
    print(json.dumps({'status': 'PASS', 'raw_journals': 2, 'independent_scalar_joins': checks,
                      'active_batches_per_journal': 268, 'payloads_per_journal': 140,
                      'scope': 'Read-only retained-byte joins; no original/native runtime launched'}, sort_keys=True))

if __name__ == '__main__':
    main()
