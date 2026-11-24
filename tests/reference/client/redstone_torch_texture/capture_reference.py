"""Root-released two-ordinary-VM scalar capture of unchanged DB texture calls."""
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
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
AUTHORED = ('.gitattributes', 'TorchTextureObservation.java', 'capture_reference.py',
            'declared-inputs.json', 'input-freeze.json', 'profiles.tsv', 'source-pins.json')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('db_texture_capture', ROOT / 'tests/reference/client/controller/capture.py')
I = module('db_texture_classfile', ROOT / 'tests/reference/client/controller/classfile.py')
P = module('db_texture_libraries', ROOT / 'tests/reference/client/picking/observe.py')


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='ascii', newline='\n')


def inventory(root):
    return {p.relative_to(root).as_posix(): C.digest(p) for p in sorted(root.rglob('*')) if p.is_file()}


def admit(raw, stderr, classes, rows):
    path = classes.as_posix()
    uri = 'file:' + ('/' if classes.drive else '') + path + '/'
    expected_stderr = ('WARNING: A terminally deprecated method in java.lang.System has been called\n'
        'WARNING: System::setSecurityManager has been called by TorchTextureObservation (' + uri + ')\n'
        'WARNING: Please consider reporting this to the maintainers of TorchTextureObservation\n'
        'WARNING: System::setSecurityManager will be removed in a future release\n').encode('ascii')
    C.ensure(stderr == expected_stderr, 'exact SecurityManager-only stderr; no setup/daemon error')
    C.ensure(raw.endswith(b'\r\n') and raw == raw.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'),
             'complete original CRLF stdout')
    lines = raw.decode('ascii').splitlines()
    C.ensure(lines[:2] == ['16 achievements', '151 recipes'], 'exact original Block bootstrap')
    C.ensure(len(lines) == 165 and lines[-3:] == [
        'DB_GUARD\tsource\tnetwork=0\tnative=0\texecs=0\twrites=0',
        'DB_GUARD\tcontrols\tnetwork=2\tnative=1\texecs=1\twrites=1', 'DB_COMPLETE\t160'],
        'closed original rows, denial guards and completion')
    saved = {}
    for index, (line, profile) in enumerate(zip(lines[2:-3], rows)):
        fields = line.split('\t')
        C.ensure(len(fields) == 21 and fields[:3] == ['DB_TEXTURE', str(index), profile['name']] and
                 fields[15:] == ['db', 'sm', 'sm', '55', '1', '1'], 'actual scalar row/class/owner identity')
        numbers = [int(value) for value in fields[3:14]]
        C.ensure(all(str(v) == s and -(2**31) <= v < 2**31 for v, s in zip(numbers, fields[3:14])) and
                 numbers[:6] == profile['values'] and fields[14] == format(numbers[-1] & 0xffffffff, '08x'),
                 'exact frozen signed operands and returned int/hex transport')
        block, side, metadata, mutate, self_bm, dust_bm = numbers[:6]
        before_self, before_dust, entered_self, entered_dust = numbers[6:10]
        saved.setdefault(block, before_self)
        saved.setdefault('dust', before_dust)
        C.ensure(before_self == saved[block] and before_dust == saved['dust'] and
                 [entered_self, entered_dust] == ([self_bm, dust_bm] if mutate else [before_self, before_dust]),
                 'declared live instance inputs restored before every next call')
    return ('\n'.join(lines[2:]) + '\n').encode('ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--jar', type=Path, default=ROOT / 'b1.7.3.jar')
    parser.add_argument('--libraries', type=Path, default=Path(os.environ.get('APPDATA', '')) / '.minecraft/libraries')
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    C.ensure(args.execute, 'prepared only; Root must release --execute')
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'unchanged original JAR')
    java, javac = [Path(shutil.which(p) or p).resolve(strict=True) for p in (args.java, args.javac)]
    C.ensure(java.parent == javac.parent, 'matching actual JDK')
    jdk = java.parent.parent
    jvm = java.parent / 'server/jvm.dll'
    if not jvm.is_file():
        jvm = jdk / 'lib/server/libjvm.so'
    libraries = [(args.libraries / name).resolve(strict=True) for name in P.LIBRARIES]
    C.ensure(all(C.digest(p) == h for p, h in zip(libraries, P.LIBRARIES.values())), 'pinned actual libraries')
    pins = json.loads((HERE / 'source-pins.json').read_text())
    with zipfile.ZipFile(jar) as archive:
        C.ensure(all(I.inspect_class(archive.read(n + '.class')) == info for n, info in pins['classes'].items()),
                 'actual selected class and Code identities')
    declaration = json.loads((HERE / 'declared-inputs.json').read_text())
    frozen = json.loads((HERE / 'input-freeze.json').read_text())
    C.ensure(all(C.digest(HERE / n) == h for n, h in frozen['files'].items()), 'inputs frozen before helper')
    rows = [{'name': f[0], 'values': list(map(int, f[1:]))}
            for f in (line.split('\t') for line in (HERE / 'profiles.tsv').read_text().splitlines())]
    C.ensure(rows == declaration['rows'] and len(rows) == 160, 'complete fixed scalar roster')
    inputs = [jar, *libraries, java, javac, jvm.resolve(strict=True), jdk / 'jmods/java.base.jmod',
              jdk / 'lib/modules', *java.parent.glob('*.dll'), *(HERE / n for n in AUTHORED),
              ROOT / 'tests/reference/client/controller/capture.py',
              ROOT / 'tests/reference/client/controller/classfile.py',
              ROOT / 'tests/reference/client/picking/observe.py', ROOT / 'tests/tools/client/test_picking.py']
    before = {str(p.resolve()): C.digest(p) for p in sorted(set(inputs))}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for n in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(n, None)
    cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
    report = {'status': 'FAIL', 'inputs_before': before, 'runs': [],
              'scope': '160 genuine scalar metadata virtual calls in each of two fresh ordinary VMs; no native/GUI/JDI/entropy qualification'}
    write(output / 'freeze.json', {'inputs_before': before})
    try:
        for role, tool in (('java', java), ('javac', javac)):
            receipt = C.capture([str(tool), '-version'], output, role, 10, environment)
            report[role] = receipt
            C.require_success(receipt)
            C.ensure('17.0.17' in (output / (role + '.stdout')).read_text() +
                     (output / (role + '.stderr')).read_text(), 'qualified JDK17.0.17')
        receipt = C.capture([str(javac), '-encoding', 'UTF-8', '-implicit:none', '-cp', cp, '-d',
                             str(classes), str(HERE / 'TorchTextureObservation.java')], output, 'compile', 30, environment)
        report['compile'] = receipt
        C.require_success(receipt, empty_stderr=True)
        report['helpers_before'] = inventory(classes)
        C.ensure(set(report['helpers_before']) == {'TorchTextureObservation.class', 'TorchTextureObservation$1.class'},
                 'only actual authored helper classes')
        for repeat in range(2):
            folder = output / ('ordinary-' + str(repeat))
            folder.mkdir()
            command = [str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                       '-Djava.security.manager=allow', '-cp', cp, 'TorchTextureObservation', str(jar), str(HERE / 'profiles.tsv')]
            receipt = C.capture(command, folder, 'original', 90, environment, folder)
            report['runs'].append({'repeat': repeat, 'capture': receipt})
        selected = []
        for run in report['runs']:
            folder = output / ('ordinary-' + str(run['repeat']))
            C.require_success(run['capture'])
            projection = admit((folder / 'original.stdout').read_bytes(),
                               (folder / 'original.stderr').read_bytes(), classes, rows)
            (folder / 'selected.tsv').write_bytes(projection)
            run.update(passed=True, rows=160, selected_sha256=C.digest(folder / 'selected.tsv'))
            selected.append(projection)
        C.ensure(selected[0] == selected[1], 'independent ordinary VM scalar outputs agree')
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {p: C.digest(Path(p)) for p in before}
        report['helpers_after'] = inventory(classes)
        report['inputs_unchanged'] = report['inputs_after'] == before
        report['helpers_unchanged'] = report.get('helpers_before') == report['helpers_after']
        if not report['inputs_unchanged'] or not report['helpers_unchanged']:
            report['status'] = 'FAIL'
        write(output / 'capture.json', report)
    C.ensure(report['status'] == 'PASS', 'complete fixed two-VM capture with input/product continuity')
    print('PASS original scalar160 x2ordinaryVMs;', output / 'capture.json')


if __name__ == '__main__':
    main()
