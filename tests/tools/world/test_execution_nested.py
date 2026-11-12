"""Retain bounded native nested-World controls; no complete SL/original claim."""
import argparse
import copy
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
TIMEOUT = 240
LIMIT = 4 * 1024**2
MAX_CHECKS = 2**32 - 1
FIXTURE_MANIFEST = '99371088de732254938b9bdbaded17f07c03eaf9c7239b6aebe8e5389d1f5096'
ACTIVATION_FIXTURE_MANIFEST = '23907e64c5de228529bc5e387a2efef516bcd0f00b41be2b4bccc80eff99cc34'
FIXTURE_SOURCES = {
    'tests/world/runtime/world_access/test_execution_nested.c':
        {'bytes': 48707, 'sha256': '5f2310c62a92f7a2f7f90b34a7459fe2889bbdaf1e7b88f9848b9a0f6270746b'},
    'tests/world/runtime/world_access/test_execution_nested_loader.c':
        {'bytes': 17767, 'sha256': 'a0951d0f655c79a474fc1b800251fb93c953ee639019000db3ea80efaf6ef9fd'},
    'tests/world/runtime/world_access/test_execution_nested_success.c':
        {'bytes': 12830, 'sha256': '3b3ae634d54610a99e8ee3bddb9fc5e2f36fb64242dd8d7142133b280729addb'},
}
ACTIVATION_SOURCES = {
    'tests/world/runtime/world_access/test_activation_execution.c':
        {'bytes': 30628, 'sha256': '2f1cfbda3cd6a42dd593a70adb3e5a3a734230cd0f748b7081a34a245f4f3a3c'},
}
HELPER_SOURCES = {
    'tests/tools/world/test_execution_ownership.py':
        {'bytes': 8350, 'sha256': '1d3616472c10b0c253e881fdd08c33d198765922d5e75bc45ec3f955ece5275e'},
    'tests/reference/client/statistics/support.py':
        {'bytes': 7123, 'sha256': '1ac10e26456a6a8c9ba21751f49499549ec8ed322874c0d5c07be74c5fed4fa1'},
    'tests/tools/world/test_source_world_construction.py':
        {'bytes': 4807, 'sha256': '6f3dedb2e5aaf9b706ab440a8661238bd650278850fed8e8159583c2968addda'},
    'tests/tools/world/test_source_saving.py':
        {'bytes': 24895, 'sha256': '1fc2f69d04e2ab6e5c48f9d2ebef1d9dd7de0c50a5a7278a81735b0a63edcff9'},
}
ACTIVATION_CASES = (
    'empty-distinct', 'success-distinct', 'success-alias', 'live-self-remove-success',
    'failure-source-distinct', 'failure-native-limit', 'failure-native-allocation-alias',
    'failure-nullout-saturated', 'failure-pop-refusal', 'success-pop-refusal',
    'native-null-context', 'actual-renderer-failure',
)
ACTIVATION_LINES = (b'ACTIVATION_OWNER_BEGIN schema=1',
                    b'16 achievements', b'151 recipes', b'16 achievements', b'151 recipes') + tuple(
    line for name in ACTIVATION_CASES for line in (
        (('ACTIVATION_OWNER_CASE ' + name + ' BEGIN').encode('ascii'),) +
        (('ACTIVATION_OWNER_CASE ' + name + ' END').encode('ascii'),)
    )
) + (b'ACTIVATION_OWNER_OPEN null-Chunk.d/slice-order dedicated full-A-query kernel TileEntity SL',)
SCHEMAS = {
    'nested': (2, tuple(b'NESTED_OWNER_CASE id=' + str(i).encode('ascii') for i in range(5)),
               rb'PASS: ([1-9][0-9]{0,9}) actual nested World owner checks;5 groups'),
    'loader': (0, tuple(b'NESTED_LOADER_CASE id=' + str(i).encode('ascii') for i in range(2)),
               rb'PASS: ([1-9][0-9]{0,9}) actual nested source-loader owner checks;2 groups'),
    'success': (1, tuple(b'NESTED_SUCCESS_CASE id=' + str(i).encode('ascii') for i in range(2)),
                rb'PASS: ([1-9][0-9]{0,9}) actual source-load/cache owner checks;2 groups'),
    'activation': (0, ACTIVATION_LINES,
                   rb'PASS: ([1-9][0-9]{0,9}) actual owned activation checks;12 component cases'),
}


def require(value, message):
    if not value:
        raise ValueError(message)


def completed(child, kind, command):
    """Pure transport reader: never resolves ROOT, imports fixtures or reads disk."""
    require(kind in SCHEMAS, 'Unknown native control kind')
    require(type(command) is list and command and all(type(x) is str for x in command),
            'Expected native command is not an exact string list')
    require(type(getattr(child, 'returncode', None)) is int and child.returncode == 0,
            'Native exit is not integer success')
    require(type(getattr(child, 'stdout', None)) is bytes and type(getattr(child, 'stderr', None)) is bytes,
            'Native streams are not bytes')
    c = getattr(child, 'capture_report', None)
    require(type(c) is dict, 'Missing native capture report')
    require(type(c.get('exit')) is int and c['exit'] == 0 and c.get('reaped') is True and
            c.get('pipe_capture_complete') is True and c.get('timed_out') is False and
            c.get('output_overflow') is False and type(c.get('reader_errors')) is dict and
            c['reader_errors'] == {}, 'Incomplete or failed native process capture')
    require(type(c.get('command')) is list and c['command'] == command and
            type(getattr(child, 'args', None)) is list and child.args == command,
            'Native command receipt mismatch')
    for name, value in [('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                        ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        require(type(c.get(name)) is int and c[name] == value, 'Wrong native bound/count: ' + name)
    require(len(child.stdout) <= LIMIT and len(child.stderr) <= LIMIT,
            'Native payload exceeds declared stream bound')
    require(child.stderr == b'' and child.stdout.endswith(b'\n'),
            'Native stderr or incomplete terminal line')
    normalized = child.stdout.replace(b'\r\n', b'\n')
    require(b'\r' not in normalized, 'Malformed native newline')
    lines = normalized.split(b'\n')
    expected = [b'16 achievements', b'151 recipes'] * SCHEMAS[kind][0] + list(SCHEMAS[kind][1])
    require(lines[-1] == b'' and len(lines) == len(expected) + 2 and lines[:-2] == expected,
            'Wrong exact setup/control sequence')
    footer = re.fullmatch(SCHEMAS[kind][2], lines[-2])
    require(footer is not None, 'Missing or invalid native completion footer')
    checks = int(footer.group(1))
    require(1 <= checks <= MAX_CHECKS, 'Native unsigned check count out of range')
    return checks


def literal_inputs(before, after, locks, require_locks):
    """Pure full-file guard; only exact eight-byte session.lock additions fit."""
    require(type(before) is dict and type(after) is dict and type(locks) is set and
            type(require_locks) is bool, 'Malformed literal input snapshots')
    for snapshot in (before, after):
        for name, value in snapshot.items():
            require(type(name) is str and name and type(value) is dict and
                    set(value) == {'bytes', 'sha256'} and type(value['bytes']) is int and
                    0 <= value['bytes'] <= LIMIT and type(value['sha256']) is str and
                    re.fullmatch(r'[0-9a-f]{64}', value['sha256']) is not None,
                    'Malformed full-file pin')
    require(all(name not in before and name.endswith('/session.lock') for name in locks),
            'Invalid declared source session paths')
    for name, value in before.items():
        require(after.get(name) == value, 'Authored source input changed or removed: ' + name)
    additions = set(after) - set(before)
    require(additions <= locks, 'Unexpected source output file')
    require(all(after[name]['bytes'] == 8 for name in additions), 'Source session token is not eight bytes')
    if require_locks:
        require(additions == locks, 'Missing actual source session token')


def verify_retained(child, byte_map):
    """Pure proof that all three retained payloads match ANY returned child."""
    names = {'native.stdout', 'native.stderr', 'native.capture.json'}
    require(type(byte_map) is dict and set(byte_map) == names, 'Missing retained native payload')
    require(all(type(value) is bytes and len(value) <= LIMIT for value in byte_map.values()),
            'Retained native payload type or extent')
    require(type(getattr(child, 'stdout', None)) is bytes and type(getattr(child, 'stderr', None)) is bytes and
            type(getattr(child, 'capture_report', None)) is dict, 'Missing actual returned child payload')
    require(byte_map['native.stdout'] == child.stdout and byte_map['native.stderr'] == child.stderr,
            'Retained native streams differ from returned bytes')

    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate retained capture JSON key')
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError('Nonstandard retained capture JSON constant: ' + value)

    def same_json(a, b):
        if type(a) is not type(b):
            return False
        if type(a) is dict:
            return set(a) == set(b) and all(type(key) is str and same_json(a[key], b[key]) for key in a)
        if type(a) is list:
            return len(a) == len(b) and all(same_json(x, y) for x, y in zip(a, b))
        return type(a) in (str, int, float, bool, type(None)) and a == b

    actual = json.loads(byte_map['native.capture.json'].decode('utf-8'),
                        object_pairs_hook=object_pairs, parse_constant=invalid_constant)
    require(type(actual) is dict and same_json(actual, child.capture_report),
            'Retained capture JSON differs from actual report')


def self_test():
    refused = 0
    retention_refused = 0
    for kind, schema in SCHEMAS.items():
        endings = {'nested': b'actual nested World owner checks;5 groups',
                   'loader': b'actual nested source-loader owner checks;2 groups',
                   'success': b'actual source-load/cache owner checks;2 groups',
                   'activation': b'actual owned activation checks;12 component cases'}
        footer = b'PASS: 1 ' + endings[kind]
        lines = [b'16 achievements', b'151 recipes'] * schema[0] + list(schema[1])
        stdout = b'\n'.join(lines + [footer]) + b'\n'
        command = ['native-fixture', kind, 'literal-input']
        report = dict(command=command.copy(), exit=0, reaped=True, pipe_capture_complete=True,
                      timed_out=False, output_overflow=False, reader_errors={}, timeout_seconds=TIMEOUT,
                      limit=LIMIT, stdout_bytes=len(stdout), stderr_bytes=0)
        good = SimpleNamespace(args=command.copy(), returncode=0, stdout=stdout, stderr=b'', capture_report=report)
        require(completed(good, kind, command) == 1, 'Pure positive transport')
        crlf = copy.deepcopy(good)
        crlf.stdout = stdout.replace(b'\n', b'\r\n')
        crlf.capture_report['stdout_bytes'] = len(crlf.stdout)
        require(completed(crlf, kind, command) == 1, 'Pure actual Windows newline transport')
        bad = []
        for name, value in [('returncode', 1), ('returncode', False), ('returncode', 0.0),
                            ('args', command[:-1]), ('stderr', b'failure\n'), ('stderr', ''),
                            ('stdout', stdout[:-1]), ('stdout', stdout + footer + b'\n'),
                            ('stdout', stdout.decode('ascii')),
                            ('stdout', b'\n'.join(lines[1:] + [footer]) + b'\n'),
                            ('stdout', b'\n'.join(lines + [lines[-1], footer]) + b'\n'),
                            ('stdout', stdout.replace(schema[1][0], b'NESTED_WRONG_CASE id=0')),
                            ('stdout', stdout.replace(footer, b'PASS: 0 ' + endings[kind])),
                            ('stdout', stdout.replace(footer, b'PASS: 4294967296 ' + endings[kind])),
                            ('stdout', stdout.replace(footer, b'PASS: True ' + endings[kind])),
                            ('stdout', stdout.replace(footer, b'PASS: 01 ' + endings[kind])),
                            ('stdout', stdout.replace(b'\n', b'\r', 1))]:
            item = copy.deepcopy(good)
            setattr(item, name, value)
            if type(item.stdout) is bytes:
                item.capture_report['stdout_bytes'] = len(item.stdout)
            if type(item.stderr) is bytes:
                item.capture_report['stderr_bytes'] = len(item.stderr)
            bad.append(item)
        reordered = copy.deepcopy(good)
        changed = lines.copy()
        changed[-1], changed[-2] = changed[-2], changed[-1]
        reordered.stdout = b'\n'.join(changed + [footer]) + b'\n'
        reordered.capture_report['stdout_bytes'] = len(reordered.stdout)
        bad.append(reordered)
        if kind == 'activation':
            misplaced = copy.deepcopy(good)
            moved = [line for line in lines if line not in (b'16 achievements', b'151 recipes')]
            position = moved.index(b'ACTIVATION_OWNER_CASE actual-renderer-failure BEGIN') + 1
            moved[position:position] = [b'16 achievements', b'151 recipes'] * 2
            misplaced.stdout = b'\n'.join(moved + [footer]) + b'\n'
            misplaced.capture_report['stdout_bytes'] = len(misplaced.stdout)
            bad.append(misplaced)
        for name, value in [('exit', False), ('exit', 1), ('exit', 0.0), ('reaped', False),
                            ('reaped', 1), ('pipe_capture_complete', False), ('pipe_capture_complete', 1),
                            ('timed_out', True), ('timed_out', 0), ('output_overflow', True),
                            ('output_overflow', 0), ('reader_errors', {'stderr': [{'phase': 'read'}]}),
                            ('reader_errors', []), ('reader_errors', False), ('timeout_seconds', 1),
                            ('timeout_seconds', True), ('limit', 0), ('limit', True),
                            ('stdout_bytes', len(stdout) + 1), ('stdout_bytes', False),
                            ('stderr_bytes', False), ('command', command[:-1]), ('command', tuple(command))]:
            item = copy.deepcopy(good)
            item.capture_report[name] = value
            bad.append(item)
        item = copy.deepcopy(good)
        item.capture_report = None
        bad.append(item)
        oversized = copy.deepcopy(good)
        oversized.stdout = b'x' * (LIMIT + 1)
        oversized.capture_report['stdout_bytes'] = len(oversized.stdout)
        bad.append(oversized)
        for item in bad:
            try:
                completed(item, kind, command)
            except ValueError:
                refused += 1
                continue
            raise ValueError('Corrupt native transport admitted: ' + kind)
        def payloads(value):
            return {'native.stdout': value.stdout, 'native.stderr': value.stderr,
                    'native.capture.json': json.dumps(value.capture_report).encode('utf-8') + b'\n'}

        verify_retained(good, payloads(good))
        semantic = payloads(good)
        semantic['native.capture.json'] = json.dumps(good.capture_report, sort_keys=True, indent=1).encode('utf-8')
        verify_retained(good, semantic)  # JSON whitespace/key order has no semantic change.
        red = copy.deepcopy(good)
        red.returncode = red.capture_report['exit'] = 1
        red.stdout = b'\n'.join(lines[:2]) + b'\n'
        red.capture_report['stdout_bytes'] = len(red.stdout)
        red.stderr = b'actual red diagnostic\n'
        red.capture_report['stderr_bytes'] = len(red.stderr)
        verify_retained(red, payloads(red))  # Nonzero exit STILL requires exact retained payloads.
        wrong_protocol = copy.deepcopy(good)
        wrong_protocol.stdout = b'unrecognized native protocol\n'
        wrong_protocol.capture_report['stdout_bytes'] = len(wrong_protocol.stdout)
        verify_retained(wrong_protocol, payloads(wrong_protocol))
        actual = payloads(red)
        bad_retained = []
        for name in actual:
            item = actual.copy()
            del item[name]
            bad_retained.append(item)
        for name in ('native.stdout', 'native.stderr'):
            item = actual.copy()
            item[name] = b'x' + item[name][1:]
            bad_retained.append(item)
        for name in actual:
            item = actual.copy()
            item[name] = item[name][:-2] if name == 'native.capture.json' else item[name][:-1]
            bad_retained.append(item)
        for name in actual:
            item = actual.copy()
            item[name] = b''
            bad_retained.append(item)
        for name, value in [('exit', 0), ('exit', True), ('exit', 1.0), ('reader_errors', None), ('foreign', 1)]:
            item = actual.copy()
            changed = copy.deepcopy(red.capture_report)
            changed[name] = value
            item['native.capture.json'] = json.dumps(changed).encode('utf-8')
            bad_retained.append(item)
        for data in (b'{', b'\xff', b'[]', b'null'):
            item = actual.copy()
            item['native.capture.json'] = data
            bad_retained.append(item)
        item = actual.copy()
        item['native.capture.json'] = item['native.capture.json'][:-2] + b', "exit": 1}\n'
        bad_retained.append(item)
        item = actual.copy()
        item['native.stdout'] = 'not bytes'
        bad_retained.append(item)
        bad_retained.append(list(actual.items()))
        item = actual.copy()
        item['native.stdout'] = b'x' * (LIMIT + 1)
        bad_retained.append(item)
        for item in bad_retained:
            try:
                verify_retained(red, item)
            except (ValueError, UnicodeError):
                retention_refused += 1
                continue
            raise ValueError('Missing/changed retained returned-child payload admitted: ' + kind)
    before = {'world/level.dat': {'bytes': 3, 'sha256': '1' * 64},
              'world/region/r.0.0.mcr': {'bytes': 12288, 'sha256': '2' * 64}}
    after = copy.deepcopy(before)
    after['world/session.lock'] = {'bytes': 8, 'sha256': '3' * 64}
    locks = {'world/session.lock'}
    literal_inputs(before, after, locks, True)
    literal_inputs(before, before, locks, False)
    bad_inputs = []
    for name, value in [('world/level.dat', {'bytes': 3, 'sha256': '4' * 64}),
                        ('world/session.lock', {'bytes': 9, 'sha256': '3' * 64}),
                        ('world/session.lock', {'bytes': False, 'sha256': '3' * 64}),
                        ('world/session.lock', {'bytes': 8, 'sha256': 'bad'}),
                        ('world/extra', {'bytes': 8, 'sha256': '5' * 64})]:
        item = copy.deepcopy(after)
        item[name] = value
        bad_inputs.append(item)
    item = copy.deepcopy(after)
    del item['world/level.dat']
    bad_inputs.append(item)
    bad_inputs.append(copy.deepcopy(before))
    input_refused = 0
    for item in bad_inputs:
        try:
            literal_inputs(before, item, locks, True)
        except ValueError:
            input_refused += 1
            continue
        raise ValueError('Corrupt literal input guard admitted')
    require(refused == 173 and input_refused == 7 and retention_refused == 96,
            'Every declared pure corruption refused')
    print('PASS:', refused, 'nested transport refusals;', input_refused, 'literal-input guard refusals;',
          retention_refused, 'retained-payload refusals')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--kind', choices=tuple(SCHEMAS))
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.kind is None or args.executable is None or args.work_dir is None:
        parser.error('kind, executable and retained-work parent are required')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='nested-' + args.kind + '-', dir=args.work_dir.resolve()))
    report = dict(status='RUNNING', kind=args.kind, preparation_fixture_manifest=FIXTURE_MANIFEST,
                  activation_preparation_fixture_manifest=ACTIVATION_FIXTURE_MANIFEST,
                  scope='Constructed native owner controls only; full SL/kernel/projection and original parity remain OPEN')
    before = input_before = None
    root = executable = ownership = fixture_root = None
    native_launch_attempted = native_completed = False
    child = None
    expected_locks = set()

    def load(name, relative):
        path = root / relative
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def guards():
        values = ownership.protected(executable)
        extra = {Path(__file__).resolve(), root / 'tests/reference/client/statistics/support.py'}
        extra.update(root / path for path in HELPER_SOURCES)
        extra.update(root / path for path in FIXTURE_SOURCES)
        extra.update(root / path for path in ACTIVATION_SOURCES)
        for path in sorted(extra):
            values[str(path)] = ownership.pin(path)
        return values

    def files():
        values = {}
        total = 0
        paths = sorted(fixture_root.rglob('*'))
        ownership.need(len(paths) <= 256, 'Fixture directory extent exceeded')
        for path in paths:
            ownership.need(not path.is_symlink(), 'Unexpected fixture symbolic link')
            if path.is_dir():
                continue
            ownership.need(path.is_file() and path.stat().st_size <= LIMIT,
                           'Unexpected or oversized fixture file')
            value = ownership.pin(path)
            total += value['bytes']
            ownership.need(total <= 16 * 1024**2, 'Fixture total file extent exceeded')
            values[path.relative_to(fixture_root).as_posix()] = value
        return values

    try:
        print(json.dumps({'retained_work': str(out), 'stage': 'PREPARING'}), flush=True)
        # Installed public path only. This is deliberately deferred until AFTER
        # --self-test; private pure checks resolve no repository data/helper.
        root = Path(__file__).resolve().parents[3]
        ownership = load('nested_actual_ownership', 'tests/tools/world/test_execution_ownership.py')
        executable = args.executable.resolve(strict=True)
        ownership.need(executable.is_file(), 'Native executable is not a file')
        for relative, expected in {**HELPER_SOURCES, **FIXTURE_SOURCES, **ACTIVATION_SOURCES}.items():
            ownership.need(ownership.pin(root / relative) == expected, 'Frozen helper/fixture mismatch: ' + relative)
        before = guards()
        report['inputs_before'] = before
        report['tables'] = list(ownership.TABLES)
        ownership.need(len(ownership.TABLES) == 6, 'Expected six current admission tables')
        support = load('nested_actual_support', 'tests/reference/client/statistics/support.py')
        fixture_root = out / ('world-\u03bb-\u4e16\u754c' if args.kind in ('nested', 'activation') else 'owned')
        fixture_root.mkdir()
        report['fixture_root'] = str(fixture_root)
        if args.kind in ('nested', 'activation'):
            selector = 'nested-owner' if args.kind == 'nested' else 'activation-owned'
            command = [str(executable), selector, str(fixture_root).encode('utf-8').hex()]
            report['fixture_path_policy'] = 'Fresh Unicode root; actual UTF8 bytes transported as hex'
        else:
            ownership.need(str(fixture_root).isascii(), 'Existing source fixture requires an ASCII owned root')
            report['fixture_path_policy'] = 'Controlled ASCII source fixture root; no Unicode pathname claim'
            command = [str(executable), str(fixture_root)]
            if args.kind == 'loader':
                recipe = load('nested_actual_source_recipe', 'tests/tools/world/test_source_world_construction.py')
                for name in ('primary-a', 'primary-b', 'compat'):
                    folder = fixture_root / name
                    (folder / 'data').mkdir(parents=True)
                    (folder / 'region').mkdir()
                    (folder / 'level.dat').write_bytes(gzip.compress(recipe.metadata(), mtime=0))
                    (folder / 'data/idcounts.dat').write_bytes(recipe.raw_ids())
                    (folder / 'region/r.0.0.mcr').write_bytes(recipe.malformed_region())
                    (folder / 'preserve.bin').write_bytes(b'unrelated source World data\x00\xff')
                    expected_locks.add(name + '/session.lock')
            else:
                recipe = load('nested_actual_success_recipe', 'tests/tools/world/test_source_saving.py')
                (fixture_root / 'world/region').mkdir(parents=True)
                info, image = recipe.seed('empty')
                (fixture_root / 'world/level.dat').write_bytes(info)
                (fixture_root / 'world/region/r.0.0.mcr').write_bytes(image)
                (fixture_root / 'world/preserve.bin').write_bytes(b'unrelated disposable World file\x00\xff')
                expected_locks = {'world/session.lock'}
        input_before = files()
        report['fixture_files_before'] = input_before
        report['expected_session_additions'] = sorted(expected_locks)
        native_launch_attempted = True
        child = support.run_bounded(command, out, 'native', timeout=TIMEOUT, limit=LIMIT, env=os.environ.copy())
        report['capture'] = child.capture_report
        report['checks'] = completed(child, args.kind, command)
        native_completed = True
        report['status'] = 'PASS'
    except BaseException as error:
        report.update(status='FAIL', failure={'type': type(error).__name__, 'message': str(error)})
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
    finally:
        report['native_launch_attempted'] = native_launch_attempted
        report['native_child_returned'] = child is not None
        report['native_completed'] = native_completed
        same = False
        if before is not None:
            try:
                after = guards()
                report['inputs_after'] = after
                same = before == after
                ownership.need(same, 'Current source/product/admission/helper set changed')
            except BaseException as error:
                report['terminal_guard_failure'] = {'type': type(error).__name__, 'message': str(error)}
        report['inputs_unchanged'] = same
        if not same:
            report['status'] = 'FAIL'
        if fixture_root is not None and ownership is not None:
            try:
                after = files()
                report['fixture_files_after'] = after
                if input_before is not None and args.kind not in ('nested', 'activation'):
                    literal_inputs(input_before, after, expected_locks, native_completed)
                    report['literal_inputs_unchanged'] = True
                else:
                    report['literal_inputs_unchanged'] = None
                    report['native_authored_files_scope'] = 'Actual C fixture authors its real transport; all produced files retained/pinned'
            except BaseException as error:
                report['status'] = 'FAIL'
                report['fixture_guard_failure'] = {'type': type(error).__name__, 'message': str(error)}
        capture_files, capture_bytes, capture_read_errors = {}, {}, {}
        for leaf in ('native.stdout', 'native.stderr', 'native.capture.json'):
            path = out / leaf
            try:
                if not path.exists():
                    capture_files[leaf] = {'missing': True}
                    continue
                require(path.is_file() and not path.is_symlink() and path.stat().st_size <= LIMIT,
                        'Retained capture file type/extent: ' + leaf)
                data = path.read_bytes()
                value = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                if ownership is not None:
                    ownership.need(ownership.pin(path) == value, 'Capture changed during terminal read: ' + leaf)
                capture_files[leaf] = value
                capture_bytes[leaf] = data
            except BaseException as error:
                detail = {'type': type(error).__name__, 'message': str(error)}
                capture_files[leaf] = {'failure': detail}
                capture_read_errors[leaf] = detail
        report['capture_files'] = capture_files
        if child is not None:
            try:
                verify_retained(child, capture_bytes)
                report['retained_capture_verified'] = True
            except BaseException as error:
                report['status'] = 'FAIL'
                report['retained_capture_verified'] = False
                report['retention_failure'] = {'type': type(error).__name__, 'message': str(error),
                                               'read_errors': capture_read_errors}
        else:
            report['retained_capture_verified'] = None
            report['retention_scope'] = 'No returned child; missing/partial capture files are informational'
        with (out / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    if report['status'] != 'PASS':
        print('FAIL: nested native control; retained report:', out / 'report.json', file=sys.stderr)
        return 1
    print('PASS:', report['checks'], 'nested native owner checks; retained', out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
