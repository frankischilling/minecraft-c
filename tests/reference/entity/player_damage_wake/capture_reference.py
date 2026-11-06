"""Run two unchanged-original CPU observations and retain raw attempts privately."""
import argparse
import importlib.util
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
sys.path.insert(0, str(HERE))
import packet as P


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def inventory(root):
    return {p.relative_to(root).as_posix(): P.digest(p) for p in sorted(root.rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    private = (ROOT / '.local').resolve()
    P.require(output.is_relative_to(private) and output != private and not output.exists(),
              'capture requires a new child of workspace .local')
    jar = args.client_jar.resolve(strict=True)
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    libraries = [(args.library_root / name).resolve(strict=True) for name in P.LIBRARIES]
    P.require(P.digest(jar) == P.JAR, 'supplied original client hash')
    P.require(all(P.digest(path) == pin for path, pin in zip(libraries, P.LIBRARIES.values())),
              'imported original SDK archive hashes')
    support = {'capture.py': ROOT / 'tests/reference/client/controller/capture.py',
               'classfile.py': HERE.parent / 'player/classfile.py',
               'ConstructorJson.java': HERE.parent / 'player/ConstructorJson.java'}
    capture = module('damage_wake_bounded_capture', support['capture.py'])
    classfile = module('damage_wake_classfile', support['classfile.py'])
    pins = P.load(HERE / 'source-pins.json')
    P.require(pins['archive'] == {'sha256': P.JAR, 'bytes': jar.stat().st_size}, 'original archive pin')
    with zipfile.ZipFile(jar) as archive:
        P.require(len(archive.namelist()) == len(set(archive.namelist())), 'unique original archive names')
        P.require(not any(name.startswith('PlayerDamageWakeObservation') or name == 'ConstructorJson.class'
                          for name in archive.namelist()), 'original/helper collision')
        for name, expected in pins['classes'].items():
            P.require(classfile.parse_class(archive.read(name + '.class')) == expected,
                      'unchanged original class and method Code ' + name)
    sources = [HERE / name for name in P.SOURCES]
    inputs = [jar, java, javac, *libraries, *support.values(), *sources]
    before = {str(path): P.digest(path) for path in inputs}
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'; classes.mkdir()
    frozen = output / 'producer'; frozen.mkdir()
    for path in sources:
        (frozen / path.name).write_bytes(path.read_bytes())
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    report = {'schema': 'PLAYER_DAMAGE_WAKE_CAPTURE_V1', 'status': 'FAIL', 'input_before_sha256': before}
    helpers = None
    try:
        for name, tool in (('java', java), ('javac', javac)):
            receipt = capture.capture([str(tool), '-version'], output, name, 15, environment)
            report[name] = receipt; capture.require_success(receipt)
            version = ''.join((output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            P.require(re.search(r'(?:version "|javac )17\.', version) is not None, 'JDK17 profile')
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = capture.capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath,
            '-d', str(classes), str(HERE / 'PlayerDamageWakeObservation.java'), str(support['ConstructorJson.java'])],
            output, 'compile', 45, environment)
        report['compile'] = receipt; capture.require_success(receipt, empty_stderr=True)
        helpers = inventory(classes); report['helper_before_sha256'] = helpers
        names = output / 'class-names.txt'
        names.write_text('\n'.join(pins['classes']) + '\n', encoding='ascii')
        rows = []; runs = []
        for index in range(2):
            work = output / ('work-' + str(index)); work.mkdir()
            receipt = capture.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                '--add-opens=java.base/java.lang=ALL-UNNAMED', '-classpath', classpath,
                'PlayerDamageWakeObservation', str(jar), str(work), str(HERE / 'profiles.tsv'), str(names)],
                output, 'original-' + str(index), 120, environment, work)
            runs.append(receipt); report['original'] = runs; capture.require_success(receipt)
            stdout = (output / receipt['stdout_file']).read_text(encoding='utf-8')
            P.require(stdout.splitlines()[-1] == 'PLAYER_DAMAGE_WAKE_COMPLETE cases=32 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0',
                      'original completion and effect guards')
            bindings = [line for line in stdout.splitlines() if line.startswith('CLASS ')]
            expected = ['CLASS ' + name + ' ' + str(value['class_bytes']) + ' ' + value['class_sha256']
                        for name, value in pins['classes'].items()]
            P.require(bindings == expected, 'actually resolved original class bindings')
            observed = [P.loads(line[len('PLAYER_DAMAGE_WAKE '):]) for line in stdout.splitlines()
                        if line.startswith('PLAYER_DAMAGE_WAKE ')]
            P.validate_rows(observed); rows.append(observed)
            (output / ('finite-' + str(index) + '.json')).write_text(json.dumps(observed, indent=2) + '\n', encoding='utf-8', newline='\n')
        P.require(P.exact(rows[0], rows[1]), 'two fresh original VM selected results agree exactly')
        finite = {'schema': P.SCHEMA, 'scope': P.SCOPE, 'rows': rows[0]}
        (output / 'finite.json').write_text(json.dumps(finite, indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS_ORIGINAL_CPU_COMPONENTS_ONLY', profiles_per_vm=32,
                      finite_sha256=P.digest(output / 'finite.json'),
                      selected_scalars=sum(P.scalars(row) for row in rows[0]),
                      public_source_sha256={path.name: P.digest(path) for path in sources},
                      support_sha256={name: P.digest(path) for name, path in support.items()},
                      binary_sha256={'client': P.digest(jar), 'java': P.digest(java), 'javac': P.digest(javac),
                                     **{name: P.digest(path) for name, path in zip(P.LIBRARIES, libraries)}})
    finally:
        report['input_after_sha256'] = {str(path): P.digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        unchanged = before == report['input_after_sha256'] and (helpers is None or helpers == report['helper_after_sha256'])
        if not unchanged: report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        P.require(unchanged, 'all executed inputs and compiled helpers stay unchanged')
    print('Two original CPU component captures pass:', output / 'capture.json')


if __name__ == '__main__':
    main()
