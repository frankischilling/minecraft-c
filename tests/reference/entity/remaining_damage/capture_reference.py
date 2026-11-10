"""Capture ten unchanged original remaining-Mob damage calls and compare typed native records."""
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
CLASSES = ('fd', 'wy', 'xa', 'gr', 'sn', 'ls', 'ii', 'bg', 'ar', 'wq', 'gi', 'uw', 'xt',
           'bp', 'bx', 'hl', 'iz', 'ok', 'lm', 'yy', 'gm')
CODE = {
    ('ls', 'a', '(Lsn;I)Z'): (364, '623ac147768dd3c8fb85bb6e6fcbd647c2a482a6f23e2a4a448408161036f930'),
    ('ls', 'b', '(Lsn;)V'): (58, '82535634da0d4e66aa09f3ab01a11d5d7c5670eedbaf38b756b53bbfb1820df3'),
    ('ls', 'q', '()V'): (39, '8ceb5f932fb38e03c314f960e2e517a6169fa3b6650c4a1508fa5aaf514f0c24'),
    ('gi', 'a', '(Lsn;I)Z'): (285, '51890f2399c649ca8c19cff5954257bb1dcc2c30c60c9e1f43aab901067436b1'),
    ('gi', 'j_', '()Ljava/lang/String;'): (3, '220a355322a6bb9d63048b1df2d8b292ea9cff9def50804c808918e1b2c516a0'),
    ('gi', 'i', '()Ljava/lang/String;'): (3, 'ebee5ee604098f1ed8977f52a5a6e1673c67cc8666b58e529185b3c8ba36b58c'),
    ('gi', 'k', '()F'): (3, '336763c44cb3e89e925efcf9017d1bd1870cab7e5df0ec6b692f4740ee4914a2'),
    ('gi', 'j', '()I'): (2, 'ea3444f7b732d2379c3e81ad7a08e24c072cb9468be2734659ffef7ecaec49ed'),
    ('uw', 'e', '(I)V'): (56, '7e9ff596010f98b0969b6dee6a43c2b4ea252bab2399eec7d7d8ffde1091affb'),
    ('uw', 'j_', '()Ljava/lang/String;'): (3, '19997f2effcb3e3b6d81522e6e8615c317e4b3f80fb5f224a8d37aa472126fba'),
    ('uw', 'i', '()Ljava/lang/String;'): (3, '19997f2effcb3e3b6d81522e6e8615c317e4b3f80fb5f224a8d37aa472126fba'),
    ('uw', 'k', '()F'): (3, '644266dfaeb68957f1e3926bf0c92dd1e9c8daff0174390e66af10478bc628c9'),
    ('uw', 'j', '()I'): (17, '852383ee42e88debc747dff14536a4646e25739c028e8c5dd70b58744ecec82a'),
    ('xt', 'j_', '()Ljava/lang/String;'): (2, '146cb1bd5c639e50589db67598e4a3f423eb736f7911d3b8329f72e0f5cd5fac'),
    ('xt', 'i', '()Ljava/lang/String;'): (2, '146cb1bd5c639e50589db67598e4a3f423eb736f7911d3b8329f72e0f5cd5fac'),
    ('xt', 'k', '()F'): (3, '336763c44cb3e89e925efcf9017d1bd1870cab7e5df0ec6b692f4740ee4914a2'),
    ('xt', 'q', '()V'): (43, 'f4eb0d4f853bc80650b680a268c25ded0b84d35a9d8358949952cd0af8765f2b'),
    ('bp', 'j_', '()Ljava/lang/String;'): (3, '19997f2effcb3e3b6d81522e6e8615c317e4b3f80fb5f224a8d37aa472126fba'),
    ('bp', 'i', '()Ljava/lang/String;'): (3, '83e424e5d40b4d8d14d99d90e3986a9021a55f073b843c457573f68aa101ce74'),
    ('bp', 'k', '()F'): (3, '644266dfaeb68957f1e3926bf0c92dd1e9c8daff0174390e66af10478bc628c9'),
    ('bp', 'j', '()I'): (7, '55af7ef6d3a16c171f76382cd1100814c0226e9b151524fbe0b33ce1caf807a7'),
}
PREFIX = 'REMAINING_DAMAGE '
COMPLETE = 'Original remaining damage: 10 rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9'
KEYS = {'profile', 'kind', 'size', 'amount', 'seed', 'result', 'health', 'previous_health',
        'resistance', 'last_damage', 'hurt', 'max_hurt', 'angle', 'velocity', 'random',
        'dying', 'dead', 'sounds', 'items'}


def profiles():
    return ([('uw', size, amount) for size in (1, 4) for amount in (4, 20)] +
            [(kind, 0, amount) for kind in ('xt', 'bp') for amount in (4, 20)] +
            [('gi', 0, 4), ('gi', 0, 40)])


def unique(pairs):
    value = {}
    for key, cell in pairs:
        ensure(key not in value, 'duplicate observation key: ' + key)
        value[key] = cell
    return value


def exact(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(exact(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return left == right


def word(value, length):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(length) + '}', value) is not None


def rows(text):
    values = [json.loads(line[len(PREFIX):], object_pairs_hook=unique)
              for line in text.splitlines() if line.startswith(PREFIX)]
    ensure(len(values) == 10, 'all ten declared observations')
    integers = ('size', 'amount', 'seed', 'health', 'previous_health', 'resistance',
                'last_damage', 'hurt', 'max_hurt')
    for row, (kind, size, amount) in zip(values, profiles()):
        ensure(type(row) is dict and row.keys() == KEYS, 'exact observation schema')
        ensure(exact([row[k] for k in ('profile', 'kind', 'size', 'amount', 'seed')],
                     [f'{kind}-size{size}-damage{amount}', kind, size, amount, 38]),
               'exact ordered profile identity')
        ensure(all(type(row[k]) is int for k in integers), 'actual integer fields')
        ensure(all(type(row[k]) is bool for k in ('result', 'dying', 'dead')), 'actual Booleans')
        ensure(word(row['angle'], 8) and word(row['random'], 16), 'angle and private Random words')
        ensure(type(row['velocity']) is list and len(row['velocity']) == 3 and
               all(word(cell, 16) for cell in row['velocity']), 'three exact velocity words')
        ensure(type(row['sounds']) is list and len(row['sounds']) == 1, 'one actual sound callback')
        sound = row['sounds'][0]
        ensure(type(sound) is dict and sound.keys() == {'name', 'volume', 'pitch'} and
               (sound['name'] is None or type(sound['name']) is str) and
               word(sound['volume'], 8) and word(sound['pitch'], 8), 'typed nullable sound and words')
        ensure(type(row['items']) is list and len(row['items']) <= 3, 'bounded ordered drops')
        for item in row['items']:
            ensure(type(item) is list and len(item) == 6 and
                   all(type(item[k]) is int for k in (0, 1, 2, 3, 5)) and type(item[4]) is bool,
                   'Item ID/count/metadata/pickup-delay/World/Chunk types')
    return values


def controls(original):
    refused = []
    for label, index, key, value in (
            ('wrong_health', 0, 'health', -13), ('wrong_private_random', 0, 'random', '0' * 16),
            ('Boolean_as_integer', 0, 'dying', 1), ('lost_squid_drop', 5, 'items', []),
            ('wrong_profile', 0, 'profile', 'wrong'),
            ('lost_null_sound', 4, 'sounds', [{**original[4]['sounds'][0], 'name': 'missing'}])):
        altered = copy.deepcopy(original)
        altered[index][key] = value
        ensure(not exact(original, altered), 'reject changed ' + label)
        refused.append(label)
    ensure(not exact(original, original[:-1]), 'reject missing row')
    return refused + ['missing_row']


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(root.rglob('*')) if p.is_file()}


def pins(paths):
    return {str(p.resolve()): digest(p) for p in paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--native-stdout', type=Path)
    parser.add_argument('--library-root', type=Path, default=(Path(os.environ['APPDATA']) / '.minecraft/libraries'
                        if 'APPDATA' in os.environ else Path.home() / '.minecraft/libraries'))
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='remaining-damage-', dir=args.work_dir)).resolve()
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
        ensure(all(digest(p) == pin for p, pin in zip(libraries, LIBRARIES.values())), 'pinned SDK metadata archives')
        jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        inputs = [jar, java, javac, jvm, java.parent.parent / 'lib/modules', *libraries,
                  HERE / 'RemainingDamageObservation.java', Path(__file__), HERE / 'README.md',
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
            selected = [m for m in source[name]['methods'] if m['name'] == method and m['descriptor'] == descriptor]
            ensure(len(selected) == 1 and
                   (selected[0].get('code_bytes'), selected[0].get('code_sha256')) == expected,
                   'unchanged Code pin: ' + name + '.' + method + descriptor)
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
        classpath = os.pathsep.join(str(p) for p in (classes, jar, *libraries))
        observed = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'RemainingDamageObservation.java')], output, 'compile', 30, environment)
        report['compile'] = observed
        require_success(observed, empty_stderr=True)
        helpers = report['helpers_before_sha256'] = inventory(classes)
        ensure(set(helpers) == {'RemainingDamageObservation.class', 'RemainingDamageObservation$1.class',
                                'RemainingDamageObservation$Observer.class'}, 'authored helper classes only')
        ensure(pins(inputs) == before, 'immediate pre-execution input fingerprints')
        observed = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                            '-classpath', classpath, 'RemainingDamageObservation', str(jar), str(target)],
                           output, 'original', 60, environment, output)
        report['original'] = observed
        require_success(observed)
        text = (output / observed['stdout_file']).read_text(encoding='utf-8')
        ensure(text.splitlines()[-1] == COMPLETE, 'complete actual original execution and guards')
        original = rows(text)
        entropy = [json.loads(line[len('REMAINING_DAMAGE_ENTROPY '):], object_pairs_hook=unique)
                   for line in text.splitlines() if line.startswith('REMAINING_DAMAGE_ENTROPY ')]
        ensure(len(entropy) == 10 and [r['profile'] for r in entropy] == [r['profile'] for r in original],
               'independent constructor and Item entropy retained per row')
        (output / 'rows.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf-8', newline='\n')
        report['comparator_refusals'] = controls(original)
        if native is not None:
            ensure(native.stat().st_size <= 4 * 1024 * 1024, 'bounded native stdout')
            native_bytes = native.read_bytes()
            native_text = native_bytes.decode('utf-8', errors='strict')
            ensure(any(re.fullmatch(r'PASS: [1-9][0-9]* Remaining damage.*checks', line)
                       for line in native_text.splitlines()), 'complete native fixture PASS')
            ensure(exact(rows(native_text), original), 'exact typed ten-row native/original comparison')
            (output / 'native.stdout').write_bytes(native_bytes)
            report['native_comparison'] = {'status': 'PASS', 'rows': 10,
                'input_sha256': digest(native),
                'scope': 'Declared return/state/sound/Item records; independent constructor/Item entropy excluded.'}
        report.update(status='PASS', rows=10, rows_sha256=digest(output / 'rows.json'))
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
    print('Original remaining damage ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
