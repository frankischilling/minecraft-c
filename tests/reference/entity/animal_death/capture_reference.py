"""Capture unchanged animal death over genuine World/Sheep/Wolf owners."""
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
    'dl': {('<init>', '(Lfd;)V'), ('a', '(Lsn;I)Z'), ('q', '()V'), ('s', '()Z'), ('r', '()I'), ('a', '(Z)V'), ('e_', '(I)V'), ('j_', '()Ljava/lang/String;'), ('i', '()Ljava/lang/String;')},
    'wh': {('<init>', '(Lfd;)V'), ('j', '()I'), ('i', '()Ljava/lang/String;')},
    'bx': {('<init>', '(Lfd;)V'), ('j', '()I'), ('i', '()Ljava/lang/String;')},
    'ww': {('<init>', '(Lfd;)V'), ('j', '()I'), ('i', '()Ljava/lang/String;')},
    'bg': {('<init>', '(Lfd;)V')},
    'ii': {('<init>', '(Lfd;)V')},
    'ls': {('<init>', '(Lfd;)V'), ('a', '(Lsn;I)Z'), ('a', '(Lsn;IDD)V'), ('b', '(I)V'), ('b', '(Lsn;)V'), ('k', '()F'), ('q', '()V'), ('j', '()I')},
    'sn': {('<init>', '(Lfd;)V'), ('ai', '()V'), ('e', '(DDD)V'), ('b', '(FF)V'), ('c', '(Lsn;I)V'), ('a', '(Lls;)V'), ('b', '(II)Lhl;'), ('a', '(IIF)Lhl;'), ('a', '(Liz;F)Lhl;')},
    'gi': {('<init>', '(Lfd;)V'), ('a', '(Lsn;F)V'), ('D', '()Z'), ('d', '(Z)V'), ('j', '()I')},
    'hl': {('<init>', '(Lfd;DDDLiz;)V')},
    'iz': {('<init>', '(III)V')},
    'fd': {('<init>', '(Lwt;Ljava/lang/String;Lxa;J)V'), ('a', '(Lsn;B)V'), ('a', '(Lsn;Ljava/lang/String;FF)V'), ('a', '(Lpm;)V'), ('b', '(Lsn;)Z'), ('c', '(Lsn;)V'), ('f', '(II)Z')},
    'ok': {('<init>', '(Lfd;Lbf;Lcl;)V'), ('a', '(II)Z'), ('b', '(II)Llm;')},
    'lm': {('<init>', '(Lfd;[BII)V'), ('a', '(Lsn;)V')},
    'wy': {('<init>', '()V')},
    'xa': {('a', '(I)Lxa;')},
    'gr': {('<init>', '(Ljava/lang/String;Ljava/lang/String;)V')},
    'eq': {('<init>', '(DDDDDD)V')},
    'in': {('a', '(D)F'), ('b', '(D)I')},
    'uu': {('<clinit>', '()V')},
    'gm': {('<clinit>', '()V')},
    'ud': {('<init>', '()V'), ('a', '(I)B')},
}
SOURCES = ('AnimalDeathObservation.java', 'AnimalDeathJdi.java', 'capture_reference.py', 'validate.py', 'profiles.txt', 'source-pins.json',
           '../mob_tick/ConstructorJson.java', '../../client/controller/capture.py', '../../client/controller/classfile.py')

PROFILE_TEXT = '''0 0 0 0 0 38 0 0 0 1 0 0 0 0
0 0 1 0 0 38 0 0 0 1 0 0 0 0
0 0 2 0 0 38 0 0 0 1 0 0 0 0
0 0 3 0 0 38 0 0 0 1 0 0 0 0
0 0 4 0 0 38 0 0 0 1 0 0 0 0
0 0 5 0 0 38 0 0 0 1 0 0 0 0
0 0 6 0 0 38 0 0 0 1 0 0 0 0
0 0 7 0 0 38 0 0 0 1 0 0 0 0
0 0 8 0 0 38 0 0 0 1 0 0 0 0
0 0 9 0 0 38 0 0 0 1 0 0 0 0
0 0 10 0 0 38 0 0 0 1 0 0 0 0
0 0 11 0 0 38 0 0 0 1 0 0 0 0
0 0 12 0 0 38 0 0 0 1 0 0 0 0
0 0 13 0 0 38 0 0 0 1 0 0 0 0
0 0 14 0 0 38 0 0 0 1 0 0 0 0
0 0 15 0 0 38 0 0 0 1 0 0 0 0
0 0 7 1 0 38 0 0 0 1 0 0 0 0
1 0 0 0 0 3 0 0 0 1 0 0 0 0
1 0 0 0 0 0 0 0 0 1 0 0 0 0
1 0 0 0 0 2 0 0 0 1 0 0 0 0
1 0 0 0 1 2 0 0 0 1 0 0 0 0
1 0 0 0 -1 0 0 0 0 1 0 0 0 0
2 0 0 0 0 3 0 0 0 1 0 0 0 0
2 0 0 0 0 0 0 0 0 1 0 0 0 0
2 0 0 0 0 2 0 0 0 1 0 0 0 0
3 0 0 0 0 3 0 0 0 1 0 0 0 0
3 0 0 0 0 0 0 0 0 1 0 0 0 0
3 0 0 0 0 2 0 0 0 1 0 0 0 0
4 1 0 0 0 38 0 0 0 1 0 0 0 0
1 1 0 0 0 3 1 0 0 1 0 0 0 0
0 1 0 0 0 38 2 0 0 1 0 0 0 0
0 0 9 0 0 38 0 1 0 1 0 0 0 0
0 1 3 0 0 38 0 0 0 2 0 0 0 0
0 0 7 0 0 38 0 0 1 1 0 0 0 0
1 0 0 0 0 2 0 0 2 1 0 0 0 0
0 0 5 0 0 38 0 0 3 1 0 0 0 0
0 1 6 0 0 38 0 0 0 1 -1 0 0 0
1 1 0 0 0 2 0 1 0 1 0 0 0 0
1 1 0 0 0 38 0 0 0 1 0 1 0 0
0 1 0 0 0 38 0 0 0 1 0 0 1 0
0 2 7 0 0 38 0 0 0 1 0 0 0 0
0 0 8 0 0 38 0 0 4 1 0 0 0 0'''


def profiles():
    lines = (HERE / 'profiles.txt').read_text(encoding='ascii').splitlines()
    ensure(lines == PROFILE_TEXT.splitlines(), 'exact animal profile inventory')
    return [list(map(int, line.split())) for line in lines]


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


def rows(text, native=False):
    from validate import rows as checked_rows
    return checked_rows(text, profiles(), native)


def verify_pins(jar, pins):
    ensure(type(pins) is dict and set(pins) == {'client_jar_sha256', 'classes'} and
           pins['client_jar_sha256'] == JAR and type(pins['classes']) is dict and
           set(pins['classes']) == set(REQUIRED) and digest(jar) == JAR, 'animal source pin authority/schema differs')
    with zipfile.ZipFile(jar) as archive:
        for name, signatures in REQUIRED.items():
            actual = inspect_class(archive.read(name + '.class'))
            actual['methods'] = [method for method in actual['methods'] if (method['name'], method['descriptor']) in signatures]
            ensure({(method['name'], method['descriptor']) for method in actual['methods']} == signatures and
                   strict_equal(actual, pins['classes'][name]), 'animal original class/method identity differs: ' + name)


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
    ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'pinned animal SDK metadata')
    verify_pins(jar, read_json(HERE / 'source-pins.json'))
    profiles()
    inputs = [java, javac, java.parent.parent / 'lib/modules', java.parent / 'server/jvm.dll', jar, *libraries, *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    classes = args.output / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': 'Unchanged fatal LS damage/death, concrete animal drops, SN Stack/Item creation and actual World/Chunk publication. '
                       'Genuine constructed World/Chunk/Sheep/Wolf/Pig/Cow/Chicken owners; declared private seeds and fields. '
                       'Constructor and shared Math entropy remains independent; each actual stream is audited separately. '
                       'Read-only JDI supplies exact original frames, live Code, default Random U/nano/seed and Item constructor returns. '
                       'Original throwing publication observers and native IO retain different categories. No whole combat/gameplay, GL or pixels.'}
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], args.output, label, 10, environment)
            require_success(receipt)
            version = (args.output / receipt['stdout_file']).read_text() + (args.output / receipt['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), 'JDK17 required')
            report[label + '_version'] = version
        classpath = os.pathsep.join(str(path) for path in (classes.resolve(), jar, *libraries))
        compiled = capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'AnimalDeathObservation.java'), str(HERE / 'AnimalDeathJdi.java'), str(HERE / '../mob_tick/ConstructorJson.java')], args.output, 'compile', 30, environment)
        report['compile'] = compiled
        require_success(compiled, empty_stderr=True)
        helper = inventory(classes)
        ensure(set(helper) == {'AnimalDeathObservation.class', 'AnimalDeathObservation$1.class', 'AnimalDeathObservation$Observer.class', 'AnimalDeathJdi.class', 'ConstructorJson.class'}, 'only authored animal helper classes')
        report['helper_before_sha256'] = helper
        original = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED', '--add-opens=java.base/java.lang=ALL-UNNAMED',
                            '-classpath', classpath, 'AnimalDeathObservation', str(jar),
                            str((HERE / 'profiles.txt').resolve())], args.output, 'original', 60,
                           environment, args.output.resolve())
        report['original'] = original
        require_success(original)
        text = (args.output / original['stdout_file']).read_text()
        ensure(text.splitlines()[-1] == 'Original animal death: 43 rows; network=0; native=0; write=0; genuine_world_chunk=1',
               'original animal completion/guard receipt')
        observed = rows(text)
        debugger = args.output / 'debugger'
        debugger.mkdir()
        jdi = capture([str(java), '-Xmx256m', '--add-modules', 'jdk.jdi', '-classpath', str(classes.resolve()),
                       'AnimalDeathJdi', str(jar), str(classes.resolve()), str(debugger.resolve()), str(java.parent.parent),
                       str((HERE / 'profiles.txt').resolve()), classpath], debugger, 'controller', 240,
                      environment, debugger.resolve())
        report['debugger'] = jdi
        require_success(jdi, empty_stderr=True)
        ensure(not (debugger / jdi['stdout_file']).read_bytes(), 'quiet read-only JDI controller')
        debug_rows = rows((debugger / 'target.stdout').read_text())
        from validate import audit_entropy, audit_jdi, projection
        report['ordinary_entropy'] = audit_entropy(observed)
        ensure(all(strict_equal(projection(a), projection(b)) for a, b in zip(observed, debug_rows)), 'independent ordinary/JDI deterministic death state')
        journal = debugger / 'checkpoints.ndjson'
        ensure(journal.stat().st_size <= 64 * 1024 * 1024, 'bounded complete JDI journal')
        records = [json.loads(line, object_pairs_hook=unique_object) for line in journal.read_text().splitlines()]
        with zipfile.ZipFile(jar) as archive:
            report['read_only_JDI'] = audit_jdi(records, debug_rows, archive, inspect_class)
        report['debugger_artifacts_sha256'] = inventory(debugger)
        finite = args.output / 'finite.json'
        finite.write_text(json.dumps({'rows': observed, 'source_sha256': {name: digest(HERE / name) for name in SOURCES}},
                                     indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=43, fixture_sha256=digest(finite))
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        same = before == report['input_after_sha256'] and ('helper_before_sha256' not in report or
                report['helper_before_sha256'] == report['helper_after_sha256'])
        if not same:
            report['status'] = 'FAIL'
        (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'animal original helper/archive/tool inputs changed')
    print('Original animal death: 43 exact CPU rows; ' + str(args.output / 'capture.json'))


if __name__ == '__main__':
    main()
