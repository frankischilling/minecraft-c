"""Capture both genuine PistonBase metadata-texture receivers in two guarded VMs."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


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
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    capture = module('piston_metadata_capture', HERE.parent / 'controller/capture.py')
    classfile = module('piston_metadata_classfile', HERE.parent / 'controller/classfile.py')
    libraries = module('piston_metadata_libraries',
                       HERE.parent / 'render_blocks_faces/capture_reference.py').LIBRARIES
    jar = args.jar.resolve(strict=True)
    capture.ensure(capture.digest(jar) == JAR, 'verified client archive')
    archives = [(args.libraries / name).resolve(strict=True) for name in libraries]
    capture.ensure(all(capture.digest(path) == pin for path, pin in zip(archives, libraries.values())),
                   'pinned original-compatible libraries')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    capture.ensure(java.parent == javac.parent, 'one observation JDK')
    jvm = java.parent / 'server/jvm.dll'
    if not jvm.is_file():
        jvm = java.parent.parent / 'lib/server/libjvm.so'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    inputs = [jar, *archives, java, javac, java.parent.parent / 'lib/modules', jvm,
              HERE / 'PistonTextureObservation.java', HERE / 'inputs.json', Path(__file__).resolve(),
              HERE.parent / 'controller/capture.py', HERE.parent / 'controller/classfile.py',
              HERE.parent / 'render_blocks_faces/capture_reference.py', *java.parent.glob('*.dll')]
    before = {str(path): capture.digest(path) for path in inputs}
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    report = {'status': 'FAIL', 'inputs_before': before, 'runs': [],
              'scope': 'Actual registered JQ metadata textures, ordered source bounds predicates, lazy WJ initialization and genuine live array-element changes. No staticfinal field replacement, GL or full inventory rendering.'}
    try:
        with zipfile.ZipFile(jar) as archive:
            pins = {'client_jar_sha256': JAR, 'classes': {
                name: classfile.inspect_class(archive.read(name + '.class')) for name in ('jq', 'uu', 'wj')}}
        (output / 'source-pins.json').write_text(json.dumps(pins, indent=2) + '\n', encoding='utf-8', newline='\n')
        for label, tool in (('java', java), ('javac', javac)):
            result = capture.capture([str(tool), '-version'], output, label, 10, environment)
            capture.require_success(result)
            text = (output / result['stdout_file']).read_text() + (output / result['stderr_file']).read_text()
            capture.ensure('17.0.17' in text, 'qualified JDK17.0.17')
            report[label + '_version'] = text
        cp = os.pathsep.join(map(str, [classes, jar, *archives]))
        result = capture.capture([str(javac), '-encoding', 'UTF-8', '-cp', cp, '-d', str(classes),
                                  str(HERE / 'PistonTextureObservation.java')], output, 'compile', 30, environment)
        capture.require_success(result, empty_stderr=True)
        helpers = {path.name: capture.digest(path) for path in classes.iterdir()}
        capture.ensure(set(helpers) == {'PistonTextureObservation.class', 'PistonTextureObservation$1.class'},
                       'only authored observation products')
        report['helpers_before'] = helpers
        selected, initialization = [], []
        for repeat in range(2):
            result = capture.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m',
                '-Djava.awt.headless=true', '-Djava.security.manager=allow', '-Xlog:class+init=info:stdout',
                '-cp', cp, 'PistonTextureObservation', str(jar)], output, 'original-' + str(repeat), 180,
                environment, output)
            report['runs'].append(result)
            capture.require_success(result)
            lines = (output / result['stdout_file']).read_text().splitlines()
            rows = [line for line in lines if line.startswith('JQ_')]
            capture.ensure(len(rows) == 34614, 'complete original call/input/guard inventory')
            events = [i for i, line in enumerate(lines) if "Initializing 'wj' " in line]
            before_index = lines.index('JQ_PREFIX_BEFORE 3')
            after_index = next(i for i, line in enumerate(lines) if line.startswith('JQ_PREFIX_AFTER 3 '))
            capture.ensure(len(events) == 1 and before_index < events[0] < after_index,
                           'actual WJ lazy initialization occurs only inside the fourth getter')
            initialization.append({'wj_initializations': 1, 'prefix_call': 3,
                                   'before_front_or_invalid_prefixes': False})
            selected.append(rows)
        capture.ensure(selected[0] == selected[1], 'two fresh original observation matrices agree')
        observations = [line for line in selected[0] if not line.startswith('JQ_GUARD ')]
        guards = [line for line in selected[0] if line.startswith('JQ_GUARD ')]
        (output / 'observations.tsv').write_text('\n'.join(observations) + '\n', encoding='ascii', newline='\n')
        (output / 'guards.txt').write_text('\n'.join(guards) + '\n', encoding='ascii', newline='\n')
        (output / 'initialization.json').write_text(json.dumps(initialization, indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=len(observations), texture_rows=34320,
                      initialization=initialization, source_guards=guards)
    finally:
        report['inputs_after'] = {str(path): capture.digest(path) for path in inputs}
        report['helpers_after'] = {path.name: capture.digest(path) for path in classes.iterdir()}
        report['inputs_unchanged'] = report['inputs_after'] == before
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    capture.ensure(report['status'] == 'PASS', 'complete unchanged original observations')
    print('PASS: 34,320 original PistonBase metadata textures and supplemental state/array/inventory rows; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
