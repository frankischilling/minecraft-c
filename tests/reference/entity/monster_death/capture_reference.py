"""Capture unchanged original Monster death calls; optionally compare declared native rows."""
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
CLASSES = ('fd', 'wy', 'xa', 'gr', 'sn', 'ls', 'ii', 'gz', 'uz', 'cn', 'fr', 'gb', 'bx',
           'hl', 'iz', 'ok', 'lm', 'yy', 'gm', 'dc', 'gs', 'kq', 'xi', 'ep', 'jl')
CODE = {
    ('ls', 'b', '(Lsn;)V'): (58, '82535634da0d4e66aa09f3ab01a11d5d7c5670eedbaf38b756b53bbfb1820df3'),
    ('ls', 'q', '()V'): (39, '8ceb5f932fb38e03c314f960e2e517a6169fa3b6650c4a1508fa5aaf514f0c24'),
    ('uz', 'j', '()I'): (7, '970448c31f30d4bd12d4eea77055473c701e30a5760451af0559cde8f2f9297c'),
    ('cn', 'j', '()I'): (7, '8ca71e8c2b75cf42b03609d3ae1398fbae10e88eb6c9f206ed0d04fc3b1ecb38'),
    ('fr', 'q', '()V'): (69, 'dd9e9da2b9282766a583c01ba66be56df1b6a93190189a37e0cee4e885aa4797'),
    ('fr', 'j', '()I'): (7, 'e686e51bbde5d5e7069d142e656120b35eec25d54d7782dbb56bb6d6c11c3cfb'),
    ('gb', 'b', '(Lsn;)V'): (34, '2a127d350a5263a9b2a532d39be7638c1b364e5a973beb2d272bf0591bb7997f'),
    ('gb', 'j', '()I'): (7, '3c32f2f497be9e8bb1eac2b7f215104d1cd60171ca6e02ab87f45f58e925b022'),
    ('gs', 'a', '(Lls;)V'): (15, 'd71b3b82427a9798d3632f8c3ec3d51df2210f0af6651f0e4094448291c55980'),
    ('gs', 'c', '(Lsn;I)V'): (37, '0bb323c30c345d478e30ee511ad8b2a9d485125524c56d0b6027dae4f750d994'),
    ('sn', 'c', '(Lsn;I)V'): (1, '149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d'),
    ('sn', 'a', '(Lls;)V'): (1, '149488d869cbef080602a371ab0d39d97af103fb726aaeb02ccd36c06f494e5d'),
}
PREFIX = 'MONSTER_DEATH '
COMPLETE = 'Original Monster death: 22 rows; network=0; native=0; rejected_write=0; genuine_world_chunk=1'
KEYS = {'profile', 'kind', 'mode', 'source', 'remote', 'seed', 'result', 'dying', 'dead',
        'health', 'random', 'items', 'sounds', 'player'}


def profiles():
    result = [(kind, 'direct', 'null', False, seed)
              for kind in ('uz', 'cn', 'fr', 'gb') for seed in (0, 2, 3)]
    result += [(kind, 'fatal', 'cow', False, 38) for kind in ('uz', 'cn', 'fr', 'gb')]
    result += [('gb', 'direct', 'skeleton', remote, seed)
               for remote in (False, True) for seed in (0, 2)]
    result += [(kind, 'fatal', 'player', False, 38) for kind in ('uz', 'fr')]
    return result


def unique(pairs):
    result = {}
    for key, value in pairs:
        ensure(key not in result, 'duplicate JSON field: ' + key)
        result[key] = value
    return result


def exact(a, b):
    if type(a) is not type(b):
        return False
    if type(a) is dict:
        return a.keys() == b.keys() and all(exact(a[k], b[k]) for k in a)
    if type(a) is list:
        return len(a) == len(b) and all(exact(x, y) for x, y in zip(a, b))
    return a == b


def rows(text, count=22):
    values = [json.loads(line[len(PREFIX):], object_pairs_hook=unique)
              for line in text.splitlines() if line.startswith(PREFIX)]
    ensure(count in (20, 22) and len(values) == count, 'all declared original/native rows')
    for row, (kind, mode, source, remote, seed) in zip(values, profiles()[:count]):
        name = f'{kind}-{mode}-{source}-{"remote" if remote else "local"}-{seed}'
        ensure(type(row) is dict and row.keys() == KEYS, 'exact row schema')
        ensure(exact([row[k] for k in ('profile', 'kind', 'mode', 'source', 'remote', 'seed')],
                     [name, kind, mode, source, remote, seed]), 'exact profile identity and order')
        ensure(type(row['dying']) is bool and type(row['dead']) is bool, 'actual Boolean fields')
        ensure(row['result'] is None if mode == 'direct' else type(row['result']) is bool,
               'void or actual Boolean return')
        ensure(type(row['health']) is int and type(row['random']) is str and
               re.fullmatch('[0-9a-f]{16}', row['random']), 'health and private Random state')
        ensure(type(row['items']) is list and len(row['items']) <= 4, 'bounded ordered Item list')
        for item in row['items']:
            ensure(type(item) is list and len(item) == 6 and
                   all(type(item[k]) is int for k in (0, 1, 2, 3, 5)) and type(item[4]) is bool,
                   'Item ID/count/metadata/pickup-delay/World-membership/Chunk-occurrences types')
        ensure(type(row['sounds']) is list and all(type(v) is str for v in row['sounds']),
               'ordered concrete sound declarations')
        ensure((row['player'] is None) if source != 'player' else
               (type(row['player']) is list and len(row['player']) == 3 and
                all(type(v) is int for v in row['player'])), 'Player score/kill/achievement scope')
    return values


def controls(original):
    refused = []
    for label, key, value in [('wrong_health', 'health', -13), ('wrong_random', 'random', '0' * 16),
                              ('Boolean_as_integer', 'dying', 1), ('lost_item', 'items', []),
                              ('wrong_profile', 'profile', 'wrong')]:
        altered = copy.deepcopy(original)
        index = next((i for i, row in enumerate(original) if row['items']), 0) if key == 'items' else 0
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
    parser.add_argument('--native-rows', type=int, choices=(20, 22), default=20,
                        help='Compare first 20 non-Player rows or all 22 rows; default excludes two Player records')
    parser.add_argument('--library-root', type=Path, default=(Path(os.environ['APPDATA']) / '.minecraft/libraries'
                        if 'APPDATA' in os.environ else Path.home() / '.minecraft/libraries'))
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='monster-death-', dir=args.work_dir)).resolve()
    classes = output / 'classes'
    classes.mkdir()
    target = output / 'target'
    target.mkdir()
    report = {'status': 'FAIL', 'native_comparison': None}
    inputs, before, failure = [], None, None
    try:
        jar = args.jar.resolve(strict=True)
        java = Path(shutil.which(args.java) or '').resolve(strict=True)
        javac = Path(shutil.which(args.javac) or '').resolve(strict=True)
        ensure(java.parent.samefile(javac.parent), 'matching JDK tools')
        ensure(digest(jar) == JAR, 'verified original archive')
        libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
        ensure(all(digest(p) == pin for p, pin in zip(libraries, LIBRARIES.values())), 'pinned SDK metadata archives')
        jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        inputs = [jar, java, javac, jvm, java.parent.parent / 'lib/modules', *libraries,
                  HERE / 'MonsterDeathObservation.java', Path(__file__), HERE / 'README.md',
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
                            str(HERE / 'MonsterDeathObservation.java')], output, 'compile', 30, environment)
        report['compile'] = observed
        require_success(observed, empty_stderr=True)
        helpers = report['helpers_before_sha256'] = inventory(classes)
        ensure(set(helpers) == {'MonsterDeathObservation.class', 'MonsterDeathObservation$1.class',
                                'MonsterDeathObservation$Observer.class'}, 'authored helper classes only')
        ensure(pins(inputs) == before, 'immediate pre-execution input fingerprints')
        observed = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                            '-classpath', classpath, 'MonsterDeathObservation', str(jar), str(target)],
                           output, 'original', 60, environment, output)
        report['original'] = observed
        require_success(observed)
        text = (output / observed['stdout_file']).read_text(encoding='utf-8')
        ensure(text.splitlines()[-1] == COMPLETE, 'complete original execution and guard receipt')
        original = rows(text)
        entropy = [json.loads(line[len('MONSTER_DEATH_ENTROPY '):], object_pairs_hook=unique)
                   for line in text.splitlines() if line.startswith('MONSTER_DEATH_ENTROPY ')]
        ensure(len(entropy) == 22 and [r['profile'] for r in entropy] == [r['profile'] for r in original],
               'independent constructor and dropped Item entropy retained per row')
        (output / 'rows.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf-8', newline='\n')
        report['comparator_refusals'] = controls(original)
        if native is not None:
            ensure(native.stat().st_size <= 4 * 1024 * 1024, 'bounded native stdout')
            native_bytes = native.read_bytes()
            native_text = native_bytes.decode('utf-8', errors='strict')
            ensure(any(re.fullmatch(r'PASS: [1-9][0-9]* Monster death.*checks', line)
                       for line in native_text.splitlines()), 'complete native fixture PASS')
            ensure(exact(rows(native_text, args.native_rows), original[:args.native_rows]),
                   'exact typed declared native/original row comparison')
            (output / 'native.stdout').write_bytes(native_bytes)
            report['native_comparison'] = {'status': 'PASS', 'rows': args.native_rows,
                'input_sha256': digest(native),
                'excluded_original_profiles': [row['profile'] for row in original[args.native_rows:]],
                'scope': 'Declared deterministic return/state/items only; independent Item and constructor entropy excluded.'}
        report.update(status='PASS', rows=22, rows_sha256=digest(output / 'rows.json'))
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
    print('Original Monster death ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
