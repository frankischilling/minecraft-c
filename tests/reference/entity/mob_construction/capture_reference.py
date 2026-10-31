"""Capture real Overworld Mob constructors normally and through a read-only JDI observer."""
import argparse
import hashlib
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
SUPPORT = HERE.parent / 'player'
sys.path.insert(0, str(SUPPORT))
from support import require, run_bounded
sys.path.insert(0, str(HERE.parents[1] / 'client/controller'))
from classfile import inspect_class

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
HELPERS = ['ConstructorJson', 'MobConstructionTarget', 'MobConstructionJdi']


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ndjson(path):
    require(path.stat().st_size < 4194304, 'bounded observations')
    return [json.loads(line) for line in path.read_text().splitlines()]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    require(not sys.flags.optimize, 'Python optimization is not supported')
    args.output.mkdir(parents=True, exist_ok=False)
    out = args.output.resolve()
    java, javac, jar = (Path(p).resolve(strict=True) for p in (args.java, args.javac, args.client_jar))
    require(java.parent.samefile(javac.parent), 'matching JDK17 tools')
    pins = json.loads((HERE / 'source-pins.json').read_text())
    require(digest(jar) == JAR == pins['client_jar_sha256'], 'original JAR pin')
    with zipfile.ZipFile(jar) as archive:
        require(sorted(name[:-6] for name in archive.namelist() if name.endswith('.class')) == pins['jar_class_names'], 'original class inventory pin')
        for name, expected in pins['classes'].items():
            require(inspect_class(archive.read(name + '.class')) == expected, 'class/method pin ' + name)
    provenance = json.loads((HERE / 'provenance.json').read_text())
    for name, expected in provenance['reference_files'].items():
        require(digest(HERE / name) == expected, 'authored reference pin ' + name)
    inputs = [java, javac, jar, HERE / 'source-pins.json', HERE / 'operation-map.json',
              HERE / 'capture_reference.py', HERE / 'validate.py', HERE / 'provenance.json', SUPPORT / 'support.py',
              HERE.parents[1] / 'client/controller/classfile.py',
              *(HERE / (name + '.java') for name in HELPERS)]
    before = {str(p): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': 'Actual original Mob constructors, ordinary VM and read-only JDI; entropy is observed and replayed separately, never supplied to or substituted for an original constructor.'}
    env = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'CLASSPATH'):
        env.pop(name, None)
    classes = out / 'classes'
    classes.mkdir()
    try:
        for name, tool in [('java', java), ('javac', javac)]:
            result = run_bounded([tool, '-version'], out, name, env=env)
            version = (result.stdout + result.stderr).decode()
            require(result.returncode == 0 and re.search(r'(?:version "|javac )17\.', version), 'JDK17 required')
            report[name + '_version'] = version
        result = run_bounded([javac, '-g:none', '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi',
                              '-classpath', jar, '-d', classes,
                              *(HERE / (name + '.java') for name in HELPERS)], out, 'compile', env=env)
        require(result.returncode == 0 and not result.stdout and not result.stderr, 'authored helper compilation')
        require({p.name for p in classes.iterdir()} == {name + '.class' for name in HELPERS}, 'helper-only directory')
        for name in HELPERS:
            require(inspect_class((classes / (name + '.class')).read_bytes()) == provenance['helper_classes'][name], 'authored helper class/Code pin ' + name)
        report['helper_before_sha256'] = {p.name: digest(p) for p in classes.iterdir()}
        names = '\n'.join(name.replace('/', '.') for name in pins['classes']) + '\n'
        for mode in ('normal', 'debugger'):
            directory = out / mode
            directory.mkdir()
            (directory / 'classes.txt').write_text(names, encoding='ascii')
        normal = out / 'normal'
        result = run_bounded([java, '-Xint', '-Xmx256m', '-Xverify:all',
                              '--add-opens=java.base/java.lang=ALL-UNNAMED', '--add-opens=java.base/java.util=ALL-UNNAMED',
                              '-Dmob.probe.classes=' + str(normal / 'classes.txt'),
                              '-Xlog:class+init=info:file=class-init.log', '-classpath', str(classes) + os.pathsep + str(jar),
                              'MobConstructionTarget', normal / 'observations.ndjson', jar], normal, 'target', env=env)
        require(result.returncode == 0 and not result.stderr, 'ordinary unchanged constructors')
        debugger = out / 'debugger'
        result = run_bounded([java, '-Xmx256m', '--add-modules', 'jdk.jdi', '-classpath', classes,
                              'MobConstructionJdi', jar, classes, debugger, java.parent.parent], debugger, 'controller', timeout=180, env=env)
        require(result.returncode == 0 and not result.stdout and not result.stderr, 'read-only JDI controller')
        require(not (debugger / 'target.stderr').read_bytes(), 'JDI target diagnostics')
        from validate import validate
        report['observations'] = validate(out, pins)
        report['status'] = 'PASS'
    finally:
        report['input_after_sha256'] = {str(p): digest(p) for p in inputs}
        report['helper_after_sha256'] = {p.name: digest(p) for p in classes.glob('*.class')}
        same = report['input_before_sha256'] == report['input_after_sha256'] and report.get('helper_before_sha256', {}) == report['helper_after_sha256']
        if not same: report['status'] = 'FAIL'
        (out / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', newline='\n')
        require(same, 'capture inputs/helpers changed')
    print('Original Mob construction: ' + str(out / 'capture.json'))


if __name__ == '__main__':
    main()
