"""Capture two unchanged client calls in each of two fresh JDK 17 VMs."""
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
FOOTER = 'FOOT_WAKE_COMPLETE cases=2 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0'
HELPERS = {'ConstructorJson.class', 'FootWakeObservation.class',
           'FootWakeObservation$Observer.class', 'PlayerDamageWakeObservation.class',
           'PlayerDamageWakeObservation$Observer.class', 'PlayerDamageWakeObservation$Guard.class'}


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
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    private = W.ROOT / '.local'
    P.require(output.is_relative_to(private) and output != private, 'new private capture directory')
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    print('Retaining original Bed foot wake attempts in ' + str(output), flush=True)
    report = {'status': 'FAIL'}
    inputs = []
    before = None
    helpers = None

    def helper_pins():
        return {path.name: digest(path) for path in sorted(classes.iterdir())}

    try:
        jar = args.client_jar.resolve(strict=True)
        java, javac, javap = [Path(shutil.which(name)).resolve(strict=True)
                              for name in ('java', 'javac', 'javap')]
        P.require(java.parent.samefile(javac.parent) and java.parent.samefile(javap.parent), 'same JDK bin directory')
        vm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        libraries = [(args.library_root / name).resolve(strict=True) for name in P.LIBRARIES]
        shared = W.shared()
        sources = W.sources()
        inputs = [jar, java, javac, javap, vm, java.parent.parent / 'lib/modules', *libraries,
                  *W.SHARED.values(), *(W.HERE / name for name in W.SOURCES)]
        before = {str(path): digest(path) for path in inputs}
        report['input_before_sha256'] = before
        P.require(before[str(jar)] == P.JAR and all(before[str(path)] == expected for path, expected
                  in zip(libraries, P.LIBRARIES.values())), 'original client and SDK archive hashes')
        capture = W.module('bed_foot_wake_capture', W.SHARED['capture'])
        classfile = W.module('bed_foot_wake_classfile', W.SHARED['classfile'])
        pins = W.class_pins()
        P.require(pins['archive'] == {'sha256': P.JAR, 'bytes': jar.stat().st_size}, 'original archive pin')
        with zipfile.ZipFile(jar) as archive:
            names = archive.namelist()
            P.require(len(names) == len(set(names)) and not set(names) & HELPERS, 'unique archive entries and helper names')
            for name, expected in pins['classes'].items():
                P.require(P.exact(classfile.parse_class(archive.read(name + '.class')), expected),
                          'unchanged original class and Code ' + name)
        names_file = output / 'class-names.txt'
        names_file.write_text('\n'.join(pins['classes']) + '\n', encoding='ascii', newline='\n')
        names_pin = digest(names_file)
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
            environment.pop(key, None)
        for label, tool in (('java', java), ('javac', javac), ('javap', javap)):
            receipt = capture.capture([str(tool), '-version'], output, label, 15, environment)
            report[label] = receipt
            capture.require_success(receipt)
            version = ''.join((output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            P.require(re.search(r'(?:version "|javac |^)(17\.)', version, re.MULTILINE), 'JDK 17 tool ' + label)
        report['bytecode'] = capture.capture([str(javap), '-p', '-c', '-s', '-classpath', str(jar), 've', 'gs', 'uu', 'hl'],
                                             output, 'bytecode', 20, environment)
        capture.require_success(report['bytecode'], empty_stderr=True)
        bytecode = (output / 'bytecode.stdout').read_text(encoding='utf-8')
        P.require(re.search(r'public static br f\(fd, int, int, int, int\);\s+descriptor: \(Lfd;IIII\)Lbr;'
                            r'\s+Code:\s+0: aload_0\s+1: iload_1\s+2: iload_2\s+3: iload_3'
                            r'\s+4: invokevirtual\s+#\d+\s+// Method fd\.e:\(III\)I', bytecode),
                  'VE.f null receiver instruction at bytecode 4')
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        report['compile'] = capture.capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath,
            '-d', str(classes), str(W.HERE / 'FootWakeObservation.java'), str(W.SHARED['setup']),
            str(W.SHARED['json'])], output, 'compile', 45, environment)
        capture.require_success(report['compile'], empty_stderr=True)
        helpers = helper_pins()
        P.require(set(helpers) == HELPERS, 'only declared authored helpers compiled')
        report['helpers_before_sha256'] = helpers
        report['original'] = []
        observations = []
        bindings = ['CLASS ' + name + ' ' + str(value['class_bytes']) + ' ' + value['class_sha256']
                    for name, value in pins['classes'].items()]
        for index in range(2):
            P.require(before == {str(path): digest(path) for path in inputs} and helpers == helper_pins()
                      and digest(names_file) == names_pin, 'input identities before VM entry')
            work = output / f'work-{index}'
            work.mkdir()
            receipt = capture.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                '--add-opens=java.base/java.lang=ALL-UNNAMED', '-classpath', classpath,
                'FootWakeObservation', str(jar), str(work), str(names_file)],
                output, f'original-{index}', 70, environment, work)
            report['original'].append(receipt)
            capture.require_success(receipt)
            lines = (output / receipt['stdout_file']).read_text(encoding='utf-8').splitlines()
            prefix = bindings + ['16 achievements', '151 recipes']
            P.require(lines[:len(prefix)] == prefix and len(lines) == len(prefix) + 3 and lines[-1] == FOOTER,
                      'actual class bindings, ordered output and effect guards')
            selected = lines[len(prefix):-1]
            P.require(all(line.startswith('FOOT_WAKE ') for line in selected), 'two original observation rows')
            rows = [P.loads(line.removeprefix('FOOT_WAKE ')) for line in selected]
            W.validate(rows)
            observations.append(rows)
        P.require(P.exact(*observations), 'two fresh VMs agree on every selected value')
        P.require(digest(names_file) == names_pin, 'class-name input unchanged')
        report['status'] = 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY'
    except Exception as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['input_after_sha256'] = {str(path): digest(path) for path in inputs}
        report['helpers_after_sha256'] = helper_pins()
        passed = report['status'] == 'PASS_ORIGINAL_CPU_COMPONENTS_ONLY'
        unchanged = (before is not None and before == report['input_after_sha256'] and
                     helpers == report['helpers_after_sha256'])
        if not unchanged:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
        if passed:
            P.require(unchanged, 'original capture input and helper preservation')
    write(output / 'finite.json', {'schema': 'BED_FOOT_WAKE_V1', 'rows': observations[0]})
    receipt_keys = ('exit', 'timed_out', 'pipe_capture_complete', 'output_overflow', 'reader_errors', 'stdout_sha256', 'stderr_sha256')
    write(output / 'provenance.json', {
        'status': report['status'], 'archive_sha256': P.JAR, 'finite_sha256': digest(output / 'finite.json'),
        'capture_sha256': digest(output / 'capture.json'), 'sources': sources, 'shared': shared,
        'methods': W.method_pins(), 'helpers': helpers,
        'original': [{key: receipt[key] for key in receipt_keys} for receipt in report['original']],
    })
    print('Two unchanged original Bed foot wake captures pass: ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
