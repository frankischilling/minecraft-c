"""Bounded ordinary/read-only-JDI capture of unchanged client World/Block ticks."""
import argparse
import hashlib
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
SUPPORT = ROOT / 'tests/reference/entity/player'
CLASSFILE = ROOT / 'tests/reference/client/controller'
sys.path.insert(0, str(SUPPORT))
from support import require, run_bounded
sys.path.insert(0, str(CLASSFILE))
from classfile import inspect_class

HELPERS = ['ConstructorJson', 'RandomTickTarget', 'RandomTickJdi']
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    require(not sys.flags.optimize, 'unoptimized Python validation')
    args.output.mkdir(parents=True, exist_ok=False)
    out = args.output.resolve()
    java, javac, jar = (Path(p).resolve(strict=True) for p in (args.java, args.javac, args.client_jar))
    require(java.parent.samefile(javac.parent), 'matching JDK17 tools')
    pins = json.loads((HERE / 'source-pins.json').read_text())
    require(digest(jar) == JAR == pins['client_jar_sha256'], 'original client JAR pin')
    with zipfile.ZipFile(jar) as archive:
        require(sorted(n[:-6] for n in archive.namelist() if n.endswith('.class')) == pins['jar_class_names'], 'original class inventory')
        for name, expected in pins['classes'].items():
            require(inspect_class(archive.read(name + '.class')) == expected, 'whole original class/Code pin ' + name)
    provenance = json.loads((HERE / 'provenance.json').read_text())
    for name, expected in provenance['reference_files'].items():
        require(digest(HERE / name) == expected, 'authored source pin ' + name)
    for name, expected in provenance['external_helpers'].items():
        require(digest(ROOT / name) == expected, 'reused helper pin ' + name)
    inputs = [java, javac, jar, *(HERE / n for n in provenance['reference_files']),
              HERE / 'provenance.json', SUPPORT / 'support.py', CLASSFILE / 'classfile.py']
    before = {str(p): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'input_before_sha256': before,
              'scope': 'Actual complete FD.n with authored cached/saved Chunk inputs; separate actual Block ticks. Source Random.setSeed inputs; unchanged default constructor entropy; read-only debugger.'}
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
        result = run_bounded([javac, '-g:none', '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-classpath', jar,
                              '-d', classes, *(HERE / (n + '.java') for n in HELPERS)], out, 'compile', env=env)
        require(result.returncode == 0 and not result.stdout and not result.stderr, 'authored helper compilation')
        require({p.stem for p in classes.iterdir()} == set(provenance['helper_classes']), 'helper-only classpath')
        for name, expected in provenance['helper_classes'].items():
            require(inspect_class((classes / (name + '.class')).read_bytes()) == expected, 'helper class/Code pin ' + name)
        report['helper_before_sha256'] = {p.name: digest(p) for p in classes.iterdir()}
        for mode in ('normal', 'debugger'):
            directory = out / mode
            directory.mkdir()
            (directory / 'classes.txt').write_text('\n'.join(pins['classes']) + '\n', encoding='ascii')
        normal = out / 'normal'
        result = run_bounded([java, '-Xint', '-Xmx512m', '-Xverify:all', '--add-opens=java.base/java.util=ALL-UNNAMED',
                              '-Drandom.tick.classes=' + str(normal / 'classes.txt'),
                              '-Xlog:class+init=info:file=class-init.log', '-classpath', str(classes) + os.pathsep + str(jar),
                              'RandomTickTarget', normal / 'observations.ndjson', jar], normal, 'target', timeout=240, env=env)
        require(result.returncode == 0 and not result.stderr, 'ordinary unchanged complete World/Block ticks')
        debugger = out / 'debugger'
        result = run_bounded([java, '-Xmx512m', '--add-modules', 'jdk.jdi', '-classpath', classes,
                              'RandomTickJdi', jar, classes, debugger, java.parent.parent], debugger, 'controller', timeout=600, env=env)
        require(result.returncode == 0 and not result.stdout and not result.stderr, 'read-only JDI capture')
        require(not (debugger / 'target.stderr').read_bytes(), 'debugged target diagnostics')
        from validate import validate
        report['observations'] = validate(out, pins)
        report['status'] = 'PASS'
    finally:
        report['input_after_sha256'] = {str(p): digest(p) for p in inputs}
        report['helper_after_sha256'] = {p.name: digest(p) for p in classes.glob('*.class')}
        same = before == report['input_after_sha256'] and report.get('helper_before_sha256', {}) == report['helper_after_sha256']
        if not same:
            report['status'] = 'FAIL'
        (out / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', newline='\n')
        require(same, 'capture inputs/helpers changed')
    print('Original World random ticks: ' + str(out / 'capture.json'))


if __name__ == '__main__':
    main()
