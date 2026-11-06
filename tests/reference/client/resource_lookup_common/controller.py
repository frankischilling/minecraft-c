"""Source-only authored controller. Execute only after a reviewed native grant."""
import argparse
import json
import os
from pathlib import Path
import re
import sys
from protocol import PROFILES, compare, fingerprint, need, parse, validate
from capture import captured_native, load, run, source_inventory, write
from origin import authority, inspect_logs, native_origin, runtime_inputs

HERE = Path(__file__).resolve().parent
sys.dont_write_bytecode = True


def input_tree(root):
    root = root.resolve(strict=True)
    result = {}
    for path in sorted(root.rglob('*')):
        need(not path.is_symlink(), 'no input symlink alias')
        if path.is_file():
            need(path.suffix.lower() not in ('.class','.jar','.dll','.so'), 'resource root has no executable/class substitute')
            need(path.stat().st_size <= 64 * 1024 * 1024, 'bounded actual input file')
            result[path.relative_to(root).as_posix()] = fingerprint(path)
    need(result and len(result) <= 1024, 'bounded nonempty actual input tree')
    return result


def main(mode):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root','resources','directory','native','native-products','output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--profile', choices=list(PROFILES[mode]), required=True)
    product_identity = parser.add_mutually_exclusive_group(required=True)
    product_identity.add_argument('--native-products-sha256')
    product_identity.add_argument('--native-products-gate', type=Path)
    parser.add_argument('--native-only', action='store_true')
    parser.add_argument('--native-stdout', type=Path)
    parser.add_argument('--native-run-receipt', type=Path)
    parser.add_argument('--native-run-receipt-sha256')
    for name in ('game','sdk','nests','runtime'):
        parser.add_argument('--authority-' + name, type=Path)
    parser.add_argument('--natives', type=Path)
    parser.add_argument('--native-archive', type=Path)
    args = parser.parse_args()
    need((args.native_stdout is not None) == (args.native_run_receipt is not None) ==
         (args.native_run_receipt_sha256 is not None), 'replay requires separate trusted closed receipt and SHA-256')
    root, output = args.root.resolve(strict=True), args.output.resolve()
    need(not output.exists() and not output.is_relative_to(root), 'fresh output outside source tree')
    metadata = json.loads((HERE / 'provenance.json').read_text())
    metadata_pin = fingerprint(HERE / 'provenance.json')
    source_before = source_inventory(root, metadata)
    resources_before, directory_before = input_tree(args.resources), input_tree(args.directory)
    need({'lang/en_US.lang','lang/stats_US.lang','achievement/map.txt'} <= set(resources_before), 'genuine full-constructor resource inputs, not prepared-App shortcut')
    env = dict(os.environ)
    for key in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS','CLASSPATH','FREETYPE_PROPERTIES'):
        need(not env.get(key), 'ambient tool/provider injection ' + key); env.pop(key, None)
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    output.mkdir(parents=True)
    report = {'status':'RUNNING', 'mode':mode, 'profile':args.profile, 'source_root':str(root), 'source_before':source_before,
              'resources_before':resources_before, 'directory_before':directory_before,
              'original_scope':'No original process requested' if args.native_only else 'Genuine unchanged original current-host attempt',
              'numeric_ID_mapping_performed':False}
    try:
        product_sha = args.native_products_sha256
        gate_pin = None
        if args.native_products_gate is not None:
            gate_pin = fingerprint(args.native_products_gate)
            gate = json.loads(args.native_products_gate.read_text())
            need(type(gate) is dict and set(gate) == {'artifact_manifest','qualification_receipt'}, 'separate closed qualification gate')
            need(gate['artifact_manifest']['path'] == str(args.native_products.resolve(strict=True)) and
                 fingerprint(args.native_products) == gate['artifact_manifest']['pin'], 'exact gate-bound native artifact map')
            need(fingerprint(Path(gate['qualification_receipt']['path'])) == gate['qualification_receipt']['pin'], 'actual separate closed qualification receipt')
            product_sha = gate['artifact_manifest']['pin']['sha256']
            report['product_gate'] = {'path':str(args.native_products_gate.resolve()),'pin':gate_pin,'record':gate}
        proof = runtime = before = native_origin_proof = None
        if not args.native_only:
            paths = {name:getattr(args, 'authority_' + name) for name in ('game','sdk','nests','runtime')}
            need(all(path is not None for path in paths.values()), 'explicit existing authority paths')
            proof = authority(paths); runtime, before = runtime_inputs(proof)
            report['original_inputs_before'] = before
            if mode == 'texture':
                need(args.natives is not None and args.native_archive is not None, 'actual original native directory and authenticated archive')
                native_origin_proof = native_origin(args.natives.resolve(strict=True), args.native_archive)
        native_stdout, native_stderr, native_receipt = captured_native(root, args.native.resolve(strict=True),
            [args.resources.resolve(), args.directory.resolve(), args.profile], output, env,
            args.native_products.resolve(strict=True), product_sha, args.native_stdout, args.native_run_receipt, args.native_run_receipt_sha256,
            {key:report[key] for key in ('mode','profile','source_root','source_before','resources_before','directory_before')})
        if args.native_products_gate is not None:
            need(native_receipt['actual_product_map']['qualification_receipt'] == gate['qualification_receipt'], 'same gate/map qualification receipt')
        current, native_banners = parse(native_stdout); validate(current, mode, args.profile, 'native')
        (output / 'native-source-stdout.raw').write_bytes(native_banners)
        report['native'] = native_receipt
        write(output / 'native-observation.json', current)
        report['native_source_banners'] = fingerprint(output / 'native-source-stdout.raw')
        if not args.native_only:
            java_name = 'java.exe' if os.name == 'nt' else 'java'
            javac_name = 'javac.exe' if os.name == 'nt' else 'javac'
            java = Path(runtime['JDK_home']) / 'bin' / java_name
            javac = Path(runtime['JDK_home']) / 'bin' / javac_name
            for name, executable in (('java',java),('javac',javac)):
                result = run(root, [executable, '-version'], output, name + '-version', env)
                need(re.search(rb'(?:version "|javac )17\.', result.stdout + result.stderr), 'actual explicit JDK17')
            classes = output / 'classes'; classes.mkdir()
            sources = [root / 'tests/reference/client/color_lookup/ColorLookupObservation.java',
                       root / 'tests/reference/client/texture_lookup/TextureLookupObservation.java']
            jar = Path(runtime['client_jar']['path']); libraries = [Path(row['path']) for row in runtime['libraries'].values()]
            cp = os.pathsep.join(map(str, [classes, args.resources.resolve(), jar, *libraries]))
            result = run(root, [javac, '--release','17','-encoding','UTF-8','-classpath',cp,'-d',classes,*sources], output, 'javac', env)
            need(result.stderr == b'', 'actual helper compile diagnostics')
            helpers = {path.relative_to(classes).as_posix():fingerprint(path) for path in classes.rglob('*') if path.is_file()}
            need(set(helpers) == {'ColorLookupObservation.class','LookupObservation.class','TextureLookupObservation.class'}, 'authored helper classes only; no game replacements')
            options = ['-Xint','-Xverify:all','-Xmx512m','-Djava.awt.headless=' + ('true' if mode == 'color' else 'false'),
                       '-Xlog:class+load=info,class+init=info:stderr','-Xlog:library=debug:stderr']
            if mode == 'texture': options.append('-Dorg.lwjgl.librarypath=' + str(args.natives.resolve()))
            helper = 'ColorLookupObservation' if mode == 'color' else 'TextureLookupObservation'
            result = run(root, [java,*options,'-classpath',cp,helper,args.directory.resolve(),args.profile], output, 'original', env)
            original, original_banners = parse(result.stdout); validate(original, mode, args.profile, 'original')
            reader = load('actual_lookup_class_reader', HERE / 'offline_classfile.py')
            origins = inspect_logs(result.stderr, classes, [jar,*libraries], proof, reader, mode, args.profile)
            if mode == 'texture':
                paths = {row['path'] for row in origins['actual_JVM_native_loads'] if 'path' in row}
                need(native_origin_proof['actual_native'] in paths, 'actual JVM-loaded SDK native equals authenticated archive entry path')
                jdk_files = {row['path'] for row in before.values()}
                need(paths <= jdk_files | {native_origin_proof['actual_native']}, 'no unlisted original native load')
            (output / 'original-source-stdout.raw').write_bytes(original_banners)
            write(output / 'original-observation.json', original)
            need({path.relative_to(classes).as_posix():fingerprint(path) for path in classes.rglob('*') if path.is_file()} == helpers, 'compiled helpers unchanged')
            need(runtime_inputs(proof)[1] == before, 'JAR/SDK/JDK inputs unchanged')
            if mode == 'texture': need(native_origin(args.natives.resolve(),args.native_archive) == native_origin_proof, 'actual native archive/entry/file unchanged')
            report['original'] = {'capture':result.capture_report, 'helpers':helpers, 'origins':origins,
                                  'stdout':fingerprint(output / 'original.stdout'), 'stderr':fingerprint(output / 'original.stderr'),
                                  'source_banners':fingerprint(output / 'original-source-stdout.raw'),
                                  'actual_native_origin':native_origin_proof}
            if args.profile in ('statics','first-setter'):
                command = result.capture_report['command']
                need(not any(argument.startswith('-Xlog:async') for argument in command),
                     'class-init order requires actual synchronous logging')
                report['original']['first_use_order'] = {'stream':'same raw original.stderr',
                    'synchronous_logging_command':command, 'explicit_flushed_markers':True}
                need(origins['colorizer_first_use']['all_observed_uses_are_first'],
                     'actual original warm prefix retained; cold-first-use claim is unavailable')
                if args.profile == 'statics':
                    initial = [event['array'] for event in original['events'] if event['label'].endswith('-initial')]
                    need(all(value is not None and value['length'] == 65536 and value['hex'] == '00000000' * 65536 for value in initial),
                         'actual confirmed first-use class initializer zero arrays')
                    ids = {event['array']['id'] for event in original['events'] if event['label'].endswith('-set')}
                    initial_ids = [value['id'] for value in initial]
                    need(len(initial_ids) == len(set(initial_ids)) and not (set(initial_ids) & ids),
                         'actual confirmed cold class arrays are distinct')
            report['comparison'] = compare(original, current)
            need(not report['comparison']['mismatches'], 'actual observation mismatch; raw source/GPU IDs retained, never mapped')
        need(source_inventory(root, metadata) == source_before, 'source/helper/authority policy unchanged')
        need(fingerprint(HERE / 'provenance.json') == metadata_pin, 'admission metadata unchanged')
        if args.native_products_gate is not None: need(fingerprint(args.native_products_gate) == gate_pin, 'closed qualification gate unchanged')
        need(input_tree(args.resources) == resources_before and input_tree(args.directory) == directory_before, 'actual File/ZIP inputs unchanged')
        report.update(status='NATIVE_OBSERVATION_ADMITTED' if args.native_only else report['comparison']['status'], source_after=source_before,
                      input_profiles_executed=1, constructor_startup_or_playable_claim=False)
    except BaseException as error:
        report.update(status='FAIL_OR_REFUSAL', error_type=type(error).__name__, error=str(error))
        write(output / 'REPORT.json', report)
        raise
    write(output / 'REPORT.json', report)
    print(json.dumps({'status':report['status'],'report':str(output / 'REPORT.json')}))
