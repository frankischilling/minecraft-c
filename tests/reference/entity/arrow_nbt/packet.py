"""Portable admission of complete original Arrow NBT observations."""
import hashlib
from pathlib import Path

from observation import HERE, compare, exact, loads, need, parse_rows, project, source_call

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SCOPE = '44 paired representable and four source-only unchanged-original client Arrow NBT/parent/save/factory calls in four VMs; actual values, exception prefixes and Code; no mutable-native-writer, loaded gameplay, renderer or performance claim.'
ORIGINAL_SOURCES = ('ArrowNbtObservation.java', 'ArrowNbtJdi.java', 'capture_reference.py',
                    'profiles.tsv', 'class-names.txt', 'source-pins.json', 'PROTOCOL.md')
PROCESSING_SOURCES = ('observation.py', 'packet.py', 'export_reference.py')
SUPPORT = {'ArrowConstructionObservation.java': HERE.parent / 'arrow_construction/ArrowConstructionObservation.java',
           'constructor-observation.py': HERE.parent / 'arrow_construction/observation.py',
           'ConstructorJson.java': HERE.parent / 'player/ConstructorJson.java',
           'classfile.py': HERE.parent / 'player/classfile.py',
           'capture.py': HERE.parents[1] / 'client/controller/capture.py'}


def digest(path):
    path = Path(path); need(path.stat().st_size <= 32 * 1024 * 1024, 'bounded evidence member')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    digest(path)
    return loads(Path(path).read_text(encoding='utf-8'))


def code_records(codes, pins):
    seen = set()
    for code in codes:
        need(type(code) is dict and set(code) == {'event', 'class', 'method', 'descriptor', 'bytes', 'sha256'} and code['event'] == 'code', 'closed live Code record')
        key = (code['class'], code['method'], code['descriptor'])
        need(key not in seen, 'unique actual Code pin'); seen.add(key)
        need(code['class'] in pins['classes'], 'actual original Code owner')
        method = next((m for m in pins['classes'][code['class']]['methods'] if (m['name'], m['descriptor']) == key[1:]), None)
        need(method is not None and code['bytes'] == method['code_length'] and code['sha256'] == method['code_sha256'], 'executing original Code identity')
    need({('sl', 'a', '(Lnu;)V'), ('sl', 'b', '(Lnu;)V'), ('sn', 'e', '(Lnu;)V'),
          ('sn', 'd', '(Lnu;)V'), ('sn', 'c', '(Lnu;)Z'), ('jc', 'a', '(Lnu;Lfd;)Lsn;')} <= seen,
         'all six complete original entries were reached')


def journal(path, rows, pins):
    digest(path)
    records = [loads(line) for line in Path(path).read_text(encoding='utf-8').splitlines()]
    need(len(records) < 4096 and all(type(r) is dict and r.get('event') in ('code', 'call') for r in records), 'closed bounded source journal')
    calls = [r for r in records if r['event'] == 'call']
    need([r['case'] for r in calls] == list(range(48)), 'all actual source calls observed')
    codes = [r for r in records if r['event'] == 'code']; code_records(codes, pins)
    receipts = [source_call(call, row) for call, row in zip(calls, rows)]
    return calls, receipts, codes


def validate_capture(capture):
    need(capture['schema'] == 'ARROW_NBT_CAPTURE_V1' and capture['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and
         capture['profiles_per_vm'] == 48 and capture['input_before_sha256'] == capture['input_after_sha256'] and
         capture['helper_before_sha256'] == capture['helper_after_sha256'] and capture['binary_sha256']['client'] == JAR,
         'complete immutable four-VM original capture')
    for name, path in SUPPORT.items():
        need(capture['support_sha256'][name] == digest(path), 'exact unchanged capture support ' + name)


def read(directory=HERE):
    directory = Path(directory); provenance = load(directory / 'provenance.json')
    need(set(provenance) == {'schema', 'scope', 'archive_sha256', 'original_sources', 'processing_sources', 'finite_sha256', 'capture_sha256', 'runs'}, 'closed original provenance')
    need(provenance['schema'] == 'ARROW_NBT_PROVENANCE_V1' and provenance['scope'] == SCOPE and provenance['archive_sha256'] == JAR, 'declared authority and finite scope')
    for key, names in (('original_sources', ORIGINAL_SOURCES), ('processing_sources', PROCESSING_SOURCES)):
        need(set(provenance[key]) == set(names), 'exact producer inventory')
        for name, pin in provenance[key].items():
            need(digest(directory / name) == pin, 'bound producer ' + name)
    need(digest(directory / 'capture-receipt.json') == provenance['capture_sha256'] and digest(directory / 'finite.json') == provenance['finite_sha256'], 'retained capture/output identities')
    capture = load(directory / 'capture-receipt.json'); validate_capture(capture)
    need(all(capture['public_source_sha256'][name] == pin for name, pin in provenance['original_sources'].items()), 'actual original producer identity')
    finite = load(directory / 'finite.json')
    need(set(finite) == {'schema', 'scope', 'rows', 'raw_runs', 'traces', 'receipts', 'code_runs'} and
         finite['schema'] == 'ARROW_NBT_FINITE_V1' and finite['scope'] == SCOPE, 'closed finite Arrow NBT packet')
    need(len(finite['raw_runs']) == 4 and len(finite['traces']) == len(finite['receipts']) == len(finite['code_runs']) == 2, 'two ordinary and two passive original VMs')
    need([r['kind'] for r in provenance['runs']] == ['ordinary', 'ordinary', 'debugger', 'debugger'], 'exact four capture roles')
    pins = load(directory / 'source-pins.json')
    for codes in finite['code_runs']:
        code_records(codes, pins)
    projections = []
    for i, raw in enumerate(finite['raw_runs']):
        values = parse_rows('\n'.join('ARROW_NBT ' + __import__('json').dumps(row) for row in raw), directory)
        if i >= 2:
            receipts = [source_call(call, row) for call, row in zip(finite['traces'][i - 2], values)]
            need(exact(receipts, finite['receipts'][i - 2]), 'exact actual source receipts')
        projections.append([project(row) for row in values[:44]])
    for rows in projections:
        compare(projections[2], rows)
    compare(finite['rows'], projections[2])
    return finite, provenance
