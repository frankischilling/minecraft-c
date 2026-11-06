"""Observe unchanged CH component bodies in two bounded original interpreter VMs."""
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
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'controller'))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
SOURCES = ('DeathScreenObservation.java', 'capture_reference.py', 'source-pins.json',
           '../controller/capture.py', '../controller/classfile.py')
SCOPE = ('Original CH/DA/KE constructors, CH.b, CH keyboard and c methods; genuine GR Session; '
         'prepared original kq App. No complete App construction, drawing, action, loaded '
         'World, respawn or gameplay qualification.')


def rows(text):
    result = [line for line in text.splitlines() if line.startswith(('DEATH ', 'DEATHBUTTON ', 'DEATHKEY '))]
    ensure(len(result) == 72 and len(set(result)) == 72, 'Death screen row extent/uniqueness')
    index = 0
    for mode in range(3):
        for size in range(4):
            ensure(re.fullmatch(rf'DEATH {mode} {size} [01] 2 0', result[index]) is not None,
                   'Death screen state ordering/schema')
            index += 1
            for slot in range(2):
                ensure(re.fullmatch(rf'DEATHBUTTON {mode} {size} {slot}(?: -?[0-9]+){{7}} [0-9a-f]+', result[index]) is not None,
                       'Death screen Button ordering/schema')
                index += 1
            for key in range(3):
                ensure(result[index] == f'DEATHKEY {mode} {size} {key} 1 2 0',
                       'Death screen keyboard ordering/schema')
                index += 1
    return result


def verify_pins(jar):
    ensure(digest(jar) == JAR, 'Original client JAR differs')
    pins = json.loads((HERE / 'source-pins.json').read_text(encoding='utf-8'))
    ensure(pins['client_jar_sha256'] == JAR, 'Death screen declared original archive')
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins['classes'].items():
            actual = inspect_class(archive.read(name.replace('.', '/') + '.class'))
            signatures = {(method['name'], method['descriptor']) for method in expected['methods']}
            actual['methods'] = [method for method in actual['methods'] if
                                 (method['name'], method['descriptor']) in signatures]
            ensure(actual == expected, 'Death screen original class/method pin: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(tool) or '').resolve(strict=True) for tool in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), 'Matching JDK17 tools')
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'Death screen SDK pins')
    verify_pins(jar)
    inputs = [java, javac, jar, *libraries, *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'scope': SCOPE, 'input_before_sha256': before}
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    classes = args.output / 'classes'
    classes.mkdir()
    provenance = None
    try:
        versions = []
        for name, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], args.output, name, 10, environment)
            require_success(receipt)
            versions.append(((args.output / receipt['stdout_file']).read_bytes() +
                             (args.output / receipt['stderr_file']).read_bytes()).decode('utf-8'))
        runtime = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0])
        compiler = re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        ensure(runtime and compiler and runtime[1] == compiler[1], 'Death screen matching JDK17')
        report['jdk'] = versions
        classpath = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        compiled = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'DeathScreenObservation.java')], args.output, 'compile', 30, environment)
        require_success(compiled)
        helper = {path.relative_to(classes).as_posix(): digest(path) for path in classes.rglob('*.class')}
        ensure(set(helper) == {'DeathScreenObservation.class', 'DeathScreenObservation$1.class'}, 'Death screen helper extent')
        report['helper_before_sha256'] = helper
        observed, receipts = [], []
        for index in range(2):
            work = args.output / ('work-' + str(index))
            work.mkdir()
            receipt = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                               '-Djava.security.manager=allow', '-classpath', classpath,
                               'DeathScreenObservation', str(jar)], args.output, 'original-' + str(index),
                              45, environment, work.resolve())
            require_success(receipt)
            stdout = (args.output / receipt['stdout_file']).read_text(encoding='utf-8')
            ensure(stdout.splitlines()[-1] == 'Original death screen: 72 rows; network=0; native=0; writes=0',
                   'Death screen original completion')
            observed.append(rows(stdout))
            receipts.append(receipt)
        ensure(observed[0] == observed[1], 'Two original Death screen VMs differ')
        finite = json.dumps({'rows': observed[0]}, indent=2) + '\n'
        (args.output / 'finite.json').write_text(finite, encoding='utf-8', newline='\n')
        provenance = {'schema': 'DEATH_SCREEN_CAPTURE_V1', 'archive_sha256': JAR, 'scope': SCOPE,
                      'rows': 72, 'source_sha256': {name: digest(HERE / name) for name in SOURCES},
                      'finite_sha256': hashlib.sha256(finite.encode('utf-8')).hexdigest(),
                      'tool_sha256': {'java.exe': digest(java), 'javac.exe': digest(javac)},
                      'helper_sha256': helper, 'original': [{key: value for key, value in receipt.items()
                                                         if key != 'command'} for receipt in receipts]}
        report.update(status='PASS', rows=72, original=receipts)
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['helper_after_sha256'] = {path.relative_to(classes).as_posix(): digest(path)
                                          for path in classes.rglob('*.class')}
        unchanged = before == report['input_after_sha256'] and (
            'helper_before_sha256' not in report or report['helper_before_sha256'] == report['helper_after_sha256'])
        if not unchanged:
            report['status'] = 'FAIL'
        (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(unchanged, 'Death screen original inputs/helpers changed')
        if report['status'] == 'PASS':
            ensure(provenance is not None, 'Death screen complete publication metadata')
            provenance['input_unchanged'] = True
            provenance['helper_unchanged'] = True
            (args.output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Original Death screen: two VMs, 72 rows each; ' + str(args.output / 'capture.json'))


if __name__ == '__main__':
    main()
