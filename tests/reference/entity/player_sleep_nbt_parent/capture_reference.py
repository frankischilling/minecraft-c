"""Reproduce observations into a fresh PRIVATE directory; never rebind history.

This current tool was not the producer of the qualified original observations.
It requires the supplied archives and the pinned Windows JDK17/SDK inventory.
No Java or native execution is needed for normal packet admission or controls.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import zipfile
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
LIBS = ('org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar',
        'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar',
        'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar',
        'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


P = module('sleep_nbt_parent_packet_for_reproduction', HERE / 'packet.py')
R = module('sleep_nbt_parent_capture_reader', HERE / 'capture_reader.py')


def inventory(path):
    return {p.relative_to(path).as_posix(): P.digest(p) for p in sorted(path.rglob('*'))
            if p.is_file() and '__pycache__' not in p.parts}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo-root', 'archive', 'server-archive', 'jdk', 'libraries', 'source-declaration', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--protect-root', type=Path, action='append', default=[])
    args = parser.parse_args()
    root, out = args.repo_root.resolve(), args.output.resolve()
    P.require(not out.exists() and out != root and out != HERE, 'fresh private output')
    if out.is_relative_to(root):
        P.require(out.is_relative_to(root / '.local') and out != root / '.local', 'capture refuses public repository output')
    P.require(not out.is_relative_to(HERE), 'capture refuses packet output')
    out.mkdir(parents=True)
    started, deadline = time.monotonic(), time.monotonic() + 720
    report = {'schema': 'PLAYER_SLEEP_NBT_PARENT_REPRODUCTION_V1', 'status': 'FAIL',
              'qualification': 'new_private_observations_only_no_historical_rebinding', 'processes': []}
    inputs = before = helper_before = generated_before = None
    classes = out / 'classes'
    classes.mkdir()
    def write(name, value):
        (out / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')
    def guarded_paths(declaration):
        paths = {'source/' + name: root / name for name in declaration}
        for folder in ('src', 'include', 'tests', 'scripts', 'cmake', 'docs', '.github', 'licenses'):
            for p in sorted((root / folder).rglob('*')):
                if p.is_file() and '__pycache__' not in p.parts:
                    paths['source/' + p.relative_to(root).as_posix()] = p
        for i, base in enumerate(args.protect_root):
            base = base.resolve()
            P.require(base.is_dir() and not out.is_relative_to(base), 'existing disjoint protected root')
            for p in sorted(base.rglob('*')):
                if p.is_file() and '__pycache__' not in p.parts:
                    paths['protected/' + str(i) + '/' + p.relative_to(base).as_posix()] = p
        return paths
    try:
        P.read_reference()
        proof = P.loads((HERE / 'provenance.json').read_text(encoding='utf-8'))
        C = module('sleep_nbt_parent_bounded_capture', root / 'tests/reference/client/controller/capture.py')
        F = module('sleep_nbt_parent_classfile', root / 'tests/reference/entity/player/classfile.py')
        declaration = P.loads(args.source_declaration.read_text(encoding='utf-8'))
        P.require(type(declaration) is dict and declaration and
                  all(P.logical(n) for n in declaration) and
                  {n: P.digest(root / n) for n in declaration} == declaration, 'exact current declared source inputs')
        jdk, libraries = args.jdk.resolve(), args.libraries.resolve()
        java, javac = jdk / 'bin/java.exe', jdk / 'bin/javac.exe'
        sdk = [libraries / name for name in LIBS]
        def binaries():
            return {**{'jdk/' + p.relative_to(jdk).as_posix(): p for folder in ('bin', 'lib')
                       for p in sorted((jdk / folder).rglob('*')) if p.is_file()},
                    **{'sdk/' + p.name: p for p in sdk}}
        binary = binaries()
        P.require({n: P.digest(p) for n, p in binary.items()} == proof['execution_binary_input_sha256'],
                  'same complete pinned152 binary inventory; changed tools need separate qualification')
        archive, server = args.archive.resolve(), args.server_archive.resolve()
        P.require(P.digest(archive) == proof['execution_input_sha256']['archive/client.jar'] and
                  P.digest(server) == proof['execution_input_sha256']['archive/server.jar'], 'unchanged supplied archives')
        source = {'archive_sha256': P.digest(archive), 'classes': {}}
        with zipfile.ZipFile(archive) as z:
            for name in sorted(z.namelist()):
                if name.endswith('.class'):
                    value = F.parse_class(z.read(name))
                    source['classes'][value['name']] = value
        write('source-pins.json', source)
        fixed = guarded_paths(declaration)
        inputs = {**fixed, **binary, 'archive/client.jar': archive, 'archive/server.jar': server,
                  'declaration/source.json': args.source_declaration.resolve(),
                  **{'packet/' + p.name: p for p in HERE.iterdir() if p.is_file()}}
        before = {n: P.digest(p) for n, p in inputs.items()}
        report['input_before_sha256'] = before
        env = os.environ.copy()
        for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
            env.pop(name, None)
        def run(command, name, limit):
            P.require(time.monotonic() + limit + 30 < deadline, 'whole-run cleanup margin')
            result = C.capture(command, out, name, limit, env)
            report['processes'].append(result)
            C.require_success(result, empty_stderr=name not in ('java-version', 'javac-version'))
        run([str(java), '-version'], 'java-version', 15)
        run([str(javac), '-version'], 'javac-version', 15)
        cp = os.pathsep.join(map(str, (classes, archive, *sdk)))
        run([str(javac), '--add-modules', 'jdk.jdi', '-encoding', 'UTF-8', '-classpath', cp,
             '-d', str(classes), str(HERE / 'PlayerNbtSleepMatrixObservation.java'),
             str(HERE / 'PlayerNbtSleepMatrixJdi.java')], 'compile', 30)
        helper_before = inventory(classes)
        P.require(helper_before == proof['execution_helper_sha256'], 'actual same compiled helper identities')
        helper = {F.parse_class(p.read_bytes())['name']: F.parse_class(p.read_bytes()) for p in classes.glob('*.class')}
        write('helper-pins.json', helper)
        method_lines = []
        for name, owner in {**source['classes'], **helper}.items():
            for method in owner['methods']:
                if 'code_sha256' in method:
                    method_lines.append(f"{name} {method['name']} {method['descriptor']} {method['code_length']} {method['code_sha256']}")
        (out / 'method-pins.tsv').write_text('\n'.join(method_lines) + '\n', encoding='ascii', newline='\n')
        generated_before = {n: P.digest(out / n) for n in ('source-pins.json', 'helper-pins.json', 'method-pins.tsv')}
        report['helper_before_sha256'], report['generated_before_sha256'] = helper_before, generated_before
        names = (HERE / 'class-names.txt').read_text(encoding='ascii').splitlines()
        projections = []
        for vm in range(2):
            run([str(java), '--add-modules', 'jdk.jdi', '-Xmx256m', '-classpath', cp,
                 'PlayerNbtSleepMatrixJdi', str(jdk), cp, str(archive), str(HERE / 'class-names.txt'),
                 str(out / 'method-pins.tsv'), str(HERE / 'profiles.tsv'), str(out / ('vm-' + str(vm)))],
                'controller-' + str(vm), 240)
            value = R.projection(out / ('vm-' + str(vm)), source, helper, names)
            write('projection-' + str(vm) + '.json', value)
            projections.append(value)
        P.require(P.exact(projections[0], projections[1]), 'two actual fresh original projections agree')
        write('finite.json', projections[0])
        report['status'] = 'PASS_NEW_ORIGINAL_REPRODUCTION_ONLY'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        if inputs is not None:
            after_paths = {**guarded_paths(declaration), **binaries(),
                           **{n: p for n, p in inputs.items() if not n.startswith(('source/', 'protected/', 'jdk/', 'sdk/'))}}
            report['input_after_sha256'] = {n: P.digest(p) for n, p in after_paths.items()}
            report['all_inputs_unchanged'] = before == report['input_after_sha256']
        if helper_before is not None:
            report['helper_after_sha256'] = inventory(classes)
            report['helpers_unchanged'] = helper_before == report['helper_after_sha256']
        if generated_before is not None:
            report['generated_after_sha256'] = {n: P.digest(out / n) for n in generated_before}
            report['generated_unchanged'] = generated_before == report['generated_after_sha256']
        report['wall_seconds'] = time.monotonic() - started
        report['all_recorded_processes_terminal'] = all(p['exit'] is not None and p['pipe_capture_complete'] for p in report['processes'])
        if report.get('all_inputs_unchanged') is not True or report.get('helpers_unchanged') is not True or report.get('generated_unchanged') is not True or report['wall_seconds'] > 720:
            report['status'] = 'FAIL'
        write('capture.json', report)
    P.require(report['status'] == 'PASS_NEW_ORIGINAL_REPRODUCTION_ONLY', 'new reproduction/guards failed; receipt retained')
    print('Private reproduction receipt:', out / 'capture.json')


if __name__ == '__main__':
    main()
