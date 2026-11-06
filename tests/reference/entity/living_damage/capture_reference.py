"""Capture unchanged Living damage over genuine World/Sheep/Wolf owners."""
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
    'dl': {('<init>', '(Lfd;)V'), ('a', '(Lsn;I)Z'), ('q', '()V'), ('s', '()Z'), ('a', '(Z)V'), ('j_', '()Ljava/lang/String;'), ('i', '()Ljava/lang/String;')},
    'bg': {('<init>', '(Lfd;)V')},
    'ii': {('<init>', '(Lfd;)V')},
    'ls': {('<init>', '(Lfd;)V'), ('a', '(Lsn;I)Z'), ('a', '(Lsn;IDD)V'), ('b', '(I)V'), ('b', '(Lsn;)V'), ('k', '()F')},
    'sn': {('<init>', '(Lfd;)V'), ('ai', '()V'), ('e', '(DDD)V'), ('c', '(Lsn;I)V'), ('a', '(Lls;)V')},
    'gi': {('<init>', '(Lfd;)V'), ('a', '(Lsn;F)V'), ('D', '()Z'), ('d', '(Z)V')},
    'fd': {('<init>', '(Lwt;Ljava/lang/String;Lxa;J)V'), ('a', '(Lsn;B)V'), ('a', '(Lsn;Ljava/lang/String;FF)V'), ('a', '(Lpm;)V')},
    'wy': {('<init>', '()V')},
    'xa': {('a', '(I)Lxa;')},
    'gr': {('<init>', '(Ljava/lang/String;Ljava/lang/String;)V')},
    'eq': {('<init>', '(DDDDDD)V')},
    'in': {('a', '(D)F')},
}
SOURCES = ('LivingDamageObservation.java', 'capture_reference.py', 'profiles.txt', 'source-pins.json',
           '../../client/controller/capture.py', '../../client/controller/classfile.py')
PROFILE_TEXT = '''0 4 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 0 0 1 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 0 0 2 c2340000 3fe8000000000000 0 38 0 0
0 3 20 11 4 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 11 4 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 7 20 11 4 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 10 4 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 1 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 0 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 2 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 1 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 1 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 7 20 11 4 0 1 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 0 0 3 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 0 0 4 41f00000 bfd8000000000000 1 38 0 0
0 4 20 0 0 0 0 0 0 5 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 0 1 0 41f00000 bfd8000000000000 0 38 0 0
0 -3 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 0 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 4 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 4 0 0 0 0 1 0 0 41f00000 bfd8000000000000 0 38 0 0
0 4 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 1
1 2 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0
1 4 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 1 0
2 4 20 0 0 0 0 0 0 0 41f00000 bfd8000000000000 0 38 0 0'''


def profiles():
    lines = (HERE / 'profiles.txt').read_text(encoding='ascii').splitlines()
    ensure(lines == PROFILE_TEXT.splitlines(), 'exact Living profile inventory')
    return [[value if i in (10, 11) else int(value) for i, value in enumerate(line.split())] for line in lines]


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


def state(value):
    keys = {'health', 'animation', 'velocity', 'attacked', 'dying', 'position', 'yaw', 'bounds', 'random',
            'math_random', 'world_random', 'source_random', 'source_cooldown', 'source_unchanged', 'sounds'}
    ensure(type(value) is dict and set(value) == keys, 'Living state schema')
    ensure(type(value['health']) is list and len(value['health']) == 8 and all(type(x) is int for x in value['health']), 'Living health/timer shape')
    for key in ('attacked', 'dying', 'source_unchanged'):
        ensure(type(value[key]) is bool, 'Living boolean shape')
    ensure(value['source_unchanged'] and type(value['source_cooldown']) is int, 'Living retained source owner')
    def word(x, length):
        return type(x) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', x)
    for key, length, count in (('animation', 8, 2), ('velocity', 16, 3), ('position', 16, 3), ('bounds', 16, 6)):
        items = value[key]
        ensure(items is None and key == 'bounds' or type(items) is list and len(items) == count and all(word(x, length) for x in items), 'Living primitive shape: ' + key)
    ensure(word(value['yaw'], 8), 'Living yaw shape')
    for key in ('random', 'math_random', 'world_random', 'source_random'):
        item = value[key]
        ensure(item is None and key == 'random' or word(item, 16) and int(item, 16) < (1 << 48), 'Living actual Random shape')
    ensure(type(value['sounds']) is list and len(value['sounds']) <= 1, 'Living finite sound shape')
    for sound in value['sounds']:
        ensure(type(sound) is dict and set(sound) == {'name', 'position', 'volume', 'pitch'} and sound['name'] == 'mob.sheep' and
               type(sound['position']) is list and len(sound['position']) == 3 and all(word(x, 16) for x in sound['position']) and
               word(sound['volume'], 8) and word(sound['pitch'], 8), 'Living actual Sheep sound shape')


def rows(text, native=False):
    found = [json.loads(line[len('LIVING_DAMAGE '):], object_pairs_hook=unique_object)
             for line in text.splitlines() if line.startswith('LIVING_DAMAGE ')]
    expected = [(index, step, profile) for index, profile in enumerate(profiles()) for step in range(3 if profile[0] == 2 else 1)]
    keys = {'case', 'profile', 'step', 'input', 'amount', 'scope', 'result', 'exception', 'state', 'completion'}
    ensure(len(found) == len(expected) == 27, 'exact Living row count')
    for index, (row, (profile_index, step, profile)) in enumerate(zip(found, expected)):
        ensure(type(row) is dict and set(row) == keys and type(row['case']) is int and row['case'] == index and
               type(row['profile']) is int and row['profile'] == profile_index and type(row['step']) is int and row['step'] == step and
               strict_equal(row['input'], profile), 'Living row identity/schema differs')
        amount = (4, 3, 7)[step] if profile[0] == 2 else profile[1]
        ensure(type(row['amount']) is int and row['amount'] == amount, 'Living amount input differs')
        exception = 'UNIMPLEMENTED' if native and profile_index == 19 else 'NPE' if profile_index in (9, 10, 20) else ('IO' if native else 'CALLBACK') if profile_index == 21 else 'NONE'
        scope = 'DEATH_ENTRY' if profile_index == 19 else 'EXCEPTION' if exception != 'NONE' else 'RETURN'
        ensure(row['exception'] == exception and row['scope'] == scope, 'Living source outcome category differs')
        ensure(row['result'] is None if exception != 'NONE' or profile[0] == 1 else type(row['result']) is bool, 'Living actual result type')
        state(row['state'])
        if profile_index == 19 and not native:
            completion = row['completion']
            ensure(type(completion) is dict and set(completion) == {'result', 'exception', 'state'} and completion['result'] is True and completion['exception'] == 'NONE', 'Living actual original death completion')
            state(completion['state'])
        else:
            ensure(row['completion'] is None, 'Living completion scope differs')
    return found

def verify_pins(jar, pins):
    ensure(type(pins) is dict and set(pins) == {'client_jar_sha256', 'classes'} and
           pins['client_jar_sha256'] == JAR and type(pins['classes']) is dict and
           set(pins['classes']) == set(REQUIRED) and digest(jar) == JAR, 'Living source pin authority/schema differs')
    with zipfile.ZipFile(jar) as archive:
        for name, signatures in REQUIRED.items():
            actual = inspect_class(archive.read(name + '.class'))
            actual['methods'] = [method for method in actual['methods'] if (method['name'], method['descriptor']) in signatures]
            ensure({(method['name'], method['descriptor']) for method in actual['methods']} == signatures and
                   strict_equal(actual, pins['classes'][name]), 'Living original class/method identity differs: ' + name)


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
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'pinned Living SDK metadata')
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
              'scope': 'Unchanged DL/LS damage and GI close-bite CPU methods, genuine constructed fd/Sheep/Wolf. '
                       'Declared geometry/health/timers/private and shared Random seeds; exact nonfatal returns and nullable failures. '
                       'Fatal row compares actual sound checkpoint before LS.b(SN)316; unchanged original complete death retained separately. '
                       'Native death, other overrides, whole combat/gameplay, GL and pixels excluded.'}
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], args.output, label, 10, environment)
            require_success(receipt)
            version = (args.output / receipt['stdout_file']).read_text() + (args.output / receipt['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), 'JDK17 required')
            report[label + '_version'] = version
        classpath = os.pathsep.join(str(path) for path in (classes.resolve(), jar, *libraries))
        compiled = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'LivingDamageObservation.java')], args.output, 'compile', 30, environment)
        report['compile'] = compiled
        require_success(compiled, empty_stderr=True)
        helper = inventory(classes)
        ensure(set(helper) == {'LivingDamageObservation.class', 'LivingDamageObservation$1.class', 'LivingDamageObservation$Observer.class'}, 'only authored Living helper classes')
        report['helper_before_sha256'] = helper
        original = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED', '--add-opens=java.base/java.lang=ALL-UNNAMED',
                            '-classpath', classpath, 'LivingDamageObservation', str(jar),
                            str((HERE / 'profiles.txt').resolve())], args.output, 'original', 60,
                           environment, args.output.resolve())
        report['original'] = original
        require_success(original)
        text = (args.output / original['stdout_file']).read_text()
        ensure(text.splitlines()[-1] == 'Original Living damage: 27 rows; network=0; native=0; write=0; genuine_world=1',
               'original Living completion/guard receipt')
        observed = rows(text)
        finite = args.output / 'finite.json'
        finite.write_text(json.dumps({'rows': observed, 'source_sha256': {name: digest(HERE / name) for name in SOURCES}},
                                     indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=27, fixture_sha256=digest(finite))
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        same = before == report['input_after_sha256'] and ('helper_before_sha256' not in report or
                report['helper_before_sha256'] == report['helper_after_sha256'])
        if not same:
            report['status'] = 'FAIL'
        (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'Living original helper/archive/tool inputs changed')
    print('Original Living damage: 27 exact CPU rows; ' + str(args.output / 'capture.json'))


if __name__ == '__main__':
    main()
