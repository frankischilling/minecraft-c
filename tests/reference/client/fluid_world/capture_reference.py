"""Capture three bounded genuine original fluid cohorts; retain every attempt."""
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
AUTHORED = ('FluidObservation.java', 'FluidJdi.java', 'profiles.txt',
            'profiles-0.txt', 'profiles-1.txt', 'profiles-2.txt',
            'declared-inputs.json', 'source-pins.json', 'capture_reference.py')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C = module('fluid_capture_support', ROOT / 'tests/reference/client/controller/capture.py')
I = module('fluid_classfile_support', ROOT / 'tests/reference/client/controller/classfile.py')
P = module('fluid_world_support', ROOT / 'tests/reference/client/picking/observe.py')


def inventory(root):
    return {p.relative_to(root).as_posix(): C.digest(p) for p in root.rglob('*') if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def selected(raw):
    return [line for line in raw.decode('ascii').splitlines() if line.startswith('FLUID')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path, default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cohort', type=int, choices=(0, 1, 2))
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
    jmod = jdk / 'jmods/java.base.jmod'
    inputs = [jar, *libraries, java, javac, jdk / 'lib/modules', jmod, jvm.resolve(strict=True),
              *(HERE / n for n in AUTHORED),
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/reference/client/controller/classfile.py',
              ROOT / 'tests/reference/client/picking/observe.py', ROOT / 'tests/tools/client/test_picking.py',
              *java.parent.glob('*.dll')]
    before = {str(path.resolve()): C.digest(path) for path in sorted(set(inputs))}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    source_copy = output / 'authored-source-before'
    source_copy.mkdir()
    for name in AUTHORED:
        shutil.copyfile(HERE / name, source_copy / name)
    write(output / 'source-freeze.json', {'inputs_sha256': before})
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    declared = json.loads((HERE / 'declared-inputs.json').read_text())
    cohorts = [c for c in declared['cohorts'] if args.cohort is None or c['index'] == args.cohort]
    C.ensure(len((HERE / 'profiles.txt').read_text().splitlines()) == 104 and
             ''.join((HERE / c['file']).read_text() for c in declared['cohorts']) == (HERE / 'profiles.txt').read_text(), 'frozen complete 104-profile partition')
    report = {'status': 'FAIL', 'inputs_before': before, 'cohorts': [],
              'scope': 'Declared whole CV.k/height/RP/BT/Math/NW original CPU calls in two fresh ordinary VMs plus one read-only JDI VM per bounded cohort. Genuine saved World/EW/NW2M owners, own-prior backing and actual pool. Warm genuine BT/MathHelper prerequisites. No native/GL/frame/gameplay/performance qualification.'}
    try:
        expected = json.loads((HERE / 'source-pins.json').read_text())
        with zipfile.ZipFile(jar) as archive:
            pins = {name: I.inspect_class(archive.read(name + '.class'))
                    for name in expected['classes'] if name != 'java.lang.Math'}
        with zipfile.ZipFile(jmod) as archive:
            pins['java.lang.Math'] = I.inspect_class(archive.read('classes/java/lang/Math.class'))
        C.ensure(pins == expected['classes'], 'every original/JDK class and Code pin')
        for name, tool in (('java', java), ('javac', javac)):
            result = C.capture([str(tool), '-version'], output, name, 10, environment)
            C.require_success(result)
            version = (output / result['stdout_file']).read_text() + (output / result['stderr_file']).read_text()
            C.ensure('17.0.17' in version, 'qualified JDK17.0.17')
            report[name + '_version'] = version
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        result = C.capture([str(javac), '-proc:none', '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-cp', cp,
                           '-d', str(classes), str(HERE / 'FluidObservation.java'), str(HERE / 'FluidJdi.java')],
                          output, 'compile', 30, environment)
        C.require_success(result, empty_stderr=True)
        helpers = inventory(classes)
        C.ensure(set(helpers) == {'FluidObservation.class', 'FluidObservation$1.class', 'FluidJdi.class'}, 'authored helpers only')
        report['helpers_before'] = helpers
        write(output / 'before-first-vm.json', {'inputs_sha256': before, 'helpers_sha256': helpers, 'cohorts': cohorts})
        for spec in cohorts:
            cohort = output / ('cohort-' + str(spec['index']))
            cohort.mkdir()
            raw = []
            nulls = sum(int(line.split()[9]) != 0 for line in (HERE / spec['file']).read_text().splitlines())
            summary = {'index': spec['index'], 'profiles': spec['profiles'], 'first': spec['first_global_index'], 'runs': []}
            report['cohorts'].append(summary)
            for repeat in range(3):
                out = cohort / ('ordinary-' + str(repeat) if repeat < 2 else 'jdi')
                out.mkdir()
                world = out / 'world'
                P.world(world)
                prior = inventory(world)
                operands = [str(jar), str(world), str(HERE / spec['file']), str(spec['first_global_index']), str(spec['index'])]
                if repeat < 2:
                    command = [str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                               '-Djava.security.manager=allow', '-cp', cp, 'FluidObservation', *operands]
                else:
                    command = [str(java), '--add-modules', 'jdk.jdi', '-cp', str(classes), 'FluidJdi',
                               str(out), str(jdk), cp, *operands, str(nulls)]
                result = C.capture(command, out, 'original' if repeat < 2 else 'observer',
                                   90 if repeat < 2 else 180, environment, out)
                run = {'capture': result, 'world_before': prior, 'world_after': inventory(world)}
                summary['runs'].append(run)
                C.require_success(result, empty_stderr=repeat == 2)
                C.ensure({k: v for k, v in prior.items() if k != 'session.lock'} ==
                         {k: v for k, v in run['world_after'].items() if k != 'session.lock'}, 'unchanged saved World except source session lock')
                raw.append((out / ('original.stdout' if repeat < 2 else 'target.stdout')).read_bytes())
            C.ensure(raw[0] == raw[1] == raw[2], 'two ordinary and read-only JDI full stdout bytes agree')
            rows = selected(raw[0])
            C.ensure(len([r for r in rows if r.startswith('FLUID\t')]) == spec['profiles'] and
                     rows[-1].startswith('FLUID_COMPLETE cohort=' + str(spec['index']) + ' rows=' + str(spec['profiles']) + ' ') and
                     rows[-1].endswith('genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0'), 'complete ordinary source rows')
            journal = (cohort / 'jdi/observer.stdout').read_text().splitlines()
            events = [json.loads(line) for line in (cohort / 'jdi/events.jsonl').read_text().splitlines()]
            live = [line for line in journal if line.startswith(('FW_LIVE_CODE\t', 'FW_LIVE_NATIVE\t'))]
            C.ensure(journal[-1].startswith('FW_JDI_COMPLETE ') and events and live and
                     sum(e['event'] == 'exception' for e in events) == nulls, 'complete actual read-only trace')
            for line in live:
                parts = line.split('\t')
                tag, owner, name, descriptor = parts[:4]
                actual = [m for m in pins[owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
                C.ensure(len(actual) == 1, 'unique live original method')
                if tag == 'FW_LIVE_CODE':
                    C.ensure(len(parts) == 6 and actual[0]['code_bytes'] == int(parts[4]) and actual[0]['code_sha256'] == parts[5], 'live Code equals original archive/JDK')
                else:
                    C.ensure(len(parts) == 5 and actual[0]['access'] == int(parts[4]) and actual[0]['access'] & 0x100 and 'code_bytes' not in actual[0], 'actual native JDK method has no invented Code')
            selected_path = output / ('selected-' + str(spec['index']) + '.txt')
            selected_path.write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
            events_path = output / ('events-' + str(spec['index']) + '.jsonl')
            events_path.write_text(''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in events), encoding='ascii', newline='\n')
            live_path = output / ('live-code-' + str(spec['index']) + '.tsv')
            live_path.write_text('\n'.join(live) + '\n', encoding='ascii', newline='\n')
            for path in (selected_path, events_path, live_path):
                if (HERE / path.name).is_file():
                    C.ensure((HERE / path.name).read_bytes() == path.read_bytes(), 'repeated retained source product: ' + path.name)
            summary.update(status='PASS', raw_stdout_sha256=[__import__('hashlib').sha256(b).hexdigest() for b in raw],
                           events=len(events), queries=sum(e['event'] == 'entry' and e['data']['at'].startswith('ew.') for e in events),
                           field_reads=sum(e['event'] == 'field-read' for e in events), field_stores=sum(e['event'] == 'field-store' for e in events),
                           exceptions=nulls, live_methods=len(live), jdi_completion=journal[-1], selected_sha256=C.digest(selected_path))
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {path: C.digest(Path(path)) for path in before}
        report['helpers_after'] = inventory(classes)
        report['inputs_unchanged'] = before == report['inputs_after']
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
    C.ensure(report['status'] == 'PASS', 'complete bounded three-VM original cohorts')
    print('PASS: original fluid cohorts; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
