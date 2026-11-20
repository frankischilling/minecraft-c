"""Capture genuine original held-Item attacks, without resetting constructor entropy."""
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
SUPPORT = ROOT / 'tests/reference/client/controller'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


C = module('held_capture', SUPPORT / 'capture.py')
I = module('held_classfile', SUPPORT / 'classfile.py')
P = module('held_compare', HERE / 'compare.py')
A = module('held_research_audit', HERE / 'audit_research.py')
D = module('held_formula_audit', HERE / 'audit.py')
W = module('held_cow_audit', HERE / 'cow.py')
JAR = A.JAR
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
CORE = ('ob', 'gs', 'ix', 'iz', 'gm', 'dq', 'qd', 'sa', 'bu', 'wh', 'ls', 'dl', 'bx',
        'dc', 'fd', 'ok', 'lm', 'xi', 'jl', 'sn', 'hl')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--research', type=Path)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'untouched original JAR')
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    C.ensure(all(C.digest(path) == expected for path, expected in zip(libraries, LIBRARIES.values())),
             'pinned supplied libraries')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'single pinned JDK17')
    jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
    tools = [java, javac, java.parent.parent / 'lib/modules', jvm]
    old = sorted((HERE.parent / 'player_attack').glob('*'))
    inputs = [jar, *libraries, *tools, HERE / 'HeldItemObservation.java', HERE / 'compare.py',
              HERE / 'audit_research.py', HERE / 'audit.py', HERE / 'cow.py', HERE / 'research-rows.json', HERE / 'cases.json', Path(__file__).resolve(),
              SUPPORT / 'capture.py', SUPPORT / 'classfile.py', *[p for p in old if p.is_file()]]
    before = {str(p): C.digest(p) for p in inputs}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for name in ('APPDATA', 'TMP', 'TEMP'):
        directory = output / name.lower()
        directory.mkdir()
        environment[name] = str(directory)
    report = {'status': 'FAIL', 'inputs_before': before, 'scope': __doc__}
    try:
        if args.research:
            report['research_audit'] = A.audit(args.research, jar)
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '-classpath', cp, '-d',
            str(classes), str(HERE / 'HeldItemObservation.java')], output, 'compile', 60, environment)
        C.require_success(report['compile'], empty_stderr=True)
        report['helpers_before'] = {p.name: C.digest(p) for p in classes.iterdir()}
        C.ensure(set(report['helpers_before']) == {'HeldItemObservation.class',
            'HeldItemObservation$Observer.class', 'HeldItemObservation$1.class'}, 'only authored helper classes')
        report['original'] = C.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m',
            '-Djava.awt.headless=true', '-Djava.security.manager=allow',
            '--add-opens=java.base/java.util=ALL-UNNAMED',
            '-Djava.io.tmpdir=' + environment['TMP'], '-classpath', cp, 'HeldItemObservation',
            str(jar), str(output / 'game')], output, 'original', 300, environment, output)
        C.require_success(report['original'])
        stdout = (output / 'original.stdout').read_text()
        report['rows'] = P.rows(stdout)
        report['inventory'] = P.inventory_rows(stdout)
        report['render_samples'] = [json.loads(line.removeprefix('HELD_ITEM_RENDER '), object_pairs_hook=P.unique)
            for line in stdout.splitlines() if line.startswith('HELD_ITEM_RENDER ')]
        report['independent_formula_audit'] = D.audit(report['rows'], report['inventory'])
        report['cow_rows'] = W.rows(stdout)
        report['cow_entropy_audit'] = W.audit(report['cow_rows'])
        C.ensure(stdout.splitlines()[-1] == 'Original held Item attack: ' + str(len(report['rows'])) +
            ' rows; cow_rows=3; inventory=' + str(len(report['inventory'])) +
            '; tools=20; network=0; native=0; genuine_owners=1; random_rebinding=0', 'complete genuine CPU capture')
        research_rows = json.loads((HERE / 'research-rows.json').read_text())['rows']
        C.ensure([{k: r[k] for k in expected} for r, expected in zip(report['rows'], research_rows)]
                 == research_rows, 'all32 prior research core rows exactly preserved')
        names = sorted(set(CORE) | {r['class'] for r in report['inventory']})
        with zipfile.ZipFile(jar) as archive:
            report['pins'] = {'client_jar_sha256': JAR,
                'scope': 'Full method declaration inventories and Code hashes for reached core and all actual Item classes; runtime reflection separately records inherited owners.',
                'classes': {name: I.inspect_class(archive.read(name + '.class')) for name in names}}
        (output / 'source-pins.json').write_text(json.dumps(report['pins'], indent=2) + '\n')
        (output / 'selected-outcomes.json').write_text(json.dumps({'client_jar_sha256': JAR,
            'rows': report['rows'], 'inventory': report['inventory']}, indent=2) + '\n')
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {str(p): C.digest(p) for p in inputs}
        report['helpers_after'] = {p.name: C.digest(p) for p in classes.iterdir()}
        if report['inputs_after'] != before or report.get('helpers_before') != report['helpers_after']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n')
    C.ensure(report['status'] == 'PASS', 'unchanged observation inputs/helpers')
    print('Original held Item attack PASS: ' + str(len(report['rows'])) + ' rows; ' + str(output))


if __name__ == '__main__':
    main()
