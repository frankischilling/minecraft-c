"""Observe unchanged original Chest predicates on genuine saved Worlds."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
CORE = ('e', 'fd', 'lm', 'mx', 'uu', 'xp', 'pe', 'st', 'eb', 'pm', 'ok', 'wi', 'xa')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('chest_placement_transport', ROOT / 'tests/reference/client/controller/capture.py')
I = module('chest_placement_classfile', ROOT / 'tests/reference/client/controller/classfile.py')
W = module('chest_placement_world_inputs', ROOT / 'tests/reference/client/picking/observe.py')


def unique(pairs):
    result = dict(pairs)
    C.ensure(len(result) == len(pairs), 'unique JSON fields')
    return result


def declarations(cases):
    lines = []
    for case in cases:
        cells = ';'.join(','.join(map(str, cell)) for cell in case['chests']) or '-'
        lines.append('\t'.join(map(str, (case['name'], case['operation'], case['x'], case['y'],
            case['z'], case['target_id'], str(case['null_world']).lower(), cells))))
    return '\n'.join(lines) + '\n'


def inventory(directory):
    return {path.relative_to(directory).as_posix(): {'bytes': path.stat().st_size,
            'sha256': C.digest(path)} for path in sorted(directory.rglob('*')) if path.is_file()}


def read_output(raw, cases, pins):
    rows, entropy, origins, complete, preamble = [], [], [], [], []
    prefixes = (('CHEST_PLACEMENT ', rows), ('CHEST_RANDOM ', entropy),
                ('CHEST_ORIGINS ', origins), ('CHEST_COMPLETE ', complete))
    for line in raw.decode('utf-8').splitlines():
        for prefix, target in prefixes:
            if line.startswith(prefix):
                target.append(json.loads(line[len(prefix):], object_pairs_hook=unique))
                break
        else:
            if line:
                preamble.append(line)
    C.ensure(preamble == ['16 achievements', '151 recipes'], 'exact original registry preamble')
    C.ensure(origins == [{'jar_sha256': JAR, 'classes': {
        name: value['class_sha256'] for name, value in pins['classes'].items()}}], 'loaded original resources and archive agree')
    C.ensure([row['name'] for row in rows] == [case['name'] for case in cases] and
             [row['index'] for row in rows] == list(range(len(cases))), 'all declared original predicates')
    C.ensure([row['name'] for row in entropy] == [case['name'] for case in cases] and
             [row['index'] for row in entropy] == list(range(len(cases))), 'all raw Random observations')
    C.ensure(complete == [{'rows': len(cases), 'network': 0, 'native': 0, 'execs': 0,
        'denied_writes': 0, 'world_writes': 3, 'canonical_chunks': 4, 'registry_unchanged': True,
        'remote': False}], 'complete guarded genuine World cohort')
    for row, random, case in zip(rows, entropy, cases):
        C.ensure(all(row[key] == case[key] for key in ('name', 'operation', 'x', 'y', 'z', 'target_id', 'null_world')),
                 'literal declared predicate arguments')
        C.ensure(row['cells'] == [cell + [54] for cell in case['chests']], 'actual authored sparse Chest cells')
        C.ensure(row['array_changes'] == 0 and row['world_fields_unchanged'] is True and
                 row['chunk_fields_unchanged'] is True and row['rng_unchanged'] is True and
                 row['queues_before'] == row['queues_after'] and row['events'] == [], 'observed read-only predicate state')
        C.ensure(random['before'] == random['after'], 'same actual World Random state before and after')
        C.ensure((row['result'] in ('true', 'false') and row['exception'] == 'NONE') or
                 (row['result'] == 'UNRETURNED' and row['exception'] == 'java.lang.NullPointerException'),
                 'literal original result or failure')
    return rows, entropy


def expected_warnings(classes):
    uri = classes.as_uri().replace('file:///', 'file:/') + '/'
    return ['WARNING: A terminally deprecated method in java.lang.System has been called',
            'WARNING: System::setSecurityManager has been called by ChestPlacementObservation (' + uri + ')',
            'WARNING: Please consider reporting this to the maintainers of ChestPlacementObservation',
            'WARNING: System::setSecurityManager will be removed in a future release']


def read_trace(directory, cases, pins):
    rows = [json.loads(line, object_pairs_hook=unique) for line in
            (directory / 'queries.ndjson').read_text(encoding='utf-8').splitlines()]
    C.ensure([row['index'] for row in rows] == list(range(len(cases))), 'complete ordered JDI case boundaries')
    code = json.loads((directory / 'live-Code.json').read_text(encoding='utf-8'), object_pairs_hook=unique)
    expected = {}
    for owner, names in (('e', {'a', 'h'}), ('fd', {'a'})):
        descriptor = '(Lfd;III)Z' if owner == 'e' else '(III)I'
        for method in pins['classes'][owner]['methods']:
            if method['name'] in names and method['descriptor'] == descriptor:
                expected[owner + '.' + method['name'] + descriptor] = {
                    key: method[key] for key in ('code_bytes', 'code_sha256')}
    C.ensure(code == expected, 'entered live original Code equals archive')
    for row, case in zip(rows, cases):
        C.ensure(set(row) == {'index', 'queries', 'entered_E'} and 0 <= len(row['queries']) <= 32 and
                 1 <= len(row['entered_E']) <= 5, 'bounded selected live trace')
        first = row['entered_E'][0]
        C.ensure(first == {'method': 'e.' + ('a' if case['operation'] == 0 else 'h') + '(Lfd;III)Z',
            'world_null': case['null_world'], 'xyz': [case['x'], case['y'], case['z']]}, 'actual entered predicate arguments')
        for query in row['queries']:
            C.ensure(set(query) == {'xyz', 'source_frames', 'result'} and len(query['xyz']) == 3 and
                all(type(value) is int and -2147483648 <= value <= 2147483647 for value in query['xyz']) and
                type(query['result']) is int and 0 <= query['result'] <= 255 and
                1 <= len(query['source_frames']) <= 2, 'actual World query arguments/result')
            for frame in query['source_frames']:
                C.ensure(set(frame) == {'method', 'pc'} and frame['method'] in expected and
                    frame['method'].startswith('e.') and type(frame['pc']) is int and
                    0 <= frame['pc'] < expected[frame['method']]['code_bytes'], 'actual original caller code location')
    return rows, code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'verified original client archive')
    libraries = [(args.libraries / name).resolve(strict=True) for name in W.LIBRARIES]
    C.ensure(all(C.digest(path) == value for path, value in zip(libraries, W.LIBRARIES.values())), 'pinned SDK libraries')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'same JDK17')
    jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    cases = json.loads((HERE / 'cases.json').read_text(encoding='utf-8'), object_pairs_hook=unique)['cases']
    declared = output / 'profiles.tsv'
    declared.write_text(declarations(cases), encoding='utf-8', newline='\n')
    helpers = [HERE / 'ChestPlacementObservation.java', HERE / 'ChestPlacementJdi.java', HERE / 'cases.json', Path(__file__).resolve(),
               ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/reference/client/controller/classfile.py',
               ROOT / 'tests/reference/client/picking/observe.py', ROOT / 'tests/tools/client/test_picking.py']
    inputs = [jar, *libraries, java, javac, jvm, java.parent.parent / 'lib/modules', *helpers, declared]
    before = {str(path): C.digest(path) for path in inputs}
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before, 'rows': None}
    with zipfile.ZipFile(jar) as archive:
        report['pins'] = {'client_jar_sha256': JAR, 'classes': {
            name: I.inspect_class(archive.read(name + '.class')) for name in CORE}}
    worlds = [output / stem / 'world' for stem in ('interpreter', 'default', 'trace-interpreter', 'trace-default')]
    for world in worlds:
        W.world(world)
    world_before = [inventory(world) for world in worlds]
    C.ensure(all(value == world_before[0] for value in world_before), 'identical frozen saved World inputs')
    report['worlds_before'] = world_before
    environment = os.environ.copy()
    report['removed_environment_names'] = [name for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS')
                                           if environment.pop(name, None) is not None]
    cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
    try:
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '-implicit:none', '-classpath', cp,
            '-d', str(classes), str(HERE / 'ChestPlacementObservation.java'), str(HERE / 'ChestPlacementJdi.java')], output, 'compile', 60, environment)
        C.require_success(report['compile'], empty_stderr=True)
        compiled = sorted(classes.rglob('*.class'))
        C.ensure(compiled and all(path.name.startswith(('ChestPlacementObservation', 'ChestPlacementJdi')) for path in compiled), 'only authored observer classes')
        report['compiled_before'] = {str(path.relative_to(output)): C.digest(path) for path in compiled}
        report['java_version'] = C.capture([str(java), '-version'], output, 'java-version', 20, environment)
        C.require_success(report['java_version'])
        report['javac_version'] = C.capture([str(javac), '-version'], output, 'javac-version', 20, environment)
        C.require_success(report['javac_version'], empty_stderr=True)
        report['entropy'] = {}
        for index, (stem, flags) in enumerate((('interpreter', ['-Xint']), ('default', []))):
            report[stem] = C.capture([str(java), *flags, '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED', '-classpath', cp,
                'ChestPlacementObservation', str(jar), str(worlds[index]), str(declared)], output, stem, 120, environment)
            C.require_success(report[stem])
            C.ensure((output / (stem + '.stderr')).read_text(encoding='utf-8').splitlines() == expected_warnings(classes), 'only declared JDK17 guard warnings')
            rows, entropy = read_output((output / (stem + '.stdout')).read_bytes(), cases, report['pins'])
            if report['rows'] is None:
                report['rows'] = rows
            else:
                C.ensure(rows == report['rows'], 'literal logical predicate rows agree between VM modes')
            report['entropy'][stem] = entropy
            after = inventory(worlds[index])
            C.ensure(set(after) == set(world_before[index]) and all(after[name] == pin for name, pin in
                world_before[index].items() if name != 'session.lock') and after['session.lock']['bytes'] == 8,
                'only actual source session lock changes on disk')
        report['traces'], report['live_Code'], report['trace_artifacts'] = {}, {}, {}
        for index, mode in enumerate(('interpreter', 'default'), 2):
            stem, trace = 'trace-' + mode, output / ('trace-' + mode + '-jdi')
            report[stem] = C.capture([str(java), '-classpath', cp, 'ChestPlacementJdi', cp, str(jar),
                str(java.parent.parent), str(trace), str(worlds[index]), str(declared), mode],
                output, stem, 180, environment)
            C.require_success(report[stem], empty_stderr=True)
            C.ensure((trace / 'target.stderr').read_text(encoding='utf-8').splitlines() == expected_warnings(classes), 'only declared traced-target warnings')
            rows, entropy = read_output((trace / 'target.stdout').read_bytes(), cases, report['pins'])
            C.ensure(rows == report['rows'], 'JDI target and ordinary original outcomes agree')
            report['entropy'][stem] = entropy
            traced, code = read_trace(trace, cases, report['pins'])
            report['traces'][stem], report['live_Code'][stem] = traced, code
            report['trace_artifacts'][stem] = {name: {'bytes': (trace / name).stat().st_size,
                'sha256': C.digest(trace / name)} for name in
                ('target.stdout', 'target.stderr', 'queries.ndjson', 'live-Code.json')}
            summary = 'CHEST_TRACE_COMPLETE rows=' + str(len(cases)) + ' queries=' + str(sum(len(row['queries']) for row in traced)) + ' live-code=' + str(len(code))
            C.ensure((output / (stem + '.stdout')).read_text(encoding='utf-8').splitlines() == [summary], 'complete live query trace footer')
            after = inventory(worlds[index])
            C.ensure(set(after) == set(world_before[index]) and all(after[name] == pin for name, pin in
                world_before[index].items() if name != 'session.lock') and after['session.lock']['bytes'] == 8,
                'only traced source session lock changes on disk')
        C.ensure(report['traces']['trace-interpreter'] == report['traces']['trace-default'] and
                 report['live_Code']['trace-interpreter'] == report['live_Code']['trace-default'],
                 'both live trace modes agree exactly')
        report['status'] = 'PASS'
    finally:
        report['worlds_after'] = [inventory(world) for world in worlds]
        report['inputs_after'] = {name: C.digest(name) for name in before}
        report['compiled_after'] = {str(path.relative_to(output)): C.digest(path) for path in sorted(classes.rglob('*.class'))}
        if report['inputs_after'] != before or report.get('compiled_before') != report['compiled_after']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Chest predicate capture: ' + str(output))
    C.ensure(report['status'] == 'PASS', 'complete immutable original observation')
    print('PASS: ' + str(len(report['rows'])) + ' genuine World Chest predicates in both VM modes')


if __name__ == '__main__':
    main()
