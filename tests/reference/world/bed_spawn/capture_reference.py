"""Capture unchanged Bed helper/wrapper calls and read-only original query journals."""
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
sys.path.insert(0, str(HERE))
from validate import digest, journals, load, profiles, require, rows
_support = importlib.util.spec_from_file_location('bed_bounded_capture', HERE.parents[1] / 'client/controller/capture.py')
_capture = importlib.util.module_from_spec(_support); _support.loader.exec_module(_capture)
_class = importlib.util.spec_from_file_location('bed_classfile', HERE.parents[1] / 'entity/player/classfile.py')
_classfile = importlib.util.module_from_spec(_class); _class.loader.exec_module(_classfile)
JAR = 'af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
SOURCES = ('BedSpawnObservation.java', 'BedSpawnJdi.java', 'capture_reference.py', 'validate.py',
           'profiles.json', 'profiles.tsv', 'source-pins.json', '../../client/controller/capture.py',
           '../../entity/player/classfile.py')
RECEIPT = ('exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete', 'reader_errors',
           'output_overflow', 'stdout_file', 'stderr_file', 'stdout_bytes', 'stderr_bytes',
           'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit')


def verify_pins(jar):
    pins = load(HERE / 'source-pins.json')
    require(set(pins) == {'schema', 'archive_sha256', 'classes'} and
            pins['schema'] == 'BED_SPAWN_CLASS_CODE_V1' and pins['archive_sha256'] == JAR and
            digest(jar) == JAR and len(pins['classes']) == 12, 'Bed original archive/pins')
    with zipfile.ZipFile(jar) as archive:
        require(not any(name.startswith('BedSpawn') for name in archive.namelist()), 'Bed helper/archive class collision')
        for name, expected in pins['classes'].items():
            actual = _classfile.parse_class(archive.read(name + '.class')); actual.pop('fields')
            selected = {(entry['name'], entry['descriptor']) for entry in expected['methods']}
            actual['methods'] = [entry for entry in actual['methods'] if (entry['name'], entry['descriptor']) in selected]
            require(actual == expected, 'Bed original class/Code pin: ' + name)
    return pins


def inventory(root):
    return {path.relative_to(root).as_posix(): digest(path) for path in root.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    java, javac = [Path(shutil.which(value) or '').resolve(strict=True) for value in (args.java, args.javac)]
    require(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), 'Bed matching Java tools')
    jar = args.client_jar.resolve(strict=True); pins = verify_pins(jar); profiles()
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    require(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'Bed exact SDK inputs')
    inputs = [java, javac, jar, *libraries, *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    classes = output / 'classes'; classes.mkdir()
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    report = {'status': 'FAIL', 'input_before_sha256': before}
    helper = None
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = _capture.capture([str(tool), '-version'], output, label, 10, environment)
            _capture.require_success(receipt)
            text = ''.join((output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            require(re.search(r'(?:version "|javac )17\.', text), 'Bed JDK17 profile')
            report[label] = receipt
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = _capture.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi',
            '-classpath', classpath, '-d', str(classes), str(HERE / 'BedSpawnObservation.java'),
            str(HERE / 'BedSpawnJdi.java')], output, 'compile', 30, environment)
        _capture.require_success(receipt, empty_stderr=True); report['compile'] = receipt
        helper = inventory(classes)
        require(set(helper) == {'BedSpawnObservation.class', 'BedSpawnObservation$Guard.class', 'BedSpawnJdi.class'}, 'Bed helper inventory')
        report['helper_before_sha256'] = helper
        names = output / 'class-names.txt'; names.write_text('\n'.join(pins['classes']) + '\n', encoding='ascii')
        command = [str(java), '-Xint', '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                   '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                   '-classpath', classpath, 'BedSpawnObservation']
        observed = []
        originals = []
        for index in range(2):
            work = output / ('original-work-' + str(index))
            receipt = _capture.capture([*command, str(jar), str(work), str(HERE / 'profiles.tsv'),
                str(names), str(work / 'bindings.tsv')], output, 'original-' + str(index), 90, environment)
            _capture.require_success(receipt)
            text = (output / receipt['stdout_file']).read_text(encoding='utf-8')
            require(text.splitlines()[-1] == 'Original Bed spawn: 30 profiles; network=0; native=0; rejected_writes=0', 'Bed original completion')
            observed.append(rows(text)); originals.append({key: receipt[key] for key in RECEIPT})
            expected = [name + '\t' + str(value['class_bytes']) + '\t' + value['class_sha256']
                        for name, value in pins['classes'].items()]
            require((work / 'bindings.tsv').read_text(encoding='utf-8').splitlines() == expected, 'Bed actual class origins/bytes')
        require(observed[0] == observed[1], 'Bed independent original VM repeat')
        work = output / 'jdi-work'; journal_output = output / 'jdi'
        receipt = _capture.capture([str(java), '--add-modules', 'jdk.jdi', '-classpath', classpath,
            'BedSpawnJdi', str(jar), str(work), str(HERE / 'profiles.tsv'), str(names),
            str(work / 'bindings.tsv'), str(journal_output), str(java.parent.parent), classpath],
            output, 'jdi-controller', 180, environment)
        _capture.require_success(receipt); report['jdi'] = receipt
        jdi_rows = rows((journal_output / 'target.stdout').read_text(encoding='utf-8'))
        require(jdi_rows == observed[0], 'Bed read-only debugger selected-state equivalence')
        require((work / 'bindings.tsv').read_text(encoding='utf-8').splitlines() == expected,
                'Bed debugged target actual class origins/bytes')
        traces, codes = journals((journal_output / 'queries.jsonl').read_text(encoding='utf-8'))
        finite = {'schema': 'BED_SPAWN_FINITE_V1', 'rows': observed[0], 'journals': traces, 'code': codes}
        finite_path = output / 'finite.json'; finite_path.write_text(json.dumps(finite, indent=2) + '\n', encoding='utf-8')
        provenance = {'schema': 'BED_SPAWN_CAPTURE_V1', 'status': 'PASS', 'archive_sha256': JAR,
            'finite_sha256': digest(finite_path), 'source_sha256': {name: digest(HERE / name) for name in SOURCES},
            'tool_sha256': {'java.exe': digest(java), 'javac.exe': digest(javac)}, 'helper_sha256': helper,
            'original': originals, 'jdi': {key: receipt[key] for key in RECEIPT},
            'jdi_target': inventory(journal_output), 'profiles_per_vm': 30,
            'query_count': sum(len(row['journal']) for row in traces), 'code_methods': len(codes),
            'input_unchanged': True, 'helper_unchanged': True,
            'scope': 'Prepared genuine FD and normal Map-provider/Chunks; direct VE helper and GS wrapper. Read-only original JDI observes caller-level ordered queries. No loaded gameplay or Bed use/sleep/wake qualification.'}
        (output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
        report['status'] = 'PASS'; report['original'] = originals
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        unchanged = report['input_after_sha256'] == before and (helper is None or report['helper_after_sha256'] == helper)
        if not unchanged:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        require(unchanged, 'Bed producer inputs/helpers changed')
    print('Original Bed spawn: two ordinary VMs and read-only JDI; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
