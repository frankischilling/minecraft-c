"""Supplied-input Windows two-fresh-VM Dispenser reproducer; no download or retry."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = None
SUPPORT = None
LIBRARIES = {
    'org/lwjgl/lwjgl/lwjgl/2.9.0/lwjgl-2.9.0.jar': 'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'org/lwjgl/lwjgl/lwjgl_util/2.9.0/lwjgl_util-2.9.0.jar': '474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar': '79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar': 'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5',
}
PRODUCTS = ('DispenserContainerObservation$Guard.class', 'DispenserContainerObservation.class')


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


A = load('dispenser_packet_audit', HERE / 'audit.py')
R = A.M
C = I = None


def utc():
    return datetime.now(timezone.utc).isoformat()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf8', newline='\n')


def check_freeze():
    A.audit()
    return A.load(HERE / 'packet.json')


def repository(explicit=None):
    candidates = [Path(explicit)] if explicit is not None else list(HERE.parents)
    for path in candidates:
        path = path.resolve(strict=True)
        if all((path / name).is_file() for name in check_freeze()['shared_inputs']):
            return path
    raise ValueError('Supply --repo-root with the declared tracked shared helpers')


def environment_record(environment):
    return {name: {'present': name in environment, 'empty': not bool(environment.get(name))}
            for name in A.OPTIONS}


def guard_environment(environment):
    value = environment_record(environment)
    R.need(all(item['empty'] for item in value.values()),
           'Implicit JDK options must be empty: ' + ','.join(name for name, item in value.items() if not item['empty']))
    return value


def guarded_spawn(factory, environment, handles, args, kwargs):
    R.need(kwargs.get('env') is environment, 'Exact copied environment reaches actual Popen boundary')
    guard_environment(kwargs['env'])
    process = factory(*args, **kwargs)
    handles.append(process)
    return process


def child_transport(module, command, output, stem, timeout, environment, handles, factory=None):
    """The same actual environment/guarded Popen composition used by main."""
    actual = subprocess.Popen if factory is None else factory
    def spawn(*args, **kwargs):
        return guarded_spawn(actual, environment, handles, args, kwargs)
    module.subprocess = types.SimpleNamespace(Popen=spawn, DEVNULL=subprocess.DEVNULL,
        PIPE=subprocess.PIPE, TimeoutExpired=subprocess.TimeoutExpired)
    return module.capture(command, output, stem, timeout, environment, str(output))



def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jar', type=Path, required=True)
    parser.add_argument('--libraries', type=Path, required=True)
    parser.add_argument('--jdk', type=Path, required=True)
    parser.add_argument('--repo-root', type=Path)
    parser.add_argument('--output-root', type=Path)
    args = parser.parse_args()
    global ROOT, SUPPORT, C, I
    freeze = check_freeze()
    ROOT = repository(args.repo_root)
    SUPPORT = ROOT / 'tests/reference/client/controller'
    for name, expected in freeze['shared_inputs'].items():
        R.equal(R.pin(ROOT / name), expected, 'Declared shared helper bytes')
    C = load('dispenser_bounded_capture', SUPPORT / 'capture.py')
    I = load('dispenser_classfile', SUPPORT / 'classfile.py')
    if args.output_root is None:
        args.output_root = ROOT / '.local/dispenser-original-captures'
    pins = R.parse(R.bounded(HERE / 'source-pins.json'))
    jar = args.jar.resolve(strict=True)
    jdk = args.jdk.resolve(strict=True)
    # Execution stays scoped to the measured Windows JDK17 profile.
    R.need(os.name == 'nt', 'Selected Windows controller; no path/platform substitution')
    java, javac = jdk / 'bin/java.exe', jdk / 'bin/javac.exe'
    libraries = [(args.libraries / name).resolve(strict=True) for name in LIBRARIES]
    tools = [java, javac, jdk / 'bin/server/jvm.dll', jdk / 'lib/modules', jdk / 'release']
    R.equal(R.pin(jar), pins['jar'], 'Untouched original archive')
    for path, digest in zip(libraries, LIBRARIES.values()):
        R.equal(R.pin(path)['sha256'], digest, 'Actual supplied SDK archive')
    R.need('JAVA_VERSION="17.0.17"' in (jdk / 'release').read_text(encoding='utf8'), 'Exact actual JDK17.0.17')
    with zipfile.ZipFile(jar) as archive:
        for name, expected in pins['classes'].items():
            R.equal(I.inspect_class(archive.read(name + '.class')), expected, 'Actual Class/Code pins ' + name)
    inputs = [HERE / name for name in A.FILES]
    inputs += [(ROOT / name).resolve(strict=True) for name in freeze['shared_inputs']]
    inputs += [jar, *libraries, *tools]
    R.need(len(inputs) == len(set(inputs)), 'Distinct actual compile/observer input inventory')
    args.output_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='component-', dir=args.output_root)).resolve(strict=True)
    classes = work / 'classes'
    classes.mkdir()
    environment = os.environ.copy()
    for name in ('APPDATA', 'TMP', 'TEMP'):
        directory = work / name.lower()
        directory.mkdir()
        environment[name] = str(directory)
    before = {str(path): R.pin(path) for path in inputs}
    report = {'schema': 'dispenser_original_capture_v2', 'status': 'FAIL', 'scope': R.SCOPE, 'work': str(work),
        'started_utc': utc(), 'finished_utc': None, 'inputs_before': before, 'inputs_after': None,
        'environment_options': environment_record(environment), 'attempts': [], 'products_before_targets': None, 'products_after': None, 'rows': None,
        'target_schedule': ['interpreter', 'default'], 'hidden_retries': 0,
        'native_comparison': 'NOT_RUN_NO_NATIVE_CREDIT', 'environment_overrides': {
            name: environment[name] for name in ('APPDATA', 'TMP', 'TEMP')}}

    def products():
        return {str(path.relative_to(classes)): R.pin(path) for path in sorted(classes.rglob('*')) if path.is_file()}

    def attempt(command, stem, timeout):
        item = {'name': stem, 'command': command, 'started_utc': utc(), 'pid': None,
            'process_receipt': None, 'inputs_before': {str(path): R.pin(path) for path in inputs},
            'inputs_after': None, 'products_before': products(), 'products_after': None,
            'owned_process_reaped': False, 'owned_process_returncode': None,
            'cleanup_kill_attempted': False, 'cleanup_errors': [], 'environment_options': environment_record(environment)}
        report['attempts'].append(item)
        R.equal(item['inputs_before'], before, 'Inputs before ' + stem)
        handles = []
        result = None
        try:
            result = child_transport(C, command, work, stem, timeout, environment, handles)
            item['process_receipt'] = result
        finally:
            # Interrupts/transport errors retain the attempted command. Only the
            # actual freshly created handle is killed/reaped; no PID-only lookup.
            for process in handles:
                item['pid'] = process.pid
                try:
                    if process.poll() is None:
                        item['cleanup_kill_attempted'] = True
                        process.kill()
                    item['owned_process_returncode'] = process.wait(timeout=5)
                    item['owned_process_reaped'] = True
                except BaseException as error:
                    item['cleanup_errors'].append({'stage': 'owned-process', 'class': type(error).__name__, 'message': str(error)})
                if result is None:
                    for stream in (process.stdout, process.stderr):
                        try:
                            stream.close()
                        except BaseException as error:
                            item['cleanup_errors'].append({'stage': 'interrupted-pipe-close', 'class': type(error).__name__, 'message': str(error)})
            item['inputs_after'] = {str(path): R.pin(path) for path in inputs}
            item['products_after'] = products()
            item['finished_utc'] = utc()
            write(work / 'run.json', report)
        C.require_success(result)
        R.need(len(handles) == 1 and type(item['pid']) is int and item['pid'] > 0 and
               item['owned_process_reaped'] and item['owned_process_returncode'] == 0 and
               not item['cleanup_kill_attempted'] and not item['cleanup_errors'], 'Natural owned child closure')
        R.equal(item['inputs_after'], before, 'Inputs after ' + stem)
        return result

    try:
        guard_environment(environment)
        cp = os.pathsep.join(map(str, [classes, jar, *libraries]))
        compile_receipt = attempt([str(javac), '-g:none', '-encoding', 'UTF-8', '-classpath', cp,
            '-d', str(classes), str(HERE / 'DispenserContainerObservation.java')], 'compile', 60)
        C.require_success(compile_receipt, empty_stderr=True)
        R.need(compile_receipt['stdout_bytes'] == 0 and tuple(sorted(products())) == PRODUCTS,
               'Only two authored observer products; no original replacement outputs')
        report['products_before_targets'] = products()
        for stem, mode in (('interpreter', ['-Xint']), ('default', [])):
            attempt([str(java), *mode, '-Xverify:all', '-Xmx256m', '-Djava.awt.headless=true',
                '-Djava.security.manager=allow', '-classpath', cp, 'DispenserContainerObservation',
                str(HERE / 'cases.tsv')], stem, 60)
            R.equal(products(), report['products_before_targets'], 'Products stable after ' + stem)
            R.equal(R.bounded(work / (stem + '.stderr')).decode('utf8').splitlines(),
                    R.warnings(classes), 'Only actual declared JDK warnings')
            rows = R.read_rows(R.bounded(work / (stem + '.stdout')), R.bounded(HERE / 'cases.tsv'), pins, jar)
            if report['rows'] is None:
                report['rows'] = rows
            else:
                # Exact actual full JSON comparison: no exception message, line,
                # class/alias or field normalization between fresh VM modes.
                R.equal(rows, report['rows'], 'Interpreter/default complete observations')
        R.equal(report['rows'], A.load(HERE / 'finite.json')['original_rows'], 'Fresh public observations equal historical scalar reference; no normalization')
        report['status'] = 'PASS_ORIGINAL_COMPONENT_ONLY'
    except BaseException as error:
        report['error'] = {'class': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['finished_utc'] = utc()
        try:
            report['inputs_after'] = {str(path): R.pin(path) for path in inputs}
            report['products_after'] = products()
            R.equal(report['inputs_after'], before, 'Final actual input closure')
            if report['products_before_targets'] is not None:
                R.equal(report['products_after'], report['products_before_targets'], 'Final compiled product closure')
        except BaseException as error:
            report['status'] = 'FAIL'
            report['final_error'] = {'class': type(error).__name__, 'message': str(error)}
            raise
        finally:
            write(work / 'run.json', report)


if __name__ == '__main__':
    main()
