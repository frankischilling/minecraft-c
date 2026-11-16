"""Observe unchanged original Furnace World.g and TC variant lifecycle."""
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
CORE = ('sk','tc','rw','ow','fd','lm','mx','uu','xp','pe','st','eb','pm','ok','wi','xa','iz','gm','ey','nu','sp','ei')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('chest_placement_transport', ROOT / 'tests/reference/client/controller/capture.py')
I = module('chest_placement_classfile', ROOT / 'tests/reference/client/controller/classfile.py')
W = module('furnace_lifecycle_world_inputs', HERE / 'fixture.py')


def unique(pairs):
    result = dict(pairs)
    C.ensure(len(result) == len(pairs), 'unique JSON fields')
    return result


def declarations(declaration):
    return '\n'.join('\t'.join(map(str,(step['name'],step['operation'],step['author'].get('cook','-'),step['author'].get('burn','-'),str(step['author'].get('fuel_null',False)).lower()))) for step in declaration['steps'])+'\n'


def inventory(directory):
    return {path.relative_to(directory).as_posix(): {'bytes': path.stat().st_size,
            'sha256': C.digest(path)} for path in sorted(directory.rglob('*')) if path.is_file()}


def read_output(raw, declaration, pins):
    rows, entropy, origins, complete, preamble, setup = [], [], [], [], [], []
    for line in raw.decode('utf8').splitlines():
        for prefix,out in (('FURNACE_WORLD_ROW ',rows),('FURNACE_WORLD_RANDOM ',entropy),('FURNACE_WORLD_ORIGINS ',origins),('FURNACE_WORLD_COMPLETE ',complete),('FURNACE_WORLD_SETUP ',setup)):
            if line.startswith(prefix):out.append(json.loads(line[len(prefix):],object_pairs_hook=unique));break
        else:
            if line:preamble.append(line)
    C.ensure(preamble==['16 achievements','151 recipes'],'exact original registry preamble')
    C.ensure(origins==[{'jar_sha256':JAR,'classes':{name:value['class_sha256'] for name,value in pins['classes'].items()}}],'actual loaded original archive/resources')
    names=[step['name'] for step in declaration['steps']]
    C.ensure([row['name'] for row in rows]==[row['name'] for row in entropy]==names and [row['index'] for row in rows]==list(range(len(names))),'all declared World steps observed')
    C.ensure(len(complete)==1 and complete[0]['rows']==len(names) and all(complete[0][key]==0 for key in ('network','native','execs','denied_writes')) and complete[0]['canonical_chunks']==4,'complete guarded World cohort')
    C.ensure(len(setup)==1,'one actual prepared World setup')
    return rows,entropy,complete[0],setup[0]


def expected_warnings(classes):
    uri = classes.as_uri().replace('file:///', 'file:/') + '/'
    return ['WARNING: A terminally deprecated method in java.lang.System has been called',
            'WARNING: System::setSecurityManager has been called by FurnaceLifecycleObservation (' + uri + ')',
            'WARNING: Please consider reporting this to the maintainers of FurnaceLifecycleObservation',
            'WARNING: System::setSecurityManager will be removed in a future release']



def read_trace(directory, declaration, pins):
    rows = [json.loads(line, object_pairs_hook=unique) for line in
            (directory / 'phases.ndjson').read_text(encoding='utf8').splitlines()]
    C.ensure([row['index'] for row in rows] == list(range(len(declaration['steps']))), 'all actual phase boundaries')
    code = json.loads((directory / 'live-Code.json').read_text(encoding='utf8'), object_pairs_hook=unique)
    methods = {owner + '.' + method['name'] + method['descriptor']: method
               for owner, value in pins['classes'].items() for method in value['methods']}
    seen = set()
    for row in rows:
        C.ensure(set(row) == {'index', 'phases'} and len(row['phases']) <= 1024, 'bounded actual phase sequence')
        for phase in row['phases']:
            C.ensure(set(phase) == {'event', 'method', 'receiver', 'arguments', 'source_parent', 'state'} and
                     phase['event'] in ('entry', 'exit') and phase['method'] in code, 'closed actual original phase')
            seen.add(phase['method'])
            for parent in phase['source_parent']:
                C.ensure(parent['method'] in methods and type(parent['pc']) is int and
                         0 <= parent['pc'] < methods[parent['method']]['code_bytes'], 'actual original source parent PC')
    C.ensure(seen == set(code), 'each live Code pin was observed')
    for name, value in code.items():
        C.ensure(name in methods and value == {key: methods[name][key] for key in ('code_bytes', 'code_sha256')},
                 'actual entered live Code equals archive')
    return rows, code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'verified original client archive')
    libraries = [(args.libraries / name).resolve(strict=True) for name in W.W.LIBRARIES]
    C.ensure(all(C.digest(path) == value for path, value in zip(libraries, W.W.LIBRARIES.values())), 'pinned SDK libraries')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'same JDK17')
    jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    cases = json.loads((HERE / 'cases.json').read_text(encoding='utf-8'), object_pairs_hook=unique)
    declared = output / 'profiles.tsv'
    declared.write_text(declarations(cases), encoding='utf-8', newline='\n')
    helpers = [HERE / 'FurnaceLifecycleObservation.java', HERE / 'FurnaceLifecycleJdi.java', HERE / 'fixture.py', HERE / 'cases.json', Path(__file__).resolve(),
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
        W.world(world,cases)
    world_before = [inventory(world) for world in worlds]
    C.ensure(all(value == world_before[0] for value in world_before), 'identical frozen saved World inputs')
    report['worlds_before'] = world_before
    environment = os.environ.copy()
    report['removed_environment_names'] = [name for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS')
                                           if environment.pop(name, None) is not None]
    cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
    try:
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '-implicit:none', '-classpath', cp,
            '-d', str(classes), str(HERE / 'FurnaceLifecycleObservation.java'), str(HERE / 'FurnaceLifecycleJdi.java')], output, 'compile', 60, environment)
        C.require_success(report['compile'], empty_stderr=True)
        compiled = sorted(classes.rglob('*.class'))
        C.ensure(compiled and all(path.name.startswith(('FurnaceLifecycleObservation', 'FurnaceLifecycleJdi')) for path in compiled), 'only authored observer classes')
        report['compiled_before'] = {str(path.relative_to(output)): C.digest(path) for path in compiled}
        report['java_version'] = C.capture([str(java), '-version'], output, 'java-version', 20, environment)
        C.require_success(report['java_version'])
        report['javac_version'] = C.capture([str(javac), '-version'], output, 'javac-version', 20, environment)
        C.require_success(report['javac_version'], empty_stderr=True)
        report['entropy'] = {}
        for index, (stem, flags) in enumerate((('interpreter', ['-Xint']), ('default', []))):
            report[stem] = C.capture([str(java), *flags, '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED', '-classpath', cp,
                'FurnaceLifecycleObservation', str(jar), str(worlds[index]), str(declared)], output, stem, 120, environment)
            C.require_success(report[stem])
            C.ensure((output / (stem + '.stderr')).read_text(encoding='utf-8').splitlines() == expected_warnings(classes), 'only declared JDK17 guard warnings')
            rows, entropy, completion, setup = read_output((output / (stem + '.stdout')).read_bytes(), cases, report['pins'])
            if report['rows'] is None:
                report['rows'] = rows
            else:
                C.ensure(rows == report['rows'], 'literal logical predicate rows agree between VM modes')
            report['entropy'][stem] = entropy
            report.setdefault('completion',{})[stem] = completion
            report.setdefault('setup',{})[stem] = setup
            after = inventory(worlds[index])
            C.ensure(set(after) == set(world_before[index]) and all(after[name] == pin for name, pin in
                world_before[index].items() if name != 'session.lock') and after['session.lock']['bytes'] == 8,
                'only actual source session lock changes on disk')
        report['traces'], report['live_Code'], report['trace_artifacts'] = {}, {}, {}
        for index, mode in enumerate(('interpreter', 'default'), 2):
            stem, trace = 'trace-' + mode, output / ('trace-' + mode + '-jdi')
            report[stem] = C.capture([str(java), '--add-modules=jdk.jdi', '-classpath', cp,
                'FurnaceLifecycleJdi', cp, str(jar), str(java.parent.parent), str(trace),
                str(worlds[index]), str(declared), mode], output, stem, 300, environment)
            C.require_success(report[stem], empty_stderr=True)
            C.ensure((trace / 'target.stderr').read_text(encoding='utf8').splitlines() == expected_warnings(classes),
                     'only declared traced-target warnings')
            rows, entropy, completion, setup = read_output((trace / 'target.stdout').read_bytes(), cases, report['pins'])
            C.ensure(rows == report['rows'], 'traced and ordinary original outcomes agree')
            report['entropy'][stem], report.setdefault('completion', {})[stem] = entropy, completion
            report.setdefault('setup',{})[stem] = setup
            phases, code = read_trace(trace, cases, report['pins'])
            report['traces'][stem], report['live_Code'][stem] = phases, code
            report['trace_artifacts'][stem] = {name: {'bytes': (trace / name).stat().st_size,
                'sha256': C.digest(trace / name)} for name in ('target.stdout', 'target.stderr', 'phases.ndjson', 'live-Code.json')}
            summary = f'FURNACE_WORLD_TRACE_COMPLETE rows={len(rows)} phases={sum(len(row["phases"]) for row in phases)} live-code={len(code)}'
            C.ensure((output / (stem + '.stdout')).read_text(encoding='utf8').splitlines() == [summary],
                     'complete actual phase journal summary')
        C.ensure(report['traces']['trace-interpreter'] == report['traces']['trace-default'] and
                 report['live_Code']['trace-interpreter'] == report['live_Code']['trace-default'],
                 'both fresh passive original phase journals agree literally')
        C.ensure(all(value == report['setup']['interpreter'] for value in report['setup'].values()),
                 'all actual prepared World fields agree')
        for index, directory in enumerate(worlds):
            after = inventory(directory)
            C.ensure(set(after) == set(world_before[index]) and all(after[name] == pin for name, pin in
                world_before[index].items() if name != 'session.lock') and after['session.lock']['bytes'] == 8,
                'only actual source session-lock bytes may change on disk in every mode')
        report['status'] = 'PASS'
    finally:
        report['worlds_after'] = [inventory(world) for world in worlds]
        report['inputs_after'] = {name: C.digest(name) for name in before}
        report['compiled_after'] = {str(path.relative_to(output)): C.digest(path) for path in sorted(classes.rglob('*.class'))}
        if report['inputs_after'] != before or report.get('compiled_before') != report['compiled_after']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Furnace lifecycle capture: ' + str(output))
    C.ensure(report['status'] == 'PASS', 'complete immutable original observation')
    print('PASS: ' + str(len(report['rows'])) + ' original World Furnace steps in both VM modes')


if __name__ == '__main__':
    main()
