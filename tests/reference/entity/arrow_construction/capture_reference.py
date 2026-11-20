"""Retain complete unchanged-original Arrow constructor/heading calls in new private attempts."""
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
ROOT = HERE.parents[3]
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}


def need(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def rows(text):
    from observation import parse_rows
    return parse_rows(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ordinary-only', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    private = (ROOT / '.local').resolve()
    need(output.is_relative_to(private) and output != private and not output.exists(), 'new private attempt destination')
    jar = args.client_jar.resolve(strict=True)
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    need(digest(jar) == JAR and all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'original archive/SDK identities')
    support = {'capture.py': ROOT / 'tests/reference/client/controller/capture.py',
               'classfile.py': HERE.parent / 'player/classfile.py', 'ConstructorJson.java': HERE.parent / 'player/ConstructorJson.java',
               'observation.py': HERE / 'observation.py'}
    capture = module('arrow_bounded_capture', support['capture.py'])
    capture.STDOUT_LIMIT = 16 * 1024 * 1024
    classfile = module('arrow_classfile', support['classfile.py'])
    pins = json.loads((HERE / 'source-pins.json').read_text(encoding='utf-8'))
    need(pins['archive'] == {'sha256': JAR, 'bytes': jar.stat().st_size}, 'archive declaration')
    with zipfile.ZipFile(jar) as archive:
        need(len(archive.namelist()) == len(set(archive.namelist())), 'unique archive members')
        need(not any(name.startswith('ArrowConstruction') or name == 'ConstructorJson.class' for name in archive.namelist()), 'no helper/original collision')
        for name, pin in pins['classes'].items():
            need(classfile.parse_class(archive.read(name + '.class')) == pin, 'original class/Code ' + name)
    sources = [HERE / name for name in ('ArrowConstructionObservation.java', 'ArrowConstructionJdi.java',
        'capture_reference.py', 'profiles.tsv', 'class-names.txt', 'source-pins.json', 'PROTOCOL.md')]
    inputs = [jar, java, javac, *libraries, *support.values(), *sources]
    before = {str(path): digest(path) for path in inputs}
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'; classes.mkdir()
    frozen = output / 'producer'; frozen.mkdir()
    for path in sources:
        shutil.copyfile(path, frozen / path.name)
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    report = {'schema': 'ARROW_CONSTRUCTION_CAPTURE_V1', 'status': 'FAIL', 'input_before_sha256': before}
    helpers = None
    try:
        for name, tool in (('java', java), ('javac', javac)):
            receipt = capture.capture([str(tool), '-version'], output, name, 15, environment)
            report[name] = receipt; capture.require_success(receipt)
            version = ''.join((output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            need(re.search(r'(?:version "|javac )17\.', version) is not None, 'JDK17 tools')
        classpath = os.pathsep.join(map(str, (classes, jar, *libraries)))
        java_sources = [HERE / 'ArrowConstructionObservation.java', support['ConstructorJson.java']]
        if not args.ordinary_only:
            java_sources.append(HERE / 'ArrowConstructionJdi.java')
        receipt = capture.capture([str(javac), '--add-modules', 'jdk.jdi', '-encoding', 'UTF-8', '-classpath', classpath,
                                   '-d', str(classes), *map(str, java_sources)], output, 'compile', 45, environment)
        report['compile'] = receipt; capture.require_success(receipt, empty_stderr=True)
        helpers = {path.relative_to(classes).as_posix(): digest(path) for path in sorted(classes.rglob('*')) if path.is_file()}
        report['helper_before_sha256'] = helpers
        runs = []
        for index in range(2):
            work = output / ('ordinary-work-' + str(index)); work.mkdir()
            receipt = capture.capture([str(java), '-Xint', '-Xverify:all', '-Xmx512m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED', '--add-opens=java.base/java.lang=ALL-UNNAMED',
                '-classpath', classpath, 'ArrowConstructionObservation', str(jar), str(work), str(HERE / 'profiles.tsv'), str(HERE / 'class-names.txt')],
                output, 'ordinary-' + str(index), 240, environment, work)
            runs.append(receipt); report['ordinary'] = runs; capture.require_success(receipt)
            text = (output / receipt['stdout_file']).read_text(encoding='utf-8')
            need(text.splitlines()[-1] == 'ARROW_CONSTRUCTION_COMPLETE cases=40 network=0 native=0 rejected_writes=0 exec=0 ticks=0 GUI=0', 'complete source/effect guard footer')
            need([line for line in text.splitlines() if line.startswith('CLASS ')] ==
                 ['CLASS ' + name + ' ' + str(pin['class_bytes']) + ' ' + pin['class_sha256'] for name, pin in pins['classes'].items()], 'actual resolved original origins')
            values = rows(text)
            (output / ('ordinary-' + str(index) + '.json')).write_text(json.dumps(values, separators=(',', ':')) + '\n', encoding='utf-8', newline='\n')
        if not args.ordinary_only:
            debuggers = []
            for index in range(2):
                folder = output / ('debugger-' + str(index)); folder.mkdir()
                work = folder / 'work'; work.mkdir()
                receipt = capture.capture([str(java), '--add-modules', 'jdk.jdi', '-Xmx512m', '-classpath', classpath, 'ArrowConstructionJdi',
                    str(java.parent.parent), classpath, str(jar), str(work), str(HERE / 'profiles.tsv'), str(HERE / 'class-names.txt'), str(folder)],
                    output, 'debugger-' + str(index), 600, environment)
                debuggers.append(receipt); report['debugger'] = debuggers; capture.require_success(receipt, empty_stderr=True)
                rows((folder / 'target.stdout').read_text(encoding='utf-8'))
        report.update(status='PASS_PREFLIGHT_COMPLETE_CALLS' if args.ordinary_only else 'PASS_COMPLETE_ORIGINAL_CALL_CAPTURES', profiles_per_vm=40,
            public_source_sha256={path.name: digest(path) for path in sources}, support_sha256={name: digest(path) for name, path in support.items()},
            binary_sha256={'client': digest(jar), 'java': digest(java), 'javac': digest(javac), **{name: digest(path) for name, path in zip(LIBRARIES, libraries)}})
    finally:
        report['input_after_sha256'] = {str(path): digest(path) for path in inputs}
        report['helper_after_sha256'] = {path.relative_to(classes).as_posix(): digest(path) for path in sorted(classes.rglob('*')) if path.is_file()}
        unchanged = before == report['input_after_sha256'] and (helpers is None or helpers == report['helper_after_sha256'])
        if not unchanged:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        need(unchanged, 'all captured inputs/helpers remain unchanged')
    print('Complete original Arrow constructor/heading capture: ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
