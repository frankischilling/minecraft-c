"""Reproduce the unchanged-original Bed observations in two VMs and read-only JDI."""
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
from validate import (JAR, RECEIPT, SCOPE, SOURCES, compare, digest, journal_text, load,
                      original_text, packet, profiles, require)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


_capture = module('bed_interaction_bounded_capture', HERE.parents[1] / 'client/controller/capture.py')
_classfile = module('bed_interaction_classfile', HERE.parents[1] / 'entity/player/classfile.py')
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
HELPERS = ('BedInteractionJdi.class', 'BedInteractionObservation$1.class', 'BedInteractionObservation.class')


def inventory(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in root.rglob('*') if p.is_file()}


def verify_pins(jar):
    require(digest(jar) == JAR, 'Bed unchanged original archive')
    pins = load(HERE / 'source-pins.json')
    require(type(pins) is dict and len(pins) == 24, 'Bed original class pin extent')
    with zipfile.ZipFile(jar) as archive:
        require(not any(name.startswith('BedInteraction') for name in archive.namelist()), 'Bed helper/archive collision')
        for name, expected in pins.items():
            compare(_classfile.parse_class(archive.read(name.replace('.', '/') + '.class')), expected,
                    'Bed original class/Code pin ' + name)
    return pins


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
    jar = args.client_jar.resolve(strict=True); verify_pins(jar); profiles()
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    require(all(digest(path) == pin for path, pin in zip(libraries, LIBRARIES.values())), 'Bed exact SDK inputs')
    inputs = [java, javac, jar, *libraries, *[HERE / name for name in SOURCES]]
    before = {str(path.resolve()): digest(path) for path in inputs}
    report = {'status': 'FAIL', 'scope': SCOPE, 'input_before_sha256': before}
    classes = output / 'classes'; classes.mkdir()
    environment = os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
        environment.pop(name, None)
    helper = None
    try:
        for label, tool in (('java', java), ('javac', javac)):
            receipt = _capture.capture([str(tool), '-version'], output, label, 15, environment)
            _capture.require_success(receipt)
            text = ''.join((output / receipt[key]).read_text() for key in ('stdout_file', 'stderr_file'))
            require(re.search(r'(?:version "|javac )17\.', text), 'Bed JDK17 profile')
            report[label] = receipt
        classpath = os.pathsep.join(str(path) for path in (classes, jar, *libraries))
        receipt = _capture.capture([str(javac), '-encoding', 'UTF-8', '--add-modules', 'jdk.jdi',
            '-classpath', classpath, '-d', str(classes), str(HERE / 'BedInteractionObservation.java'),
            str(HERE / 'BedInteractionJdi.java')], output, 'compile', 30, environment)
        _capture.require_success(receipt); report['compile'] = receipt
        helper = inventory(classes); require(set(helper) == set(HELPERS), 'Bed generated helper inventory')
        report['helper_before_sha256'] = helper
        command = [str(java), '-Xint', '-Xverify:all', '-Xmx512m', '-Djava.awt.headless=true',
                   '-Djava.security.manager=allow', '--add-opens=java.base/java.util=ALL-UNNAMED',
                   '-classpath', classpath, 'BedInteractionObservation', str(jar)]
        observed, originals = [], []
        for index in range(2):
            receipt = _capture.capture(command, output, 'original-' + str(index), 180, environment)
            _capture.require_success(receipt)
            observed.append(original_text((output / receipt['stdout_file']).read_text(encoding='utf-8')))
            originals.append({key: receipt[key] for key in RECEIPT})
            report['original'] = originals
            print('Original Bed interaction VM ' + str(index) + ':85 rows', flush=True)
        compare(observed[0], observed[1], 'Bed independent original VM repeat')
        debug = output / 'jdi'
        receipt = _capture.capture([str(java), '--add-modules', 'jdk.jdi', '-classpath', classpath,
            'BedInteractionJdi', str(java.parent.parent), classpath, str(jar), str(debug)],
            output, 'jdi-controller', 240, environment)
        _capture.require_success(receipt); report['jdi'] = receipt
        target = original_text((debug / 'target.stdout').read_text(encoding='utf-8'))
        compare(target, observed[0], 'Bed read-only debugger selected-state agreement')
        calls, code = journal_text((debug / 'calls.jsonl').read_text(encoding='utf-8'), observed[0])
        finite = packet({'rows': observed[0], 'journals': calls, 'code': code})
        compare(finite, packet(load(HERE / 'finite.json')), 'Bed reviewed original observation continuity')
        finite_path = output / 'finite.json'
        finite_path.write_text(json.dumps(finite, indent=2) + '\n', encoding='utf-8', newline='\n')
        provenance = {'schema': 'BED_INTERACTION_CAPTURE_V1', 'status': 'PASS', 'archive_sha256': JAR,
            'finite_sha256': digest(finite_path), 'source_sha256': {name: digest(HERE / name) for name in SOURCES},
            'tool_sha256': {'java.exe': digest(java), 'javac.exe': digest(javac)},
            'library_sha256': LIBRARIES, 'helper_sha256': helper, 'original': originals,
            'jdi': {key: receipt[key] for key in RECEIPT}, 'jdi_target': inventory(debug),
            'profiles_per_vm': 85, 'query_count': 408, 'code_methods': 20, 'game_code_methods': 19,
            'class_count': 24, 'input_unchanged': True, 'helper_unchanged': True,
            'reviewed_observations_equal': True, 'scope': SCOPE}
        (output / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8', newline='\n')
        report['status'] = 'PASS'
    finally:
        report['input_after_sha256'] = {str(path.resolve()): digest(path) for path in inputs}
        report['helper_after_sha256'] = inventory(classes)
        unchanged = report['input_after_sha256'] == before and (helper is None or report['helper_after_sha256'] == helper)
        if not unchanged:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(unchanged, 'Bed producer inputs/helpers unchanged')
    print('Original Bed interaction: two VMs and read-only JDI; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
