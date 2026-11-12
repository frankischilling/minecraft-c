"""Portable admission of complete unchanged-original Arrow evidence."""
import hashlib
from pathlib import Path

from observation import HERE, compare, exact, loads, need, parse_rows, project, source_call

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SCOPE = '40 complete unchanged-original client SL constructor/heading calls in four VMs; typed source state, failure prefixes, independent default entropy and declared seeded heading inputs; no Arrow tick, C3, renderer, NBT, loaded gameplay or performance claim.'
ORIGINAL_SOURCES = ('ArrowConstructionObservation.java', 'ArrowConstructionJdi.java', 'capture_reference.py',
                    'profiles.tsv', 'class-names.txt', 'source-pins.json', 'PROTOCOL.md')
PROCESSING_SOURCES = ('observation.py', 'packet.py', 'export_reference.py')


def digest(path):
    path = Path(path); need(path.stat().st_size <= 16 * 1024 * 1024, 'bounded evidence member')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    digest(path)
    return loads(Path(path).read_text(encoding='utf-8'))


def journal(path, rows, pins):
    records = [loads(line) for line in Path(path).read_text(encoding='utf-8').splitlines()]
    need(len(records) < 4096 and all(record.get('event') in ('code', 'call') for record in records), 'bounded closed journal events')
    calls = [record for record in records if record['event'] == 'call']
    need([call['case'] for call in calls] == list(range(40)), 'complete passive source call inventory')
    codes = [record for record in records if record['event'] == 'code']
    code_records(codes, pins)
    receipts = [source_call(call, row) for call, row in zip(calls, rows)]
    return calls, receipts


def code_records(codes, pins):
    seen = set()
    for code in codes:
        need(set(code) == {'event', 'class', 'method', 'descriptor', 'bytes', 'sha256'}, 'closed live Code record')
        key = (code['class'], code['method'], code['descriptor']); need(key not in seen, 'unique live Code pin'); seen.add(key)
        need(code['class'] in pins['classes'], 'actual source Code owner')
        method = next((m for m in pins['classes'][code['class']]['methods'] if m['name'] == code['method'] and m['descriptor'] == code['descriptor']), None)
        need(method is not None and code['bytes'] == method['code_length'] and code['sha256'] == method['code_sha256'], 'actual executing original Code')
    need({('sl', '<init>', '(Lfd;)V'), ('sl', '<init>', '(Lfd;DDD)V'), ('sl', '<init>', '(Lfd;Lls;)V'),
          ('sl', 'a', '(DDDFF)V')} <= seen, 'all four complete original methods were entered')


def read(directory=HERE):
    directory = Path(directory); provenance = load(directory / 'provenance.json')
    need(set(provenance) == {'schema', 'scope', 'archive_sha256', 'original_sources', 'processing_sources', 'finite_sha256', 'capture_sha256', 'runs'}, 'closed provenance')
    need(provenance['schema'] == 'ARROW_CONSTRUCTION_PROVENANCE_V1' and provenance['scope'] == SCOPE and provenance['archive_sha256'] == JAR, 'scope and authority')
    for key, names in (('original_sources', ORIGINAL_SOURCES), ('processing_sources', PROCESSING_SOURCES)):
        need(set(provenance[key]) == set(names), 'exact producer inventory')
        for name, pin in provenance[key].items():
            need(digest(directory / name) == pin, 'bound producer ' + name)
    need(digest(directory / 'capture-receipt.json') == provenance['capture_sha256'] and digest(directory / 'finite.json') == provenance['finite_sha256'], 'exact retained capture and output identities')
    capture = load(directory / 'capture-receipt.json')
    need(capture['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and capture['profiles_per_vm'] == 40 and
         capture['input_before_sha256'] == capture['input_after_sha256'] and capture['helper_before_sha256'] == capture['helper_after_sha256'] and
         capture['binary_sha256']['client'] == JAR, 'immutable complete four-VM capture')
    need(all(capture['public_source_sha256'][name] == pin for name, pin in provenance['original_sources'].items()), 'actual captured producer bindings')
    finite = load(directory / 'finite.json')
    need(set(finite) == {'schema', 'scope', 'rows', 'raw_runs', 'traces', 'receipts', 'code_runs'} and finite['schema'] == 'ARROW_CONSTRUCTION_FINITE_V1' and finite['scope'] == SCOPE, 'closed finite packet')
    need(len(finite['raw_runs']) == 4 and len(finite['traces']) == len(finite['receipts']) == 2, 'four actual independent VMs')
    need([run['kind'] for run in provenance['runs']] == ['ordinary', 'ordinary', 'debugger', 'debugger'], 'four actual capture roles')
    need(len(finite['code_runs']) == 2, 'two passive live Code inventories')
    pins = load(directory / 'source-pins.json')
    for codes in finite['code_runs']:
        code_records(codes, pins)
    projections = []
    for index, rows in enumerate(finite['raw_runs']):
        values = parse_rows('\n'.join('ARROW_CONSTRUCTION ' + __import__('json').dumps(row) for row in rows), directory)
        trace = finite['traces'][index - 2] if index >= 2 else None
        if trace is not None:
            receipts = [source_call(call, row) for call, row in zip(trace, values)]
            need(exact(receipts, finite['receipts'][index - 2]), 'retained actual heading/source receipts')
        projections.append([project(row, trace[n] if trace else None) for n, row in enumerate(values)])
    compare(projections[2], projections[3])
    # Ordinary failed constructor receivers are unavailable; compare every
    # remaining field, preserving that explicit omission instead of filling it.
    for rows in projections[:2]:
        expected = __import__('copy').deepcopy(projections[2])
        for case in (14, 15):
            expected[case]['arrow_after'] = None
        compare(expected, rows)
    compare(finite['rows'], projections[2])
    return finite, provenance
