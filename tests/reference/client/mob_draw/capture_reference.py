"""Capture unchanged-original Sheep drawing on real Linux LWJGL2 Displays."""
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
sys.path.insert(0, str(ROOT / 'tests/reference/entity/player'))
from support import require, run_bounded
sys.path.insert(0, str(ROOT / 'tests/reference/client/controller'))
from classfile import inspect_class

JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SDK = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
NATIVE_ARCHIVE = 'f89797962cd00024588b9972b7b2fb9d1c1c81817db0e47499824df2f8a310fd'
NATIVE = '60a9a67061a3dc6b9d055946c58f1f64792abb2127ba0156eaf735dbee5bc729'


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--natives-archive', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--xvfb-run', default=shutil.which('xvfb-run'))
    args = parser.parse_args()
    require(not sys.flags.optimize and sys.platform == 'linux', 'unoptimized Linux Python; separate real Xvfb Display')
    args.output.mkdir(parents=True, exist_ok=False)
    out = args.output.resolve()
    java, javac, jar, archive, xvfb = (Path(p).resolve(strict=True) for p in
                                      (args.java, args.javac, args.client_jar, args.natives_archive, args.xvfb_run))
    require(java.parent == javac.parent, 'matching JDK17 tools')
    pins = json.loads((HERE / 'source-pins.json').read_text())
    provenance = json.loads((HERE / 'provenance.json').read_text())
    require(digest(jar) == JAR == pins['client_jar_sha256'], 'fixed original JAR')
    with zipfile.ZipFile(jar) as original:
        require(sorted(n[:-6] for n in original.namelist() if n.endswith('.class')) == pins['jar_class_names'], 'original class inventory')
        for name, expected in pins['classes'].items():
            require(inspect_class(original.read(name + '.class')) == expected, 'original whole-class/Code pin ' + name)
    for name, expected in provenance['reference_files'].items():
        require(digest(HERE / name) == expected, 'public input pin ' + name)
        require(b'\r' not in (HERE / name).read_bytes(), 'LF public text ' + name)
    require(b'\r' not in (HERE / 'provenance.json').read_bytes(), 'LF provenance')
    for name, expected in provenance['external_helpers'].items():
        require(digest(ROOT / name) == expected, 'reused authored helper pin ' + name)
    libraries = [(args.libraries / name).resolve(strict=True) for name in SDK]
    for path, expected in zip(libraries, SDK.values()):
        require(digest(path) == expected, 'SDK input pin ' + str(path))
    require(digest(archive) == NATIVE_ARCHIVE, 'Linux SDK archive pin')
    natives = out / 'natives'
    natives.mkdir()
    with zipfile.ZipFile(archive) as native_zip:
        native = native_zip.read('liblwjgl64.so')
    require(hashlib.sha256(native).hexdigest() == NATIVE, 'actual SDK liblwjgl64.so')
    (natives / 'liblwjgl64.so').write_bytes(native)
    runtime = [java.parent.parent / 'lib/server/libjvm.so', java.parent.parent / 'lib/modules']
    display_tools = [Path(shutil.which(name)).resolve(strict=True) for name in ('Xvfb', 'xauth')]
    inputs = [java, javac, jar, archive, xvfb, *display_tools, *runtime, Path(sys.executable).resolve(), *libraries,
              natives / 'liblwjgl64.so', *(HERE / n for n in provenance['reference_files']), HERE / 'provenance.json',
              *(ROOT / n for n in provenance['external_helpers'])]
    before = {str(p): digest(p) for p in inputs}
    report = {'status': 'FAIL', 'scope': provenance['scope'], 'input_before_sha256': before,
              'repo_input_before_sha256': {str(p.relative_to(ROOT)): digest(p) for p in inputs if p.is_relative_to(ROOT)}}
    env = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'CLASSPATH', 'DISPLAY'):
        env.pop(name, None)
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    classes = out / 'classes'
    classes.mkdir()
    try:
        for name, tool in [('java', java), ('javac', javac)]:
            result = run_bounded([tool, '-version'], out, name, env=env)
            version = (result.stdout + result.stderr).decode()
            require(result.returncode == 0 and re.search(r'(?:version "|javac )17\.', version), 'JDK17 required')
            report[name + '_version'] = version
        compile_command = [javac, '-g:none', '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi', '-classpath',
                           os.pathsep.join(map(str, [jar, *libraries])), '-d', classes,
                           *(HERE / (name + '.java') for name in ('ConstructorJson', 'MobDrawTarget', 'MobDrawJdi'))]
        report['compile_command'] = list(map(str, compile_command))
        result = run_bounded(compile_command, out, 'compile', env=env)
        require(result.returncode == 0 and not result.stdout and not result.stderr, 'authored helper compilation')
        require({p.stem for p in classes.iterdir()} == set(provenance['helper_classes']), 'helper-only classpath')
        for name, expected in provenance['helper_classes'].items():
            require(inspect_class((classes / (name + '.class')).read_bytes()) == expected, 'helper class/Code pin ' + name)
        report['helper_before_sha256'] = {p.name: digest(p) for p in classes.iterdir()}
        commands = {}
        for mode in ('normal', 'debugger'):
            directory = out / mode
            directory.mkdir()
            (directory / 'resources').mkdir()
            (directory / 'classes.txt').write_text('\n'.join(pins['classes']) + '\n', encoding='ascii', newline='\n')
            classpath = os.pathsep.join(map(str, [classes, directory / 'resources', jar, *libraries]))
            display = [xvfb, '-a', '-s', '-screen 0 640x480x24 -noreset']
            if mode == 'normal':
                command = [*display, java, '-Xint', '-Xmx512m', '-Xverify:all', '--add-opens=java.base/java.util=ALL-UNNAMED',
                           '--add-opens=java.base/java.lang=ALL-UNNAMED', '-Djava.awt.headless=true', '-Djava.security.manager=allow',
                           '-Dorg.lwjgl.librarypath=' + str(natives), '-Dmobdraw.classes=' + str(directory / 'classes.txt'),
                           '-Xlog:class+init=info:file=class-init.log', '-classpath', classpath, 'MobDrawTarget', directory, HERE]
            else:
                command = [*display, java, '--add-modules', 'jdk.jdi', '-classpath', classes, 'MobDrawJdi', directory,
                           java.parent.parent, natives, classpath, jar, HERE]
            commands[mode] = list(map(str, command))
            result = run_bounded(command, directory, 'process', timeout=900, env=env)
            require(result.returncode == 0, mode + ' actual original source process')
        report['commands'] = commands
        from validate import validate
        report['observations'] = validate(out, pins, jar)
        report['status'] = 'PASS'
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        report['input_after_sha256'] = {str(p): digest(p) for p in inputs}
        report['helper_after_sha256'] = {p.name: digest(p) for p in classes.glob('*.class')}
        same = before == report['input_after_sha256'] and report.get('helper_before_sha256', {}) == report['helper_after_sha256']
        if not same:
            report['status'] = 'FAIL'
        (out / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', newline='\n')
        require(same, 'capture inputs or compiled helpers changed')
    print('Original Sheep draw: ' + str(out / 'capture.json'))


if __name__ == '__main__':
    main()
