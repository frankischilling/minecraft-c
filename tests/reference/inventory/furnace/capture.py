"""Capture genuine unchanged original Furnace SK/BM component operations."""
import argparse
import base64
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
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
CORE = ('sk', 'ow', 'lw', 'iz', 'gm', 'uu', 'ln', 'ey', 'nu', 'sp', 'ij', 'bm', 'vq', 'gp', 'dw', 'ix', 'ec', 'tc', 'fd', 'gs')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


C = module('furnace_bounded_capture', SUPPORT / 'capture.py')
I = module('furnace_classfile', SUPPORT / 'classfile.py')


def unique(pairs):
    result = dict(pairs)
    C.ensure(len(result) == len(pairs), 'unique JSON fields')
    return result


def declarations(cases):
    def locations(value):
        return ';'.join(str(index) + ':' + label for index, label in value.items()) or '-'
    lines = []
    for c in cases:
        stacks = ';'.join(':'.join(map(str, (name, *[v[key] for key in ('id', 'count', 'damage', 'animation')]))) for name, v in c['stacks'].items()) or '-'
        n = c['nbt']
        lines.append('\t'.join(map(str, (c['profile'], c['operation'], c['index'], c['amount'], *c['fields'], ','.join(map(str,c['xyz'])), stacks, locations(c['tile']), locations(c['player']), c['argument'] or '-', ';'.join(','.join(map(str,row)) for row in n['items']) or '-', n['burn'], n['cook'], ','.join(map(str,n['xyz'])), n['mode'], str(c['array_null']).lower(), str(c['player_null']).lower(), str(c['tile_null']).lower()))))
    return '\n'.join(lines) + '\n'


N = module('furnace_independent_nbt', ROOT / 'tests/tools/world/test_source_saving.py')


def decode_wire(text):
    if text is None:
        return None
    raw = base64.b64decode(text, validate=True)
    return {'bytes': len(raw), 'sha256': C.hashlib.sha256(raw).hexdigest(), 'tags': N.Nbt(raw).root()}


def read_rows(raw, cases, pins):
    lines = raw.decode('utf-8', errors='strict').splitlines()
    rows, guards, origins, preamble, recipes_before, recipes_after = [], [], [], [], [], []
    for line in lines:
        for prefix, out in (('FURNACE_ROW ', rows), ('FURNACE_COMPLETE ', guards), ('FURNACE_ORIGINS ', origins), ('FURNACE_RECIPES_BEFORE ', recipes_before), ('FURNACE_RECIPES_AFTER ', recipes_after)):
            if line.startswith(prefix):
                out.append(json.loads(line[len(prefix):], object_pairs_hook=unique))
                break
        else:
            if line:
                preamble.append(line)
    C.ensure(preamble == ['16 achievements', '151 recipes'], 'actual original registry preamble')
    C.ensure(guards == [{'rows':len(cases), 'network':0, 'native':0, 'execs':0, 'writes':0}], 'complete guarded original process')
    C.ensure(origins == [{'jar_sha256':JAR, 'classes':{name:value['class_sha256'] for name,value in pins['classes'].items()}}], 'loaded original resources and source archive')
    C.ensure(len(recipes_before) == len(recipes_after) == 1 and len(recipes_before[0]) == len(recipes_after[0]) == 10, 'actual full original recipe catalog snapshots')
    C.ensure([row['profile'] for row in rows] == [case['profile'] for case in cases] and [row['index'] for row in rows] == list(range(len(cases))), 'all predeclared cases in order')
    for row in rows:
        row['nbt'] = decode_wire(row.pop('nbt_base64'))
        row['input_nbt'] = decode_wire(row.pop('input_nbt_base64'))
    return {'rows':rows, 'recipes_before':recipes_before[0], 'recipes_after':recipes_after[0]}


def expected_warnings(classes):
    uri = classes.as_uri().replace('file:///', 'file:/') + '/'
    return [
        'WARNING: A terminally deprecated method in java.lang.System has been called',
        'WARNING: System::setSecurityManager has been called by FurnaceObservation (' + uri + ')',
        'WARNING: Please consider reporting this to the maintainers of FurnaceObservation',
        'WARNING: System::setSecurityManager will be removed in a future release',
    ]


def read_trace(directory, cases, pins):
    rows = [json.loads(line, object_pairs_hook=unique) for line in
            (directory / 'calls.ndjson').read_text(encoding='utf8').splitlines()]
    C.ensure([row['index'] for row in rows] == list(range(len(cases))), 'all ordered actual JDI boundaries')
    code = json.loads((directory / 'live-Code.json').read_text(encoding='utf8'), object_pairs_hook=unique)
    archive = {owner + '.' + method['name'] + method['descriptor']: method
               for owner, value in pins['classes'].items() for method in value['methods']}
    for name, value in code.items():
        C.ensure(name in archive and value == {key: archive[name][key] for key in
                 ('code_bytes', 'code_sha256')}, 'actual entered live Code equals original archive')
    seen = set()
    for row in rows:
        C.ensure(set(row) == {'index', 'entries'} and len(row['entries']) <= 512,
                 'closed bounded actual method-entry journal')
        for entry in row['entries']:
            C.ensure(set(entry) == {'method', 'receiver', 'arguments', 'source_parent'} and
                     entry['method'] in code and len(entry['source_parent']) <= 1,
                     'actual method entry and nearest selected source parent')
            seen.add(entry['method'])
            for parent in entry['source_parent']:
                C.ensure(set(parent) == {'method', 'pc'} and parent['method'] in archive and
                         type(parent['pc']) is int and 0 <= parent['pc'] < archive[parent['method']]['code_bytes'],
                         'actual source caller Code index')
    C.ensure(seen == set(code), 'all published live Code pins were entered')
    return rows, code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    jar = args.jar.resolve(strict=True)
    C.ensure(C.digest(jar) == JAR, 'untouched original JAR')
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    C.ensure(all(C.digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())),
             'pinned supplied libraries')
    java, javac = [Path(shutil.which(name)).resolve(strict=True) for name in ('java', 'javac')]
    C.ensure(java.parent == javac.parent and '17.' in java.parent.parent.name, 'same JDK17')
    jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
    tools = [java, javac, java.parent.parent / 'lib/modules', jvm]
    helpers = [HERE / name for name in ('FurnaceObservation.java', 'FurnaceJdi.java', 'cases.json', 'capture.py', 'declare.py')]
    helpers += [SUPPORT / 'capture.py', SUPPORT / 'classfile.py', ROOT / 'tests/tools/world/test_source_saving.py']
    C.ensure(all(b'\r' not in path.read_bytes() for path in helpers if path.parent == HERE),
             'authored execution helpers use LF')
    inputs = [jar, *libraries, *tools, *helpers]
    before = {str(path): C.digest(path) for path in inputs}
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    cases = json.loads((HERE / 'cases.json').read_text(encoding='utf-8'), object_pairs_hook=unique)['cases']
    declaration = output / 'declared-inputs.tsv'
    declaration.write_text(declarations(cases), encoding='utf-8', newline='\n')
    before[str(declaration)] = C.digest(declaration)
    environment = os.environ.copy()
    for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(key, None)
    for name in ('APPDATA', 'TMP', 'TEMP'):
        directory = output / name.lower()
        directory.mkdir()
        environment[name] = str(directory)
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before}
    try:
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        report['compile'] = C.capture([str(javac), '-encoding', 'UTF-8', '-classpath', cp,
            '-d', str(classes), str(HERE / 'FurnaceObservation.java'), str(HERE / 'FurnaceJdi.java')], output, 'compile', 60, environment)
        C.require_success(report['compile'], empty_stderr=True)
        compiled = sorted(classes.rglob('*.class'))
        C.ensure(compiled and all(path.name.startswith(('FurnaceObservation', 'FurnaceJdi')) for path in compiled),
                 'helper output contains no original-class replacements')
        report['compiled_before'] = {str(path.relative_to(output)): C.digest(path) for path in compiled}
        with zipfile.ZipFile(jar) as archive:
            report['pins'] = {'client_jar_sha256': JAR, 'classes': {
                name: I.inspect_class(archive.read(name + '.class')) for name in CORE}}
        report['java_version'] = C.capture([str(java), '-version'], output, 'java-version', 20, environment)
        C.require_success(report['java_version'])
        report['javac_version'] = C.capture([str(javac), '-version'], output, 'javac-version', 20, environment)
        C.require_success(report['javac_version'], empty_stderr=True)
        for stem, mode in (('original', ['-Xint']), ('original-repeat', [])):
            report[stem] = C.capture([str(java), *mode, '-Xverify:all', '-Xmx256m',
                '-Djava.awt.headless=true', '-Djava.security.manager=allow', '-classpath', cp,
                'FurnaceObservation', str(declaration)], output, stem, 60, environment)
            C.require_success(report[stem])
            C.ensure((output / (stem + '.stderr')).read_text(encoding='utf-8').splitlines() ==
                     expected_warnings(classes), 'only declared JDK17 guard warnings')
            rows = read_rows((output / (stem + '.stdout')).read_bytes(), cases, report['pins'])
            if stem == 'original':
                report['observations'] = rows
            else:
                C.ensure(rows == report['observations'], 'independent interpreter/default-VM observations agree')
        report['traces'], report['live_Code'], report['trace_artifacts'] = {}, {}, {}
        for stem, mode in (('trace-interpreter', 'interpreter'), ('trace-default', 'default')):
            target = output / (stem + '-jdi')
            report[stem] = C.capture([str(java), '--add-modules=jdk.jdi', '-Xmx256m', '-classpath', cp,
                'FurnaceJdi', cp, str(java.parent.parent), str(target), str(declaration), mode],
                output, stem, 300, environment)
            C.require_success(report[stem], empty_stderr=True)
            C.ensure((target / 'target.stderr').read_text(encoding='utf8').splitlines() ==
                     expected_warnings(classes), 'only declared debugger-target warnings')
            C.ensure(read_rows((target / 'target.stdout').read_bytes(), cases, report['pins']) ==
                     report['observations'], 'traced and ordinary original outcomes agree')
            trace, code = read_trace(target, cases, report['pins'])
            report['traces'][stem], report['live_Code'][stem] = trace, code
            report['trace_artifacts'][stem] = {name: {'bytes': (target / name).stat().st_size,
                'sha256': C.digest(target / name)} for name in ('target.stdout', 'target.stderr', 'calls.ndjson', 'live-Code.json')}
            expected = f'FURNACE_TRACE_COMPLETE rows={len(cases)} entries={sum(len(row["entries"]) for row in trace)} live-code={len(code)}'
            C.ensure((output / (stem + '.stdout')).read_text(encoding='utf8').splitlines() == [expected],
                     'complete debugger transport/journal summary')
        C.ensure(report['traces']['trace-interpreter'] == report['traces']['trace-default'] and
                 report['live_Code']['trace-interpreter'] == report['live_Code']['trace-default'],
                 'both fresh passive original trace modes agree literally')
        report['status'] = 'PASS'
    finally:
        report['inputs_after'] = {name: C.digest(name) for name in before}
        report['compiled_after'] = {str(path.relative_to(output)): C.digest(path)
                                    for path in sorted(classes.rglob('*.class'))}
        if report['inputs_after'] != before or report.get('compiled_before') != report['compiled_after']:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Furnace capture artifacts: ' + str(output))
    C.ensure(report['status'] == 'PASS', 'immutable complete original capture')
    print('PASS: ' + str(len(report['observations']['rows'])) + ' original SK/BM cases')


if __name__ == '__main__':
    main()
