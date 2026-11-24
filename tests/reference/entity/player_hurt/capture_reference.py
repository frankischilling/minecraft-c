"""Capture unchanged LocalPlayer hurt calls on genuine original owners."""
import argparse
import json
import importlib.util
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / 'client/controller'))
from capture import capture, digest, ensure, require_success
_class_spec = importlib.util.spec_from_file_location('player_hurt_classfile', HERE.parent / 'player/classfile.py')
_classfile = importlib.util.module_from_spec(_class_spec)
_class_spec.loader.exec_module(_classfile)
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SOURCES = ('PlayerHurtObservation.java', 'capture_reference.py', 'profiles.json', 'profiles.tsv',
           'source-pins.json', 'player-fields.json', '../player/ConstructorJson.java',
           '../../client/controller/capture.py', '../player/classfile.py')


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        ensure(key not in value, 'duplicate JSON key: ' + key)
        value[key] = item
    return value


def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique_object)


def equal(a, b):
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(equal(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(equal(x, y) for x, y in zip(a, b))
    return a == b


def profiles():
    result = [[int(x) for x in line.split()] for line in
              (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
    declared = read_json(HERE / 'profiles.json')
    ensure(len(result) == len(declared) == 22, 'Player profile count')
    for index, (row, entry) in enumerate(zip(result, declared)):
        expected = [index, entry['health'], entry['by'], entry['previous_damage'], entry['remainder'],
                    entry['item'], entry['damage'], entry['mode'], len(entry['amounts']), *entry['amounts']]
        ensure(row == expected and entry['profile'] == index and
               entry['actor_seed'] == 38 and entry['world_seed'] == 2468 and entry['math_seed'] == 0,
               'Player exact declared profile: ' + str(index))
    ensure(sum(row[8] for row in result) == 28, 'Player call count')
    return result


def state(value):
    ensure(type(value) is dict and set(value) == {'fields', 'bounds', 'filters', 'inventory',
           'random', 'world', 'writer', 'aliases', 'sounds'}, 'Player state schema')
    fields = read_json(HERE / 'player-fields.json')
    ensure(type(value['fields']) is dict and value['fields'].keys() == fields.keys(), 'Player field inventory')
    def word(item, count):
        return type(item) is str and re.fullmatch('[0-9a-f]{' + str(count) + '}', item) is not None
    for name, kind in fields.items():
        item = value['fields'][name]
        ensure((kind == 'z' and type(item) is bool) or
               (kind == 'i' and type(item) is int and -(1 << 31) <= item < (1 << 31)) or
               (kind in ('f', 'd') and type(item) is dict and set(item) == {kind.upper()} and
                word(item[kind.upper()], 8 if kind == 'f' else 16)) or
               (kind == 'r' and (item is None or item == {'nonnull': True})) or
               (kind == 's' and (item is None or type(item) is dict and set(item) == {'utf16'} and
                type(item['utf16']) is list and all(type(x) is int and 0 <= x <= 65535 for x in item['utf16']))),
               'Player typed field ' + name)
    ensure(type(value['bounds']) is list and len(value['bounds']) == 6 and
           all(type(x) is dict and set(x) == {'D'} and word(x['D'], 16) for x in value['bounds']), 'Player bounds')
    ensure(type(value['filters']) is list and len(value['filters']) == 3 and all(
           type(x) is dict and set(x) == {'cu.a', 'cu.b', 'cu.c'} and all(
           type(y) is dict and set(y) == {'F'} and word(y['F'], 8) for y in x.values()) for x in value['filters']),
           'Player filters')
    inv = value['inventory']
    ensure(type(inv) is dict and set(inv) == {'visible', 'main', 'armor', 'selected', 'cursor', 'stack'} and
           type(inv['visible']) is bool and type(inv['selected']) is int and
           inv['cursor'] is None, 'Player Inventory shape')
    for key, length in (('main', 36), ('armor', 4)):
        ensure(inv[key] is None and key == 'armor' or type(inv[key]) is list and len(inv[key]) == length and
               all(x is None or x == 'stack' for x in inv[key]), 'Player slot shape')
    stack = inv['stack']
    if stack is not None:
        ensure(type(stack) is dict and set(stack) == {'id', 'count', 'damage', 'animation', 'item'} and
               all(type(stack[x]) is int for x in ('id', 'count', 'damage', 'animation')), 'Player Stack shape')
        item = stack['item']
        ensure(item is None or type(item) is dict and set(item) == {'class', 'maximum', 'defense'} and
               item['class'] in ('wa', 'other') and type(item['maximum']) is int and
               (type(item['defense']) is int if item['class'] == 'wa' else item['defense'] is None), 'Player Item shape')
    ensure(type(value['random']) is list and len(value['random']) == 3, 'Player Random shape')
    for random in value['random']:
        ensure(random is None or type(random) is list and len(random) == 3 and word(random[0], 16) and
               int(random[0], 16) < (1 << 48) and type(random[1]) is bool and word(random[2], 16), 'Player Random fields')
    ensure(type(value['world']) is dict and set(value['world']) == {'fields', 'counts'} and
           type(value['world']['fields']) is dict and type(value['world']['counts']) is list and
           len(value['world']['counts']) == 4 and all(type(x) is int for x in value['world']['counts']), 'Player World shape')
    writer = value['writer']
    ensure(type(writer) is dict and set(writer) == {'total', 'unsent', 'dirty'} and type(writer['dirty']) is bool,
           'Player Writer shape')
    for key in ('total', 'unsent'):
        rows = writer[key]
        ensure(type(rows) is list and all(type(x) is list and len(x) == 2 and all(type(y) is int for y in x) for x in rows)
               and all(rows[i][0] < rows[i + 1][0] for i in range(len(rows) - 1)), 'Player Writer map')
    ensure(type(value['aliases']) is dict and set(value['aliases']) == {'world', 'app', 'writer', 'inventory',
           'main_array', 'armor_array', 'bounds', 'actor_random', 'default_active_container', 'filters_distinct'} and
           all(type(x) is bool for x in value['aliases'].values()), 'Player alias shape')
    ensure(type(value['sounds']) is list and len(value['sounds']) <= 4, 'Player sound count')
    for sound in value['sounds']:
        ensure(type(sound) is dict and set(sound) == {'name', 'position', 'volume', 'pitch'} and
               sound['name'] == 'random.hurt' and type(sound['position']) is list and len(sound['position']) == 3 and
               all(word(x, 16) for x in sound['position']) and word(sound['volume'], 8) and word(sound['pitch'], 8),
               'Player sound fields')


def rows(text):
    found = [json.loads(line[len('PLAYER_HURT '):], object_pairs_hook=unique_object)
             for line in text.splitlines() if line.startswith('PLAYER_HURT ')]
    expected = [(profile, step, row) for profile, row in enumerate(profiles()) for step in range(row[8])]
    ensure(len(found) == len(expected) == 28, 'Player row count')
    for index, (item, (profile, step, row)) in enumerate(zip(found, expected)):
        ensure(type(item) is dict and set(item) == {'schema', 'case', 'profile', 'step', 'input', 'amount',
               'scope', 'result', 'exception', 'before', 'state', 'completion'} and item['schema'] == 'PLAYER_HURT_V1' and
               type(item['case']) is int and item['case'] == index and type(item['profile']) is int and
               item['profile'] == profile and type(item['step']) is int and item['step'] == step and
               equal(item['input'], row) and type(item['amount']) is int and item['amount'] == row[9 + step], 'Player row identity')
        ensure(item['scope'] in ('RETURN', 'EXCEPTION', 'FATAL_PREFIX') and
               item['exception'] in ('NONE', 'NPE', 'INDEX', 'CALLBACK', 'IO', 'DEPENDENCY'), 'Player outcome shape')
        ensure(item['result'] is None or type(item['result']) is bool, 'Player result type')
        state(item['before']); state(item['state'])
        if item['completion'] is not None:
            ensure(profile == 21 and type(item['completion']) is dict and
                   set(item['completion']) == {'result', 'exception', 'state'} and
                   item['completion']['result'] is True and item['completion']['exception'] == 'NONE', 'Player fatal completion')
            state(item['completion']['state'])
    return found


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*') if p.is_file()}


def compare_original_runs(a, b):
    independent_player = ('sn.aD', 'ls.F', 'ls.G', 'ls.aj')
    independent_world = ('fd.I', 'fd.Q', 'fd.g')
    def visit(x, y, path):
        ensure(type(x) is type(y), 'Player original VM type: ' + path)
        if isinstance(x, dict):
            ensure(x.keys() == y.keys(), 'Player original VM keys: ' + path)
            for key in x:
                if (path in ('/before/fields', '/state/fields', '/completion/state/fields') and key in independent_player or
                    path in ('/before/world/fields', '/state/world/fields', '/completion/state/world/fields') and key in independent_world):
                    continue
                visit(x[key], y[key], path + '/' + key)
        elif isinstance(x, list):
            ensure(len(x) == len(y), 'Player original VM length: ' + path)
            for i, (left, right) in enumerate(zip(x, y)):
                visit(left, right, path + '/' + str(i))
        else:
            ensure(x == y, 'Player original VM value: ' + path)
    ensure(len(a) == len(b) == 28, 'Player original VM count')
    for left, right in zip(a, b):
        for row in (left, right):
            for key in independent_player:
                ensure(equal(row['before']['fields'][key], row['state']['fields'][key]), 'Player constructor field preservation')
            for key in independent_world:
                ensure(equal(row['before']['world']['fields'][key], row['state']['world']['fields'][key]), 'Player World constructor preservation')
        visit(left, right, '')


def verify_pins(jar):
    pins = read_json(HERE / 'source-pins.json')
    ensure(digest(jar) == JAR and pins['archive']['sha256'] == JAR and
           pins['archive']['bytes'] == jar.stat().st_size, 'Player original archive')
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins['classes'].items():
            actual = _classfile.parse_class(archive.read(name + '.class'))
            del actual['fields']
            signatures = {(x['name'], x['descriptor']) for x in expected['methods']}
            actual['methods'] = [x for x in actual['methods'] if (x['name'], x['descriptor']) in signatures]
            ensure(equal(actual, expected), 'Player original class/method pin: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(x)).resolve(strict=True) for x in ('java', 'javac')]
    jar = args.client_jar.resolve(strict=True)
    libs = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(path) == pin for path, pin in zip(libs, LIBRARIES.values())), 'Player SDK pins')
    verify_pins(jar); profiles()
    classes = args.output / 'classes'; classes.mkdir()
    classpath = os.pathsep.join(str(p) for p in (classes.resolve(), jar, *libs))
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    inputs = [java, javac, jar, *libs, *[HERE / name for name in SOURCES]]
    before = {str(p.resolve()): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before}
    try:
        for name, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], args.output, name, 10, environment)
            require_success(receipt)
            version = ''.join((args.output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            ensure(re.search(r'(?:version "|javac )17\.', version), 'Player JDK17')
            report[name] = receipt
        compiled = capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes.resolve()),
                            str(HERE / 'PlayerHurtObservation.java'), str(HERE / '../player/ConstructorJson.java')],
                           args.output, 'compile', 60, environment)
        report['compile'] = compiled; require_success(compiled, empty_stderr=True)
        report['helper_before_sha256'] = inventory(classes)
        names = args.output / 'class-names.txt'
        names.write_text('\n'.join(read_json(HERE / 'source-pins.json')['classes']) + '\n', encoding='ascii')
        captures, observations = [], []
        for index in range(2):
            work = args.output / ('original-work-' + str(index)); work.mkdir()
            binding = work / 'binding.tsv'
            receipt = capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                               '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                               '--add-opens=java.base/java.lang=ALL-UNNAMED', '-classpath', classpath,
                               'PlayerHurtObservation', str(jar), str(work.resolve()), str((HERE / 'profiles.tsv').resolve()),
                               str(names.resolve()), str(binding.resolve())], args.output, 'original-' + str(index),
                              90, environment, work.resolve())
            captures.append(receipt); report['original'] = captures; require_success(receipt)
            text = (args.output / receipt['stdout_file']).read_text()
            ensure(text.splitlines()[-1] == 'Original Player hurt: 28 rows; 22 profiles; network=0; native=0; rejected_writes=0',
                   'Player original completion/guards')
            observed = rows(text)
            observations.append(observed)
            expected_binding = ''.join(name + '\t' + str(value['class_bytes']) + '\t' + value['class_sha256'] + '\n'
                                       for name, value in read_json(HERE / 'source-pins.json')['classes'].items())
            ensure(binding.read_text() == expected_binding, 'Player actual original class origins')
            (args.output / ('finite-' + str(index) + '.json')).write_text(json.dumps({'rows': observed,
                'source_sha256': {name: digest(HERE / name) for name in SOURCES}}, indent=2) + '\n', encoding='utf-8')
        compare_original_runs(*observations)
        provenance = {'schema': 'PLAYER_HURT_CAPTURE_V1', 'status': 'PASS', 'archive_sha256': JAR,
                      'finite_sha256': digest(args.output / 'finite-0.json'),
                      'source_sha256': {name: digest(HERE / name) for name in SOURCES},
                      'tool_sha256': {p.name: digest(p) for p in (java, javac)},
                      'helper_sha256': report['helper_before_sha256'],
                      'original': [{k: v for k, v in receipt.items() if k != 'command'} for receipt in captures],
                      'input_unchanged': True, 'helper_unchanged': True, 'rows_per_vm': 28, 'profiles_per_vm': 22,
                      'independent_player_fields': ['sn.aD', 'ls.F', 'ls.G', 'ls.aj'],
                      'independent_world_fields': ['fd.I', 'fd.Q', 'fd.g']}
        ensure(before == {str(p.resolve()): digest(p) for p in inputs} and
               inventory(classes) == report['helper_before_sha256'], 'Player capture input/helper guard')
        (args.output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
        report['status'] = 'PASS'
    finally:
        report['input_after_sha256'] = {str(p.resolve()): digest(p) for p in inputs}
        report['helper_after_sha256'] = inventory(classes)
        unchanged = before == report['input_after_sha256'] and report.get('helper_before_sha256') == report['helper_after_sha256']
        if not unchanged:
            report['status'] = 'FAIL'
        (args.output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        ensure(unchanged, 'Player original inputs/helper changed')
    print('Original Player hurt: two VMs, 28 calls each; ' + str(args.output / 'capture.json'))


if __name__ == '__main__':
    main()
