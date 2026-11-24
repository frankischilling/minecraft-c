"""Capture unchanged original chat components in two bounded JDK17 VMs."""
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
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    sys.modules[name] = value
    spec.loader.exec_module(value)
    return value


P = module('chat_capture_packet', HERE / 'packet.py')
C = module('chat_capture_process', HERE.parent / 'controller/capture.py')
F = module('chat_capture_classfile', HERE.parent / 'controller/classfile.py')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path, required=True)
    parser.add_argument('--library-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java', default=shutil.which('java'))
    parser.add_argument('--javac', default=shutil.which('javac'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    jar = args.client_jar.resolve(strict=True)
    java, javac = [Path(shutil.which(name) or '').resolve(strict=True) for name in (args.java, args.javac)]
    P.require(java.is_file() and javac.is_file() and java.parent.samefile(javac.parent), 'matching JDK tools')
    libraries = [(args.library_root / name).resolve(strict=True) for name in LIBRARIES]
    P.require(P.digest(jar) == P.JAR and all(P.digest(path) == pin
              for path, pin in zip(libraries, LIBRARIES.values())), 'unchanged original JAR/dependencies')
    pins = P.read(HERE / 'source-pins.json')
    P.require(pins['client_jar_sha256'] == P.JAR, 'chat original archive identity')
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins['classes'].items():
            actual = F.inspect_class(archive.read(name + '.class'))
            selected = {(m['name'], m['descriptor']) for m in expected['methods']}
            actual['methods'] = [m for m in actual['methods'] if (m['name'], m['descriptor']) in selected]
            P.require(actual == expected, 'chat original class/method pin: ' + name)
    classes = output / 'classes'
    classes.mkdir()
    for relative in ('font.txt', 'lang/en_US.lang', 'lang/stats_US.lang'):
        path = classes / relative
        path.parent.mkdir(exist_ok=True)
        path.write_bytes((HERE / 'inputs' / relative).read_bytes())
    source_before = P.source_digests()
    inputs = [jar, java, javac, *libraries, *[p for p in classes.rglob('*') if p.is_file()]]
    before = {str(p): P.digest(p) for p in inputs}
    report = {'status': 'FAIL', 'scope': P.SCOPE, 'sources_before': source_before, 'inputs_before': before}
    helper_before = {}
    try:
        environment = os.environ.copy()
        for key in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS'):
            environment.pop(key, None)
        versions = []
        for tool, name in ((java, 'java'), (javac, 'javac')):
            receipt = C.capture([str(tool), '-version'], output, name, 15, environment)
            C.require_success(receipt)
            versions.append(((output / receipt['stdout_file']).read_bytes() +
                             (output / receipt['stderr_file']).read_bytes()).decode())
        runtime, compiler = re.search(r'version "(17\.[0-9]+\.[0-9]+)', versions[0]), re.search(r'javac (17\.[0-9]+\.[0-9]+)', versions[1])
        P.require(runtime and compiler and runtime[1] == compiler[1], 'matching JDK17 runtime/compiler')
        report['jdk'] = versions
        classpath = os.pathsep.join(str(p) for p in (classes, jar, *libraries))
        compiled = C.capture([str(javac), '-encoding', 'UTF-8', '-classpath', classpath,
                              '-d', str(classes), str(HERE / 'ChatObservation.java')], output, 'compile', 30, environment)
        C.require_success(compiled)
        helper_before = {p.relative_to(classes).as_posix(): P.digest(p) for p in classes.rglob('*.class')}
        P.closed(helper_before, ('ChatObservation.class', 'ChatObservation$1.class'), 'compiled helper extent')
        observed, receipts = [], []
        for index in range(2):
            work = output / f'work-{index}'
            work.mkdir()
            receipt = C.capture([str(java), '-Xint', '-Xverify:all', '-Dfile.encoding=UTF-8',
                                  '-Dline.separator=\n', '--add-opens=java.base/java.util=ALL-UNNAMED',
                                  '-cp', classpath, 'ChatObservation', str(jar)], output,
                                 f'original-{index}', 60, environment, work)
            receipts.append(receipt)
            C.require_success(receipt)
            lines = (output / receipt['stdout_file']).read_text().splitlines()
            P.require(lines and lines[-1] == P.COMPLETION, 'original chat completion')
            observed.append(P.validate_rows(lines[:-1]))
        P.require(observed[0] == observed[1], 'two original component VMs agree exactly')
        finite = {'format': 1, 'client_jar_sha256': P.JAR, 'scope': P.SCOPE, 'rows': observed[0]}
        (output / 'finite.json').write_text(json.dumps(finite, indent=2) + '\n', encoding='utf-8', newline='\n')
        report.update(status='PASS', rows=81, receipts=receipts, helper_before=helper_before)
        proof = {'schema': 'GUI_INGAME_CHAT_CAPTURE_V1', 'client_jar_sha256': P.JAR,
                 'scope': P.SCOPE, 'rows': 81, 'finite_sha256': P.digest(output / 'finite.json'),
                 'source_sha256': source_before, 'helper_sha256': helper_before,
                 'tools': {'java': P.digest(java), 'javac': P.digest(javac)},
                 'vms': [{key: receipt[key] for key in ('exit', 'timed_out', 'output_overflow',
                     'pipe_capture_complete', 'stdout_bytes', 'stdout_sha256', 'stderr_bytes',
                     'stderr_sha256')} for receipt in receipts],
                 'input_unchanged': True, 'helper_unchanged': True}
        (output / 'provenance.json').write_text(json.dumps(proof, indent=2) + '\n', encoding='utf-8', newline='\n')
    finally:
        report['inputs_after'] = {str(p): P.digest(p) for p in inputs}
        report['sources_after'] = P.source_digests()
        report['helper_after'] = {p.relative_to(classes).as_posix(): P.digest(p) for p in classes.rglob('*.class')}
        unchanged = (report['inputs_after'] == before and report['sources_after'] == source_before and
                     report['helper_after'] == helper_before)
        if not unchanged:
            report['status'] = 'FAIL'
        (output / 'capture.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
        P.require(unchanged, 'original chat inputs/helpers changed')
    P.require(report['status'] == 'PASS', 'original chat capture completed')
    print('Original chat components: two VMs, 81 rows; ' + str(output / 'capture.json'))


if __name__ == '__main__':
    main()
