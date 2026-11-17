"""Capture fresh Sign component observations into a private output directory."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


P = module('sign_capture_packet', HERE / 'packet.py')
I = module('sign_capture_classes', HERE / 'inspect_classes.py')
T = module('sign_capture_transport', ROOT / 'tests/reference/client/controller/capture.py')


def file_pin(path):
    return P.pin(path.read_bytes())


def complete(receipt):
    P.need(receipt['exit'] == 0 and receipt['timed_out'] is False and
           receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {} and
           receipt['output_overflow'] is False, 'complete successful bounded child')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', required=True, type=Path)
    parser.add_argument('--jdk', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cohort', choices=('main', 'null-lines'), default='main')
    args = parser.parse_args()
    P.need(not any(os.environ.get(name) for name in ('JAVA_TOOL_OPTIONS', '_JAVA_OPTIONS', 'JDK_JAVA_OPTIONS')),
           'undeclared JVM option environment')
    jar, jdk = args.jar.resolve(strict=True), args.jdk.resolve(strict=True)
    suffix = '.exe' if os.name == 'nt' else ''
    java, javac = jdk / ('bin/java' + suffix), jdk / ('bin/javac' + suffix)
    for path in (java, javac, jdk / 'release'):
        P.need(path.is_file(), 'selected JDK input missing')
    P.need(P.digest(jar.read_bytes()) == P.JAR, 'unchanged original client archive')
    pins = P.parse((HERE / 'source-pins.json').read_bytes())
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins['classes'].items():
            P.need(I.inspect(archive.read(name + '.class')) == expected, 'original Class/Code identity: ' + name)
    for name, (length, sha) in P.FROZEN.items():
        P.need(file_pin(HERE / name) == {'bytes': length, 'sha256': sha}, 'frozen authored input: ' + name)
    output = args.output.resolve()
    P.need(output != ROOT and not output.is_relative_to(HERE), 'private output must be outside the public packet')
    output.mkdir(parents=True, exist_ok=False)
    classes = output / 'classes'
    classes.mkdir()
    sources = ['SignNbtObservation.java'] + ([] if args.cohort == 'main' else ['NullSignWriteObservation.java'])
    helper = 'SignNbtObservation' if args.cohort == 'main' else 'NullSignWriteObservation'
    inputs = [jar, java, javac, jdk / 'release', Path(__file__), HERE / 'packet.py', HERE / 'inspect_classes.py',
              HERE / 'source-pins.json', Path(T.__file__), *(HERE / name for name in sources)]
    before = {str(path): file_pin(path) for path in inputs}
    report = {'schema': 1, 'status': 'FAIL', 'cohort': args.cohort, 'inputs_before': before, 'processes': [],
              'scope': 'Fresh authored component capture only; this does not update or replace the retained public packet'}
    try:
        command = [str(javac), '--release', '17', *(['-encoding', 'UTF-8'] if args.cohort == 'main' else []),
                   '-cp', str(jar), '-d', str(classes), *(str(HERE / name) for name in sources)]
        compiler = T.capture(command, output, 'compile', 60, working_directory=ROOT)
        report['compiler'] = compiler
        complete(compiler)
        P.need((output / compiler['stdout_file']).read_bytes() == b'' and
               (output / compiler['stderr_file']).read_bytes() == b'', 'complete empty compiler streams')
        products = sorted(path for path in classes.rglob('*') if path.is_file())
        expected = {'SignNbtObservation.class', 'SignNbtObservation$1.class', 'SignNbtObservation$SourceCall.class'}
        if args.cohort != 'main':
            expected |= {'NullSignWriteObservation.class', 'NullSignWriteObservation$1.class'}
        P.need({path.name for path in products} == expected and all(path.parent == classes for path in products),
               'exact actual helper-only Class inventory')
        product_pins = {str(path): file_pin(path) for path in products}
        report['products'] = product_pins
        previous = None
        for ordinal, mode in enumerate(('interpreter', 'default', 'interpreter', 'default')):
            P.need({str(path): file_pin(path) for path in inputs} == before and
                   {str(path): file_pin(path) for path in products} == product_pins, 'all inputs/products before child')
            command = [str(java), '-Xmx128m', '-XX:-UsePerfData', *(['-Xint'] if mode == 'interpreter' else []),
                       '-cp', str(classes) + os.pathsep + str(jar), helper, str(jar)]
            receipt = T.capture(command, output, str(ordinal) + '-' + mode, 60, working_directory=ROOT)
            report['processes'].append({'mode': mode, **receipt})
            complete(receipt)
            stdout, stderr = ((output / receipt[name]).read_bytes() for name in ('stdout_file', 'stderr_file'))
            origin = classes.as_uri().replace('file:///', 'file:/') + '/'
            warnings = ['WARNING: A terminally deprecated method in java.lang.System has been called',
                        'WARNING: System::setSecurityManager has been called by ' + helper + ' (' + origin + ')',
                        'WARNING: Please consider reporting this to the maintainers of ' + helper,
                        'WARNING: System::setSecurityManager will be removed in a future release']
            P.need(stderr.decode('utf-8').splitlines() == warnings, 'exact declared JDK guard warning only')
            # Validate the complete selected observation. The retained packet's
            # Windows/JDK context remains explicit; other contexts retain a FAIL
            # receipt instead of being silently normalized into that cohort.
            report['observation'] = P.validate_rows(P.read_rows(stdout), args.cohort, pins)
            if previous is None:
                previous = (stdout, stderr)
            P.need(previous == (stdout, stderr), 'entire raw pipes equal across all fresh modes/replicates')
            P.need({str(path): file_pin(path) for path in inputs} == before and
                   {str(path): file_pin(path) for path in products} == product_pins, 'all inputs/products after child')
        report.update(status='PASS', complete_raw_outputs_equal=True,
                      products_after={str(path): file_pin(path) for path in products})
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
    finally:
        report['inputs_after'] = {str(path): file_pin(path) for path in inputs}
        if report['inputs_after'] != before:
            report['status'] = 'FAIL'
            report['continuity_error'] = True
        (output / 'run.json').write_bytes((json.dumps(report, indent=2) + '\n').encode())
    print(json.dumps({'status': report['status'], 'output': str(output), 'processes': len(report['processes']),
                      'error': report.get('error')}))
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
