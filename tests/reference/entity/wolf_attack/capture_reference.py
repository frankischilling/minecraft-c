"""Observe unchanged protected GI attack over actual World-NULL Wolf/FallingSand."""
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
SUPPORT = HERE.parents[1] / 'client/controller'
sys.path.insert(0, str(SUPPORT))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
REQUIRED = {
    'gi': {('<init>', '(Lfd;)V'), ('a', '(Lsn;F)V'), ('D', '()Z'), ('d', '(Z)V')},
    'ju': {('<init>', '(Lfd;)V')},
    'sn': {('<init>', '(Lfd;)V'), ('a', '(Lsn;I)Z'), ('ai', '()V'), ('e', '(DDD)V')},
    'ls': {('<init>', '(Lfd;)V')},
    'in': {('a', '(D)F')},
}
SOURCES = ('WolfAttackObservation.java', 'capture_reference.py', 'profiles.txt', 'source-pins.json',
           '../../client/controller/capture.py', '../../client/controller/classfile.py')
PROFILE_TEXT = '''0 3fc00000 1 0 0 0 0 123
0 40000000 1 0 0 0 0 123
0 40c00000 1 0 0 0 0 123
0 7f800000 1 0 0 0 0 123
0 7fc12345 1 0 0 0 0 123
1 40400000 1 0 0 0 0 123
0 40400000 0 0 0 0 0 123
0 40800000 1 1 0 0 0 123
0 40400000 1 2 0 0 0 123
0 40400000 1 5 0 0 0 123
0 40bfffff 1 6 0 0 0 123
0 40000001 1 0 0 0 0 123
38 3f800000 1 3 0 0 0 123
38 3f800000 1 3 1 0 0 -7
38 3fbfffff 1 3 0 0 0 123
38 3f800000 1 3 0 0 3 123
38 3f800000 1 3 0 0 4 123
0 40400000 1 0 0 1 0 123
38 3f800000 1 3 0 1 0 123
38 3f800000 1 3 0 0 1 123
38 3f800000 1 3 0 0 2 123
0 40400000 1 1 0 0 1 123
38 bf800000 1 3 0 0 0 123
38 3f800000 1 4 0 0 0 123'''


def profiles():
    lines = (HERE / 'profiles.txt').read_text(encoding='ascii').splitlines()
    ensure(lines == PROFILE_TEXT.splitlines(), 'exact Wolf profile inventory')
    return [[int(row[0]), row[1], *map(int, row[2:])] for row in (line.split() for line in lines)]


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
        return actual.keys() == expected.keys() and all(strict_equal(actual[key], expected[key]) for key in actual)
    if isinstance(actual, list):
        return len(actual) == len(expected) and all(strict_equal(a, b) for a, b in zip(actual, expected))
    return actual == expected


def rows(text):
    found = [json.loads(line[len('WOLF_ATTACK '):], object_pairs_hook=unique_object)
             for line in text.splitlines() if line.startswith('WOLF_ATTACK ')]
    expected = profiles()
    keys = {'case', 'profile', 'exception', 'velocity', 'cooldown', 'random', 'target_unchanged',
            'target_attacked', 'actor_position', 'target_position', 'actor_bounds', 'target_bounds'}
    ensure(len(found) == len(expected) == 24, 'exact Wolf row count')
    for index, (row, profile) in enumerate(zip(found, expected)):
        ensure(type(row) is dict and set(row) == keys and type(row['case']) is int and
               row['case'] == index and strict_equal(row['profile'], profile), 'Wolf row identity/schema differs')
        ensure(row['exception'] == ('NPE' if 17 <= index <= 20 else 'NONE'), 'Wolf source exception category differs')
        ensure(type(row['cooldown']) is int and row['cooldown'] == (20 if index in (12, 13, 14, 22) else profile[7]),
               'Wolf source cooldown differs')
        ensure(type(row['target_unchanged']) is bool and row['target_unchanged'] and
               type(row['target_attacked']) is bool and row['target_attacked'] == (index in (12, 13, 14, 22)),
               'actual FallingSand target effects differ')
        ensure(type(row['random']) is str and re.fullmatch('[0-9a-f]{16}', row['random']) and
               int(row['random'], 16) < (1 << 48), 'actual private Random state shape differs')
        for name in ('velocity', 'actor_position', 'target_position', 'actor_bounds', 'target_bounds'):
            value = row[name]
            null = name == 'actor_bounds' and profile[6] == 2 or name == 'target_bounds' and profile[6] == 1
            ensure(value is None if null else type(value) is list and len(value) == (6 if name.endswith('bounds') else 3)
                   and all(type(word) is str and re.fullmatch('[0-9a-f]{16}', word) for word in value),
                   'actual Wolf primitive/bounds shape differs: ' + name)
    return found


def verify_pins(jar, pins):
    ensure(type(pins) is dict and set(pins) == {'client_jar_sha256', 'classes'} and
           pins['client_jar_sha256'] == JAR and type(pins['classes']) is dict and
           set(pins['classes']) == set(REQUIRED) and digest(jar) == JAR, 'Wolf source pin authority/schema differs')
    with zipfile.ZipFile(jar) as archive:
        for name, signatures in REQUIRED.items():
            actual = inspect_class(archive.read(name + '.class'))
            actual['methods'] = [method for method in actual['methods'] if (method['name'], method['descriptor']) in signatures]
            ensure({(method['name'], method['descriptor']) for method in actual['methods']} == signatures and
                   strict_equal(actual, pins['classes'][name]), 'Wolf original class/method identity differs: ' + name)


def inventory(root):
    return {path.relative_to(root).as_posix(): digest(path) for path in root.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(value) or '').resolve(strict=True) for value in (args.java, args.javac)]
    ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), 'matching JDK17 tools')
    jar = args.client_jar.resolve(strict=True)
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'pinned Wolf SDK metadata')
    verify_pins(jar, read_json(HERE / 'source-pins.json'))
    profiles()
    inputs = [java, javac, jar, *libraries, *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    classes = args.output / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': 'Unchanged GI.a(SN,F) CPU on actual GI(NULL World), JU(NULL World). '
                       'Declared private Random seeds, positions/velocities/bounds/ground/tame/cooldown; '
                       'far/miss/air/pounce, NULL bounds/target and inherited SN.a-false bite. '
                       'Full Living hurt, World queries/scheduling, GL and pixels excluded.'}
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], args.output, label, 10, environment)
            require_success(receipt)
            version = (args.output / receipt['stdout_file']).read_text() + (args.output / receipt['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), 'JDK17 required')
            report[label + '_version'] = version
        classpath = os.pathsep.join(str(path) for path in (classes.resolve(), jar, *libraries))
        compiled = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'WolfAttackObservation.java')], args.output, 'compile', 30, environment)
        report['compile'] = compiled
        require_success(compiled, empty_stderr=True)
        helper = inventory(classes)
        ensure(set(helper) == {'WolfAttackObservation.class', 'WolfAttackObservation$1.class'}, 'only authored Wolf helper classes')
        report['helper_before_sha256'] = helper
        original = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                            '-classpath', classpath, 'WolfAttackObservation', str(jar),
                            str((HERE / 'profiles.txt').resolve())], args.output, 'original', 60,
                           environment, args.output.resolve())
        report['original'] = original
        require_success(original)
        text = (args.output / original['stdout_file']).read_text()
        ensure(text.splitlines()[-1] == 'Original Wolf attack: 24 rows; network=0; native=0; write=0; world=0',
               'original Wolf completion/guard receipt')
        observed = rows(text)
        finite = args.output / 'finite.json'
        finite.write_text(json.dumps({'rows': observed, 'source_sha256': {name: digest(HERE / name) for name in SOURCES}},
                                     indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=24, fixture_sha256=digest(finite))
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        same = before == report['input_after_sha256'] and ('helper_before_sha256' not in report or
                report['helper_before_sha256'] == report['helper_after_sha256'])
        if not same:
            report['status'] = 'FAIL'
        (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'Wolf original helper/archive/tool inputs changed')
    print('Original Wolf attack: 24 exact CPU rows; ' + str(args.output / 'capture.json'))


if __name__ == '__main__':
    main()
