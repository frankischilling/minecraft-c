"""Capture unchanged original farming Item/Stack calls on genuine saved World owners."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'CMakeLists.txt').is_file())
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
AUTHORED = ('FarmingObservation.java', 'FarmingJdi.java', 'profiles.tsv',
            'declared-inputs.json', 'source-pins.json', 'capture_reference.py')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C = module('farming_capture_support', ROOT / 'tests/reference/client/controller/capture.py')
I = module('farming_classfile_support', ROOT / 'tests/reference/client/controller/classfile.py')
P = module('farming_world_support', ROOT / 'tests/reference/client/picking/observe.py')


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def inventory(root):
    return {p.relative_to(root).as_posix(): C.digest(p) for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path, default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'verified unchanged original archive')
    libraries = [(args.libraries / name).resolve(strict=True) for name in P.LIBRARIES]
    C.ensure(all(C.digest(p) == h for p, h in zip(libraries, P.LIBRARIES.values())), 'pinned SDK dependencies')
    java, javac = [Path(shutil.which(p) or p).resolve(strict=True) for p in (args.java, args.javac)]
    C.ensure(java.parent == javac.parent, 'matching observation JDK')
    jdk = java.parent.parent
    vm = java.parent / 'server/jvm.dll'
    if not vm.is_file():
        vm = jdk / 'lib/server/libjvm.so'
    inputs = [jar, *libraries, java, javac, jdk / 'lib/modules', vm.resolve(strict=True),
              *(HERE / name for name in AUTHORED), ROOT / 'tests/reference/client/controller/capture.py',
              ROOT / 'tests/reference/client/controller/classfile.py', ROOT / 'tests/reference/client/picking/observe.py',
              ROOT / 'tests/tools/client/test_picking.py', *java.parent.glob('*.dll')]
    before = {str(p.resolve()): C.digest(p) for p in sorted(set(inputs))}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes, frozen = output / 'classes', output / 'frozen'
    classes.mkdir()
    frozen.mkdir()
    for name in AUTHORED:
        shutil.copy2(HERE / name, frozen / name)
    write(output / 'source-freeze.json', {'inputs_sha256': before})
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    report = {'status': 'FAIL', 'inputs_before': before, 'runs': [],
              'scope': '118 genuine original direct Item and separate IZ wrapper calls on saved World/Chunk/Player/Stack/statistic owners. Two ordinary VMs and one read-only JDI VM; no graphics, original game-class replacement, native library, network, server, full farming or performance qualification.'}
    try:
        declared = json.loads((HERE / 'declared-inputs.json').read_text(encoding='utf-8'))
        profiles = [{'name': line.split('\t')[0], 'values': list(map(int, line.split('\t')[1:]))}
                    for line in (HERE / 'profiles.tsv').read_text(encoding='ascii').splitlines()]
        C.ensure(declared['rows'] == profiles and len(profiles) == 118 and
                 len({p['name'] for p in profiles}) == 118, 'exact frozen input rows')
        expected = json.loads((HERE / 'source-pins.json').read_text(encoding='utf-8'))
        with zipfile.ZipFile(jar) as z:
            actual = {'client_jar_sha256': JAR, 'classes':
                      {name: I.inspect_class(z.read(name + '.class')) for name in expected['classes']}}
        C.ensure(actual == expected, 'all whole original classes and Code pins')
        for role, tool in (('java', java), ('javac', javac)):
            result = C.capture([str(tool), '-version'], output, role, 10, environment)
            C.require_success(result)
            version = (output / result['stdout_file']).read_text() + (output / result['stderr_file']).read_text()
            C.ensure('17.0.17' in version, 'qualified JDK17.0.17')
            report[role + '_version'] = version
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        result = C.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-cp', cp,
                            '-d', str(classes), str(HERE / 'FarmingObservation.java'), str(HERE / 'FarmingJdi.java')],
                           output, 'compile', 30, environment)
        C.require_success(result, empty_stderr=True)
        helpers = inventory(classes)
        C.ensure(set(helpers) == {'FarmingObservation.class', 'FarmingObservation$Observer.class',
                                 'FarmingObservation$1.class', 'FarmingJdi.class'}, 'authored helpers only')
        report['helpers_before'] = helpers
        raw = []
        for repeat in range(3):
            out = output / ('ordinary-' + str(repeat) if repeat < 2 else 'jdi')
            out.mkdir()
            world = out / 'world'
            P.world(world)
            prior = inventory(world)
            if repeat < 2:
                command = [str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                           '-Djava.security.manager=allow', '-cp', cp, 'FarmingObservation', str(jar),
                           str(world), str(HERE / 'profiles.tsv')]
            else:
                command = [str(java), '--add-modules', 'jdk.jdi', '-cp', str(classes), 'FarmingJdi',
                           str(out), str(jdk), cp, str(jar), str(world), str(HERE / 'profiles.tsv')]
            result = C.capture(command, out, 'original' if repeat < 2 else 'observer',
                               90 if repeat < 2 else 330, environment, out)
            report['runs'].append({'capture': result, 'world_before': prior, 'world_after': inventory(world)})
            C.require_success(result, empty_stderr=repeat == 2)
            C.ensure(all(name == 'session.lock' or report['runs'][-1]['world_after'].get(name) == h
                         for name, h in prior.items()), 'preexisting private saved inputs unchanged except source session lock')
            raw.append((out / ('original.stdout' if repeat < 2 else 'target.stdout')).read_bytes())
        C.ensure(raw[0] == raw[1] == raw[2], 'two ordinary and read-only JDI complete stdout bytes agree')
        selected = [line for line in raw[0].decode('ascii').splitlines() if line.startswith(('FARMING\t', 'FARMING_GUARD ', 'FARMING_COMPLETE '))]
        records = [json.loads(line.split('\t', 1)[1]) for line in selected if line.startswith('FARMING\t')]
        C.ensure(len(records) == 118 and selected[-1] == 'FARMING_COMPLETE rows=118 game-code-replacements=0 jdi-source-writes=0', 'complete original rows and terminal guard')
        C.ensure([{'name': r['name'], 'values': r['values']} for r in records] == profiles, 'all actual executed input values')
        events = [json.loads(line) for line in (output / 'jdi/events.jsonl').read_text(encoding='ascii').splitlines()]
        journal = (output / 'jdi/observer.stdout').read_text(encoding='ascii').splitlines()
        live = [line for line in journal if line.startswith('FU_LIVE_CODE\t')]
        C.ensure(events and live and journal[-1].startswith('FU_JDI_COMPLETE '), 'complete live source observation')
        for line in live:
            _, owner, name, descriptor, length, code = line.split('\t')
            methods = [m for m in actual['classes'][owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
            C.ensure(len(methods) == 1 and methods[0]['code_bytes'] == int(length) and methods[0]['code_sha256'] == code, 'live reached Code equals unchanged archive')
        (output / 'selected.txt').write_text('\n'.join(selected) + '\n', encoding='ascii', newline='\n')
        (output / 'live-code.tsv').write_text('\n'.join(live) + '\n', encoding='ascii', newline='\n')
        report.update(status='PASS', profiles=118, ordinary_fresh_vms=2, read_only_jdi_fresh_vms=1,
                      events=len(events), live_methods=len(live), jdi_completion=journal[-1],
                      exceptions=sum(r['exception'] != 'NONE' for r in records),
                      raw_stdout_sha256=[__import__('hashlib').sha256(r).hexdigest() for r in raw],
                      selected_sha256=C.digest(output / 'selected.txt'))
    finally:
        report['inputs_after'] = {path: C.digest(Path(path)) for path in before}
        report['inputs_unchanged'] = report['inputs_after'] == before
        report['helpers_after'] = inventory(classes)
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
    C.ensure(report['status'] == 'PASS', 'complete three-VM original cohort')
    print('PASS: original farming use; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
