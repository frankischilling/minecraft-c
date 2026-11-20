"""Optionally capture four unchanged original Monster damage returns and compare native rows."""
import argparse
import copy
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import traceback
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'CMakeLists.txt').is_file())
SUPPORT = ROOT / 'tests/reference/client/controller'
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
CLASSES = ('fd', 'wy', 'xa', 'gr', 'sn', 'ls', 'ii', 'gz', 'uz', 'fr', 'gb', 'cn', 'bx', 'wh', 'eq', 'in')
CODE = {
    ('gz', 'a', '(Lsn;I)Z'): (41, '3a564dbff0d8aafe6a9adcb734753a4b334a763954343aba45255c4114e8c7fb'),
    ('ls', 'a', '(Lsn;I)Z'): (364, '623ac147768dd3c8fb85bb6e6fcbd647c2a482a6f23e2a4a448408161036f930'),
    ('ls', 'a', '(Lsn;IDD)V'): (122, '85cbc458153364de53b4960fb127aef9d37087d947bfd34cc6a1dd56e9e87976'),
    ('ls', 'b', '(I)V'): (11, '480a3c004425a22b6d79b97ef4baff48c0fb1aef3f24bfba96c69bd346e9994b'),
    ('ls', 'k', '()F'): (2, 'd9cd8c1ff77fbad798dbd032706c992ad89384e8a40d95ca0bfc1185834392df'),
    ('in', 'a', '(D)F'): (6, '29535f3f290f84c169aa98a9f64450a8ce757e84d08424806548dc5c55a473eb'),
    ('fd', 'a', '(Lsn;Ljava/lang/String;FF)V'): (65, '09ab73ca4cacb1419a1f01389eaa0b97acbb36838d2ef89a48d5b0cc8418f651'),
    ('fd', 'a', '(Lsn;B)V'): (1, '149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d'),
    ('uz', 'j_', '()Ljava/lang/String;'): (3, 'c4da0d8a6165d94d26b71d7644c3b49d7730372d252a19f9d8139a8ced18571e'),
    ('fr', 'j_', '()Ljava/lang/String;'): (3, '3e38b555284ede7b6f42e0458f7790c2600fb319f3997e52377119982e53be44'),
    ('gb', 'j_', '()Ljava/lang/String;'): (3, 'c4da0d8a6165d94d26b71d7644c3b49d7730372d252a19f9d8139a8ced18571e'),
    ('cn', 'j_', '()Ljava/lang/String;'): (3, 'c4da0d8a6165d94d26b71d7644c3b49d7730372d252a19f9d8139a8ced18571e'),
}
SOUNDS = ['mob.zombiehurt', 'mob.skeletonhurt', 'mob.creeper', 'mob.spider']
ROW_KEYS = {'sound', 'result', 'health', 'previous_health', 'resistance', 'last_damage',
            'hurt', 'max_hurt', 'angle', 'velocity', 'attacked', 'target_is_source'}
PREFIX = 'MONSTER_DAMAGE_RETURN '
COMPLETE = 'Original Monster damage: 4 rows; network=0; native=0; write=0; genuine_world=1; seed_reset=0'


def pins(paths):
    return {str(path.resolve()): digest(path) for path in paths}


def inventory(directory):
    return {p.relative_to(directory).as_posix(): digest(p) for p in sorted(directory.rglob('*')) if p.is_file()}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        ensure(key not in result, 'duplicate JSON member: ' + key)
        result[key] = value
    return result


def strict_equal(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(strict_equal(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(strict_equal(x, y) for x, y in zip(a, b))
    return a == b


def rows(text):
    result = [json.loads(line[len(PREFIX):], object_pairs_hook=unique_object)
              for line in text.splitlines() if line.startswith(PREFIX)]
    ensure(len(result) == 4, 'exactly four complete return rows')
    for i, row in enumerate(result):
        ensure(type(row) is dict and set(row) == ROW_KEYS and row['sound'] == SOUNDS[i],
               'exact return schema and concrete order')
        ensure(all(type(row[k]) is bool for k in ('result', 'attacked', 'target_is_source')),
               'actual Boolean return and field types')
        ensure(all(type(row[k]) is int for k in ('health', 'previous_health', 'resistance',
                                               'last_damage', 'hurt', 'max_hurt')),
               'actual integer health/timer types')
        ensure(type(row['angle']) is str and re.fullmatch('[0-9a-f]{8}', row['angle']),
               'raw binary32 angle')
        ensure(type(row['velocity']) is list and len(row['velocity']) == 3 and
               all(type(v) is str and re.fullmatch('[0-9a-f]{16}', v) for v in row['velocity']),
               'all three raw binary64 velocities')
    return result


def comparison_controls(original):
    refused = []
    for label, key, value in (
            ('Boolean_as_integer', 'result', 1), ('integer_as_float', 'health', 16.0),
            ('wrong_health', 'health', 15), ('wrong_angle', 'angle', '00000000'),
            ('wrong_velocity', 'velocity', ['0000000000000000'] * 3),
            ('lost_target', 'target_is_source', False)):
        wrong = copy.deepcopy(original)
        wrong[0][key] = value
        ensure(not strict_equal(wrong, original), 'comparator refused ' + label)
        refused.append(label)
    wrong = copy.deepcopy(original)
    del wrong[0]['attacked']
    ensure(not strict_equal(wrong, original), 'comparator refused missing field')
    refused.append('missing_field')
    ensure(not strict_equal(original[:-1], original), 'comparator refused missing kind')
    refused.append('missing_kind')
    return refused


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--native-stdout', type=Path)
    default_libraries = (Path(os.environ['APPDATA']) / '.minecraft/libraries'
                         if 'APPDATA' in os.environ else Path.home() / '.minecraft/libraries')
    parser.add_argument('--library-root', type=Path, default=default_libraries)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='monster-retaliation-', dir=args.work_dir)).resolve()
    report = {'status': 'FAIL', 'native_comparison': None,
              'scope': 'Four unchanged original nonfatal GZ/LS calls over genuine World and concrete owners; independent constructor entropy; no seeds reset. Optional native stdout is a content comparison, not proof of its build or execution provenance.'}
    inputs = []
    before = None
    classes = output / 'classes'
    classes.mkdir()
    failure = None
    try:
        jar = args.jar.resolve(strict=True)
        java = Path(shutil.which(args.java) or '').resolve(strict=True)
        javac = Path(shutil.which(args.javac) or '').resolve(strict=True)
        libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
        ensure(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), 'same JDK tools')
        ensure(digest(jar) == JAR, 'verified original JAR')
        ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'pinned SDK libraries')
        jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        inputs = [jar, java, javac, jvm, java.parent.parent / 'lib/modules', *libraries,
                  HERE / 'MonsterDamageObservation.java', Path(__file__), HERE / 'README.md',
                  SUPPORT / 'capture.py', SUPPORT / 'classfile.py']
        native = None
        if args.native_stdout is not None:
            native = args.native_stdout.resolve(strict=True)
            inputs.append(native)
        before = pins(inputs)
        report['input_before_sha256'] = before
        source = {}
        with zipfile.ZipFile(jar) as archive:
            for name in CLASSES:
                source[name] = inspect_class(archive.read(name + '.class'))
        for (name, method, descriptor), expected in CODE.items():
            selected = [m for m in source[name]['methods']
                        if m['name'] == method and m['descriptor'] == descriptor]
            ensure(len(selected) == 1 and
                   (selected[0].get('code_bytes'), selected[0].get('code_sha256')) == expected,
                   'original Code pin: ' + name + '.' + method + descriptor)
        report['original_classes'] = source
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
            environment.pop(key, None)
        for label, tool in (('java', java), ('javac', javac)):
            observed = capture([str(tool), '-version'], output, label, 10, environment)
            report[label] = observed
            require_success(observed)
            text = (output / observed['stdout_file']).read_text() + (output / observed['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), 'JDK17 required')
        classpath = os.pathsep.join(str(path.resolve()) for path in (classes, jar, *libraries))
        observed = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'MonsterDamageObservation.java')], output, 'compile', 30, environment)
        report['compile'] = observed
        require_success(observed, empty_stderr=True)
        helpers_before = inventory(classes)
        ensure(set(helpers_before) == {'MonsterDamageObservation.class', 'MonsterDamageObservation$1.class',
                                       'MonsterDamageObservation$Observer.class'}, 'only authored helper classes')
        report['helpers_before_sha256'] = helpers_before
        ensure(pins(inputs) == before, 'immediate pre-execution input pins')
        observed = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '-classpath', classpath,
                            'MonsterDamageObservation', str(jar)], output, 'original', 60, environment, output)
        report['original'] = observed
        require_success(observed)
        text = (output / observed['stdout_file']).read_text(encoding='utf-8')
        ensure(text.splitlines()[-1] == COMPLETE, 'complete actual source/guard receipt')
        original = rows(text)
        (output / 'rows.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf-8', newline='\n')
        report['comparator_refusals'] = comparison_controls(original)
        if native is not None:
            ensure(native.stat().st_size <= 4 * 1024 * 1024, 'bounded supplied native stdout')
            native_bytes = native.read_bytes()
            native_text = native_bytes.decode('utf-8', errors='strict')
            ensure(any(re.fullmatch(r'PASS: [1-9][0-9]* Monster damage source-order/ownership checks', line)
                       for line in native_text.splitlines()),
                   'complete native fixture PASS line')
            ensure(strict_equal(rows(native_text), original), 'exact typed four-row native/original comparison')
            (output / 'native.stdout').write_bytes(native_bytes)
            report['native_comparison'] = {'status': 'PASS', 'rows': 4,
                                           'input_sha256': digest(native), 'scope': 'Exact typed declared return fields only; independent pitch, constructor entropy and unrecorded state excluded.'}
        report.update(status='PASS', rows=4, rows_sha256=digest(output / 'rows.json'))
    except Exception as error:
        failure = error
        report['error'] = f'{type(error).__name__}: {error}'
        (output / 'driver-error.txt').write_text(traceback.format_exc(), encoding='utf-8', newline='\n')
    finally:
        try:
            report['input_after_sha256'] = pins(inputs)
            report['helpers_after_sha256'] = inventory(classes)
            report['inputs_unchanged'] = before is not None and report['input_after_sha256'] == before
            report['helpers_unchanged'] = report.get('helpers_before_sha256') == report['helpers_after_sha256']
            ensure(report['inputs_unchanged'] and report['helpers_unchanged'], 'final input/helper fingerprints')
        except Exception as error:
            report['status'] = 'FAIL'
            report['final_audit_error'] = f'{type(error).__name__}: {error}'
            if failure is None:
                failure = error
        report['output_sha256'] = inventory(output)
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Original Monster damage ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
