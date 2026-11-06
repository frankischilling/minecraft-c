"""Repeat guarded unchanged-original NW calls and read-only modification watchpoints."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import zipfile

from packet import HERE, JAR, digest, read_packet

ROOT = HERE.parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path,
                        default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    corpus = read_packet()
    capture = module('translation_reference_capture', HERE.parent / 'controller/capture.py')
    classfile = module('translation_reference_classfile', HERE.parent / 'controller/classfile.py')
    libraries = module('translation_reference_libraries',
                       HERE.parent / 'render_blocks_faces/capture_reference.py').LIBRARIES
    jar = args.jar.resolve(strict=True)
    capture.ensure(digest(jar) == JAR, 'verified original archive')
    archives = [(args.libraries / name).resolve(strict=True) for name in libraries]
    capture.ensure(all(digest(path) == pin for path, pin in zip(archives, libraries.values())),
                   'pinned original-compatible libraries')
    java, javac = [Path(shutil.which(name) or name).resolve(strict=True)
                   for name in (args.java, args.javac)]
    capture.ensure(java.parent == javac.parent, 'one observation JDK')
    jvm = java.parent / 'server/jvm.dll'
    if not jvm.is_file():
        jvm = java.parent.parent / 'lib/server/libjvm.so'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    debugger = output / 'debugger'
    debugger.mkdir()
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    inputs = [jar, *archives, java, javac, java.parent.parent / 'lib/modules', jvm.resolve(strict=True),
              HERE / 'NwOffsetObservation.java', HERE / 'NwOffsetJdi.java', HERE / 'profiles.tsv',
              HERE / 'declared-inputs.json', HERE / 'source-pins.json', HERE / 'packet.py',
              HERE / 'provenance.json', HERE / 'original-stdout.txt', Path(__file__).resolve(),
              HERE.parent / 'controller/capture.py', HERE.parent / 'controller/classfile.py',
              HERE.parent / 'render_blocks_faces/capture_reference.py', *java.parent.glob('*.dll')]
    before = {str(path): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'inputs_before': before, 'runs': [],
              'scope': 'Actual original singleton NW additive calls and read-only JDI stores; no Display or native implementation validation.'}
    try:
        with zipfile.ZipFile(jar) as archive:
            pins = {'client_jar_sha256': JAR, 'classes': {
                name: classfile.inspect_class(archive.read(name + '.class')) for name in ('nw', 'ge')}}
        capture.ensure(pins == json.loads((HERE / 'source-pins.json').read_text()),
                       'complete original class and Code inventory')
        for label, tool in (('java', java), ('javac', javac)):
            result = capture.capture([str(tool), '-version'], output, label, 10, environment)
            capture.require_success(result)
            text = (output / result['stdout_file']).read_text() + (output / result['stderr_file']).read_text()
            capture.ensure('17.0.17' in text, 'this observation profile requires JDK17.0.17')
            report[label + '_version'] = text
        cp = os.pathsep.join(map(str, [classes, jar, *archives]))
        compiled = capture.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi',
                                    '-cp', cp, '-d', str(classes), str(HERE / 'NwOffsetObservation.java'),
                                    str(HERE / 'NwOffsetJdi.java')], output, 'compile', 30, environment)
        capture.require_success(compiled, empty_stderr=True)
        helpers = {path.name: digest(path) for path in classes.iterdir()}
        capture.ensure(set(helpers) == {'NwOffsetObservation.class', 'NwOffsetObservation$1.class',
                                       'NwOffsetJdi.class'}, 'only authored helper classes')
        report['helpers_before'] = helpers
        command = [str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                   '-Djava.security.manager=allow', '-cp', cp, 'NwOffsetObservation', str(jar),
                   str(archives[0]), str(HERE / 'profiles.tsv')]
        raw = []
        for repeat in range(2):
            child = capture.capture(command, output, 'original-' + str(repeat), 30, environment, output)
            report['runs'].append(child)
            capture.require_success(child)
            raw.append((output / child['stdout_file']).read_bytes())
        capture.ensure(raw[0] == raw[1] == (HERE / 'original-stdout.txt').read_bytes(),
                       'two fresh byte-identical ordinary VMs match retained originals')
        child = capture.capture([str(java), '--add-modules', 'jdk.jdi', '-cp', str(classes),
                                 'NwOffsetJdi', str(debugger), cp, str(jar), str(archives[0]),
                                 str(HERE / 'profiles.tsv')], output, 'watchpoints', 90, environment, output)
        report['watchpoints'] = child
        capture.require_success(child, empty_stderr=True)
        capture.ensure((debugger / 'target.stdout').read_bytes() == raw[0],
                       'read-only JDI target matches ordinary original bytes')
        watch = (output / child['stdout_file']).read_text().splitlines()
        stores = [line.split('\t') for line in watch if line.startswith('NW_STORE\t')]
        capture.ensure(stores == corpus['stores'], 'all actual store PCs and prefixes match')
        capture.ensure(corpus['provenance']['live_code'] in watch, 'actual unchanged live method Code')
        report.update(status='PASS', profiles=36, rows=106, stores=318, target_stdout_equal=True)
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['helpers_after'] = {path.name: digest(path) for path in classes.iterdir()}
        report['inputs_unchanged'] = report['inputs_after'] == before
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    capture.ensure(report['status'] == 'PASS', 'complete unchanged original observation')
    print('PASS: 106 original NW additive calls and 318 read-only store observations; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
