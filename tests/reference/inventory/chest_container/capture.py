"""Capture untouched original Chest CD/OG/HP with controlled LW arguments."""
import argparse
import base64
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
SUPPORT = ROOT / 'tests/reference/client/controller'
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
CORE = ('cd', 'og', 'lw', 'gp', 'dw', 'iz', 'gm', 'hp', 'e', 'gs', 'dc', 'js', 'ix', 'uu',
        'id', 'da', 'bb', 'bw', 'cv')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('chest_bounded_capture', SUPPORT / 'capture.py')
I = module('chest_classfile', SUPPORT / 'classfile.py')


def unique(pairs):
    result = dict(pairs)
    C.ensure(len(result) == len(pairs), 'unique JSON fields')
    return result


def declarations(cases):
    def sizes(inventory):
        return '-' if inventory is None else ','.join(map(str, inventory['sizes']))

    def value(inventory, key, default):
        return default if inventory is None else inventory[key]

    def boolean(item):
        return str(item).lower()

    def locations(inventory):
        return ('-' if inventory is None else
                ';'.join(f'{index}:{name}' for index, name in inventory['slots'].items()) or '-')

    result = []
    for case in cases:
        left, right = case['left'], case['right']
        stacks = ';'.join(':'.join(map(str, (name, *[stack[key] for key in
                           ('id', 'count', 'damage', 'animation')])))
                          for name, stack in case['stacks'].items()) or '-'
        name = '-' if case['name'] is None else base64.b64encode(case['name'].encode('utf-8')).decode('ascii')
        result.append('\t'.join(map(str, (
            case['profile'], case['target'], case['operation'], sizes(left), sizes(right),
            value(left, 'limit', 64), value(right, 'limit', 64),
            boolean(value(left, 'usable', True)), boolean(value(right, 'usable', True)),
            boolean(case['shared']), name, case['index'], case['amount'],
            boolean(case['player_null']), locations(left), locations(right), stacks,
            value(left, 'failure', 'none'), value(right, 'failure', 'none')))))
    return '\n'.join(result) + '\n'


def read_rows(raw, cases, pins):
    lines = raw.decode('utf-8', errors='strict').splitlines()
    rows, guards, origins, preamble = [], [], [], []
    for line in lines:
        if line.startswith('CHEST_CONTAINER '):
            rows.append(json.loads(line[len('CHEST_CONTAINER '):], object_pairs_hook=unique))
        elif line.startswith('CHEST_GUARD '):
            guards.append(json.loads(line[len('CHEST_GUARD '):], object_pairs_hook=unique))
        elif line.startswith('CHEST_ORIGINS '):
            origins.append(json.loads(line[len('CHEST_ORIGINS '):], object_pairs_hook=unique))
        elif line:
            preamble.append(line)
    C.ensure(preamble == ['16 achievements', '151 recipes'], 'actual original registry preamble')
    C.ensure(guards == [{'network': 0, 'native_links': 0}], 'no external operations')
    C.ensure(origins == [{'jar_sha256': JAR, 'classes': {
        name: value['class_sha256'] for name, value in pins['classes'].items()}}],
        'loaded original class resources and common source JAR match archive pins')
    C.ensure([row['profile'] for row in rows] == [case['profile'] for case in cases],
             'one ordered result for every declared case')
    return rows


def expected_warnings(classes):
    uri = classes.as_uri().replace('file:///', 'file:/') + '/'
    return [
        'WARNING: A terminally deprecated method in java.lang.System has been called',
        'WARNING: System::setSecurityManager has been called by ChestContainerObservation (' + uri + ')',
        'WARNING: Please consider reporting this to the maintainers of ChestContainerObservation',
        'WARNING: System::setSecurityManager will be removed in a future release',
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'untouched original JAR')
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    C.ensure(all(C.digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
             'pinned supplied libraries')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'same JDK17')
    jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
    tools = [java, javac, java.parent.parent / 'lib/modules', jvm]
    helpers = [HERE / name for name in ('ChestContainerObservation.java', 'cases.json', 'capture.py')]
    helpers += [SUPPORT / 'capture.py', SUPPORT / 'classfile.py']
    C.ensure(all(b'\r' not in path.read_bytes() for path in helpers if path.parent == HERE),
             'authored execution helpers use LF')
    inputs = [jar, *libraries, *tools, *helpers]
    before = {str(path): C.digest(path) for path in inputs}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    cases = json.loads((HERE / 'cases.json').read_text(encoding='utf-8'), object_pairs_hook=unique)['cases']
    declaration = output / 'declared-inputs.tsv'
    declaration.write_text(declarations(cases), encoding='utf-8', newline='\n')
    before[str(declaration)] = C.digest(declaration)
    environment = os.environ.copy()
    for name in ('APPDATA', 'TMP', 'TEMP'):
        directory = output / name.lower()
        directory.mkdir()
        environment[name] = str(directory)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before}
    try:
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '-classpath', cp,
            '-d', str(classes), str(HERE / 'ChestContainerObservation.java')], output, 'compile', 60, environment)
        C.require_success(report['compile'], empty_stderr=True)
        compiled = sorted(classes.rglob('*.class'))
        C.ensure(compiled and all(path.name.startswith('ChestContainerObservation') for path in compiled),
                 'helper output contains no original-class replacements')
        report['compiled_before'] = {str(path.relative_to(output)): C.digest(path) for path in compiled}
        with zipfile.ZipFile(jar) as archive:
            report['pins'] = {'client_jar_sha256': JAR, 'classes': {
                name: I.inspect_class(archive.read(name + '.class')) for name in CORE}}
        report['java_version'] = C.capture([str(java), '-version'], output, 'java-version', 20, environment)
        C.require_success(report['java_version'])
        report['javac_version'] = C.capture([str(javac), '-version'], output, 'javac-version', 20, environment)
        C.require_success(report['javac_version'], empty_stderr=True)
        for stem, mode in (('original', ['-Xint']), ('original-repeat', [])):
            report[stem] = C.capture([str(java), *mode, '-Xverify:all', '-Xmx256m',
                '-Djava.awt.headless=true', '-Djava.security.manager=allow', '-classpath', cp,
                'ChestContainerObservation', str(declaration)], output, stem, 60, environment)
            C.require_success(report[stem])
            C.ensure((output / (stem + '.stderr')).read_text(encoding='utf-8').splitlines() ==
                     expected_warnings(classes), 'only declared JDK17 guard warnings')
            rows = read_rows((output / (stem + '.stdout')).read_bytes(), cases, report['pins'])
            if stem == 'original':
                report['rows'] = rows
            else:
                C.ensure(rows == report['rows'], 'independent interpreter/default-VM observations agree')
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {name: C.digest(name) for name in before}
        report['compiled_after'] = {str(path.relative_to(output)): C.digest(path)
                                    for path in sorted(classes.rglob('*.class'))}
        if report['inputs_after'] != before or report.get('compiled_before') != report['compiled_after']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Chest capture artifacts: ' + str(output))
    C.ensure(report['status'] == 'PASS', 'immutable complete original capture')
    print('PASS: ' + str(len(report['rows'])) + ' original CD/OG/HP controlled-inventory cases')


if __name__ == '__main__':
    main()
