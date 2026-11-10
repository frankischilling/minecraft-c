"""Observe unchanged original quiet metadata setters and compare declared native scalar rows."""
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
ROOT = next(path for path in HERE.parents if (path / 'CMakeLists.txt').is_file())
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
CLASSES = ('fd', 'wy', 'xa', 'gr', 'ok', 'lm', 'li', 'wi', 'yy')
CODE = {
    ('fd', 'e', '(IIII)Z'): (75, '4e83e98b439eae25f9d4d189cff2af2a36047dbc68b42256a3abbc28725f4302'),
    ('fd', 'c', '(II)Llm;'): (12, '5b877b0cb443786fceebeb49f3e7f01bfe49b721af5a2a27dad581d254837d5c'),
    ('lm', 'b', '(IIII)V'): (18, '8cabeffbb5345993aa0fc5ca85009713701247450f9f2508c72ff4672bbe159a'),
    ('li', 'b', '(IIII)V'): (1, '149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d'),
    ('wi', 'a', '(IIII)V'): (85, '45e66074582303b9700d08fd56978635980a951a804b164f88f6fbcb23a7d2a0'),
    ('wi', '<init>', '([B)V'): (10, '95881b0f70ed99cf8c8c10f1e814ca0502cac2d1d060b99f5a1d940d27bf520e'),
}
PREFIX = 'METADATA_SET '
COMPLETE = 'Original metadata set: 15 rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9'
KEYS = {'profile', 'x', 'y', 'z', 'metadata', 'returned', 'result', 'exception',
        'dirty_before', 'dirty_after', 'nibble_null', 'backing_null', 'backing_length',
        'before_byte', 'after_byte', 'fixed_before', 'fixed_after', 'fixed_unchanged',
        'unselected_unchanged', 'cache_membership_unchanged', 'world_random_unchanged', 'events'}


def profiles():
    values = [('replacement-even18', 8, 64, 8, 18), ('replacement-odd-minus1', 8, 65, 8, -1)]
    values += [(f'{kind}-{parity}', 8, y, 8, 18)
               for kind in ('e-null', 'a-null', 'short') for parity, y in (('even', 64), ('odd', 65))]
    values += [('blank-null-e', 8, 64, 8, 18), ('outside-y128', 8, 128, 8, 18),
               ('outside-y-minus1', 8, -1, 8, 18), ('outside-x-upper', 32000000, 64, 8, 18),
               ('outside-z-upper', 8, 64, 32000001, 18), ('normal-even18', 8, 64, 8, 18),
               ('normal-odd-minus1', 8, 65, 8, -1)]
    return values


def unique(pairs):
    value = {}
    for key, cell in pairs:
        ensure(key not in value, 'duplicate metadata observation key: ' + key)
        value[key] = cell
    return value


def exact(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(exact(left[key], right[key]) for key in left)
    if type(left) is list:
        return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return left == right


def rows(text):
    values = [json.loads(line[len(PREFIX):], object_pairs_hook=unique)
              for line in text.splitlines() if line.startswith(PREFIX)]
    ensure(len(values) == 15, 'all fifteen ordered metadata observations')
    booleans = ('returned', 'dirty_before', 'dirty_after', 'nibble_null', 'backing_null',
                'fixed_unchanged', 'unselected_unchanged', 'cache_membership_unchanged', 'world_random_unchanged')
    for row, profile in zip(values, profiles()):
        ensure(type(row) is dict and row.keys() == KEYS, 'exact metadata row schema')
        ensure(exact([row[key] for key in ('profile', 'x', 'y', 'z', 'metadata')], list(profile)),
               'exact profile identity and operands')
        ensure(all(type(row[key]) is bool for key in booleans), 'exact Boolean observation types')
        ensure(all(type(row[key]) is int for key in ('x', 'y', 'z', 'metadata', 'fixed_before', 'fixed_after', 'events')),
               'exact integer observation types')
        ensure(row['result'] is None or type(row['result']) is bool, 'actual Boolean or unreturned result')
        ensure(row['exception'] is None or type(row['exception']) is str, 'actual Throwable type')
        ensure(all(row[key] is None or type(row[key]) is int for key in ('backing_length', 'before_byte', 'after_byte')),
               'nullable actual backing observations')
        ensure(not row['dirty_before'] and row['events'] == 0 and row['unselected_unchanged'] and
               row['cache_membership_unchanged'] and row['world_random_unchanged'], 'quiet call unaffected state')
        name = profile[0]
        if name.startswith(('e-null', 'a-null', 'short')):
            expected = ('java/lang/ArrayIndexOutOfBoundsException' if name.startswith('short')
                        else 'java/lang/NullPointerException')
            ensure(not row['returned'] and row['result'] is None and row['exception'] == expected and
                   row['dirty_after'] and row['fixed_unchanged'], 'dirty store before actual failed child')
        else:
            ensure(row['returned'] and row['exception'] is None and
                   row['result'] == (not name.startswith('outside')) and
                   row['dirty_after'] == (name.startswith(('replacement', 'normal'))),
                   'completed ordinary, Blank or out-of-domain source return')
    return values


def controls(original):
    refused = []
    for label, index, key, value in (
            ('wrong-byte', 0, 'after_byte', 0), ('lost-dirty-prefix', 2, 'dirty_after', False),
            ('forged-return', 2, 'returned', True), ('fixed-plane-write', 0, 'fixed_unchanged', False),
            ('Boolean-as-integer', 0, 'returned', 1), ('wrong-exception', 2, 'exception', 'wrong')):
        altered = copy.deepcopy(original)
        altered[index][key] = value
        ensure(not exact(original, altered), 'typed comparator refuses ' + label)
        refused.append(label)
    ensure(not exact(original, original[:-1]), 'typed comparator refuses missing row')
    return refused + ['missing-row']


def inventory(root):
    return {path.relative_to(root).as_posix(): digest(path)
            for path in sorted(root.rglob('*')) if path.is_file()}


def pins(paths):
    return {str(path.resolve()): digest(path) for path in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--native-stdout', type=Path)
    parser.add_argument('--native-pass', default=r'PASS: [1-9][0-9]* Metadata set.*checks')
    parser.add_argument('--library-root', type=Path, default=(Path(os.environ['APPDATA']) / '.minecraft/libraries'
                        if 'APPDATA' in os.environ else Path.home() / '.minecraft/libraries'))
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='metadata-set-', dir=args.work_dir)).resolve()
    classes, target = output / 'classes', output / 'target'
    classes.mkdir(); target.mkdir()
    report = {'status': 'FAIL', 'native_comparison': None}
    inputs, before, failure = [], None, None
    try:
        jar = args.jar.resolve(strict=True)
        java = Path(shutil.which(args.java) or '').resolve(strict=True)
        javac = Path(shutil.which(args.javac) or '').resolve(strict=True)
        ensure(java.parent.samefile(javac.parent), 'matching JDK tools')
        ensure(digest(jar) == JAR, 'verified unchanged original archive')
        libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
        ensure(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'pinned SDK metadata archives')
        jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        inputs = [jar, java, javac, jvm, java.parent.parent / 'lib/modules', *libraries,
                  HERE / 'MetadataSetObservation.java', Path(__file__), HERE / 'README.md',
                  SUPPORT / 'capture.py', SUPPORT / 'classfile.py']
        native = None
        if args.native_stdout is not None:
            native = args.native_stdout.resolve(strict=True)
            inputs.append(native)
        before = report['input_before_sha256'] = pins(inputs)
        source = {}
        with zipfile.ZipFile(jar) as archive:
            for name in CLASSES:
                source[name] = inspect_class(archive.read(name + '.class'))
        for (name, method, descriptor), expected in CODE.items():
            selected = [row for row in source[name]['methods'] if row['name'] == method and row['descriptor'] == descriptor]
            ensure(len(selected) == 1 and (selected[0].get('code_bytes'), selected[0].get('code_sha256')) == expected,
                   'unchanged Code pin: ' + name + '.' + method + descriptor)
        report['original_classes'] = source
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'CLASSPATH'):
            environment.pop(key, None)
        for label, tool in (('java', java), ('javac', javac)):
            observed = capture([str(tool), '-version'], output, label, 10, environment)
            report[label] = observed
            require_success(observed)
            text = (output / observed['stdout_file']).read_text() + (output / observed['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), 'JDK17 required')
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        observed = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'MetadataSetObservation.java')], output, 'compile', 30, environment)
        report['compile'] = observed
        require_success(observed, empty_stderr=True)
        helpers = report['helpers_before_sha256'] = inventory(classes)
        ensure(set(helpers) == {'MetadataSetObservation.class', 'MetadataSetObservation$1.class',
                                'MetadataSetObservation$Observer.class'}, 'authored helper classes only')
        ensure(pins(inputs) == before, 'immediate pre-execution input fingerprints')
        observed = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                            '-classpath', classpath, 'MetadataSetObservation', str(jar), str(target)],
                           output, 'original', 60, environment, output)
        report['original'] = observed
        require_success(observed)
        text = (output / observed['stdout_file']).read_text(encoding='utf-8')
        ensure(text.splitlines()[-1] == COMPLETE, 'complete original execution and external-effect guards')
        original = rows(text)
        failures = [json.loads(line[len('METADATA_SET_FAILURE '):], object_pairs_hook=unique)
                    for line in text.splitlines() if line.startswith('METADATA_SET_FAILURE ')]
        ensure(len(failures) == 6 and [row['profile'] for row in failures] == [row['profile'] for row in original[2:8]],
               'six unchanged original failure records retained')
        (output / 'rows.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf-8', newline='\n')
        (output / 'failures.json').write_text(json.dumps(failures, indent=2) + '\n', encoding='utf-8', newline='\n')
        report['comparator_refusals'] = controls(original)
        if native is not None:
            ensure(native.stat().st_size <= 4 * 1024 * 1024, 'bounded native stdout')
            native_bytes = native.read_bytes()
            native_text = native_bytes.decode('utf-8', errors='strict')
            ensure(any(re.fullmatch(args.native_pass, line) for line in native_text.splitlines()),
                   'complete native fixture PASS')
            ensure(exact(rows(native_text), original), 'exact typed fifteen-row native/original comparison')
            (output / 'native.stdout').write_bytes(native_bytes)
            report['native_comparison'] = {'status': 'PASS', 'rows': 15, 'input_sha256': digest(native),
                                         'scope': 'Declared scalar return/state/backing records only.'}
        report.update(status='PASS', rows=15, rows_sha256=digest(output / 'rows.json'))
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
            ensure(report['inputs_unchanged'] and report['helpers_unchanged'], 'final immutable input/helper fingerprints')
        except Exception as error:
            report['status'] = 'FAIL'
            report['final_audit_error'] = f'{type(error).__name__}: {error}'
            if failure is None:
                failure = error
        report['output_sha256'] = inventory(output)
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Original metadata set ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
