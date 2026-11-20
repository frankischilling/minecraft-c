"""Prepare and observe genuine original specialized inventory calls on a hidden context."""
import argparse
import hashlib
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
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
SOURCES = ('InventoryGeometryTarget.java', 'InventoryGeometryJdi.java', 'ConstructorJson.java',
           'audit.py', 'compare.py', 'capture_reference.py', 'test_packet.py', 'declared-inputs.json', 'profiles.json')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('inventory_geometry_capture', ROOT / 'tests/reference/client/controller/capture.py')
I = module('inventory_geometry_classfile', ROOT / 'tests/reference/client/controller/classfile.py')
A = module('inventory_geometry_audit', HERE / 'audit.py')


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def redacted(text):
    lines = []
    for line in text.splitlines():
        if line.startswith('INVENTORY_GEOMETRY_COMPLETE '):
            value = json.loads(line.split(' ', 1)[1])
            value['links'] = ['natives/lwjgl64.dll' if Path(name).is_absolute() else name
                              for name in value['links']]
            line = 'INVENTORY_GEOMETRY_COMPLETE ' + json.dumps(value, separators=(',', ':'))
        lines.append(line)
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('immediate', 'compile'), default='immediate')
    parser.add_argument('--observe', action='store_true')
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path, default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    args = parser.parse_args()
    C.ensure(os.name == 'nt', 'pinned Windows x64 LWJGL capture profile')
    out = args.output.resolve()
    if not args.observe:
        out.mkdir(parents=True, exist_ok=False)
        classes, natives = out / 'classes', out / 'natives'
        classes.mkdir()
        natives.mkdir()
        jar = args.jar.resolve(strict=True)
        C.ensure(C.digest(jar) == JAR, 'untouched supplied original archive')
        libroot = args.libraries.resolve(strict=True)
        libraries = [libroot / name for name in LIBRARIES]
        C.ensure(all(C.digest(path) == wanted for path, wanted in zip(libraries, LIBRARIES.values())),
                 'exact pinned libraries')
        archive = libroot / 'org/lwjgl/lwjgl/lwjgl-platform/2.9.0/lwjgl-platform-2.9.0-natives-windows.jar'
        C.ensure(C.digest(archive) == '68d4e8a53168123051cc5729c41e89f4eef074f7fe1b42984cbc140f6bef6d43',
                 'pinned native archive')
        with zipfile.ZipFile(archive) as image:
            (natives / 'lwjgl64.dll').write_bytes(image.read('lwjgl64.dll'))
        java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
        C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'one supplied JDK17')
        files = [jar, *libraries, archive, natives / 'lwjgl64.dll', java, javac,
                 java.parent.parent / 'lib/modules', java.parent / 'server/jvm.dll',
                 *(HERE / name for name in SOURCES),
                 ROOT / 'tests/reference/client/controller/capture.py',
                 ROOT / 'tests/reference/client/controller/classfile.py',
                 *(path for path in java.parent.glob('*.dll') if path.is_file())]
        environment = os.environ.copy()
        for name in ('APPDATA', 'TMP', 'TEMP'):
            directory = out / name.lower()
            directory.mkdir()
            environment[name] = str(directory)
        classpath = os.pathsep.join(map(str, [classes, jar, *libraries]))
        report = {'status': 'PREPARED', 'mode': args.mode,
                  'inputs_before': {str(path.resolve()): C.digest(path) for path in files},
                  'environment': {name: environment[name] for name in ('APPDATA', 'TMP', 'TEMP')},
                  'java': str(java), 'classpath': classpath, 'jar': str(jar), 'natives': str(natives)}
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi',
            '-cp', classpath, '-d', str(classes), *(str(HERE / name) for name in SOURCES if name.endswith('.java'))],
            out, 'compile', 60, environment, out)
        write(out / 'capture.json', report)
        C.require_success(report['compile'], empty_stderr=True)
        report['helpers_before'] = {path.name: C.digest(path) for path in classes.iterdir()}
        with zipfile.ZipFile(jar) as image:
            names = ('kq', 'net/minecraft/client/Minecraft', 'kv', 'ik', 'ji', 'sj', 'cv', 'nw', 'uu', 'ss', 'or', 'jw', 'ge')
            report['pins'] = {'client_jar_sha256': JAR,
                             'classes': {name: I.inspect_class(image.read(name + '.class')) for name in names}}
            report['resources'] = {name: {'bytes': len(image.read(name)),
                                         'sha256': hashlib.sha256(image.read(name)).hexdigest()}
                                   for name in ('terrain.png', 'font/default.png', 'pack.png')}
        write(out / 'source-pins.json', report['pins'])
        write(out / 'capture.json', report)
        print('Prepared genuine original hidden-context inventory probe: ' + str(out))
        return
    report = json.loads((out / 'capture.json').read_text())
    C.ensure(report['status'] == 'PREPARED' and report['mode'] == args.mode, 'matching prepared observer mode')
    C.ensure(all(C.digest(path) == wanted for path, wanted in report['inputs_before'].items()), 'unchanged prepared inputs')
    environment = os.environ.copy()
    environment.update(report['environment'])
    java, natives, classpath, jar = Path(report['java']), report['natives'], report['classpath'], report['jar']
    report['status'] = 'FAIL'
    try:
        report['original'] = C.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m',
            '-Djava.awt.headless=true', '-Djava.security.manager=allow', '-Dorg.lwjgl.librarypath=' + natives,
            '-cp', classpath, 'InventoryGeometryTarget', jar, str(out / 'game'), natives, args.mode],
            out, 'original', 180, environment, out)
        C.require_success(report['original'])
        ordinary = (out / 'original.stdout').read_text(encoding='utf-8')
        C.ensure(len(A.tagged(ordinary, 'COMPLETE')) == 1, 'complete ordinary original run')
        debug = out / 'debugger'
        debug.mkdir()
        report['debugger'] = C.capture([str(java), '--add-modules', 'jdk.jdi', '-Xmx256m', '-cp', str(out / 'classes'),
            'InventoryGeometryJdi', str(debug), str(java.parent.parent), natives, classpath, jar, args.mode],
            debug, 'controller', 300, environment, debug)
        C.require_success(report['debugger'], empty_stderr=True)
        target = (debug / 'target.stdout').read_text(encoding='utf-8')
        events = [json.loads(line, object_pairs_hook=A.P.unique)
                  for line in (debug / 'journal.ndjson').read_text().splitlines()]
        report['audit'] = A.audit(report, ordinary, target, events)
        report['status'] = 'PASS'
        write(out / 'finite.json', {'schema': 1, 'client_jar_sha256': JAR, 'mode': args.mode,
            'scope': 'Direct unchanged-original specialized CV inventory CPU geometry; own-VM inactive backing audited separately. No fullframe, pixels or immediate per-draw GL-state comparison.',
            'rows': report['audit']['rows'], 'inputs': A.tagged(target, 'INPUT'), 'returns': A.tagged(target, 'RETURN')})
        write(out / 'observations.json', {'mode': args.mode, 'ordinary_stdout': redacted(ordinary),
                                        'debugger_stdout': redacted(target), 'events': events})
    finally:
        report['inputs_after'] = {path: C.digest(path) for path in report['inputs_before']}
        report['helpers_after'] = {path.name: C.digest(path) for path in (out / 'classes').iterdir()}
        if report['inputs_before'] != report['inputs_after'] or report['helpers_before'] != report['helpers_after']:
            report['status'] = 'FAIL'
        write(out / 'capture.json', report)
    C.ensure(report['status'] == 'PASS', 'complete qualified unchanged original capture')
    print(json.dumps({'status': 'PASS', 'mode': args.mode,
        **{key: report['audit'][key] for key in ('draws', 'raw_words', 'active_words', 'inactive_word_checks')},
        'report': str(out / 'capture.json')}))


if __name__ == '__main__':
    main()
