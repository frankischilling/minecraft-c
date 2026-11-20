"""Capture bounded original nested Stair getters without GL or source cycles."""
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
ROOT = HERE.parents[4]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('side_chain_process', ROOT / 'tests/reference/client/controller/capture.py')
I = module('side_chain_classfile', ROOT / 'tests/reference/client/controller/classfile.py')
L = module('side_chain_libraries', ROOT / 'tests/reference/block/harvest/capture_reference.py')
P = module('side_chain_packet', HERE / 'packet.py')


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path, default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == P.JAR, 'unchanged original supplied JAR')
    libraries = [args.libraries / name for name in L.LIBRARIES]
    C.ensure(all(C.digest(path) == wanted for path, wanted in zip(libraries, L.LIBRARIES.values())),
             'pinned original-compatible library archives')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'one supplied JDK17')
    vm = java.parent / 'server/jvm.dll'
    if not vm.is_file():
        vm = java.parent.parent / 'lib/server/libjvm.so'
    files = [jar, *libraries, java, javac, java.parent.parent / 'lib/modules', vm.resolve(strict=True),
             *(HERE / name for name in P.SOURCES),
             ROOT / 'tests/reference/client/controller/capture.py',
             ROOT / 'tests/reference/client/controller/classfile.py',
             ROOT / 'tests/reference/block/harvest/capture_reference.py']
    report = {'status': 'FAIL', 'inputs_before': {str(path.resolve()): C.digest(path) for path in files},
              'original_runs': [], 'scope': 'Two ordinary fresh guarded CPU VMs plus a third fresh read-only-JDI VM; no GL/context or source cycles.'}
    classpath = os.pathsep.join(map(str, [classes, jar, *libraries]))
    try:
        with zipfile.ZipFile(jar) as image:
            full_pins = {'client_jar_sha256': P.JAR, 'classes': {
                name: I.inspect_class(image.read(name + '.class')) for name in ('uu', 'ss')}}
        pins = P.selected_pins(full_pins)
        write(output / 'source-pins.json', pins)
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi',
            '-cp', classpath, '-d', str(classes), *(str(HERE / name) for name in P.SOURCES if name.endswith('.java'))],
            output, 'compile', 60)
        C.require_success(report['compile'], empty_stderr=True)
        report['helpers_before'] = {path.name: C.digest(path) for path in classes.iterdir()}
        ordinary = []
        for repeat in range(2):
            result = C.capture([str(java), '-Xint', '-Xverify:all', '-Xmx128m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '-cp', classpath, 'InventorySideChainsTarget', str(jar)],
                output, 'original-' + str(repeat), 60)
            report['original_runs'].append(result)
            C.require_success(result)
            ordinary.append((output / ('original-' + str(repeat) + '.stdout')).read_text())
            P.validate_target(ordinary[-1])
        C.ensure(ordinary[0] == ordinary[1], 'two independent guarded source VMs agree exactly')
        debug = output / 'debugger'
        debug.mkdir()
        report['debugger'] = C.capture([str(java), '--add-modules', 'jdk.jdi', '-Xmx128m', '-cp', str(classes),
            'InventorySideChainsJdi', str(debug), str(java.parent.parent), classpath, str(jar)],
            debug, 'controller', 120)
        C.require_success(report['debugger'], empty_stderr=True)
        target = (debug / 'target.stdout').read_text()
        C.ensure(target == ordinary[0], 'third read-only-JDI fresh source VM agrees exactly')
        events = [json.loads(line, object_pairs_hook=P.unique)
                  for line in (debug / 'journal.ndjson').read_text().splitlines()]
        observations = P.validate_target(target)
        observations.update(events=events, live_code=[event for event in events if event['event'] == 'live-code'])
        result = P.audit(observations, pins)
        write(output / 'observations.json', observations)
        report.update(status='PASS', ordinary_fresh_vms=2, read_only_jdi_fresh_vms=1, audit=result,
                      field_modifiers=observations['owners']['field_modifiers'],
                      field_final=observations['owners']['field_final'])
    finally:
        report['inputs_after'] = {path: C.digest(path) for path in report['inputs_before']}
        report['helpers_after'] = {path.name: C.digest(path) for path in classes.iterdir()}
        C.ensure(report['inputs_before'] == report['inputs_after'] and
                 report.get('helpers_before') == report['helpers_after'], 'all original/helper inputs unchanged')
        write(output / 'capture.json', report)
    print(json.dumps({'status': report['status'], **report['audit'], 'report': str(output / 'capture.json')}))


if __name__ == '__main__':
    main()
