"""Execute unchanged original Ladder neighbors and compare declared native rows."""
import argparse
import copy
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import traceback
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'metadata_set'))
import capture_reference as common

CODE = {
    ('dp', 'b', '(Lfd;IIII)V'): (139, 'd861b7c1ad99cb7816bd130ae59ce56aa6a0894439504e923dcf0ff25ac2f443'),
    ('fd', 'e', '(III)I'): (72, '8b3c32792768032d38f020f47f0407bcc922ffa18ba4df38f9729079c2430957'),
    ('fd', 'h', '(III)Z'): (45, '9159cea1434cb4ef1f90e98a19398660c319d0876cab388766cdd40b18e9dd11'),
}
PREFIX = 'LADDER_NEIGHBOR '
COMPLETE = 'Original Ladder neighbor: 20 rows; network=0; native=0; rejected_write=0; genuine_world_chunks=9'
KEYS = {'profile', 'metadata', 'missing', 'returned', 'exception', 'id_after',
        'metadata_after', 'dirty_after', 'world_random_unchanged'}


def profiles():
    return [(f'all-walls-{m}', m, -1) for m in range(16)] + [
        (f'missing-wall-{i + 2}', i + 2, i) for i in range(4)]


def rows(text):
    values = [json.loads(line[len(PREFIX):], object_pairs_hook=common.unique)
              for line in text.splitlines() if line.startswith(PREFIX)]
    common.ensure(len(values) == 20, 'twenty complete ordered Ladder rows')
    for row, operands in zip(values, profiles()):
        common.ensure(type(row) is dict and row.keys() == KEYS, 'exact Ladder row keys')
        common.ensure(common.exact([row[k] for k in ('profile', 'metadata', 'missing')], list(operands)),
                      'exact ordered Ladder operands')
        common.ensure(all(type(row[k]) is bool for k in ('returned', 'dirty_after', 'world_random_unchanged')),
                      'exact Ladder Boolean types')
        common.ensure(all(type(row[k]) is int for k in ('metadata', 'missing', 'id_after', 'metadata_after')),
                      'exact Ladder integer types')
        common.ensure(row['exception'] is None or type(row['exception']) is str, 'actual nullable Throwable type')
    return values


def controls(original):
    refused = []
    for name, key, value in [('scalar-type', 'returned', 1), ('operand', 'metadata', 17),
                             ('state', 'id_after', 0), ('missing-key', 'missing', None)]:
        altered = copy.deepcopy(original)
        if name == 'state':
            altered[2][key] = value
        elif name == 'missing-key':
            del altered[0][key]
        else:
            altered[0][key] = value
        try:
            parsed = rows('\n'.join(PREFIX + json.dumps(row) for row in altered))
            common.ensure(common.exact(parsed, original), 'unchanged typed comparison')
        except (AssertionError, ValueError, KeyError):
            refused.append(name)
    for name, altered in [('ordering', list(reversed(original))), ('missing-row', original[:-1])]:
        try:
            rows('\n'.join(PREFIX + json.dumps(row) for row in altered))
        except (AssertionError, ValueError):
            refused.append(name)
    common.ensure(len(refused) == 6, 'all six altered comparisons rejected')
    return refused


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--native-stdout', type=Path)
    parser.add_argument('--library-root', type=Path, default=(Path(os.environ['APPDATA']) / '.minecraft/libraries'
                        if 'APPDATA' in os.environ else Path.home() / '.minecraft/libraries'))
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ladder-neighbor-', dir=args.work_dir)).resolve()
    classes, target = output / 'classes', output / 'target'
    classes.mkdir(); target.mkdir()
    inputs, before, failure = [], None, None
    report = {'status': 'FAIL', 'native_comparison': None}
    try:
        jar = args.jar.resolve(strict=True)
        java = Path(shutil.which(args.java) or '').resolve(strict=True)
        javac = Path(shutil.which(args.javac) or '').resolve(strict=True)
        common.ensure(java.parent.samefile(javac.parent), 'matching JDK tools')
        common.ensure(common.digest(jar) == common.JAR, 'verified unchanged original archive')
        libraries = [(args.library_root / name).resolve(strict=True) for name in common.LIBRARIES]
        common.ensure(all(common.digest(path) == pin for path, pin in zip(libraries, common.LIBRARIES.values())),
                      'pinned original runtime dependencies')
        jvm = java.parent / 'server/jvm.dll' if os.name == 'nt' else java.parent.parent / 'lib/server/libjvm.so'
        inputs = [jar, java, javac, jvm, java.parent.parent / 'lib/modules', *libraries, Path(__file__),
                  HERE / 'LadderNeighborObservation.java', HERE / 'README.md', Path(common.__file__),
                  common.HERE / 'MetadataSetObservation.java', common.SUPPORT / 'capture.py',
                  common.SUPPORT / 'classfile.py']
        native = None
        if args.native_stdout is not None:
            native = args.native_stdout.resolve(strict=True)
            inputs.append(native)
        before = report['input_before_sha256'] = common.pins(inputs)
        source = {}
        with zipfile.ZipFile(jar) as archive:
            for name in sorted(set(common.CLASSES) | {'dp', 'uu'}):
                source[name] = common.inspect_class(archive.read(name + '.class'))
        for (name, method, descriptor), expected in CODE.items():
            selected = [row for row in source[name]['methods'] if row['name'] == method and row['descriptor'] == descriptor]
            common.ensure(len(selected) == 1 and
                          (selected[0].get('code_bytes'), selected[0].get('code_sha256')) == expected,
                          'unchanged Code pin: ' + name + '.' + method + descriptor)
        report['original_classes'] = source
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS', 'CLASSPATH'):
            environment.pop(key, None)
        for label, tool in (('java', java), ('javac', javac)):
            observed = common.capture([str(tool), '-version'], output, label, 10, environment)
            report[label] = observed
            common.require_success(observed)
            text = (output / observed['stdout_file']).read_text() + (output / observed['stderr_file']).read_text()
            common.ensure(re.search(r'(?:version "|javac )17\.', text), 'JDK17 required')
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        observed = common.capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath, '-d', str(classes),
                                   str(common.HERE / 'MetadataSetObservation.java'),
                                   str(HERE / 'LadderNeighborObservation.java')], output, 'compile', 30, environment)
        report['compile'] = observed
        common.require_success(observed, empty_stderr=True)
        helpers = report['helpers_before_sha256'] = common.inventory(classes)
        common.ensure(set(helpers) == {'MetadataSetObservation.class', 'MetadataSetObservation$1.class',
                      'MetadataSetObservation$Observer.class', 'LadderNeighborObservation.class',
                      'LadderNeighborObservation$1.class'}, 'authored helper classes only')
        common.ensure(common.pins(inputs) == before, 'immediate pre-execution inputs')
        observed = common.capture([str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                                   '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                                   '-classpath', classpath, 'LadderNeighborObservation', str(jar), str(target)],
                                  output, 'original', 60, environment, output)
        report['original'] = observed
        common.require_success(observed)
        text = (output / observed['stdout_file']).read_text(encoding='utf-8')
        common.ensure(text.splitlines()[-1] == COMPLETE, 'original completion and external-effect guards')
        original = rows(text)
        events = [json.loads(line[len('LADDER_NEIGHBOR_EVENTS '):], object_pairs_hook=common.unique)
                  for line in text.splitlines() if line.startswith('LADDER_NEIGHBOR_EVENTS ')]
        common.ensure(len(events) == 20 and [r['profile'] for r in events] == [r[0] for r in profiles()],
                      'complete excluded original notification observations')
        (output / 'rows.json').write_text(json.dumps(original, indent=2) + '\n', encoding='utf-8', newline='\n')
        (output / 'events.json').write_text(json.dumps(events, indent=2) + '\n', encoding='utf-8', newline='\n')
        report['comparator_refusals'] = controls(original)
        if native is not None:
            common.ensure(native.stat().st_size <= 4 * 1024 * 1024, 'bounded native stdout')
            native_bytes = native.read_bytes()
            text = native_bytes.decode('utf-8', errors='strict')
            common.ensure(any(re.fullmatch(r'PASS: [1-9][0-9]* Ladder checks \(world-ladder\)', line)
                              for line in text.splitlines()), 'complete native Ladder fixture')
            common.ensure(common.exact(rows(text), original), 'exact twenty-row typed original/native comparison')
            (output / 'native.stdout').write_bytes(native_bytes)
            report['native_comparison'] = {'status': 'PASS', 'rows': 20, 'input_sha256': common.digest(native)}
        report.update(status='PASS', rows=20, rows_sha256=common.digest(output / 'rows.json'))
    except Exception as error:
        failure = error
        report['error'] = f'{type(error).__name__}: {error}'
        (output / 'driver-error.txt').write_text(traceback.format_exc(), encoding='utf-8', newline='\n')
    finally:
        try:
            report['input_after_sha256'] = common.pins(inputs)
            report['helpers_after_sha256'] = common.inventory(classes)
            report['inputs_unchanged'] = before is not None and report['input_after_sha256'] == before
            report['helpers_unchanged'] = report.get('helpers_before_sha256') == report['helpers_after_sha256']
            common.ensure(report['inputs_unchanged'] and report['helpers_unchanged'], 'final immutable inputs/helpers')
        except Exception as error:
            report['status'] = 'FAIL'
            report['final_audit_error'] = f'{type(error).__name__}: {error}'
            failure = failure or error
        report['output_sha256'] = common.inventory(output)
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print('Original Ladder neighbor ' + report['status'] + ': ' + str(output / 'capture.json'))
    if failure is not None:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
