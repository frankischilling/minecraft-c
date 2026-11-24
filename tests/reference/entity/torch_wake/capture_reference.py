"""Capture two unchanged original Torch wake calls in each of two fresh JDK17 VMs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
import packet as W

P = W.P
FOOTER = 'TORCH_WAKE_COMPLETE cases=2 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0'


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def write(path, value):
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(value, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--java-bin', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    private = W.ROOT / '.local'
    P.require(output.is_relative_to(private) and output != private, 'new private capture directory')
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    print('Retaining original Torch wake attempts in ' + str(output), flush=True)
    report = {'status': 'FAIL', 'original': []}
    inputs, before, helpers, names_file, names_pin, class_pin = [], None, None, None, None, None
    observations = []

    def helper_pins():
        return {path.name: digest(path) for path in sorted(classes.iterdir())}

    try:
        jar = args.client_jar.resolve(strict=True)
        tools = []
        for name in ('java', 'javac', 'javap'):
            executable = name + ('.exe' if os.name == 'nt' else '')
            path = args.java_bin / executable if args.java_bin is not None else shutil.which(name)
            P.require(path is not None, 'available JDK tool ' + name)
            tools.append(Path(path).resolve(strict=True))
        java, javac, javap = tools
        P.require(java.parent.samefile(javac.parent) and java.parent.samefile(javap.parent), 'same JDK bin directory')
        vm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        libraries = [(args.library_root / name).resolve(strict=True) for name in P.LIBRARIES]
        binaries = {'client': jar, 'java': java, 'javac': javac, 'javap': javap, 'jvm': vm,
            'modules': java.parent.parent / 'lib/modules', 'python': Path(sys.executable).resolve(strict=True),
            **dict(zip(P.LIBRARIES, libraries))}
        inputs = list(dict.fromkeys([*binaries.values(), *W.SHARED.values(),
                                     *(W.HERE / name for name in W.SOURCES)]))
        before = {str(path): digest(path) for path in inputs}
        report['input_before_sha256'] = before
        P.require(before[str(jar)] == P.JAR and all(before[str(path)] == expected for path, expected
                  in zip(libraries, P.LIBRARIES.values())), 'original client and SDK archive hashes')
        capture = W.module('torch_wake_bounded_capture', W.SHARED['capture'])
        classfile = W.module('torch_wake_original_classfile', W.SHARED['classfile'])
        pins = W.class_pins()
        P.require(pins['archive'] == {'sha256': P.JAR, 'bytes': jar.stat().st_size} and
                  len(pins['classes']) == 31, 'original archive and class inventory')
        methods = W.method_pins()
        with zipfile.ZipFile(jar) as archive:
            names = archive.namelist()
            P.require(len(names) == len(set(names)) and not set(names) & W.HELPERS,
                      'unique archive entries and helper names')
            for name, expected in pins['classes'].items():
                P.require(P.exact(classfile.parse_class(archive.read(name + '.class')), expected),
                          'unchanged original class and Code ' + name)
        write(output / 'class-pins.json', pins)
        class_pin = digest(output / 'class-pins.json')
        names_file = output / 'class-names.txt'
        names_file.write_text('\n'.join(pins['classes']) + '\n', encoding='ascii', newline='\n')
        names_pin = digest(names_file)
        report['class_names_before_sha256'] = names_pin
        report['class_pins_before_sha256'] = class_pin
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
            environment.pop(key, None)
        for label, tool in (('java', java), ('javac', javac), ('javap', javap)):
            receipt = capture.capture([str(tool), '-version'], output, label, 15, environment)
            report[label] = receipt
            capture.require_success(receipt)
            version = ''.join((output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            P.require(re.search(r'(?:version "|javac |^)(17\.)', version, re.MULTILINE), 'JDK17 tool ' + label)
        report['bytecode'] = capture.capture([str(javap), '-p', '-c', '-s', '-classpath', str(jar),
            'vm', 've', 'gs', 'uu', 'hl'], output, 'bytecode', 20, environment)
        capture.require_success(report['bytecode'], empty_stderr=True)
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        report['compile'] = capture.capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath,
            '-d', str(classes), str(W.HERE / 'TorchWakeObservation.java'), str(W.SHARED['setup']),
            str(W.SHARED['json'])], output, 'compile', 45, environment)
        capture.require_success(report['compile'], empty_stderr=True)
        helpers = helper_pins()
        P.require(set(helpers) == W.HELPERS, 'only declared authored helpers compiled')
        report['helpers_before_sha256'] = helpers
        bindings = ['CLASS ' + name + ' ' + str(value['class_bytes']) + ' ' + value['class_sha256']
                    for name, value in pins['classes'].items()]
        for index in range(2):
            P.require(before == {str(path): digest(path) for path in inputs} and helpers == helper_pins() and
                      digest(names_file) == names_pin and digest(output / 'class-pins.json') == class_pin,
                      'input identities before VM entry')
            work = output / f'work-{index}'
            work.mkdir()
            receipt = capture.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                '--add-opens=java.base/java.lang=ALL-UNNAMED', '-classpath', classpath,
                'TorchWakeObservation', str(jar), str(work), str(names_file)],
                output, f'original-{index}', 70, environment, work)
            report['original'].append(receipt)
            capture.require_success(receipt)
            lines = (output / receipt['stdout_file']).read_text(encoding='utf-8').splitlines()
            prefix = bindings + ['16 achievements', '151 recipes']
            P.require(lines[:len(prefix)] == prefix and len(lines) == len(prefix) + 3 and lines[-1] == FOOTER,
                      'actual class bindings, ordered output and effect guards')
            selected = lines[len(prefix):-1]
            P.require(all(line.startswith('TORCH_WAKE ') for line in selected), 'two actual original rows')
            rows = [P.loads(line.removeprefix('TORCH_WAKE ')) for line in selected]
            W.validate(rows)
            observations.append(rows)
        P.require(P.exact(*observations), 'two fresh VMs agree on every selected field')
        P.require(all(report['original'][0][key] == report['original'][1][key] for key in
                      ('stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256')),
                  'two identical complete original streams')
        report['status'] = 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY'
    except Exception as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['input_after_sha256'] = {str(path): digest(path) for path in inputs}
        report['helpers_after_sha256'] = helper_pins()
        report['class_names_after_sha256'] = digest(names_file) if names_file is not None else None
        report['class_pins_after_sha256'] = digest(output / 'class-pins.json') if class_pin is not None else None
        report['inputs_unchanged'] = before is not None and before == report['input_after_sha256']
        report['helpers_unchanged'] = helpers is not None and helpers == report['helpers_after_sha256']
        report['class_names_unchanged'] = names_pin is not None and names_pin == report['class_names_after_sha256']
        report['class_pins_unchanged'] = class_pin is not None and class_pin == report['class_pins_after_sha256']
        unchanged = all(report[key] for key in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged',
                                               'class_pins_unchanged'))
        passed = report['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY'
        if not unchanged:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
        if passed:
            P.require(unchanged, 'input/helper/class-name preservation')
    write(output / 'finite.json', {'schema': W.SCHEMA, 'scope': W.SCOPE, 'rows': observations[0]})
    proof = {'schema': W.PROVENANCE, 'status': report['status'], 'archive_sha256': P.JAR,
        'finite_sha256': digest(output / 'finite.json'), 'capture_sha256': digest(output / 'capture.json'),
        'sources': W.sources(), 'shared': W.shared(), 'methods': methods, 'helpers': helpers,
        'binaries': {name: digest(path) for name, path in binaries.items()}, 'class_count': 31,
        'rows_per_vm': 2, 'original': [W.public_receipt(receipt) for receipt in report['original']],
        **{key: report[key] for key in ('inputs_unchanged', 'helpers_unchanged', 'class_names_unchanged',
                                       'class_pins_unchanged')}}
    write(output / 'provenance.json', proof)
    W.read_reference(output)
    print('Two original Torch wake captures pass: ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
