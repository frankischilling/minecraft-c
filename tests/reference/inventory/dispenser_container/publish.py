"""Pure retained-evidence readback/publication; never starts a process or repairs source."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('dispenser_public_audit', HERE / 'audit.py')
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)
M = A.M


def physical(path, expected):
    M.equal(M.pin(path), expected, 'Retained actual bytes: ' + Path(path).name)


def transport(value, directory):
    M.equal(value['exit'], 0, 'Actual natural exit0')
    for key, expected in {'timed_out': False, 'output_overflow': False, 'pipe_capture_complete': True,
                          'reader_errors': {}}.items():
        M.equal(value[key], expected, 'Complete actual child ' + key)
    M.need(type(value['command']) is list and value['command'] and all(type(v) is str for v in value['command']), 'Actual command argv')
    M.need(type(value['timeout_seconds']) is int and 0 < value['timeout_seconds'] <= 60, 'Bounded deadline')
    streams = {}
    for name, maximum in (('stdout', 4194304), ('stderr', 1048576)):
        filename = value[name + '_file']
        M.need(type(filename) is str and Path(filename).name == filename and '/' not in filename and '\\' not in filename,
               'Local raw stream receipt filename')
        path = directory / filename
        physical(path, {'bytes': value[name + '_bytes'], 'sha256': value[name + '_sha256']})
        M.equal(value[name + '_limit'], maximum, 'Actual declared stream bound')
        M.need(type(value[name + '_bytes']) is int and 0 <= value[name + '_bytes'] <= maximum, 'Complete bounded raw stream')
        streams[name] = M.bounded(path, maximum)
    return streams


def public_package(run):
    """Bind the actual captured package to this verifier, allowing byte-identical relocation."""
    sources = [Path(path) for path, expected in run['inputs_before'].items()
               if Path(path).name == 'DispenserContainerObservation.java' and expected['sha256'] == A.JAVA_SHA]
    M.need(len(sources) == 1, 'One actual measured public Java source')
    package = sources[0].parent
    A.audit(package)
    manifest = A.load(package / 'packet.json')
    M.equal(manifest, A.load(HERE / 'packet.json'), 'Captured public manifest equals this frozen verifier package')
    for name in A.FILES:
        expected = M.pin(HERE / name)
        M.equal(M.pin(package / name), expected, 'Captured public file equals this verifier: ' + name)
        M.equal(run['inputs_before'][str(package / name)], expected, 'Actual captured public input equals verifier: ' + name)
    return package, manifest


def original(directory, preflight=None):
    directory = Path(directory).resolve(strict=True)
    run = A.load(directory / 'run.json')
    M.equal(run['status'], 'PASS_ORIGINAL_COMPONENT_ONLY', 'Completed original cohort')
    M.equal(run['work'], str(directory), 'Actual retained original capture directory')
    M.equal(run['scope'], M.SCOPE, 'Bounded actual original scope')
    M.equal(run['inputs_before'], run['inputs_after'], 'Original input bracket')
    public = run.get('schema') == 'dispenser_original_capture_v2'
    captured_public = public_package(run) if public else None
    for path, expected in run['inputs_before'].items():
        physical(Path(path), expected)
    attempts = run['attempts']
    M.equal([v['name'] for v in attempts], ['compile', 'interpreter', 'default'], 'One compile/two actual fresh VM attempts')
    M.equal(run['target_schedule'], ['interpreter', 'default'], 'Actual VM mode order')
    M.equal(run['hidden_retries'], 0, 'No hidden retries')
    if public:
        for options in [run['environment_options'], *[item['environment_options'] for item in attempts]]:
            M.equal(set(options), set(A.OPTIONS), 'Actual five-option public child environment')
            for value in options.values():
                M.closed(value, ('present', 'empty'), 'Actual environment presence/empty flags')
                M.need(type(value['present']) is bool and value['empty'] is True, 'All five actual options empty')
    else:
        M.need(preflight is not None, 'Historical privateV1 requires its actual same-env fifth-option supplement')
        value = A.load(preflight)
        M.equal(set(value['implicit_options']), set(A.OPTIONS), 'Five measured historical options')
        for option in value['implicit_options'].values():
            M.equal(option, {'present': False, 'empty': True}, 'Actual historical option preflight')
        controller = [Path(p) for p in run['inputs_before'] if Path(p).name == 'capture.py' and
                      M.pin(p)['sha256'] == value['controller_sha256']]
        M.need(len(controller) == 1 and str(controller[0]) in value['argv'], 'Supplement joins actual historical controller bytes/argv')
        physical(Path(preflight), A.load(HERE / 'finite.json')['historical_lineage']['original']['supplemental_actual_same_environment_preflight'])
    products = run['products_before_targets']
    M.equal(run['products_after'], products, 'Original final product bracket')
    M.equal(sorted(products), ['DispenserContainerObservation$Guard.class', 'DispenserContainerObservation.class'], 'Only authored products')
    actual_products = {str(path.relative_to(directory / 'classes')): M.pin(path)
                       for path in sorted((directory / 'classes').rglob('*')) if path.is_file()}
    M.equal(actual_products, products, 'Complete physical authored class inventory')
    pins = A.load(HERE / 'source-pins.json')
    jars = [Path(path) for path, expected in run['inputs_before'].items() if expected == pins['jar']]
    M.need(len(jars) == 1, 'Exactly one actual original archive')
    parsers = [Path(path) for path, expected in run['inputs_before'].items() if Path(path).name == 'classfile.py' and
               expected == A.load(HERE / 'packet.json')['shared_inputs']['tests/reference/client/controller/classfile.py']]
    M.need(len(parsers) == 1, 'Actual pinned pure Class/Code parser')
    spec = importlib.util.spec_from_file_location('dispenser_archive_identity', parsers[0])
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    with zipfile.ZipFile(jars[0]) as archive:
        for name, expected in pins['classes'].items():
            M.equal(parser.inspect_class(archive.read(name + '.class')), expected, 'Actual supplied original Class/Code identity')
    declared = M.bounded(HERE / 'cases.tsv')
    observed = []
    compile_command = attempts[0]['command']
    M.need(len(compile_command) == 9 and Path(compile_command[0]).name == 'javac.exe' and
           compile_command[1:5] == ['-g:none', '-encoding', 'UTF-8', '-classpath'] and
           compile_command[6:8] == ['-d', str(directory / 'classes')], 'Actual fixed compiler invocation')
    java_sources = [Path(path) for path, expected in run['inputs_before'].items()
                    if Path(path).name == 'DispenserContainerObservation.java' and expected['sha256'] == A.JAVA_SHA]
    M.need(len(java_sources) == 1 and compile_command[8] == str(java_sources[0]), 'Actual measured unchanged Java input')
    cp = compile_command[5].split(';')
    M.need(len(cp) == 6 and cp[:2] == [str(directory / 'classes'), str(jars[0])], 'Closed actual Windows classpath')
    for path, expected in zip(cp[2:], M.LIBRARY_SHAS):
        M.equal(M.pin(path)['sha256'], expected, 'Actual ordered SDK archive')
    if public:
        package, public_manifest = captured_public
        shared = []
        for relative, expected in public_manifest['shared_inputs'].items():
            matches = [Path(path) for path, value in run['inputs_before'].items()
                       if value == expected and str(Path(path)).replace('\\', '/').endswith(relative)]
            M.need(len(matches) == 1, 'Exact actual declared shared dependency')
            shared.append(matches[0])
        jdk = Path(compile_command[0]).parent.parent
        required = [package / name for name in A.FILES] + shared + [jars[0]] + [Path(path) for path in cp[2:]]
        required += [jdk / name for name in ('bin/java.exe', 'bin/javac.exe', 'bin/server/jvm.dll', 'lib/modules', 'release')]
        M.equal(run['inputs_before'], {str(path): M.pin(path) for path in required}, 'Closed fresh public actual input inventory')
    for index, item in enumerate(attempts):
        stem = item['name']
        M.equal(item['inputs_before'], run['inputs_before'], 'Actual attempt pre-input bracket')
        M.equal(item['inputs_after'], run['inputs_before'], 'Actual attempt post-input bracket')
        for key, expected in {'owned_process_reaped': True, 'owned_process_returncode': 0,
                              'cleanup_kill_attempted': False, 'cleanup_errors': []}.items():
            M.equal(item[key], expected, 'Actual owned process closure')
        M.need(type(item['pid']) is int and item['pid'] > 0, 'Actual owned process handle PID')
        receipt = A.load(directory / (stem + '.capture.json'))
        M.equal(receipt, item['process_receipt'], 'Physical actual capture receipt')
        M.equal(receipt['command'], item['command'], 'Command receipt join')
        raw = transport(receipt, directory)
        if index == 0:
            M.equal(item['products_before'], {}, 'Fresh compiler directory')
            M.equal(raw, {'stdout': b'', 'stderr': b''}, 'Clean actual compile')
        else:
            M.equal(item['products_before'], products, 'Target product pre-bracket')
            command = [str(Path(compile_command[0]).with_name('java.exe'))]
            if index == 1:
                command.append('-Xint')
            command += ['-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true', '-Djava.security.manager=allow',
                        '-classpath', compile_command[5], 'DispenserContainerObservation']
            M.equal(item['command'][:-1], command, 'Exact actual fresh VM command')
            declaration = Path(item['command'][-1])
            M.equal(declaration.read_bytes(), declared, 'Actual declared cases, never rewritten')
            M.equal(raw['stderr'].decode('utf8').splitlines(), M.warnings(directory / 'classes'), 'Exact actual guard warnings')
            observed.append(M.read_rows(raw['stdout'], declared, pins, jars[0]))
        M.equal(item['products_after'], products, 'Actual attempt product post-bracket')
    M.equal(observed[0], observed[1], 'Full original VM fields/Throwable equality')
    M.equal(observed[0], run['rows'], 'Every raw original field joins summary')
    return observed[0], public


def archive_readback(manifest_path, archive_path, expected=None):
    manifest = A.load(manifest_path)
    declared = manifest['files']
    M.need(type(declared) is dict and 0 < len(declared) <= 20000, 'Bounded retained archive inventory')
    wanted = manifest.get('sha256', manifest.get('archive_sha256'))
    M.equal(M.pin(archive_path)['sha256'], wanted, 'Actual retained archive digest')
    mapped = {}
    total = 0
    with zipfile.ZipFile(archive_path) as archive:
        info = archive.infolist()
        M.equal(len(info), len(declared), 'No extra or duplicate archive members')
        M.equal({item.filename for item in info}, set(declared), 'Complete archive member join')
        for item in info:
            path = PurePosixPath(item.filename)
            M.need(not path.is_absolute() and '..' not in path.parts and '\\' not in item.filename and not item.is_dir(), 'Safe nonextracting archive member')
            value = declared[item.filename]
            M.need(0 <= item.file_size == value['bytes'] <= 256 * 1024 * 1024, 'Bounded actual member size')
            total += item.file_size
            M.need(total <= 4 * 1024 * 1024 * 1024, 'Bounded total archive bytes')
            sha = hashlib.sha256()
            count = 0
            with archive.open(item) as stream:
                while True:
                    block = stream.read(65536)
                    if not block:
                        break
                    count += len(block)
                    M.need(count <= value['bytes'], 'No member overread')
                    sha.update(block)
            M.equal({'bytes': count, 'sha256': sha.hexdigest()}, {k: value[k] for k in ('bytes', 'sha256')}, 'Actual full archive member bytes')
            name = str(PureWindowsPath(value['original_path'])).casefold()
            if name in mapped:
                M.equal(mapped[name], {k: value[k] for k in ('bytes', 'sha256')}, 'Consistent repeated actual input')
            mapped[name] = {k: value[k] for k in ('bytes', 'sha256')}
    if expected is not None:
        for name, value in expected.items():
            M.equal(mapped[str(PureWindowsPath(name)).casefold()], value, 'Every actual source/compiler/link/product input archived')
    return len(declared)


def native(directory, rows):
    directory = Path(directory).resolve(strict=True)
    receipt = A.load(directory / 'receipt.json')
    M.equal(receipt['status'], 'PASS_NATIVE_V2_COMPONENT_PROCESS_INPUT_CLOSURE', 'Completed actual native V2 receipt')
    M.equal(receipt['failure'], None, 'No failed native transport')
    M.equal(receipt['before_after_equal'], True, 'Native full bracket agreement')
    M.need(len(receipt['closure']) == 1 and type(receipt['closure'][0]['pid']) is int and
           receipt['closure'][0]['pid'] > 0, 'One actual native owned handle')
    for key, expected in {'kill_requested': False, 'exit': 0}.items():
        M.equal(receipt['closure'][0][key], expected, 'Actual natural native closure')
    M.need('cleanup_error' not in receipt['closure'][0], 'No owned handle cleanup error')
    raw = transport(receipt['capture'], directory)
    M.equal(receipt['capture'], A.load(directory / 'native.capture.json'), 'Native physical raw capture receipt')
    M.equal(receipt['argv'], receipt['capture']['command'], 'Actual native argv join')
    before, after = A.load(directory / 'before.json'), A.load(directory / 'after.json')
    M.equal(before, after, 'Actual native source/compiler/link/product brackets')
    M.equal(len(before['source']), receipt['source_files'], 'Actual source closure extent')
    M.equal(len(before['compile_link_product_inputs']), receipt['compile_link_product_inputs'], 'Actual compiler/link extent')
    combined = dict(before['source'])
    combined.update(before['compile_link_product_inputs'])
    archive_readback(directory / 'source-and-build-inputs-v2.json', directory / 'source-and-build-inputs-v2.zip', combined)
    archive_readback(directory / 'sdk-dll-products-companion-v1.json', directory / 'sdk-dll-products-companion-v1.zip')
    M.need(receipt['argv'][0] in before['compile_link_product_inputs'], 'Actual executed product belongs to actual input ledger')
    summary = M.join(rows, raw['stdout'], raw['stderr'])
    return raw, summary


def verify_historical(original_dir, native_dir, preflight):
    data = A.load(HERE / 'finite.json')
    # Immutable qualified receipt roots are checked before trusting their
    # contents, rather than accepting repaired summaries with omitted inputs.
    physical(Path(original_dir) / 'run.json', data['historical_lineage']['original']['actual_artifacts']['run_json'])
    native_names = {'stdout': 'native.stdout', 'stderr': 'native.stderr', 'process_receipt': 'receipt.json',
        'semantic_join': 'semantic-join-v2.json', 'root_audit': 'root-audit-v2.json',
        'before_manifest': 'before.json', 'after_manifest': 'after.json'}
    for role, expected in data['historical_lineage']['native_v2']['actual_artifacts'].items():
        physical(Path(native_dir) / native_names[role], expected)
    rows, public = original(original_dir, preflight)
    M.equal(public, False, 'Historical publication uses privateV1 with its measured supplement, not rebinding public tools')
    M.equal(rows, data['original_rows'], 'Every actual original field retained publicly')
    raw, summary = native(native_dir, rows)
    # Exact field comparisons against separately parsed actual native rows;
    # reserialization is only an offline schema representation, not raw proof.
    actual_rows = [M.parse(line[len('SH_NATIVE_ROW '):]) for line in raw['stdout'].decode('utf8').splitlines() if line.startswith('SH_NATIVE_ROW ')]
    actual_details = [M.parse(line[len('SH_NATIVE_DETAIL '):]) for line in raw['stderr'].decode('utf8').splitlines() if line.startswith('SH_NATIVE_DETAIL ')]
    actual_retirements = [M.parse(line[len('SH_NATIVE_RETIRE '):]) for line in raw['stderr'].decode('utf8').splitlines() if line.startswith('SH_NATIVE_RETIRE ')]
    for key, value in (('native_rows', actual_rows), ('native_details', actual_details), ('native_retirements', actual_retirements)):
        M.equal(data[key], value, 'Every public actual native field ' + key)
    lineage = data['historical_lineage']
    for role, expected in lineage['original']['actual_artifacts'].items():
        stem, stream = role.rsplit('_', 1)
        physical(Path(original_dir) / (stem + '.' + stream), expected)
    names = {'stdout': 'native.stdout', 'stderr': 'native.stderr', 'process_receipt': 'receipt.json',
             'semantic_join': 'semantic-join-v2.json', 'root_audit': 'root-audit-v2.json',
             'before_manifest': 'before.json', 'after_manifest': 'after.json'}
    for role, expected in lineage['native_v2']['actual_artifacts'].items():
        physical(Path(native_dir) / names[role], expected)
    for group, manifest, archive in (('retained_archive', 'source-and-build-inputs-v2.json', 'source-and-build-inputs-v2.zip'),
                                    ('sdk_owned_companion', 'sdk-dll-products-companion-v1.json', 'sdk-dll-products-companion-v1.zip')):
        physical(Path(native_dir) / manifest, lineage['native_v2'][group]['manifest'])
        physical(Path(native_dir) / archive, lineage['native_v2'][group]['archive'])
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--native', type=Path)
    parser.add_argument('--historical-env-preflight', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify-fresh-original', action='store_true')
    args = parser.parse_args()
    A.audit()
    if args.verify_fresh_original:
        rows, public = original(args.original)
        M.equal(public, True, 'New public reproducer execution remains separately labeled')
        M.equal(rows, A.load(HERE / 'finite.json')['original_rows'], 'Literal fresh-original vs historical field comparison')
        with args.output.open('x', encoding='utf8', newline='\n') as stream:
            json.dump({'status': 'PASS_FRESH_PUBLIC_ORIGINAL_REPRODUCTION_ONLY', 'historical_dataset_rebound': False,
                       'rows': 36, 'native_current_qualification': False}, stream, indent=2)
            stream.write('\n')
        return
    M.need(args.native is not None and args.historical_env_preflight is not None, 'Historical publication requires native archive/raw evidence and actual environment supplement')
    summary = verify_historical(args.original, args.native, args.historical_env_preflight)
    args.output.mkdir(parents=True, exist_ok=False)
    # Publication copies only the closed authored source/data manifest, after
    # all evidence gates pass. It does not replace a public directory or source.
    for name in A.FILES:
        shutil.copyfile(HERE / name, args.output / name)
    A.audit(args.output)
    print(json.dumps({'status': 'PASS_PURE_HISTORICAL_PUBLICATION_READBACK', 'files': len(A.FILES),
          'original_cases': 18, 'native_cases': 16, 'native_summary': summary,
          'public_controller_executed_in_historical_runs': False}, indent=2))


if __name__ == '__main__':
    main()
