"""Two fresh guarded VMs observe unchanged RU/QM on authored saved Worlds."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE.parent / 'controller'))
from capture import capture, digest, ensure, require_success
from classfile import inspect_class
sys.path.insert(0, str(HERE))
from inputs import world

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
CLASSES = 'ru wb uu qm xw sn fd lm xv xa ia jh rh mx'.split()
GUARDS = ['GUARD\tsource_connect\t0', 'GUARD\tsource_listen\t0', 'GUARD\tsource_link\t0',
          'GUARD\tconnect_denied\t1', 'GUARD\tlisten_denied\t1', 'GUARD\tlink_denied\t1']


def inventory(path):
    return {p.relative_to(path).as_posix(): digest(p) for p in path.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    ensure(digest(jar) == JAR, 'unchanged original client archive')
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    ensure(all(digest(p) == h for p, h in zip(libraries, LIBRARIES.values())), 'four pinned metadata archives')
    java, javac = (Path(p).resolve(strict=True) for p in (args.java, args.javac))
    ensure(java.parent == javac.parent, 'one observation JDK')
    vm = java.parent / 'server/jvm.dll'
    if not vm.is_file():
        vm = java.parent.parent / 'lib/server/libjvm.so'
    inputs = [jar, *libraries, java, javac, java.parent.parent / 'lib/modules', vm,
              HERE / 'TallGrassTintObservation.java', HERE / 'inputs.py', HERE / 'profiles.tsv',
              Path(__file__).resolve(), HERE.parent / 'controller/capture.py',
              HERE.parent / 'controller/classfile.py', HERE.parent / 'picking/observe.py',
              ROOT / 'tests/tools/client/test_picking.py']
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    products = out / 'classes'
    products.mkdir()
    before = {str(p): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before, 'runs': []}
    env = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        env.pop(name, None)
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = capture([str(tool), '-version'], out, label, 10, env)
            require_success(receipt)
            version = (out / receipt['stdout_file']).read_text() + (out / receipt['stderr_file']).read_text()
            ensure(re.search(r'(?:version "|javac )17\.', version), 'JDK17 observation runtime')
        pins = {'client_jar_sha256': JAR, 'classes': {}}
        with zipfile.ZipFile(jar) as archive:
            for name in CLASSES:
                pins['classes'][name] = inspect_class(archive.read(name + '.class'))
        (out / 'source-pins.json').write_text(json.dumps(pins, indent=2) + '\n', encoding='utf-8', newline='\n')
        classpath = os.pathsep.join(str(p) for p in (products, jar, *libraries))
        compiled = capture([str(javac), '-encoding', 'UTF-8', '-cp', classpath, '-d', str(products),
                            str(HERE / 'TallGrassTintObservation.java')], out, 'compile', 45, env)
        require_success(compiled, empty_stderr=True)
        helpers = inventory(products)
        ensure(set(helpers) == {'TallGrassTintObservation.class', 'TallGrassTintObservation$1.class'},
               'only authored observer classes, no game replacements')
        selections = []
        for repeat in range(2):
            saved = out / ('world-' + str(repeat))
            world(saved, original=True)
            disk_before = inventory(saved)
            receipt = capture([str(java), '-Xint', '-Xverify:all', '-Xmx512m', '-Djava.awt.headless=true',
                               '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                               '-cp', classpath, 'TallGrassTintObservation', str(jar), str(saved),
                               str(HERE / 'profiles.tsv')], out, 'original-' + str(repeat), 90, env, out)
            report['runs'].append(receipt)
            require_success(receipt)
            lines = (out / receipt['stdout_file']).read_text().splitlines()
            ensure(lines[-1] == 'Original TallGrass tint: 28 rows', 'original normal completion')
            selected = [line for line in lines if line.startswith('TINT\t')]
            ensure(len(selected) == 28 and [line for line in lines if line.startswith('GUARD\t')] == GUARDS,
                   'complete rows and actual guard denial controls')
            ensure({k: v for k, v in disk_before.items() if k != 'session.lock'} ==
                   {k: v for k, v in inventory(saved).items() if k != 'session.lock'}, 'saved input bytes unchanged')
            selections.append(selected)
        ensure(selections[0] == selections[1], 'two independently fresh VMs agree')
        ensure(inventory(products) == helpers, 'unchanged helper products')
        (out / 'observations.tsv').write_text('\n'.join(selections[0]) + '\n', encoding='utf-8', newline='\n')
        (out / 'guards.tsv').write_text('\n'.join(GUARDS) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=28, fresh_vms=2, helper_sha256=helpers,
                      observations_sha256=digest(out / 'observations.tsv'), guards_sha256=digest(out / 'guards.tsv'))
    finally:
        report['inputs_after'] = {str(p): digest(p) for p in inputs}
        same = report['inputs_after'] == before
        if not same:
            report['status'] = 'FAIL'
        (out / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        ensure(same, 'original/JDK/helper/source inputs unchanged')
    print(json.dumps({'status': report['status'], 'rows': report.get('rows'), 'fresh_vms': report.get('fresh_vms'),
                      'report': str(out / 'capture.json')}))


if __name__ == '__main__':
    main()
