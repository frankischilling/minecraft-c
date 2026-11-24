"""Admit the retained four-VM original Skeleton firing source packet."""
import ast
import hashlib
from pathlib import Path
import re

import observation as O

HERE = O.HERE
ROOT = O.ROOT
need, exact, loads = O.need, O.exact, O.loads
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CAPTURE = '53a6fe2dac7b562455a37355ef3a9308c1a89365cde8603f5ae0e14e9b581308'
SEAL = 'af26f78ba9ef2bbff432092b62936376cde16cb298110297ec984a9d0bc14f23'
POSTFLIGHT = '53c9af7be5c0362e328a09fda97e7d9b8b81c1537e91f5b057adaca7d2b958af'
SCOPE = '24 complete original client FR firing calls in each of four VMs; four-VM ordinary-common and two-VM additional local-Arrow source domains; independent entropy and native outcome boundaries explicit; no native FR, SL tick, encounter, audiovisual or performance qualification.'
CAPTURED = {name: name for name in ('SkeletonFireObservation.java', 'SkeletonFireJdi.java', 'profiles.tsv', 'class-names.txt', 'source-pins.json')}
CAPTURED.update({name: 'captured/' + name for name in ('capture_reference.py', 'PROTOCOL.md', 'observation.py', 'preflight.py', 'leaf_ledger.py', 'seal.json')})
PROCESSING = ('observation.py', 'packet.py', 'export_reference.py', 'leaf_ledger.py')
SUPPORT = {'ArrowConstructionObservation.java': 'tests/reference/entity/arrow_construction/ArrowConstructionObservation.java',
           'constructor-observation.py': 'tests/reference/entity/arrow_construction/observation.py',
           'ConstructorJson.java': 'tests/reference/entity/player/ConstructorJson.java',
           'classfile.py': 'tests/reference/entity/player/classfile.py',
           'capture.py': 'tests/reference/client/controller/capture.py'}
NATIVE_AUDIT = ('src/world/runtime/world_access.c', 'src/world/runtime/movement.c',
                'include/mc/world/runtime/world_access_error.h')


def digest(path):
    path = Path(path); need(path.stat().st_size <= 32 * 1024 * 1024, 'bounded source evidence member')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def data_digest(value):
    return hashlib.sha256(value.encode('utf-8')).hexdigest()


def load(path):
    digest(path)
    return loads(Path(path).read_text(encoding='utf-8'))


def metadata_class(value):
    need(type(value) is dict and set(value) == {'name', 'super', 'interfaces', 'major', 'minor', 'access', 'class_sha256', 'class_bytes', 'fields', 'methods'}, 'class metadata only, without original bytecode')
    need(type(value['class_bytes']) is int and 0 < value['class_bytes'] <= 262144 and
         re.fullmatch('[0-9a-f]{64}', value['class_sha256']) is not None, 'whole class extent and identity')
    for f in value['fields']:
        need(set(f) == {'access', 'name', 'descriptor'}, 'closed field declaration metadata')
    for m in value['methods']:
        plain = {'access', 'name', 'descriptor'}
        code = {'code_length', 'code_sha256', 'code_attribute_sha256', 'max_stack', 'max_locals', 'exception_table'}
        need(set(m) in (plain, plain | code), 'closed method/Code metadata without bytes')
        if 'code_length' in m:
            need(type(m['code_length']) is int and 0 < m['code_length'] < 65536 and
                 all(re.fullmatch('[0-9a-f]{64}', m[k]) is not None for k in ('code_sha256', 'code_attribute_sha256')), 'complete Code metadata identity')


def source_pins(directory=HERE):
    directory = Path(directory); pins = load(directory / 'source-pins.json')
    need(set(pins) == {'archive', 'classes'} and pins['archive'] == {'sha256': JAR, 'bytes': 1465375} and
         len(pins['classes']) == 35 and list(pins['classes']) == (directory / 'class-names.txt').read_text(encoding='ascii').splitlines(), 'all35 exact original class declarations')
    for value in pins['classes'].values():
        metadata_class(value)
    return pins


def code_records(records, pins, helper_pins):
    source, helper, seen = [], [], set()
    for code in records:
        need(type(code) is dict and set(code) == {'event', 'class', 'method', 'descriptor', 'bytes', 'sha256'} and code['event'] in ('code', 'helper-code') and
             type(code['bytes']) is int and all(type(code[k]) is str for k in ('class', 'method', 'descriptor', 'sha256')), 'closed actual live Code witness')
        key = (code['event'], code['class'], code['method'], code['descriptor'])
        need(key not in seen, 'unique actual live Code identity'); seen.add(key)
        inventory = pins['classes'] if code['event'] == 'code' else helper_pins['classes']
        need(code['class'] in inventory and (code['event'] == 'code' or code['class'] == 'SkeletonFireObservation$Probe'), 'original and authored helper Code authorities stay separate')
        m = next((m for m in inventory[code['class']]['methods'] if (m['name'], m['descriptor']) == key[2:]), None)
        need(m is not None and (m['code_length'], m['code_sha256']) == (code['bytes'], code['sha256']), 'complete executing Code identity')
        (source if code['event'] == 'code' else helper).append(code)
    required = {('fr', 'a', '(Lsn;F)V'), ('sl', '<init>', '(Lfd;Lls;)V'), ('sl', 'a', '(DDDFF)V'),
                ('fd', 'a', '(Lsn;Ljava/lang/String;FF)V'), ('fd', 'b', '(Lsn;)Z'), ('fd', 'c', '(Lsn;)V'), ('lm', 'a', '(Lsn;)V')}
    need(len(source) == 26 and len(helper) == 2 and required <= {tuple((c['class'], c['method'], c['descriptor'])) for c in source}, 'actual26 original and two authored helper live Code bodies')
    return source, helper


def process_receipt(value, timeout, quiet=False):
    need(type(value) is dict and type(value.get('exit')) is int and value['exit'] == 0 and value.get('timed_out') is False and
         value.get('pipe_capture_complete') is True and value.get('reader_errors') == {} and value.get('output_overflow') is False and
         value.get('timeout_seconds') == timeout and value.get('stdout_limit') == value.get('stderr_limit') == 16 * 1024 * 1024,
         'complete bounded original process and stream receipt')
    if quiet:
        need(value['stderr_bytes'] == 0, 'quiet compiler/debugger controller')


def validate_capture(value, directory=HERE):
    directory = Path(directory)
    need(value['schema'] == 'SKELETON_FIRE_CAPTURE_V1' and value['status'] == 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES' and
         value['profiles_per_vm'] == 24 and value['seal_sha256'] == SEAL and value['input_before_sha256'] == value['input_after_sha256'] and
         value['helper_before_sha256'] == value['helper_after_sha256'] and len(value['helper_before_sha256']) == 6 and
         value['binary_sha256']['client'] == JAR, 'exact immutable first complete four-VM capture')
    need(set(value['producer_source_sha256']) == set(CAPTURED), 'complete captured supplier inventory')
    for name, path in CAPTURED.items():
        need(digest(directory / path) == value['producer_source_sha256'][name], 'unchanged captured supplier ' + name)
    need(digest(directory / 'captured/seal.json') == SEAL and value['support_sha256']['observation.py'] == digest(directory / 'captured/observation.py'), 'captured validator is preserved separately from current processing')
    for name, path in SUPPORT.items():
        need(digest(ROOT / path) == value['support_sha256'][name], 'unchanged original capture support ' + name)
    process_receipt(value['java'], 15); process_receipt(value['javac'], 15)
    process_receipt(value['compile'], 45, quiet=True)
    need(len(value['ordinary']) == len(value['debugger']) == 2, 'two ordinary and two passive fresh VMs')
    for receipt in value['ordinary']:
        process_receipt(receipt, 240)
    for receipt in value['debugger']:
        process_receipt(receipt, 600, quiet=True)


def helper_metadata(value, capture):
    need(set(value) == {'schema', 'classes'} and value['schema'] == 'SKELETON_FIRE_CAPTURED_HELPER_METADATA_V1' and
         len(value['classes']) == 6, 'metadata for all six actual compiled helpers')
    need({name + '.class': c['class_sha256'] for name, c in value['classes'].items()} == capture['helper_before_sha256'], 'whole compiled helper identities match the actual capture')
    for c in value['classes'].values():
        metadata_class(c)


def processing_derivation(directory=HERE):
    directory = Path(directory)
    captured = ast.parse((directory / 'captured/observation.py').read_text(encoding='utf-8'))
    current = ast.parse((directory / 'observation.py').read_text(encoding='utf-8'))
    old = {n.name: ast.dump(n, include_attributes=False) for n in captured.body if isinstance(n, ast.FunctionDef)}
    new = {n.name: ast.dump(n, include_attributes=False) for n in current.body if isinstance(n, ast.FunctionDef)}
    need(old and all(new.get(name) == body for name, body in old.items()), 'every captured validator function body remains unchanged in current processing')
    return {'captured_validator_sha256': digest(directory / 'captured/observation.py'),
            'current_validator_sha256': digest(directory / 'observation.py'), 'captured_functions_preserved': sorted(old),
            'role': 'Current ROOT path adapter plus additional closed callback/arithmetic/domain validation; no source recapture or supplier edit'}


def stream_rows(stream, receipt, pins, *, passive=False):
    need(set(stream) == {'stdout', 'stderr'} and all(type(stream[k]) is str for k in stream), 'closed exact original streams')
    for name in ('stdout', 'stderr'):
        raw = stream[name].encode('utf-8')
        need(len(raw) == receipt[name + '_bytes'] and hashlib.sha256(raw).hexdigest() == receipt[name + '_sha256'], 'exact original ' + name + ' bytes')
    text = stream['stdout']; lines = text.splitlines()
    need(text.endswith('\n') and lines[-1] == 'SKELETON_FIRE_COMPLETE cases=24 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0' and
         lines.count('FR_CONSTRUCTOR declaring=fr descriptor=(Lfd;)V modifiers=1') == 1, 'actual complete footer and genuine reflected constructor witness')
    need([line for line in lines if line.startswith('CLASS ')] == ['CLASS ' + n + ' ' + str(p['class_bytes']) + ' ' + p['class_sha256'] for n, p in pins['classes'].items()], 'all35 actual resolved original class origins')
    warnings = stream['stderr'].splitlines()
    need(len(warnings) == 4 and warnings[0] == 'WARNING: A terminally deprecated method in java.lang.System has been called' and
         warnings[1].startswith('WARNING: System::setSecurityManager has been called by SkeletonFireObservation (file:/') and warnings[1].endswith('/classes/)') and
         warnings[2] == 'WARNING: Please consider reporting this to the maintainers of SkeletonFireObservation' and
         warnings[3] == 'WARNING: System::setSecurityManager will be removed in a future release', 'only the declared exact JDK17 SecurityManager warning')
    return O.strict_rows(text)


def journal_records(text, rows, pins, helpers):
    need(type(text) is str and len(text.encode('utf-8')) <= 32 * 1024 * 1024 and text.endswith('\n'), 'complete bounded passive journal')
    records = [loads(line) for line in text.splitlines()]
    need(len(records) < 4096 and all(type(r) is dict and r.get('event') in ('code', 'helper-code', 'call') for r in records), 'closed source/helper journal roles')
    calls = [r for r in records if r['event'] == 'call']
    need([r['case'] for r in calls] == list(range(24)), 'all24 actual passive call witnesses')
    codes, helper_codes = code_records([r for r in records if r['event'] != 'call'], pins, helpers)
    receipts = [O.strict_source_call(call, row) for call, row in zip(calls, rows)]
    return calls, receipts, codes, helper_codes


def validate_postflight(value):
    need(value['schema'] == 'PRIVATE_SKELETON_FIRE_CAPTURE_POSTFLIGHT_V1' and value['status'] == 'PASS_FIRST_COMPLETE_ORIGINAL_CAPTURE_NO_NATIVE_OR_PUBLIC_EXPORT' and
         value['capture_sha256'] == CAPTURE and value['reviewed_seal_sha256'] == SEAL and value['total_calls'] == 96 and
         value['normal_returns'] == 60 and value['exceptions'] == 36 and value['producer_inputs_unchanged'] is True and
         value['compiled_helpers_unchanged'] is True and value['retry_count'] == 0 and value['public_export'] is False and
         value['native_execution'] is False and [(r['kind'], r['index']) for r in value['runs']] == [('ordinary', 0), ('ordinary', 1), ('debugger', 0), ('debugger', 1)],
         'exact retained first-source postflight witness, separate from current processing')
    for i, run in enumerate(value['runs']):
        need(run['calls'] == 24 and run['normal_returns'] == 15 and
             run['exceptions'] == {'java/lang/NullPointerException': 7, 'java/lang/IllegalStateException': 2}, 'all actual whole-parent source outcomes')
        if i >= 2:
            target = run['target_receipt']
            need(target['reaped'] is True and type(target['exit']) is int and target['exit'] == 0 and target['timed_out'] is False and
                 target['streams_complete'] is True and target['reader_error'] is None and target['timeout_seconds'] == 540 and
                 target['limit_each'] == 16 * 1024 * 1024 and run['events'] == 2671 and run['source_Code'] == 26 and run['helper_Code'] == 2 and
                 run['gaussian_calls'] == 81 and run['Skeleton_float_calls'] == 16, 'complete reaped passive targets and actual source inventories')


def source_domains(streams, journals, capture, postflight, pins, helpers):
    need(type(streams) is list and len(streams) == 4 and type(journals) is list and len(journals) == 2, 'four exact source streams and two actual journals')
    raw = []
    for i, stream in enumerate(streams):
        witness = postflight['runs'][i]
        receipt = capture['ordinary'][i] if i < 2 else {
            'stdout_bytes': witness['target_receipt']['stdout_bytes'], 'stdout_sha256': witness['stdout_sha256'],
            'stderr_bytes': witness['target_receipt']['stderr_bytes'], 'stderr_sha256': witness['stderr_sha256']}
        need(receipt['stdout_sha256'] == witness['stdout_sha256'] and receipt['stderr_sha256'] == witness['stderr_sha256'], 'central and retained source stream identities agree')
        raw.append(stream_rows(stream, receipt, pins, passive=i >= 2))
    traces, receipts, code_runs, helper_runs = [], [], [], []
    for i, journal in enumerate(journals):
        need(data_digest(journal) == postflight['runs'][i + 2]['journal_sha256'], 'exact captured passive journal bytes')
        calls, observed, codes, authored = journal_records(journal, raw[i + 2], pins, helpers)
        traces.append(calls); receipts.append(observed); code_runs.append(codes); helper_runs.append(authored)
    common = [[O.common_projection(row) for row in rows] for rows in raw]
    for candidate in common:
        O.compare_rows(common[0], candidate, range(24))
    children = [[O.child_projection(calls[n], raw[i + 2][n]) for n in O.INVISIBLE] for i, calls in enumerate(traces)]
    O.compare_rows(children[0], children[1], O.INVISIBLE)
    return raw, traces, receipts, code_runs, helper_runs, common[0], children[0]


def read(directory=HERE):
    directory = Path(directory); provenance = load(directory / 'provenance.json')
    need(set(provenance) == {'schema', 'scope', 'archive_sha256', 'capture_sha256', 'source_postflight_sha256', 'captured_sources', 'current_processing',
                            'finite_sha256', 'helper_metadata_sha256', 'processing_receipt_sha256', 'runs'}, 'closed source provenance')
    need(provenance['schema'] == 'SKELETON_FIRE_SOURCE_PROVENANCE_V1' and provenance['scope'] == SCOPE and provenance['archive_sha256'] == JAR and
         provenance['capture_sha256'] == CAPTURE and provenance['source_postflight_sha256'] == POSTFLIGHT, 'finite source scope and original capture authority')
    need(set(provenance['captured_sources']) == set(CAPTURED) and set(provenance['current_processing']) == set(PROCESSING), 'captured suppliers and current processing have distinct complete inventories')
    for name, path in CAPTURED.items():
        need(provenance['captured_sources'][name] == {'path': path, 'sha256': digest(directory / path)}, 'exact captured supplier identity')
    for name in PROCESSING:
        need(digest(directory / name) == provenance['current_processing'][name], 'bound current processing ' + name)
    need(digest(directory / 'capture-receipt.json') == CAPTURE and digest(directory / 'finite.json') == provenance['finite_sha256'] and
         digest(directory / 'helper-pins.json') == provenance['helper_metadata_sha256'] and
         digest(directory / 'processing-receipt.json') == provenance['processing_receipt_sha256'], 'exact retained source and derived artifact identities')
    capture = load(directory / 'capture-receipt.json'); validate_capture(capture, directory)
    need(digest(directory / 'source-postflight.json') == POSTFLIGHT, 'exact prior source postflight proof')
    postflight = load(directory / 'source-postflight.json'); validate_postflight(postflight)
    helpers = load(directory / 'helper-pins.json'); helper_metadata(helpers, capture)
    processing = load(directory / 'processing-receipt.json')
    need(processing['derivation'] == processing_derivation(directory) and processing['source_capture_sha256'] == CAPTURE and
         processing['source_postflight_sha256'] == POSTFLIGHT and processing['input_before_sha256'] == processing['input_after_sha256'] and
         processing['JVM_started'] is processing['native_started'] is processing['source_supplier_edits'] is False and
         processing['source_retries'] == 0 and processing['native_boundary_source_review'] == load(directory / 'native-boundaries.json'), 'explicit processing-only source revalidation')
    need(provenance['runs'] == postflight['runs'], 'run-role evidence matches the retained source witness')
    finite = load(directory / 'finite.json')
    need(set(finite) == {'schema', 'scope', 'source_streams', 'source_journals', 'ordinary_common', 'passive_children',
                        'source_receipts', 'source_code_runs', 'helper_code_runs', 'raw_leaf_ledgers', 'counts'} and
         finite['schema'] == 'SKELETON_FIRE_SOURCE_FINITE_V1' and finite['scope'] == SCOPE, 'closed complete finite source packet')
    raw, traces, receipts, codes, authored, common, children = source_domains(finite['source_streams'], finite['source_journals'], capture, postflight, source_pins(directory), helpers)
    O.compare_rows(finite['ordinary_common'], common, range(24)); O.compare_rows(finite['passive_children'], children, O.INVISIBLE)
    need(exact(finite['source_receipts'], receipts) and exact(finite['source_code_runs'], codes) and exact(finite['helper_code_runs'], authored), 'actual source receipts and Code witnesses, without native events')
    import leaf_ledger
    need(exact(finite['raw_leaf_ledgers'], [leaf_ledger.summary(rows) for rows in raw]), 'actual disjoint raw leaf-location ledger')
    counts = {'VMs': 4, 'calls': sum(len(rows) for rows in raw), 'normal_returns': sum(row['returned'] for rows in raw for row in rows),
              'source_exceptions': sum(not row['returned'] for rows in raw for row in rows), 'ordinary_common_scalars': O.scalars(common),
              'additional_passive_child_scalars': O.scalars(children), 'source_events_each': [sum(len(t['events']) for t in calls) for calls in traces],
              'original_Code_each': [len(v) for v in codes], 'authored_helper_Code_each': [len(v) for v in authored]}
    need(exact(finite['counts'], counts), 'counts derive from actual validated source comparisons')
    return finite, provenance
