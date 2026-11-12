"""Run unchanged client UW.K on genuine Worlds/Chunks; retain bounded Java evidence."""
import argparse
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
sys.path.pop(0)  # Do not shadow this packet's compare.py with the support module.

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
UW_K = (144, 'd3fa87376c230baf75e13c90f711bed5b071d09ca2890e8576dc66a3dd060cd5')
CLASSES = ('uw', 'ls', 'sn', 'fd', 'ok', 'lm', 'wy', 'xa', 'gr', 'ud', 'pm', 'eq', 'yy', 'in')
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
SOURCES = ('SlimeRemovalObservation.java', 'capture_reference.py', 'compare.py', 'test_packet.py',
           'profiles.tsv', 'source-pins.json', 'README.md')
PREFIX = 'SLIME_REMOVAL '
DIRECT_PROFILES = ('direct-split', 'size-one', 'size-zero', 'remote', 'positive-health',
                   'null-world', 'watcher-type', 'callback-pose', 'callback-size',
                   'callback-reseed', 'callback-null-world')
ROW_KEYS = {'profile', 'step', 'set_size', 'size', 'health', 'remote', 'exception', 'dead',
            'random', 'world_loaded', 'children'}
CHILD_KEYS = {'index', 'size', 'health', 'position', 'yaw', 'pitch', 'world', 'world_member',
              'chunk_members', 'parent_dead', 'random_after'}


def unique(pairs):
    value = {}
    for key, cell in pairs:
        ensure(key not in value, 'duplicate JSON key: ' + key)
        value[key] = cell
    return value


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def exact(left, right):
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(exact(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(exact(a, b) for a, b in zip(left, right))
    return left == right


def profiles():
    result = []
    for line in (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines():
        cells = line.split('\t')
        ensure(len(cells) == 7 and re.fullmatch('[a-z0-9-]+', cells[0]), 'declared profile shape')
        values = list(map(int, cells[1:]))
        ensure(values[2] in (0, 1, 2) and values[3] in (1, 2) and values[4] in (0, 1) and
               0 <= values[5] <= 7, 'bounded declared calls/mutations')
        result.append((cells[0], values))
    ensure(len(result) == 24 and len(set(name for name, _ in result)) == 24 and
           tuple(name for name, _ in result[:11]) == DIRECT_PROFILES, 'finite ordered profile inventory')
    return result


def word(value, count):
    return type(value) is str and re.fullmatch('[0-9a-f]{' + str(count) + '}', value) is not None


def validate_row(row):
    ensure(type(row) is dict and row.keys() == ROW_KEYS, 'exact Slime observation keys')
    ensure(type(row['profile']) is str and type(row['step']) is int and 0 <= row['step'] <= 1,
           'named bounded actual call')
    ensure(all(type(row[k]) is int for k in ('set_size', 'health', 'world_loaded')) and
           (row['size'] is None or type(row['size']) is int), 'integer Slime state, nullable throwing getter')
    ensure(row['remote'] is None or type(row['remote']) is bool, 'nullable actual World remote Boolean')
    ensure(type(row['dead']) is bool and row['exception'] in ('NONE', 'NPE', 'CCE', 'CALLBACK'),
           'actual source completion and parent removed flag')
    ensure(row['random'] is None or word(row['random'], 16), 'parent Random state word')
    ensure(type(row['children']) is list and len(row['children']) <= 8, 'bounded cumulative child callbacks')
    for index, child in enumerate(row['children']):
        ensure(type(child) is dict and child.keys() == CHILD_KEYS, 'exact child observation keys')
        ensure(all(type(child[k]) is int for k in ('index', 'size', 'health', 'chunk_members')) and
               child['index'] == index, 'actual callback order and signed child state')
        ensure(type(child['position']) is list and len(child['position']) == 3 and
               all(word(v, 16) for v in child['position']) and word(child['yaw'], 8) and
               word(child['pitch'], 8), 'exact binary64 pose and binary32 rotation')
        ensure(child['world'] in ('PRIMARY', 'SECONDARY', None) and
               type(child['world_member']) is bool and type(child['parent_dead']) is bool,
               'actual World/membership and callback-time parent flag')
        ensure(child['random_after'] is None or word(child['random_after'], 16), 'actual callback-time parent stream')
    return row


def rows(text, complete=False):
    result = [validate_row(json.loads(line[len(PREFIX):], object_pairs_hook=unique))
              for line in text.splitlines() if line.startswith(PREFIX)]
    ensure(result, 'at least one actual Slime observation')
    identities = [(row['profile'], row['step']) for row in result]
    ensure(len(identities) == len(set(identities)), 'no duplicate actual calls')
    if complete:
        expected = [(name, step) for name, values in profiles() for step in range(values[3])]
        ensure(identities == expected, 'complete exact ordered original call inventory')
        ensure(text.splitlines()[-1] == footer(len(expected)), 'actual original completion and guards')
        allowed = ('16 achievements', '151 recipes', 'SLIME_REMOVAL_REFERENCE_BEGIN schema=1 profiles=24',
                   footer(len(expected)))
        ensure(all(line in allowed or line.startswith((PREFIX, 'SLIME_REMOVAL_ENTROPY '))
                   for line in text.splitlines()), 'no unclassified original output')
    return result


def footer(count):
    return f'Original Slime removal: {count} rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9'


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(root.rglob('*')) if p.is_file()}


def pins(paths):
    return {str(p.resolve()): digest(p) for p in paths}


def source_pins(jar):
    with zipfile.ZipFile(jar) as archive:
        return {'client_jar_sha256': digest(jar),
                'classes': {name: inspect_class(archive.read(name + '.class')) for name in CLASSES}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, default=(Path(os.environ['APPDATA']) / '.minecraft/libraries'
                        if 'APPDATA' in os.environ else Path.home() / '.minecraft/libraries'))
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='slime-original-', dir=args.work_dir)).resolve()
    classes, target = output / 'classes', output / 'target'; classes.mkdir(); target.mkdir()
    print('Original Slime capture retained at ' + str(output), flush=True)
    report = {'status': 'FAIL', 'scope': 'Direct unchanged UW.K only; no delayed Living tick/particle continuation'}
    inputs, before, failure = [], None, None
    try:
        jar = args.jar.resolve(strict=True)
        java = Path(shutil.which(args.java) or '').resolve(strict=True)
        javac = Path(shutil.which(args.javac) or '').resolve(strict=True)
        javap = java.with_name('javap.exe' if os.name == 'nt' else 'javap')
        ensure(java.parent.samefile(javac.parent) and javap.is_file(), 'matching complete JDK tools')
        ensure(digest(jar) == JAR, 'verified unchanged client archive')
        libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
        ensure(all(digest(p) == pin for p, pin in zip(libraries, LIBRARIES.values())), 'pinned SDK metadata')
        inputs = [jar, java, javac, javap, java.parent.parent / 'lib/modules',
                  java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so',
                  *libraries, *(HERE / name for name in SOURCES), SUPPORT / 'capture.py', SUPPORT / 'classfile.py']
        before = report['input_before_sha256'] = pins(inputs)
        report['source_before_sha256'] = {name: digest(HERE / name) for name in SOURCES}
        source = source_pins(jar)
        ensure(exact(source, read_json(HERE / 'source-pins.json')), 'all original class/Code pins')
        selected = [m for m in source['classes']['uw']['methods'] if m['name'] == 'K' and m['descriptor'] == '()V']
        ensure(len(selected) == 1 and (selected[0]['code_bytes'], selected[0]['code_sha256']) == UW_K,
               'audited original UW.K exact144-byte Code identity')
        report['original_classes'] = source
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
            environment.pop(key, None)
        for label, tool in (('java', java), ('javac', javac)):
            observed = capture([str(tool), '-version'], output, label, 10, environment)
            report[label] = observed; require_success(observed)
            text = (output / observed['stdout_file']).read_text() + (output / observed['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', text), 'JDK17 required')
        observed = capture([str(javap), '-classpath', str(jar), '-p', '-c', '-s', 'uw', 'sn', 'ls'],
                           output, 'javap', 20, environment)
        report['javap'] = observed; require_success(observed, empty_stderr=True)
        classpath = os.pathsep.join(str(p) for p in (classes, jar, *libraries))
        observed = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                            str(HERE / 'SlimeRemovalObservation.java')], output, 'compile', 30, environment)
        report['compile'] = observed; require_success(observed, empty_stderr=True)
        helpers = report['helpers_before_sha256'] = inventory(classes)
        ensure(set(helpers) == {'SlimeRemovalObservation.class', 'SlimeRemovalObservation$1.class',
                                'SlimeRemovalObservation$Observer.class'}, 'authored helper classes only')
        ensure(pins(inputs) == before, 'immediate pre-execution input identities')
        observed = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                            '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                            '--add-opens=java.base/java.lang=ALL-UNNAMED', '-classpath', classpath,
                            'SlimeRemovalObservation', str(jar), str(HERE / 'profiles.tsv'), str(target)],
                           output, 'original', 60, environment, output)
        report['original'] = observed; require_success(observed)
        text = (output / observed['stdout_file']).read_text(encoding='utf-8')
        original = rows(text, complete=True)
        entropy = [json.loads(line[len('SLIME_REMOVAL_ENTROPY '):], object_pairs_hook=unique)
                   for line in text.splitlines() if line.startswith('SLIME_REMOVAL_ENTROPY ')]
        ensure([(r['profile'], r['step']) for r in entropy] == [(r['profile'], r['step']) for r in original],
               'separate actual constructor/Math entropy per invocation')
        (output / 'rows.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=len(original), rows_sha256=digest(output / 'rows.json'))
    except Exception as error:
        failure = error; report['error'] = f'{type(error).__name__}: {error}'
        (output / 'driver-error.txt').write_text(traceback.format_exc(), encoding='utf-8', newline='\n')
    finally:
        try:
            report['input_after_sha256'] = pins(inputs)
            report['helpers_after_sha256'] = inventory(classes)
            report['inputs_unchanged'] = before is not None and report['input_after_sha256'] == before
            report['helpers_unchanged'] = report.get('helpers_before_sha256') == report['helpers_after_sha256']
            ensure(report['inputs_unchanged'] and report['helpers_unchanged'], 'immutable input/helper identities')
        except Exception as error:
            report['status'] = 'FAIL'; report['final_audit_error'] = f'{type(error).__name__}: {error}'
            failure = failure or error
        report['output_sha256'] = inventory(output)
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Original Slime removal ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
