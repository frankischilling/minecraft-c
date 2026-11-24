"""Publish only completed, reparsed Chest predicate captures; executes no game code."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

HERE = Path(__file__).resolve().parent


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


A = module('chest_predicate_packet', HERE / 'packet.py')
C = module('chest_predicate_capture', HERE / 'capture_reference.py')
P = module('chest_predicate_projection', HERE / 'projection.py')
MODES = ('interpreter', 'default', 'trace-interpreter', 'trace-default')


def artifact(path, pin, limit=4194304):
    A.require(path.is_file() and not path.is_symlink() and path.stat().st_size <= limit, 'bounded immutable artifact: ' + path.name)
    with path.open('rb') as source:
        raw = source.read(limit + 1)
    A.require(len(raw) <= limit and pin == {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()},
              'recorded artifact extent/hash: ' + path.name)
    return raw


def process(directory, record, stem):
    A.require(type(record.get('exit')) is int and record['exit'] == 0 and record.get('timed_out') is False and
              record.get('pipe_capture_complete') is True and record.get('reader_errors') == {} and
              record.get('output_overflow') is False and type(record.get('timeout_seconds')) is int and
              1 <= record['timeout_seconds'] <= 300, 'complete bounded process: ' + stem)
    streams = {}
    for stream, limit in (('stdout', C.C.STDOUT_LIMIT), ('stderr', C.C.STDERR_LIMIT)):
        name = stem + '.' + stream
        A.require(record.get(stream + '_file') == name and record.get(stream + '_limit') == limit,
                  'exact recorded stream identity')
        streams[stream] = artifact(directory / name,
            {'bytes': record.get(stream + '_bytes'), 'sha256': record.get(stream + '_sha256')}, limit)
    return streams


def validate_transport(directory, report, cases):
    for key, stem in (('compile', 'compile'), ('java_version', 'java-version'), ('javac_version', 'javac-version')):
        streams = process(directory, report[key], stem)
        if key != 'java_version':
            A.require(not streams['stderr'], 'clean recorded compiler transport')
    for stem in ('interpreter', 'default'):
        streams = process(directory, report[stem], stem)
        A.require(streams['stderr'].decode('utf-8').splitlines() == C.expected_warnings(directory / 'classes'),
                  'only captured ordinary guard warnings')
        rows, random = C.read_output(streams['stdout'], cases, report['pins'])
        A.require(rows == report['rows'] and random == report['entropy'][stem], 'ordinary raw transport equals reported observations')
    for stem in ('trace-interpreter', 'trace-default'):
        streams = process(directory, report[stem], stem)
        A.require(not streams['stderr'], 'clean debugger transport')
        target = directory / (stem + '-jdi')
        pins = report['trace_artifacts'][stem]
        A.require(set(pins) == {'target.stdout', 'target.stderr', 'queries.ndjson', 'live-Code.json'}, 'complete debugger artifact inventory')
        raw = {name: artifact(target / name, pin) for name, pin in pins.items()}
        A.require(raw['target.stderr'].decode('utf-8').splitlines() == C.expected_warnings(directory / 'classes'),
                  'only captured traced-target warnings')
        rows, random = C.read_output(raw['target.stdout'], cases, report['pins'])
        traced, code = C.read_trace(target, cases, report['pins'])
        A.require(rows == report['rows'] and random == report['entropy'][stem] and
                  traced == report['traces'][stem] and code == report['live_Code'][stem], 'reparsed debugger evidence equals report')
        summary = f'CHEST_TRACE_COMPLETE rows={len(cases)} queries={sum(len(row["queries"]) for row in traced)} live-code={len(code)}'
        A.require(streams['stdout'].decode('utf-8').splitlines() == [summary], 'complete debugger journal receipt')
    A.require(report['traces']['trace-interpreter'] == report['traces']['trace-default'] and
              report['live_Code']['trace-interpreter'] == report['live_Code']['trace-default'], 'both actual trace modes agree')
    A.validate_rows(cases, report['rows'])
    A.validate_random(cases, report['entropy'])
    A.validate_traces(cases, report['traces']['trace-interpreter'], report['live_Code']['trace-interpreter'])


def validate_capture(directory):
    report = A.load(directory / 'capture.json')
    A.require(report['status'] == 'PASS' and report['inputs_before'] == report['inputs_after'] and
              report['compiled_before'] == report['compiled_after'], 'immutable completed capture')
    for name, pin in report['inputs_before'].items():
        A.require(A.digest(name) == pin, 'captured input still unchanged: ' + name)
    for name, pin in report['compiled_before'].items():
        A.require(A.digest(directory / name) == pin, 'captured observer product unchanged')
    for name in A.EXECUTION:
        A.require(report['inputs_before'][str((HERE / name).resolve())] == A.digest(HERE / name), 'exact captured public execution helper')
    cases = A.load(HERE / 'cases.json')['cases']
    A.require((directory / 'profiles.tsv').read_bytes() == C.declarations(cases).encode('utf-8'), 'actual predeclared argument transport')
    jars = [Path(name) for name, pin in report['inputs_before'].items() if pin == A.JAR]
    A.require(len(jars) == 1, 'one unchanged original archive')
    with zipfile.ZipFile(jars[0]) as archive:
        pins = {'client_jar_sha256': A.JAR, 'classes': {name: C.I.inspect_class(archive.read(name + '.class')) for name in C.CORE}}
    A.require(pins == report['pins'], 'reported Code inventory equals original archive')
    A.require(len(report['worlds_before']) == len(report['worlds_after']) == 4 and
              all(value == report['worlds_before'][0] for value in report['worlds_before']), 'four identical declared saved inputs')
    for index, stem in enumerate(MODES):
        before, after = report['worlds_before'][index], report['worlds_after'][index]
        A.require(C.inventory(directory / stem / 'world') == after and set(before) == set(after) and
                  all(after[name] == pin for name, pin in before.items() if name != 'session.lock') and
                  after['session.lock']['bytes'] == 8, 'only original session-lock bytes may change')
    validate_transport(directory, report, cases)
    return report, cases


def write(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    directory = args.capture.resolve(strict=True)
    report, cases = validate_capture(directory)
    write('finite.json', {'schema': 1, 'client_jar_sha256': A.JAR, 'scope': 'Genuine original direct Chest predicates; no actual block placement, Player or GUI.', 'rows': report['rows']})
    write('traces.json', {'schema': 1, 'client_jar_sha256': A.JAR, 'rows': report['traces']['trace-interpreter'],
                         'live_Code': report['live_Code']['trace-interpreter']})
    write('random.json', {'schema': 1, 'client_jar_sha256': A.JAR, 'streams': report['entropy']})
    write('source-pins.json', report['pins'])
    for name, generated in (('native-observations.h', P.header(cases, report['rows'])),
                            ('query-observations.h', P.query_header(cases, report['traces']['trace-interpreter']))):
        path = HERE / name
        if not path.exists() or P.tokens(path.read_text()) != P.tokens(generated):
            path.write_text(generated, encoding='utf-8', newline='\n')
    failed = []
    for path in sorted(directory.parent.glob('capture-*/capture.json')):
        value = A.load(path)
        if value['status'] != 'PASS':
            failed.append({'id': path.parent.name, 'sha256': A.digest(path), 'status': value['status']})
    write('provenance.json', {'schema': 1, 'client_jar_sha256': A.JAR,
        'capture': {'id': directory.name, 'sha256': A.digest(directory / 'capture.json'), 'status': 'PASS',
                    'ordinary_targets': 2, 'traced_targets': 2, 'cases_per_target': len(cases)},
        'execution_helpers': {name: A.digest(HERE / name) for name in A.EXECUTION},
        'publication_helpers': {name: A.digest(HERE / name) for name in A.PUBLICATION},
        'outputs': {name: A.digest(HERE / name) for name in A.OUTPUTS},
        'captured_input_fingerprints': report['inputs_before'], 'compiled_helpers': report['compiled_before'],
        'world_inputs': report['worlds_before'], 'world_outputs': report['worlds_after'],
        'transport': {stem: report[stem] for stem in ('compile', 'java_version', 'javac_version', *MODES)},
        'trace_artifacts': report['trace_artifacts'], 'removed_environment_names': report['removed_environment_names'],
        'failed_preparations': failed,
        'scope': 'Registered original E54, genuine FD/MX and four saved canonical LM Chunks. Directly declared arrays and Chunk i/n author inputs; no World setter or replacement body. Both ordinary and JDI modes agree.',
        'trace_scope': 'Read-only JDI on a separate loopback debugger transport; helper-only breakpoints, no original breakpoint, target method invocation or field store. FD.a entries/returns and E entries are observed, not inferred. NULL World has zero FD entries because dereference fails before callee entry.',
        'random_scope': 'All four actual seed/Gaussian streams retained; only within-process unchanged-state and sequential joins asserted. No cross-process constructor-entropy equality claim.',
        'publication_scope': 'Post-execution helpers verify process completion/bytes/hashes, reparse both ordinary streams and both target/journal streams, rederive archive pins and check saved-world inventories before any public write. No game code executes during publication.',
        'remaining': 'Actual Chest placement, Item use/consumption, TileEntity creation, audio, saved-world gameplay, GUI, native callback refusals and complete parity/FPS remain separate controls.'})
    print(A.audit())


if __name__ == '__main__':
    main()
