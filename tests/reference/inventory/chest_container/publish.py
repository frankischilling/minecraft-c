"""Publish completed Chest observations; this command executes no game code."""
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


A = module('chest_publication_audit', HERE / 'audit.py')
P = module('chest_publication_projection', HERE / 'projection.py')
C = module('chest_publication_capture', HERE / 'capture.py')


def captured_process(directory, record, stem):
    """Read recorded bounded transport, without starting a child process."""
    A.require(type(record) is dict and type(record.get('exit')) is int and record['exit'] == 0 and
              record.get('timed_out') is False and record.get('pipe_capture_complete') is True and
              record.get('reader_errors') == {} and record.get('output_overflow') is False,
              'completed recorded process: ' + stem)
    A.require(type(record.get('timeout_seconds')) is int and 1 <= record['timeout_seconds'] <= 300,
              'bounded recorded process: ' + stem)
    result = {}
    for stream, limit in (('stdout', C.C.STDOUT_LIMIT), ('stderr', C.C.STDERR_LIMIT)):
        name = stem + '.' + stream
        A.require(record.get(stream + '_file') == name and record.get(stream + '_limit') == limit,
                  'exact recorded stream identity: ' + name)
        path = directory / name
        A.require(path.is_file() and not path.is_symlink() and path.stat().st_size <= limit,
                  'bounded recorded stream: ' + name)
        with path.open('rb') as source:
            raw = source.read(limit + 1)
        A.require(len(raw) <= limit and type(record.get(stream + '_bytes')) is int and
                  record[stream + '_bytes'] == len(raw) and
                  record.get(stream + '_sha256') == hashlib.sha256(raw).hexdigest(),
                  'recorded stream byte count and hash: ' + name)
        result[stream] = raw
    return result


def validate_transport(directory, report, cases):
    """Bind report rows to both immutable original streams before publication."""
    for key, stem in (('compile', 'compile'), ('java_version', 'java-version'),
                      ('javac_version', 'javac-version')):
        streams = captured_process(directory, report[key], stem)
        if key in ('compile', 'javac_version'):
            A.require(not streams['stderr'], 'clean recorded compiler transport')
    parsed = []
    for stem in ('original', 'original-repeat'):
        streams = captured_process(directory, report[stem], stem)
        A.require(streams['stderr'].decode('utf-8').splitlines() ==
                  C.expected_warnings(directory / 'classes'), 'only declared original-process warnings')
        try:
            rows = C.read_rows(streams['stdout'], cases, report['pins'])
        except AssertionError as error:
            raise ValueError('invalid captured original protocol: ' + stem) from error
        A.require(rows == report['rows'], 'report rows equal captured original transport: ' + stem)
        parsed.append(rows)
    A.require(parsed[0] == parsed[1], 'both captured original processes agree')


def validate_capture(directory):
    report = A.load(directory / 'capture.json')
    A.require(report['status'] == 'PASS' and report['inputs_before'] == report['inputs_after'] and
              report['compiled_before'] == report['compiled_after'], 'complete immutable original capture')
    for path, pin in report['inputs_before'].items():
        A.require(A.digest(path) == pin, 'captured execution input still unchanged: ' + path)
    for path, pin in report['compiled_before'].items():
        A.require(A.digest(directory / path) == pin, 'compiled observer still unchanged')
    for name in A.EXECUTION:
        A.require(report['inputs_before'][str((HERE / name).resolve())] == A.digest(HERE / name),
                  'exact captured public execution helper')
    cases = A.load(HERE / 'cases.json')['cases']
    declaration = C.declarations(cases).encode('utf-8')
    A.require((directory / 'declared-inputs.tsv').read_bytes() == declaration,
              'actual captured declaration equals current pinned arguments')
    jars = [Path(path) for path, pin in report['inputs_before'].items() if pin == A.JAR]
    A.require(len(jars) == 1, 'one hash-checked original archive input')
    with zipfile.ZipFile(jars[0]) as archive:
        pins = {'client_jar_sha256': A.JAR, 'classes': {
            name: C.I.inspect_class(archive.read(name + '.class')) for name in C.CORE}}
    A.require(report['pins'] == pins, 'reported class/Code pins equal unchanged original archive')
    validate_transport(directory, report, cases)
    A.validate(cases, report['rows'])
    return report, cases


def write(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    args = parser.parse_args()
    directory = args.capture.resolve(strict=True)
    source = directory / 'capture.json'
    report, cases = validate_capture(directory)
    write('finite.json', {'schema': 1, 'client_jar_sha256': A.JAR,
        'scope': 'Untouched CD/OG/HP on controlled LW arguments; no genuine Player/Chest/World, screen initialization, drawing or gameplay qualification.',
        'rows': report['rows']})
    write('source-pins.json', report['pins'])
    header = P.header(cases, report['rows'])
    native = HERE / 'native-observations.h'
    # Keep standard formatter output if the generated C tokens are identical.
    if not native.exists() or P.tokens(native.read_text(encoding='utf-8')) != P.tokens(header):
        native.write_text(header, encoding='utf-8', newline='\n')
    artifacts = ['original.stdout', 'original.stderr', 'original-repeat.stdout', 'original-repeat.stderr',
                 'declared-inputs.tsv', 'java-version.stdout', 'java-version.stderr',
                 'javac-version.stdout', 'javac-version.stderr']
    write('provenance.json', {
        'schema': 1, 'client_jar_sha256': A.JAR,
        'scope': 'Controlled LW callbacks are declared test inputs. All CD/OG/HP/GP/DW/IZ bodies and selected class resources come from the hash-checked original JAR.',
        'capture': {'id': directory.name, 'sha256': A.digest(source), 'status': report['status'],
                    'processes': 2, 'rows_per_process': len(report['rows']),
                    'input_continuity': True, 'compiled_continuity': True,
                    'modes': ['-Xint -Xverify:all', '-Xverify:all']},
        'execution_helpers': {name: A.digest(HERE / name) for name in A.EXECUTION},
        'publication_helpers': {name: A.digest(HERE / name) for name in A.PUBLICATION},
        'supporting_helper_inputs': {Path(path).name: pin for path, pin in report['inputs_before'].items()
                                    if Path(path).parent.name == 'controller'},
        'compiled_helpers': report['compiled_before'],
        'captured_input_fingerprints': report['inputs_before'],
        'private_artifacts': {name: A.digest(directory / name) for name in artifacts},
        'java_version': (directory / 'java-version.stderr').read_text(encoding='utf-8').splitlines(),
        'javac_version': (directory / 'javac-version.stdout').read_text(encoding='utf-8').splitlines(),
        'outputs': {name: A.digest(HERE / name) for name in A.OUTPUTS},
        'observer_scope': 'Reflective field snapshots and callback records; no JDI/live bytecode instrumentation. Loaded original resource hashes and common source-JAR hash match archived class pins. No original class replacements are compiled.',
        'publication_guard_scope': 'This post-execution publisher rechecks recorded transport completion/byte counts/hashes and reparses both original streams against report rows before any public write. It also rederives selected class/Code pins from the unchanged archive. It executes no game code and changes no captured execution helper or observation.',
        'controlled_inventory_scope': 'Each declared LW owns a 64-cell test array. Its split returns the same nullable Stack without mutation; set stores directly without clamping or dirty. These are OG dispatch inputs, not original JS method observations.',
        'native_projection_scope': 'Exact CD layouts and selected Shift inputs/results/Stack aliases. Empty controlled-array tails beyond Player36 and storage27/54 are checked before omission. Includes no native acceptance claim.',
        'remaining': 'Actual JS/LW eligibility and dirty effects, genuine Player containers/publication, HP initialization/close/drawing, physical input, saved-world drift, image parity and FPS require separate controls.',
        'failed_preparations': [{'id': 'capture-01', 'status': 'FAIL',
            'capture_sha256': A.digest(directory.parent / 'capture-01/capture.json'),
            'scope': 'GM-first registry preparation failed in circular class initialization before any declared case. No result admitted.'}]
            if (directory.parent / 'capture-01/capture.json').is_file() else [],
    })
    print(A.audit())


if __name__ == '__main__':
    main()
