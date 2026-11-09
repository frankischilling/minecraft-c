"""Retain genuine World-output to SDK checked cleanup; full Minecraft.k remains open."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
TIMEOUT = 240
LIMIT = 1024 ** 2
MODES = ('source-failure', 'counter-retry')
TARGET = 'mc_test_arrow_world_tick_cleanup'
BUNDLE_SHA = '6c4ce503205864074e6c455c0a4efa7e0647b41c86f8f6987421cc2c083359e6'
TABLES = ('application_owner', 'biped', 'render_resources', 'display_sdk',
          'loading_draw', 'resource_lookup_common')


def need(value, message):
    if not value:
        raise ValueError(message)


def pin(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def regular(path, directory=False):
    """Check the lexical path before resolving; never bypass an ancestor link."""
    path = path.absolute()
    need('..' not in path.parts, 'Parent traversal is not an input namespace')
    for current in reversed((path, *path.parents)):
        metadata = current.lstat()
        need(not stat.S_ISLNK(metadata.st_mode), 'Input symbolic link')
        if os.name == 'nt':
            attributes = getattr(metadata, 'st_file_attributes', None)
            need(type(attributes) is int and attributes & 0x400 == 0,
                 'Input reparse metadata or reparse point')
        need((stat.S_ISDIR if current != path or directory else stat.S_ISREG)(metadata.st_mode),
             'Input directory/regular-file type')
    return path.resolve(strict=True)


def lines(mode):
    need(mode in MODES, 'Unknown SDK cleanup mode')
    return [f'ARROW_SDK_CLEANUP_BEGIN schema=1 mode={mode}',
            '16 achievements', '151 recipes', 'ARROW_SDK_CLEANUP_CASE BEGIN',
            'ARROW_SDK_CLEANUP_CASE END world_calls=3 callbacks=1',
            'ARROW_SDK_CLEANUP_OPEN full-Minecraft.k/App-startup/window/victim/render']


def completed(child, mode, command):
    need(mode in MODES and type(command) is list and len(command) == 3 and
         all(type(item) is str for item in command) and command[1] == mode,
         'Exact selected command required')
    need(type(child.args) is list and child.args == command and
         type(child.returncode) is int and child.returncode == 0 and
         type(child.stdout) is bytes and type(child.stderr) is bytes,
         'Native command/exit/stream types')
    capture = child.capture_report
    need(type(capture) is dict and type(capture.get('command')) is list and
         capture['command'] == command and type(capture.get('exit')) is int and
         capture['exit'] == 0 and capture.get('reaped') is True and
         capture.get('pipe_capture_complete') is True and capture.get('timed_out') is False and
         capture.get('output_overflow') is False and type(capture.get('reader_errors')) is dict and
         capture['reader_errors'] == {}, 'Incomplete native capture')
    for key, expected in [('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                          ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        need(type(capture.get(key)) is int and capture[key] == expected,
             'Wrong actual capture bound/count: ' + key)
    need(len(child.stdout) <= LIMIT and child.stderr == b'' and child.stdout.endswith(b'\n'),
         'Native output extent/stderr/final newline')
    normalized = child.stdout.replace(b'\r\n', b'\n')
    need(b'\r' not in normalized, 'Malformed native newline')
    parts = normalized.split(b'\n')
    need(child.stdout in (b'\n'.join(parts[:-1]) + b'\n',
                          b'\r\n'.join(parts[:-1]) + b'\r\n'), 'Mixed native newline transport')
    expected = [item.encode('ascii') for item in lines(mode)]
    need(len(parts) == 8 and parts[-1] == b'' and parts[:-2] == expected,
         'Wrong exact native mode/banner/control sequence')
    match = re.fullmatch(rb'PASS: ([1-9][0-9]{0,9}) actual Arrow World-to-SDK cleanup checks;mode=' +
                         mode.encode('ascii'), parts[-2])
    need(match is not None, 'Wrong native completion footer')
    checks = int(match.group(1))
    need(1 <= checks <= 65536, 'Native check count outside declared bounded UInt32')
    return checks


def self_test():
    positives = refused = 0
    for mode in MODES:
        command = ['actual-native', mode, 'fresh-root-hex']
        text = ('\n'.join(lines(mode) + [
            f'PASS: 1 actual Arrow World-to-SDK cleanup checks;mode={mode}']) + '\n').encode('ascii')
        good = SimpleNamespace(args=command, returncode=0, stdout=text, stderr=b'',
            capture_report={'command': command, 'exit': 0, 'reaped': True,
                'pipe_capture_complete': True, 'timed_out': False, 'output_overflow': False,
                'reader_errors': {}, 'timeout_seconds': TIMEOUT, 'limit': LIMIT,
                'stdout_bytes': len(text), 'stderr_bytes': 0})
        for ending in (b'\n', b'\r\n'):
            item = copy.deepcopy(good)
            item.stdout = text.replace(b'\n', ending)
            item.capture_report['stdout_bytes'] = len(item.stdout)
            need(completed(item, mode, command) == 1, 'Synthetic protocol positive')
            positives += 1
        mutations = []
        for key, value in [('returncode', False), ('returncode', 1), ('stderr', b'failure'),
                           ('stdout', text[:-1]), ('stdout', text + b'extra\n'),
                           ('stdout', text.replace(b'16 achievements\n', b'')),
                           ('stdout', text.replace(b'PASS: 1 ', b'PASS: 0 ')),
                           ('stdout', text.replace(b'PASS: 1 ', b'PASS: 65537 ')),
                           ('stdout', text.replace(b'world_calls=3', b'world_calls=True')),
                           ('stdout', text.replace(b'world_calls=3', b'world_calls=4')),
                           ('stdout', text.replace(b'callbacks=1', b'callbacks=2')),
                           ('stdout', text.replace(b'callbacks=1', b'callbacks=True')),
                           ('stdout', text.replace(b'\n', b'\r\n', 1)),
                           ('stdout', text.replace(mode.encode('ascii'), b'foreign-mode'))]:
            item = copy.deepcopy(good)
            setattr(item, key, value)
            item.capture_report.update(stdout_bytes=len(item.stdout), stderr_bytes=len(item.stderr))
            mutations.append(item)
        for key, value in [('exit', False), ('reaped', False), ('pipe_capture_complete', False),
                           ('timed_out', True), ('output_overflow', True), ('reader_errors', {'x': 'lost'}),
                           ('timeout_seconds', False), ('limit', LIMIT + 1), ('stdout_bytes', False),
                           ('stderr_bytes', False), ('command', ['wrong'])]:
            item = copy.deepcopy(good)
            item.capture_report[key] = value
            mutations.append(item)
        for item in mutations:
            try:
                completed(item, mode, command)
            except (ValueError, TypeError, KeyError):
                refused += 1
            else:
                raise ValueError('Synthetic malformed transport admitted')
    need(positives == 4 and refused == 50, 'Every declared synthetic case ran')
    print(f'PASS: {positives} synthetic positives; {refused} transport refusals; no native execution')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--mode', choices=tuple(MODES))
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--project-root', type=Path)
    parser.add_argument('--bundle', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    need(args.mode is not None and args.executable is not None and args.work_dir is not None and
         args.bundle is not None, 'Mode/executable/work-dir/bundle are required')
    root = regular(args.project_root or Path(__file__).resolve().parents[3], directory=True)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    work_parent = regular(args.work_dir, directory=True)
    out = Path(tempfile.mkdtemp(prefix='arrow-sdk-cleanup-' + args.mode + '-', dir=work_parent))
    report = {'status': 'RUNNING', 'mode': args.mode, 'scope': 'Genuine World output to SDK checked cleanup only; '
              'full Minecraft.k/App/startup/window/victim/render OPEN',
              'guards_unchanged': False, 'native_protocol_complete': False}
    before = child = capture = native_root = identity = None
    launch_attempted = False

    def fail(key, error):
        detail = {'type': type(error).__name__, 'message': str(error)}
        report[key] = detail
        report.setdefault('first_failure', detail)
        report['status'] = 'FAIL'

    def load(name, relative):
        path = regular(root / relative)
        need(path.is_relative_to(root), 'Helper outside actual project')
        need(pin(path) == bundle['helpers'][relative], 'Actual helper changed: ' + relative)
        spec = importlib.util.spec_from_file_location(name, path)
        need(spec is not None and spec.loader is not None, 'Actual helper loader missing')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    try:
        bundle_path = regular(args.bundle)
        need(pin(bundle_path)['sha256'] == BUNDLE_SHA, 'Wrong immutable SDK cleanup bundle')
        bundle = json.loads(bundle_path.read_text(encoding='utf-8'))
        need(type(bundle) is dict and set(bundle) ==
             {'schema', 'target', 'required_parent', 'required_natural', 'required_sdk',
              'supplier_protocol', 'sources', 'helpers'} and
             bundle['schema'] == 'ARROW_WORLD_SDK_CLEANUP_BINDINGS_V1' and
             bundle['target'] == TARGET and
             bundle['required_parent'] == '05a1335fe4992e203398388234f672d3a5f439a4d37751303ae681011b31ccad' and
             bundle['required_natural'] == '4521b32257c2e7e7a0008bacc32bd0996ba17d96469f5ca8564c906bee15b597' and
             bundle['required_sdk'] == '5870a6e2cdbb5d072d09d4a697d26c1e1c64c98d707ddb706b05e427e9517faa' and
             bundle['supplier_protocol'] == {'bytes': 2037, 'sha256':
                 'ba1eb984e15aacaa4b07b29ca989b48bbabb40e63941783d6bbc0a228e17a0a0'} and
             type(bundle['sources']) is dict and type(bundle['helpers']) is dict and
             'tests/client/gameplay/test_arrow_world_tick_cleanup.c' in bundle['sources'] and
             'tests/world/runtime/test_arrow_natural_flight.c' in bundle['sources'],
             'Wrong explicit prospective bundle')
        common = load('sdk_cleanup_actual_common', 'tests/tools/world/test_world_query_controls.py')
        ownership = load('sdk_cleanup_actual_ownership', 'tests/tools/world/test_execution_ownership.py')
        support = load('sdk_cleanup_actual_capture', 'tests/reference/client/statistics/support.py')
        need(tuple(ownership.TABLES) == TABLES and common.LIMIT == LIMIT,
             'Actual six-table/capture helper contract differs')
        report['provenance_tables'] = list(TABLES)
        report['prospective_bundle'] = pin(bundle_path)
        executable = regular(args.executable)
        need(executable.stem == TARGET, 'Wrong SDK cleanup executable basename')

        def guarded():
            result = ownership.protected(executable)
            for section in ('sources', 'helpers'):
                for relative, expected in bundle[section].items():
                    need(type(relative) is str and relative and '\\' not in relative and
                         ':' not in relative and not Path(relative).is_absolute() and
                         '..' not in Path(relative).parts, 'Closed relative source pin required')
                    path = regular(root / relative)
                    need(path.is_relative_to(root) and pin(path) == expected,
                         'Prospective source mismatch: ' + relative)
                    result[str(path)] = expected
            for path in (regular(Path(__file__)), bundle_path):
                result[str(path)] = pin(path)
            for path in executable.parent.glob('*.dll'):
                result[str(regular(path))] = pin(path)
            return result

        before = guarded()
        report['guard_before'] = before
        native_root = out / 'worlds'
        report['native_root'] = str(native_root)
        native_root.mkdir()
        regular(native_root, directory=True)
        metadata = common.literal_metadata(native_root)
        identity = [metadata.st_dev, metadata.st_ino]
        need(common.fixture_files(native_root) == {}, 'Native producer must start from a fresh empty root')
        command = [str(executable), args.mode, str(native_root).encode('utf-8').hex()]
        report['command'] = command
        environment = os.environ.copy()
        environment['ALSOFT_DRIVERS'] = 'null'  # Existing headless client test policy.
        report['declared_environment'] = {'ALSOFT_DRIVERS': 'null'}
        launch_attempted = True
        child = support.run_bounded(command, out, 'native', timeout=TIMEOUT, limit=LIMIT,
                                    env=environment)
        capture = child.capture_report
        report['capture'] = capture
        report['checks'] = completed(child, args.mode, command)
        report['C_entry_witnesses_not_Source_calls'] = {'world_calls': 3, 'callbacks': 1}
        report['native_protocol_complete'] = True
        report['status'] = 'PASS'
    except BaseException as error:
        fail('initial_failure', error)
        if getattr(error, 'capture_report', None) is not None:
            capture = error.capture_report
            report['failed_capture'] = capture
    finally:
        # No heavy read/guard or next child while termination is unproved.
        safe = not launch_attempted or (type(capture) is dict and capture.get('reaped') is True)
        report['native_launch_attempted'] = launch_attempted
        report['native_child_returned'] = child is not None
        report['termination_proved_or_no_child'] = safe
        if not safe:
            fail('unsafe_boundary', ValueError('Root intervention required; terminal guards deferred'))
        if safe:
            if before is not None:
                try:
                    after = guarded()
                    report['guard_after'] = after
                    need(after == before, 'Source/product/helper/tables/bundle changed')
                    report['guards_unchanged'] = True
                except BaseException as error:
                    fail('terminal_guard_failure', error)
            if native_root is not None and identity is not None:
                try:
                    metadata = common.literal_metadata(native_root)
                    regular(native_root, directory=True)
                    need([metadata.st_dev, metadata.st_ino] == identity, 'Native root replaced')
                    report['native_authored_files_after'] = common.fixture_files(native_root)
                    report['native_root_identity_unchanged'] = True
                except BaseException as error:
                    fail('native_root_guard_failure', error)
            elif native_root is not None:
                report['native_root_identity_not_established'] = True
            if child is not None:
                try:
                    payloads = {}
                    for leaf in ('native.stdout', 'native.stderr', 'native.capture.json'):
                        path = out / leaf
                        metadata = common.literal_metadata(path)
                        need(stat.S_ISREG(metadata.st_mode) and metadata.st_size <= LIMIT,
                             'Retained payload regular-file extent')
                        payloads[leaf] = path.read_bytes()
                    common.verify_retained(child, payloads)
                    report['retained_payloads'] = {name: {'bytes': len(raw),
                        'sha256': hashlib.sha256(raw).hexdigest()} for name, raw in payloads.items()}
                    report['retained_capture_verified'] = True
                except BaseException as error:
                    fail('retention_failure', error)
        with (out / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': retained ' + str(out / 'report.json'), flush=True)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
