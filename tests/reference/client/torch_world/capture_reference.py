"""Repeat genuine saved-World Torch calls and read-only query/static-read/UV-vertex observations."""
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
ROOT = HERE.parents[3]
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C = module('torch_capture_support', ROOT / 'tests/reference/client/controller/capture.py')
I = module('torch_classfile_support', ROOT / 'tests/reference/client/controller/classfile.py')
P = module('torch_world_support', ROOT / 'tests/reference/client/picking/observe.py')


def inventory(root):
    return {p.relative_to(root).as_posix(): C.digest(p) for p in root.rglob('*') if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def selected(raw):
    return [line for line in raw.decode('ascii').splitlines()
            if line.startswith(('TORCH\t', 'TORCH_INPUT\t', 'TORCH_GUARD ', 'TORCH_COMPLETE ', 'TORCH_STATIC', 'TORCH_BACKING\t'))]


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
    C.ensure(all(C.digest(path) == pin for path, pin in zip(libraries, P.LIBRARIES.values())), 'pinned libraries')
    java, javac = [Path(shutil.which(name) or name).resolve(strict=True) for name in (args.java, args.javac)]
    C.ensure(java.parent == javac.parent, 'matching observation JDK')
    jdk = java.parent.parent
    jvm = java.parent / 'server/jvm.dll'
    if not jvm.is_file():
        jvm = jdk / 'lib/server/libjvm.so'
    inputs = [jar, *libraries, java, javac, jdk / 'lib/modules', jvm.resolve(strict=True),
              *(HERE / name for name in ('TorchObservation.java', 'TorchJdi.java', 'profiles.txt',
                                        'declared-inputs.json', 'source-pins.json', 'capture_reference.py')),
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/reference/client/controller/classfile.py',
              ROOT / 'tests/reference/client/picking/observe.py', ROOT / 'tests/tools/client/test_picking.py',
              *(HERE / name for name in ('selected.txt', 'events.jsonl', 'live-code.tsv') if (HERE / name).is_file()),
              *java.parent.glob('*.dll')]
    before = {str(path.resolve()): C.digest(path) for path in sorted(set(inputs))}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    write(output / 'source-freeze.json', {'inputs_sha256': before})
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    report = {'status': 'FAIL', 'inputs_before': before, 'runs': [],
              'scope': '66 actual original Torch World CV.c/a calls in each of two fresh ordinary VMs and a third read-only JDI VM; genuine saved World/EW/NW owners and constructor backing. No native/GL/frame/gameplay/performance qualification.'}
    try:
        with zipfile.ZipFile(jar) as archive:
            pins = {'client_jar_sha256': JAR, 'classes': {name: I.inspect_class(archive.read(name + '.class'))
                    for name in json.loads((HERE / 'source-pins.json').read_text())['classes']}}
        C.ensure(pins == json.loads((HERE / 'source-pins.json').read_text()), 'every class and method Code pin')
        for name, tool in (('java', java), ('javac', javac)):
            result = C.capture([str(tool), '-version'], output, name, 10, environment)
            C.require_success(result)
            version = (output / result['stdout_file']).read_text() + (output / result['stderr_file']).read_text()
            C.ensure('17.0.17' in version, 'qualified JDK17.0.17')
            report[name + '_version'] = version
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        result = C.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-cp', cp,
                           '-d', str(classes), str(HERE / 'TorchObservation.java'), str(HERE / 'TorchJdi.java')],
                          output, 'compile', 30, environment)
        C.require_success(result, empty_stderr=True)
        helpers = inventory(classes)
        C.ensure(set(helpers) == {'TorchObservation.class', 'TorchObservation$1.class', 'TorchJdi.class'}, 'authored helpers only')
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
                           '-Djava.security.manager=allow', '-cp', cp, 'TorchObservation', str(jar),
                           str(world), str(HERE / 'profiles.txt')]
            else:
                command = [str(java), '--add-modules', 'jdk.jdi', '-cp', str(classes), 'TorchJdi',
                           str(out), str(jdk), cp, str(jar), str(world), str(HERE / 'profiles.txt')]
            result = C.capture(command, out, 'original' if repeat < 2 else 'observer',
                               90 if repeat < 2 else 180, environment, out)
            run = {'capture': result, 'world_before': prior, 'world_after': inventory(world)}
            report['runs'].append(run)
            C.require_success(result, empty_stderr=repeat == 2)
            C.ensure({k: v for k, v in prior.items() if k != 'session.lock'} ==
                     {k: v for k, v in run['world_after'].items() if k != 'session.lock'}, 'unchanged saved World except source session lock')
            raw.append((out / ('original.stdout' if repeat < 2 else 'target.stdout')).read_bytes())
        C.ensure(raw[0] == raw[1] == raw[2], 'two ordinary and read-only JDI full stdout bytes agree')
        rows = selected(raw[0])
        C.ensure(len(rows) == 202 and len([r for r in rows if r.startswith('TORCH\t')]) == 66 and
                 rows[-1].startswith('TORCH_COMPLETE rows=66 ') and rows[-1].endswith('genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0'), 'complete ordinary source rows')
        journal = (output / 'jdi/observer.stdout').read_text().splitlines()
        events = [json.loads(line) for line in (output / 'jdi/events.jsonl').read_text().splitlines()]
        live = [line for line in journal if line.startswith('TW_LIVE_CODE\t')]
        C.ensure(journal[-1].startswith('TW_JDI_COMPLETE ') and events and live and
                 sum(e['event'] == 'exception' for e in events) == 5, 'complete actual read-only query/store/exception trace')
        for line in live:
            _, owner, name, descriptor, length, code_hash = line.split('\t')
            actual = [m for m in pins['classes'][owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
            C.ensure(len(actual) == 1 and actual[0]['code_bytes'] == int(length) and actual[0]['code_sha256'] == code_hash, 'live Code equals original archive')
        (output / 'selected.txt').write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
        (output / 'events.jsonl').write_text(''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in events), encoding='ascii', newline='\n')
        (output / 'live-code.tsv').write_text('\n'.join(live) + '\n', encoding='ascii', newline='\n')
        for name in ('selected.txt', 'events.jsonl', 'live-code.tsv'):
            if (HERE / name).is_file():
                C.ensure((HERE / name).read_bytes() == (output / name).read_bytes(), 'repeated retained source product: ' + name)
        report.update(status='PASS', profiles=66, ordinary_fresh_vms=2, read_only_jdi_fresh_vms=1,
                      raw_stdout_sha256=[__import__('hashlib').sha256(b).hexdigest() for b in raw],
                      events=len(events), queries=sum(e['event'] == 'entry' and e['at'].startswith('ew.') for e in events),
                      field_reads=sum(e['event'] == 'field-read' for e in events), exceptions=5, live_methods=len(live),
                      shared_calls=sum(e['event']=='entry' and e['at'].startswith('cv.a(Luu;DDDDD)V@') for e in events),
                      nw_vertex_calls=sum(e['event']=='entry' and e['at'].startswith('nw.a(DDDDD)V@') for e in events),
                      jdi_completion=journal[-1], selected_sha256=C.digest(output / 'selected.txt'))
    finally:
        report['inputs_after'] = {path: C.digest(Path(path)) for path in before}
        report['helpers_after'] = inventory(classes)
        report['inputs_unchanged'] = before == report['inputs_after']
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
    C.ensure(report['status'] == 'PASS', 'complete three-VM original cohort')
    print('PASS: 66 Torch World source calls x 3 VMs; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
