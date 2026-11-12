"""Repeat genuine saved-World Redstone calls and read-only query/static-read/UV-vertex observations."""
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
ROOT = next(p for p in HERE.parents if (p / 'tests/reference/client/controller/capture.py').is_file())
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C = module('redstone_capture_support', ROOT / 'tests/reference/client/controller/capture.py')
I = module('redstone_classfile_support', ROOT / 'tests/reference/client/controller/classfile.py')
P = module('redstone_world_support', ROOT / 'tests/reference/client/picking/observe.py')
W = module('redstone_signed_world_inputs', HERE / 'world_inputs.py')


def inventory(root):
    return {p.relative_to(root).as_posix(): C.digest(p) for p in root.rglob('*') if p.is_file()}


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def selected(raw):
    return [line for line in raw.decode('ascii').splitlines()
            if line.startswith(('SIGNED\t', 'SIGNED_INPUT\t', 'SIGNED_GUARD ', 'SIGNED_COMPLETE ', 'SIGNED_STATIC', 'SIGNED_BACKING\t'))]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path, default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--execute', action='store_true', help='Explicit Root release: compiles authored helpers and launches three original targets')
    args = parser.parse_args()
    C.ensure(args.execute, 'Preparation only by default; Root must explicitly release --execute')
    authored = module('redstone_authored_profiles', HERE / 'generate_profiles.py')
    C.ensure(all((HERE / name).read_bytes() == wire.encode('ascii') for name, wire in authored.products().items()), 'exact authored matrix and declaration')
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
              *(HERE / name for name in ('RedstoneSignedObservation.java', 'RedstoneSignedJdi.java', 'generate_profiles.py', 'profiles.txt',
                                        'declared-inputs.json', 'source-pins.json', 'capture_reference.py', 'world_inputs.py', 'owner-contract.json')),
              ROOT / 'tests/reference/client/controller/capture.py', ROOT / 'tests/reference/client/controller/classfile.py',
              ROOT / 'tests/reference/client/picking/observe.py', ROOT / 'tests/tools/client/test_picking.py',
              *(HERE / name for name in ('selected.txt', 'events.jsonl', 'live-code.tsv', 'owners.tsv') if (HERE / name).is_file()),
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
              'tool_captures': {},
              'scope': '8 actual original direct Redstone CV.g signed-coordinate calls in each of two fresh ordinary VMs and a third read-only JDI VM; genuine saved World/EW/NW owners and constructor backing. No native/GL/frame/gameplay/performance qualification.'}
    try:
        with zipfile.ZipFile(jar) as archive:
            pins = {'client_jar_sha256': JAR, 'classes': {name: I.inspect_class(archive.read(name + '.class'))
                    for name in json.loads((HERE / 'source-pins.json').read_text())['classes']}}
        C.ensure(pins == json.loads((HERE / 'source-pins.json').read_text()), 'every class and method Code pin')
        for name, tool in (('java', java), ('javac', javac)):
            result = C.capture([str(tool), '-version'], output, name, 10, environment)
            report['tool_captures'][name] = result
            C.require_success(result)
            version = (output / result['stdout_file']).read_text() + (output / result['stderr_file']).read_text()
            C.ensure('17.0.17' in version, 'qualified JDK17.0.17')
            report[name + '_version'] = version
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        result = C.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-cp', cp,
                           '-d', str(classes), str(HERE / 'RedstoneSignedObservation.java'), str(HERE / 'RedstoneSignedJdi.java')],
                          output, 'compile', 30, environment)
        report['compile'] = result
        C.require_success(result, empty_stderr=True)
        helpers = inventory(classes)
        C.ensure(set(helpers) == {'RedstoneSignedObservation.class', 'RedstoneSignedObservation$1.class', 'RedstoneSignedJdi.class'}, 'authored helpers only')
        report['helpers_before'] = helpers
        raw = []
        for repeat in range(3):
            out = output / ('ordinary-' + str(repeat) if repeat < 2 else 'jdi')
            out.mkdir()
            world = out / 'world'
            W.world(world)
            prior = inventory(world)
            write(out / 'world-freeze.json', {'inputs_sha256': prior})
            if repeat < 2:
                command = [str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                           '-Djava.security.manager=allow', '-cp', cp, 'RedstoneSignedObservation', str(jar),
                           str(world), str(HERE / 'profiles.txt')]
            else:
                command = [str(java), '--add-modules', 'jdk.jdi', '-cp', str(classes), 'RedstoneSignedJdi',
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
        all_lines = raw[0].decode('ascii').splitlines()
        owner_lines = [line for line in all_lines if line.startswith('SIGNED_OWNERS\t')]
        C.ensure(len(owner_lines) == 1 and all_lines.index(owner_lines[0]) + 1 ==
                 next(i for i,line in enumerate(all_lines) if line.startswith('SIGNED_STATIC\t')),
                 'one actual owner receipt immediately before STATIC')
        (output / 'owners.tsv').write_text(owner_lines[0] + '\n', encoding='ascii', newline='\n')
        rows = selected(raw[0])
        C.ensure(len(rows) == 28 and len([r for r in rows if r.startswith('SIGNED\t')]) == 8 and
                 rows[-1].startswith('SIGNED_COMPLETE rows=8 ') and rows[-1].endswith('genuine-backing=2097152 begins=1 draw=0 raw-allocations=0 source-replacements=0'), 'complete ordinary source rows')
        journal = (output / 'jdi/observer.stdout').read_text().splitlines()
        events = [json.loads(line) for line in (output / 'jdi/events.jsonl').read_text().splitlines()]
        live = [line for line in journal if line.startswith('RS_LIVE_CODE\t')]
        C.ensure(journal[-1].startswith('RS_JDI_COMPLETE direct-g=8 ') and events and live, 'complete actual read-only query/color/UV/vertex/exception trace')
        for line in live:
            _, owner, name, descriptor, length, code_hash = line.split('\t')
            actual = [m for m in pins['classes'][owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor]
            C.ensure(len(actual) == 1 and actual[0]['code_bytes'] == int(length) and actual[0]['code_sha256'] == code_hash, 'live Code equals original archive')
        (output / 'selected.txt').write_text('\n'.join(rows) + '\n', encoding='ascii', newline='\n')
        (output / 'events.jsonl').write_text(''.join(json.dumps(e, separators=(',', ':')) + '\n' for e in events), encoding='ascii', newline='\n')
        (output / 'live-code.tsv').write_text('\n'.join(live) + '\n', encoding='ascii', newline='\n')
        for name in ('selected.txt', 'events.jsonl', 'live-code.tsv', 'owners.tsv'):
            if (HERE / name).is_file():
                C.ensure((HERE / name).read_bytes() == (output / name).read_bytes(), 'repeated retained source product: ' + name)
        C.ensure(sum(e['event'] == 'entry' and e['at'].startswith('cv.g(Luu;III)Z@') for e in events) == 8, 'all 8 actual direct bodies observed')
        report.update(status='PASS', capture_only=True, profiles=8, ordinary_fresh_vms=2, read_only_jdi_fresh_vms=1,
                      raw_stdout_sha256=[__import__('hashlib').sha256(b).hexdigest() for b in raw],
                      events=len(events), queries=sum(e['event'] == 'entry' and e['at'].startswith('ew.') for e in events),
                      field_reads=sum(e['event'] == 'field-read' for e in events), exceptions=sum(e['event'] == 'exception' for e in events), live_methods=len(live),
                      direct_g_calls=sum(e['event']=='entry' and e['at'].startswith('cv.g(Luu;III)Z@') for e in events),
                      nw_vertex_calls=sum(e['event']=='entry' and e['at'].startswith('nw.a(DDDDD)V@') for e in events),
                      jdi_completion=journal[-1], selected_sha256=C.digest(output / 'selected.txt'), owners_sha256=C.digest(output / 'owners.tsv'))
    finally:
        report['inputs_after'] = {path: C.digest(Path(path)) for path in before}
        report['helpers_after'] = inventory(classes)
        report['inputs_unchanged'] = before == report['inputs_after']
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
    C.ensure(report['status'] == 'PASS', 'complete three-VM original cohort')
    print('PASS: 8 direct signed-coordinate source calls x 3 VMs; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
