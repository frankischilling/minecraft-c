"""Capture unchanged crossed-plant geometry and NW prefixes on actual owners."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SUPPORT = HERE.parent / 'controller'
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar':
        'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar':
        '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar':
        '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar':
        'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
REQUIRED = {
    'cv': {('<init>', '()V'), ('<init>', '(Lxp;)V'), ('<clinit>', '()V'),
           ('a', '(Luu;IDDD)V'), ('i', '(Luu;III)Z')},
    'nw': {('<init>', '(I)V'), ('<clinit>', '()V'), ('b', '()V'), ('b', '(DDD)V'),
           ('a', '(IIII)V'), ('a', '(DDDDD)V'), ('a', '(DDD)V')},
    'uu': {('<clinit>', '()V'), ('a', '(II)I')},
}
SOURCES = ('PlantObservation.java', 'capture_reference.py', 'profiles.txt', 'source-pins.json',
           '../controller/capture.py', '../controller/classfile.py')


def profiles():
    plants = [(6, 0), (6, 1), (6, 2), (6, 3), (31, 0), (31, 1), (31, 2),
              (32, 0), (37, 0), (38, 0), (39, 0), (40, 0), (83, 0)]
    result = [[block, meta, override, point, -1] for block, meta in plants
              for override in (-1, 0, 302) for point in range(3)]
    result += [[1, 0, -1, 0, kind] for kind in range(8)]
    return result


def profile_file():
    lines = (HERE / 'profiles.txt').read_text(encoding='ascii').splitlines()
    ensure(lines == [' '.join(map(str, profile)) for profile in profiles()],
           'exact declared plant profile inventory')
    return profiles()


def rows(text):
    result = [line for line in text.splitlines() if line.startswith('PLANT ')]
    expected = profile_file()
    ensure(len(result) == len(expected) == 125, 'exact original plant row inventory')
    for index, (row, profile) in enumerate(zip(result, expected)):
        cells = row.split()
        ensure(len(cells) >= 72 and cells[:7] == ['PLANT', str(index), *map(str, profile)],
               'plant row identity/profile differs')
        ensure(cells[7] == '0' and cells[35] == '0', 'retained CV AO flag/boolean scratch differs')
        ensure(all(re.fullmatch(r'[0-9a-f]{8}', value) for value in
                   [*cells[8:35], *cells[36:48], cells[57], *cells[72:]]) and
               all(re.fullmatch(r'[0-9a-f]{16}', value) for value in [*cells[48:54], *cells[58:63]]),
               'raw CV/Block/NW field representation differs')
        ensure(all(value in ('0', '1') for value in cells[63:68]), 'NW boolean field shape differs')
        kind = profile[4]
        exception = 'NONE' if kind < 0 else 'NPE' if kind < 2 else 'AIOOBE'
        capacity = 2097152 if kind < 2 else (0, 4, 8, 16, 32, 47)[kind - 2]
        count = -1 if kind == 1 else min(capacity, 192)
        ensure(cells[70] == exception and cells[71] == str(count) and cells[69] == str(capacity)
               and len(cells) == 72 + max(count, 0), 'exact plant failure/backing extent differs')
        if kind < 0:
            ensure(cells[54:58] == ['24', '16', '192', 'ffe97f1f'] and
                   cells[63:69] == ['1', '1', '0', '0', '1', '7'],
                   'actual complete source quad conversion/NW state differs')
    return result


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        ensure(key not in result, 'duplicate metadata key: ' + key)
        result[key] = value
    return result


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique_object)


def strict_equal(actual, expected):
    if type(actual) is not type(expected):
        return False
    if isinstance(actual, dict):
        return actual.keys() == expected.keys() and all(strict_equal(actual[k], expected[k]) for k in actual)
    if isinstance(actual, list):
        return len(actual) == len(expected) and all(strict_equal(a, b) for a, b in zip(actual, expected))
    return actual == expected


def verify_pins(jar, pins):
    ensure(type(pins) is dict and set(pins) == {'client_jar_sha256', 'classes'} and
           pins['client_jar_sha256'] == JAR and type(pins['classes']) is dict and
           set(pins['classes']) == set(REQUIRED), 'plant static pin authority/schema differs')
    ensure(digest(jar) == JAR, 'unchanged original client archive')
    with zipfile.ZipFile(jar) as archive:
        for name, signatures in REQUIRED.items():
            actual = inspect_class(archive.read(name + '.class'))
            actual['methods'] = [method for method in actual['methods']
                                 if (method['name'], method['descriptor']) in signatures]
            ensure({(method['name'], method['descriptor']) for method in actual['methods']} == signatures
                   and strict_equal(actual, pins['classes'][name]),
                   'unchanged plant class/method identity: ' + name)


def inventory(root):
    return {path.relative_to(root).as_posix(): digest(path) for path in root.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', required=True, type=Path)
    parser.add_argument('--library-root', required=True, type=Path)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(value) or '').resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), 'matching JDK17 tools')
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'pinned SDK metadata')
    verify_pins(jar, read_json(HERE / 'source-pins.json'))
    profile_file()
    inputs = [java, javac, jar, *libraries, HERE / 'PlantObservation.java', HERE / 'capture_reference.py',
              HERE / 'source-pins.json', HERE / 'profiles.txt', SUPPORT / 'capture.py', SUPPORT / 'classfile.py']
    before = {str(path): digest(path) for path in inputs}
    classes = args.output / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': 'Unchanged cv.a(Block,int,DDD) and NW on actual registered Block/CV/NW owners. '
                       '117 geometry/metadata/override/position rows and eight NULL/short-array prefixes. '
                       'Prepared CPU operands only; cv.i/jitter/color/anaglyph/World/DK/GL/pixels excluded.'}
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], args.output, label, 10, environment)
            require_success(receipt)
            version = (args.output / receipt['stdout_file']).read_text() + (args.output / receipt['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), 'JDK17 required')
            report[label + '_version'] = version
        classpath = os.pathsep.join(str(path) for path in (classes.resolve(), jar, *libraries))
        compiled = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'PlantObservation.java')], args.output, 'compile', 30, environment)
        report['compile'] = compiled
        require_success(compiled, empty_stderr=True)
        helper = inventory(classes)
        ensure(set(helper) == {'PlantObservation.class', 'PlantObservation$1.class'}, 'only authored observer classes')
        report['helper_before_sha256'] = helper
        original = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '-classpath', classpath, 'PlantObservation',
                            str(jar), str((HERE / 'profiles.txt').resolve())], args.output, 'original',
                           60, environment, args.output.resolve())
        report['original'] = original
        require_success(original)
        stdout = (args.output / original['stdout_file']).read_text()
        ensure(stdout.splitlines()[-1] == 'Original crossed plants: 125 rows; network=0; native=0; write=0; draw=0',
               'original completion/guard receipt')
        observed = rows(stdout)
        finite = args.output / 'finite.json'
        finite.write_text(json.dumps({'rows': observed, 'source_sha256':
                          {name: digest(HERE / name) for name in SOURCES}}, indent=2) + '\n',
                          encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=125, fixture_sha256=digest(finite))
    finally:
        report['input_after_sha256'] = {str(path): digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        same = before == report['input_after_sha256'] and ('helper_before_sha256' not in report or
                report['helper_before_sha256'] == report['helper_after_sha256'])
        if not same:
            report['status'] = 'FAIL'
        (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'original helper/runtime/source inputs changed')
    print('Original crossed plants: 125 exact CPU rows; ' + str(args.output / 'capture.json'))


if __name__ == '__main__':
    main()
