"""Retain bounded actual kernel-loader and Tile lookup controls; no full SL claim."""
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
LIMIT = 1024**2
MAX_CHECKS = 2**32 - 1
BUNDLE_SHA = '5dda4c47e7add8cb462473ff8e218e049c3541babbb1632685a6630581cd9e24'
MODES = {'loader': ('error', 'null-error'),
 'lookup': ('map', 'lazy', 'armed', 'held', 'nested'),
 'collision': ('id-first',
               'metadata-first',
               'domain',
               'loader-error',
               'loader-null-error',
               'armed',
               'held')}
SUPPLIERS = {'loader': {'manifest': '01e30de5a0245269259815a878ffd059768858b79b1cf11fc06bfcba93f9685b',
            'protocol': 'fc0b553cf1b23449001bf22a95f943c053a6cc58122eab4d42f408c0548f59f5',
            'source': 'tests/world/runtime/world_access/test_kernel_loader_execution.c',
            'source_pin': {'bytes': 23441,
                           'sha256': 'bfb1d1eba0a87b715c155901479d9eef80fb2f0990706342b7dea6ff3685c06e'}},
 'lookup': {'manifest': '1662e2b260efd3cc5bff67768cbe5ed5c4f197abde7853147f7fc3a9251df0dc',
            'protocol': '4ee2be5a82fee5e27e7301cee3aff67a4d3583323b55d4fa42d296ea6c98a395',
            'source': 'tests/world/runtime/world_access/test_tile_lookup_execution.c',
            'source_pin': {'bytes': 34102,
                           'sha256': '9b44acdf52f8756d856e86016a8c9f6f8788134a1c329272598cdae1024fed48'}},
 'collision': {'manifest': '8881737d8b31805c0f43fd25e99ef274509125bda65fa1e4b3cc583261bccc9c',
               'protocol': '6fd0849ab48f9b3741756f160f022c003e24bdcdbe94fec36996fb7980acad0f',
               'source': 'tests/world/runtime/world_access/test_collision_query_execution.c',
               'source_pin': {'bytes': 39905,
                              'sha256': '8032d0b673620aea1f1cce08685ff751509878c707a53f45b6f8098361702938'}}}


COLLISION_MARKERS = {'id-first': ('cold BEGIN', 'cold END', 'cache BEGIN', 'cache END'),
 'metadata-first': ('cold BEGIN', 'cold END', 'cache BEGIN', 'cache END'),
 'domain': ('domain BEGIN', 'domain END'),
 'loader-error': ('loader entered',
                  'loader selected',
                  'loader retained',
                  'loader retired'),
 'loader-null-error': ('loader entered',
                       'loader selected',
                       'loader retained',
                       'loader retired'),
 'armed': ('cold BEGIN', 'cold END', 'callback BEGIN', 'callback END'),
 'held': ('cold BEGIN', 'cold END', 'callback BEGIN', 'callback END')}


def require(value, message):
    if not value:
        raise ValueError(message)


def record_failure(report, key, error):
    detail = {'type': type(error).__name__, 'message': str(error)}
    report[key] = detail
    report.setdefault('failure', detail)
    report['status'] = 'FAIL'


def protocol_lines(kind, mode):
    require(kind in MODES and mode in MODES[kind], 'Unknown literal query control mode')
    if kind == 'collision':
        return [b'COLLISION_QUERY_BEGIN schema=1 mode=' + mode.encode('ascii'), b'16 achievements', b'151 recipes'] + [
            b'COLLISION_QUERY_CASE ' + marker.encode('ascii') for marker in COLLISION_MARKERS[mode]] + [
                b'COLLISION_QUERY_OPEN Blank/foreign-Store/structural-unwind/full-SL']
    if kind == 'loader':
        return [b'KERNEL_LOADER_BEGIN ' + mode.encode('ascii'), b'16 achievements', b'151 recipes'] + [
            b'KERNEL_LOADER_CASE ' + name.encode('ascii')
            for name in ('entered', 'selected', 'caller-cleared', 'popped', 'retired')]
    markers = ('cold BEGIN', 'cold END', 'cache BEGIN', 'cache END',
               'unused-defining BEGIN', 'unused-defining END') if mode in ('map', 'lazy') else (
                   'cold BEGIN', 'cold END', 'callback BEGIN', 'callback END')
    return [b'TILE_LOOKUP_BEGIN schema=1 mode=' + mode.encode('ascii'),
            b'16 achievements', b'151 recipes'] + [
                b'TILE_LOOKUP_CASE ' + name.encode('ascii') for name in markers] + [
                    b'TILE_LOOKUP_OPEN foreign-Store/null-Chunk-world/caller-unwind/full-SL']


def footer_pattern(kind, mode):
    require(kind in MODES and mode in MODES[kind], 'Unknown footer mode')
    scope = b'owned collision scalar' if kind == 'collision' else (
        b'kernel direct-loader' if kind == 'loader' else b'Tile lookup')
    return rb'PASS: ([1-9][0-9]{0,9}) actual ' + scope + b' checks;mode=' + mode.encode('ascii')


def completed(child, kind, mode, command):
    require(type(command) is list and len(command) == 3 and all(type(x) is str for x in command),
            'Exact native command required')
    require(command[1] == mode, 'Native command mode differs from selected literal mode')
    require(type(getattr(child, 'returncode', None)) is int and child.returncode == 0,
            'Native exit is not integer success')
    require(type(getattr(child, 'stdout', None)) is bytes and type(getattr(child, 'stderr', None)) is bytes,
            'Native streams are not bytes')
    capture = getattr(child, 'capture_report', None)
    require(type(capture) is dict, 'Missing capture report')
    require(type(capture.get('command')) is list and capture['command'] == command and
            type(getattr(child, 'args', None)) is list and child.args == command,
            'Actual command receipt mismatch')
    require(type(capture.get('exit')) is int and capture['exit'] == 0 and
            capture.get('reaped') is True and capture.get('pipe_capture_complete') is True and
            capture.get('timed_out') is False and capture.get('output_overflow') is False and
            type(capture.get('reader_errors')) is dict and capture['reader_errors'] == {},
            'Failed/incomplete native capture')
    for name, value in [('timeout_seconds', TIMEOUT), ('limit', LIMIT),
                        ('stdout_bytes', len(child.stdout)), ('stderr_bytes', len(child.stderr))]:
        require(type(capture.get(name)) is int and capture[name] == value,
                'Wrong capture bound/count: ' + name)
    require(len(child.stdout) <= LIMIT and len(child.stderr) <= LIMIT and child.stderr == b'' and
            child.stdout.endswith(b'\n'), 'Native stream extent, stderr or final newline')
    normalized = child.stdout.replace(b'\r\n', b'\n')
    require(b'\r' not in normalized, 'Malformed native newline')
    lines = normalized.split(b'\n')
    expected = protocol_lines(kind, mode)
    require(lines[-1] == b'' and len(lines) == len(expected) + 2 and lines[:-2] == expected,
            'Wrong exact mode/banner/control sequence')
    footer = re.fullmatch(footer_pattern(kind, mode), lines[-2])
    require(footer is not None, 'Missing/wrong literal completion footer')
    checks = int(footer.group(1))
    require(1 <= checks <= MAX_CHECKS, 'Unsigned native check count outside range')
    return checks


def bundle_payload(data):
    require(type(data) is bytes and len(data) <= LIMIT and
            hashlib.sha256(data).hexdigest() == BUNDLE_SHA, 'Unapproved explicit dependency bundle')
    bundle = json.loads(data.decode('utf-8'))
    require(type(bundle) is dict and bundle.get('schema') == 'EXPLICIT_WORLD_QUERY_CONTROLS_BUNDLE_V1' and
            bundle.get('selection') == 'a97:nested3+Entity2+kernel2+Tile2;loader1+lookup1-nbt-capacity-fix1;collision-owner1+collision-fixture1',
            'Wrong selected dependency schema')
    for kind, expected in SUPPLIERS.items():
        actual = bundle['fixtures'][kind]
        require(actual['manifest']['sha256'] == expected['manifest'] and
                actual['protocol']['sha256'] == expected['protocol'] and
                actual['source'] == expected['source'] and actual['source_pin'] == expected['source_pin'] and
                bundle['sources'][expected['source']] == expected['source_pin'],
                'Wrong immutable fixture/protocol/source supplier: ' + kind)
    return bundle


def pin(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


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


def literal_metadata(path):
    """Use lstat on Python3.10+, never silently skip Windows reparse checks."""
    metadata = path.lstat()
    require(not stat.S_ISLNK(metadata.st_mode), 'Literal path is a symbolic link')
    if os.name == 'nt':
        attributes = getattr(metadata, 'st_file_attributes', None)
        require(type(attributes) is int, 'Windows literal reparse metadata unavailable')
        require(attributes & 0x400 == 0, 'Literal path is a Windows reparse point')
    return metadata


def fixture_files(fixture_root):
    """Snapshot a real owned directory; a missing/replaced root is a failure."""
    require(isinstance(fixture_root, Path), 'Missing literal fixture root path')
    require(stat.S_ISDIR(literal_metadata(fixture_root).st_mode),
            'Literal fixture root is not a real directory')
    result = {}
    total = 0
    paths = sorted(fixture_root.rglob('*'))
    require(len(paths) <= 256, 'Fixture directory extent exceeded')
    for path in paths:
        metadata = literal_metadata(path)
        if stat.S_ISDIR(metadata.st_mode):
            continue
        require(stat.S_ISREG(metadata.st_mode) and metadata.st_size <= LIMIT,
                'Unexpected fixture file type/extent')
        value = pin(path)
        total += value['bytes']
        require(total <= 16 * LIMIT, 'Fixture total bytes exceeded')
        result[path.relative_to(fixture_root).as_posix()] = value
    require(stat.S_ISDIR(literal_metadata(fixture_root).st_mode),
            'Literal fixture root disappeared during snapshot')
    return result


def terminal_literal_guard(report, fixture_root, literal_before, protocol_complete):
    """Called unconditionally once a pre-run literal snapshot exists."""
    actual = fixture_files(fixture_root)
    report['literal_after'] = actual
    literal_inputs(literal_before, actual, {'world/session.lock'}, protocol_complete)
    report['literal_inputs_unchanged'] = True


def literal_root_self_test():
    """Python-only real directory controls, never Source data or a native child."""
    refused = 0
    temporary_parent = Path(tempfile.gettempdir()).resolve(strict=True)
    for operation in ('removed', 'renamed', 'replaced-empty'):
        with tempfile.TemporaryDirectory(prefix='kernel-terminal-root-', dir=temporary_parent) as name:
            base = Path(name).resolve(strict=True)
            # Validate the exact newly created cleanup target before the
            # TemporaryDirectory owner's eventual recursive disposal.
            require(base.parent == temporary_parent and base.name.startswith('kernel-terminal-root-'),
                    'Disposable Python control escaped its intended parent')
            owned = base / 'owned'
            owned.mkdir()
            literal = owned / 'literal.bin'
            literal.write_bytes(b'Python guard control only; no Source fixture')
            before = fixture_files(owned)
            positive = {}
            terminal_literal_guard(positive, owned, before, False)
            require(positive.get('literal_inputs_unchanged') is True, 'Real-directory guard positive')
            require(owned.resolve(strict=True).parent == base, 'Exact disposable root boundary')
            if operation == 'removed':
                literal.unlink()
                owned.rmdir()
            else:
                renamed = base / 'renamed'
                require(renamed.parent.resolve(strict=True) == base and not renamed.exists(),
                        'Exact disposable rename boundary')
                owned.rename(renamed)
                if operation == 'replaced-empty':
                    owned.mkdir()
            # This is the same terminal helper as main's finally path. A
            # simulated already-PASS envelope must be downgraded on refusal.
            report = {'status': 'PASS'}
            try:
                terminal_literal_guard(report, owned, before, False)
            except (ValueError, OSError):
                report['status'] = 'FAIL'
                refused += 1
            require(report['status'] == 'FAIL' and report.get('literal_inputs_unchanged') is not True,
                    'Missing/renamed/replaced literal root admitted')
    require(refused == 3, 'Every real whole-root mutation refused')
    return refused


def self_test():
    rejected = retention = literals = bundles = 0
    for kind, modes in MODES.items():
        for mode in modes:
            command = ['actual-native', mode, 'fresh-root']
            footer = b'PASS: 1 actual ' + (b'owned collision scalar' if kind == 'collision' else
                (b'kernel direct-loader' if kind == 'loader' else b'Tile lookup')) + b' checks;mode=' + mode.encode('ascii')
            lines = protocol_lines(kind, mode)
            stdout = b'\n'.join(lines + [footer]) + b'\n'
            report = dict(command=command.copy(), exit=0, reaped=True, pipe_capture_complete=True,
                          timed_out=False, output_overflow=False, reader_errors={}, timeout_seconds=TIMEOUT,
                          limit=LIMIT, stdout_bytes=len(stdout), stderr_bytes=0)
            good = SimpleNamespace(args=command.copy(), returncode=0, stdout=stdout, stderr=b'',
                                   capture_report=report)
            require(completed(good, kind, mode, command) == 1, 'Pure LF protocol positive')
            windows = copy.deepcopy(good)
            windows.stdout = stdout.replace(b'\n', b'\r\n')
            windows.capture_report['stdout_bytes'] = len(windows.stdout)
            require(completed(windows, kind, mode, command) == 1, 'Pure CRLF protocol positive')
            maximum = copy.deepcopy(good)
            maximum.stdout = stdout.replace(b'PASS: 1 ', b'PASS: 4294967295 ')
            maximum.capture_report['stdout_bytes'] = len(maximum.stdout)
            require(completed(maximum, kind, mode, command) == MAX_CHECKS, 'Actual uint32 ceiling accepted')
            bad = []
            for key, value in [('returncode', 1), ('returncode', False), ('returncode', 0.0),
                               ('stderr', b'real failure\n'), ('stderr', ''), ('stdout', stdout[:-1]),
                               ('stdout', stdout + footer + b'\n'), ('stdout', stdout.decode('ascii')),
                               ('args', command[:-1]), ('stdout', stdout.replace(b'16 achievements\n', b'')),
                               ('stdout', stdout.replace(b'151 recipes\n', b'151 recipes\n16 achievements\n151 recipes\n')),
                               ('stdout', stdout.replace(lines[3], b'UNKNOWN_CASE', 1)),
                               ('stdout', stdout.replace(footer, footer.replace(b'PASS: 1 ', b'PASS: 0 '))),
                               ('stdout', stdout.replace(footer, footer.replace(b'PASS: 1 ', b'PASS: 01 '))),
                               ('stdout', stdout.replace(footer, footer.replace(b'PASS: 1 ', b'PASS: 4294967296 '))),
                               ('stdout', stdout.replace(b'\n', b'\r', 1))]:
                item = copy.deepcopy(good)
                setattr(item, key, value)
                if type(item.stdout) is bytes:
                    item.capture_report['stdout_bytes'] = len(item.stdout)
                if type(item.stderr) is bytes:
                    item.capture_report['stderr_bytes'] = len(item.stderr)
                bad.append(item)
            for key, value in [('exit', True), ('exit', 1), ('exit', 0.0), ('reaped', False), ('reaped', 1),
                               ('pipe_capture_complete', False), ('pipe_capture_complete', 1),
                               ('timed_out', True), ('timed_out', 0), ('output_overflow', True),
                               ('output_overflow', 0), ('reader_errors', []), ('reader_errors', {'stdout': 'lost'}),
                               ('command', command[:-1]), ('command', tuple(command)), ('timeout_seconds', True),
                               ('timeout_seconds', 1), ('limit', True), ('limit', LIMIT + 1), ('stdout_bytes', False),
                               ('stdout_bytes', len(stdout) + 1), ('stderr_bytes', False)]:
                item = copy.deepcopy(good)
                item.capture_report[key] = value
                bad.append(item)
            # Actual banner placement is part of the protocol, not a count-only check.
            misplaced = copy.deepcopy(good)
            changed = lines[:1] + lines[3:5] + lines[1:3] + lines[5:]
            misplaced.stdout = b'\n'.join(changed + [footer]) + b'\n'
            misplaced.capture_report['stdout_bytes'] = len(misplaced.stdout)
            bad.append(misplaced)
            swapped = copy.deepcopy(good)
            changed = lines.copy()
            changed[3], changed[4] = changed[4], changed[3]
            swapped.stdout = b'\n'.join(changed + [footer]) + b'\n'
            swapped.capture_report['stdout_bytes'] = len(swapped.stdout)
            bad.append(swapped)
            wrong = copy.deepcopy(good)
            wrong.stdout = stdout.replace(b'mode=' + mode.encode('ascii'), b'mode=wrong')
            # Loader's global line has no mode= field; its footer still changes.
            wrong.capture_report['stdout_bytes'] = len(wrong.stdout)
            bad.append(wrong)
            oversized = copy.deepcopy(good)
            oversized.stdout = b'x' * (LIMIT + 1)
            oversized.capture_report['stdout_bytes'] = len(oversized.stdout)
            bad.append(oversized)
            missing = copy.deepcopy(good)
            missing.capture_report = None
            bad.append(missing)
            for item in bad:
                try:
                    completed(item, kind, mode, command)
                except ValueError:
                    rejected += 1
                    continue
                raise ValueError('Corrupt protocol accepted: ' + kind + '/' + mode)
            other_command = command.copy()
            other_command[1] = 'foreign'
            other_child = copy.deepcopy(good)
            other_child.args = other_command.copy()
            other_child.capture_report['command'] = other_command.copy()
            try:
                completed(other_child, kind, mode, other_command)
            except ValueError:
                rejected += 1
            else:
                raise ValueError('Native command mode differs from selected literal mode')
            # A valid row for any different kind/mode is never accepted here.
            for other_kind, other_modes in MODES.items():
                for other_mode in other_modes:
                    if (other_kind, other_mode) == (kind, mode):
                        continue
                    try:
                        completed(good, other_kind, other_mode, command)
                    except ValueError:
                        rejected += 1
                        continue
                    raise ValueError('Different native mode admitted')

            def payloads(child):
                return {'native.stdout': child.stdout, 'native.stderr': child.stderr,
                        'native.capture.json': json.dumps(child.capture_report).encode('utf-8') + b'\n'}

            verify_retained(good, payloads(good))
            semantic = payloads(good)
            semantic['native.capture.json'] = json.dumps(good.capture_report, sort_keys=True, indent=1).encode('utf-8')
            verify_retained(good, semantic)
            red = copy.deepcopy(good)
            red.returncode = red.capture_report['exit'] = 1
            red.stdout, red.stderr = b'actual partial prefix\n', b'actual source failure\n'
            red.capture_report.update(stdout_bytes=len(red.stdout), stderr_bytes=len(red.stderr))
            actual = payloads(red)
            verify_retained(red, actual)
            corrupted = []
            for name in actual:
                item = actual.copy()
                del item[name]
                corrupted.append(item)
                item = actual.copy()
                item[name] = actual[name][:-2] if name == 'native.capture.json' else actual[name][:-1]
                corrupted.append(item)
                item = actual.copy()
                item[name] = b''
                corrupted.append(item)
            for key, value in [('exit', 0), ('exit', True), ('exit', 1.0), ('reader_errors', None), ('extra', 1)]:
                item = actual.copy()
                changed = copy.deepcopy(red.capture_report)
                changed[key] = value
                item['native.capture.json'] = json.dumps(changed).encode('utf-8')
                corrupted.append(item)
            for value in (b'{', b'null', b'[]', b'\xff', b'{"exit":1,"exit":1}', b'{"x":NaN}'):
                item = actual.copy()
                item['native.capture.json'] = value
                corrupted.append(item)
            for item in corrupted:
                try:
                    verify_retained(red, item)
                except (ValueError, UnicodeError):
                    retention += 1
                    continue
                raise ValueError('Corrupt returned-red retention accepted')
    before = {'world/level.dat': {'bytes': 3, 'sha256': '1' * 64},
              'world/region/r.0.0.mcr': {'bytes': 12288, 'sha256': '2' * 64},
              'world/preserve.bin': {'bytes': 4, 'sha256': '4' * 64}}
    after = copy.deepcopy(before)
    after['world/session.lock'] = {'bytes': 8, 'sha256': '3' * 64}
    literal_inputs(before, after, {'world/session.lock'}, True)
    literal_inputs(before, before, {'world/session.lock'}, False)
    bad = []
    for name, value in [('world/level.dat', {'bytes': 3, 'sha256': '5' * 64}),
                        ('world/session.lock', {'bytes': 9, 'sha256': '3' * 64}),
                        ('world/session.lock', {'bytes': True, 'sha256': '3' * 64}),
                        ('world/preserve.bin', {'bytes': 4, 'sha256': 'bad'}),
                        ('world/unexpected', {'bytes': 8, 'sha256': '6' * 64})]:
        item = copy.deepcopy(after)
        item[name] = value
        bad.append(item)
    item = copy.deepcopy(after)
    del item['world/level.dat']
    bad.append(item)
    bad.append(before)
    for item in bad:
        try:
            literal_inputs(before, item, {'world/session.lock'}, True)
        except ValueError:
            literals += 1
            continue
        raise ValueError('Corrupt literal input accepted')
    for data in (b'', b'{}', b'{"selection":"Tile1"}', b'{"selection":"loader1+lookup1"}'):
        try:
            bundle_payload(data)
        except ValueError:
            bundles += 1
            continue
        raise ValueError('Unapproved bundle accepted')
    require((rejected, retention, literals, bundles) == (798, 280, 7, 4),
            'Every declared pure corruption refused')
    roots = literal_root_self_test()
    primary = {'status': 'RUNNING'}
    record_failure(primary, 'initial_failure', ValueError('actual primary failure'))
    original = primary['failure'].copy()
    record_failure(primary, 'terminal_guard_failure', OSError('later guard failure'))
    require(primary['status'] == 'FAIL' and primary['failure'] == original and
            primary['terminal_guard_failure']['type'] == 'OSError', 'Primary failure overwritten')
    print('PASS:', rejected, 'query transport refusals;', retention, 'retained-red payload refusals;',
          literals, 'literal guards;', bundles, 'dependency bundle refusals;', roots,
          'real literal-root refusals;1 primary-failure preservation')



def expected_input_names(bundle, kind, mode):
    """Closed per-kind/mode literal filename contract; no Source outcome model."""
    require(kind in MODES and mode in MODES[kind], 'Unknown literal input kind/mode')
    inputs = bundle['inputs'][kind]
    if kind == 'collision':
        require(type(inputs) is dict and set(inputs) == set(MODES['collision']),
                'Exact collision per-mode input mapping required')
        names = inputs[mode]
    else:
        names = inputs
    expected = ('world/level.dat', 'world/data/idcounts.dat', 'world/region/r.0.0.mcr',
                'world/preserve.bin') if kind in ('loader', 'collision') else (
                    'world/level.dat', 'world/region/r.0.0.mcr', 'world/preserve.bin')
    require(type(names) is list and tuple(names) == expected,
            'Exact canonical literal filenames required for ' + kind + '/' + mode)
    return expected


def input_mapping_self_test():
    four = ['world/level.dat', 'world/data/idcounts.dat', 'world/region/r.0.0.mcr', 'world/preserve.bin']
    three = ['world/level.dat', 'world/region/r.0.0.mcr', 'world/preserve.bin']
    good = {'inputs': {'loader': four, 'lookup': three,
                       'collision': {mode: four.copy() for mode in MODES['collision']}}}
    refused = 0
    for kind, modes in MODES.items():
        for mode in modes:
            require(expected_input_names(good, kind, mode) == tuple(four if kind != 'lookup' else three),
                    'Exact input mapping positive')
            for replacement in ([], four + ['world/extra'], tuple(four),
                                ['world/level.dat', 'world/region/r.0.0.mcr']):
                changed = copy.deepcopy(good)
                if kind == 'collision':
                    changed['inputs'][kind][mode] = replacement
                else:
                    changed['inputs'][kind] = replacement
                try:
                    expected_input_names(changed, kind, mode)
                except ValueError:
                    refused += 1
                    continue
                raise ValueError('Malformed literal filename mapping accepted')
    for replacement in ({}, [], {'foreign': four}, {mode: four for mode in MODES['collision'][:-1]}):
        changed = copy.deepcopy(good)
        changed['inputs']['collision'] = replacement
        try:
            expected_input_names(changed, 'collision', 'domain')
        except ValueError:
            refused += 1
            continue
        raise ValueError('Incomplete/malformed collision mode map accepted')
    before = {name: {'bytes': 2, 'sha256': '1' * 64} for name in four}
    after = copy.deepcopy(before)
    after['world/session.lock'] = {'bytes': 8, 'sha256': '2' * 64}
    literal_inputs(before, after, {'world/session.lock'}, True)
    for mutate in ('remove', 'change'):
        changed = copy.deepcopy(after)
        if mutate == 'remove':
            del changed['world/data/idcounts.dat']
        else:
            changed['world/data/idcounts.dat'] = {'bytes': 2, 'sha256': '3' * 64}
        try:
            literal_inputs(before, changed, {'world/session.lock'}, True)
        except ValueError:
            refused += 1
            continue
        raise ValueError('Removed/changed canonical saved-data input admitted')
    require(refused == 62, 'All per-mode/saved-data literal mapping corruptions refused')
    return refused


def collision_recipe_self_test(project_root):
    """Pure authored-byte/NBT checks only; no filesystem fixture or native child."""
    import gzip
    import struct
    import zlib

    here = Path(__file__).resolve().parent
    root = (project_root if project_root is not None else here.parents[2]).resolve(strict=True)
    bundle = bundle_payload((here / 'world_query_controls_bundle.json').read_bytes())
    checked = 0

    def check(value, message):
        nonlocal checked
        require(value, message)
        checked += 1

    def load(name, path, relative):
        check(pin(path) == bundle['helpers'][relative], 'Exact pure input supplier: ' + relative)
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    saving = load('query_pure_canonical_saving', root / 'tests/tools/world/test_source_saving.py',
                  'tests/tools/world/test_source_saving.py')
    construction = load('query_pure_canonical_construction', root / 'tests/tools/world/test_source_world_construction.py',
                        'tests/tools/world/test_source_world_construction.py')
    recipe = load('query_pure_collision_inputs', here / 'collision_query_inputs.py',
                  'tests/tools/world/collision_query_inputs.py')
    base_info, base_region = saving.seed('empty')

    def unpack(region):
        check(type(region) is bytes and len(region) == 12288, 'Exact independent one-sector literal MCR')
        check(struct.unpack_from('>I', region, 0)[0] == ((2 << 8) | 1), 'Actual selected region descriptor')
        length = struct.unpack_from('>i', region, 8192)[0]
        check(1 < length < 4092 and region[8196] == 2, 'Actual bounded selector2 record')
        decoder = zlib.decompressobj()
        raw = decoder.decompress(region[8197:8196 + length], saving.BOUND + 1)
        check(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
              not decoder.unconsumed_tail, 'One complete bounded literal NBT stream')
        return raw

    base_raw = unpack(bytes(base_region))
    base_level = saving.Nbt(base_raw).root()['Level'][1]
    cell = (8 << 11) | (8 << 7) | 64
    blocks, data = bytearray(32768), bytearray(16384)
    blocks[cell], blocks[cell + 1], data[cell >> 1] = 1, 3, 0xA5
    expected_raw = base_raw
    for key, old, new in (('Blocks', bytes(32768), bytes(blocks)), ('Data', bytes(16384), bytes(data))):
        old_tag = saving.named(7, key, struct.pack('>i', len(old)) + old)
        check(expected_raw.count(old_tag) == 1, 'One independently selected canonical ' + key + ' field')
        expected_raw = expected_raw.replace(old_tag, saving.named(7, key, struct.pack('>i', len(new)) + new), 1)
    for mode in MODES['collision']:
        values = recipe.prepared(mode, saving, construction)
        check(type(values) is dict and set(values) == set(expected_input_names(bundle, 'collision', mode)),
              'Every actual literal input and no extra file: ' + mode)
        check(all(type(value) is bytes for value in values.values()), 'Strict bytes representation: ' + mode)
        check(values['world/data/idcounts.dat'] == construction.raw_ids(), 'Canonical actual saved-data bytes: ' + mode)
        check(values['world/preserve.bin'] == b'collision query unrelated data\0\xff', 'Exact independent preservation sentinel: ' + mode)
        region = values['world/region/r.0.0.mcr']
        raw = unpack(region)
        if mode.startswith('loader-'):
            check(values['world/level.dat'] == gzip.compress(construction.metadata(), mtime=0), 'Canonical malformed World metadata: ' + mode)
            check(region == construction.malformed_region() and raw == b'\1\0\0\7', 'Actual complete non-Compound NBT root: ' + mode)
        else:
            check(values['world/level.dat'] == base_info and region[:8192] == bytes(base_region[:8192]), 'Metadata and region descriptor unchanged: ' + mode)
            check(raw == expected_raw, 'Only exact declared stored array bytes changed: ' + mode)
            level = saving.Nbt(raw).root()['Level'][1]
            check(level['Blocks'] == [7, bytes(blocks)] and level['Data'] == [7, bytes(data)], 'Actual parsed Blocks1/3 and packed Data0xA5: ' + mode)
            check(level['Entities'] == [9, [10, []]] and level['TileEntities'] == [9, [10, []]], 'No authored stored Entity/Tile graph: ' + mode)
            check({k: v for k, v in level.items() if k not in ('Blocks', 'Data')} ==
                  {k: v for k, v in base_level.items() if k not in ('Blocks', 'Data')}, 'Every other actual NBT field unchanged: ' + mode)
    for mode in ('', 'ID-FIRST', 'loader'):
        try:
            recipe.prepared(mode, saving, construction)
        except ValueError:
            checked += 1
            continue
        raise ValueError('Unknown input mode admitted')
    print('PASS:', checked, 'pure collision recipe byte/NBT checks;no bootstrap/native execution')
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--kind', choices=tuple(MODES))
    parser.add_argument('--mode')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--bundle', type=Path)
    parser.add_argument('--project-root', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        mapping_refusals = input_mapping_self_test()
        print('PASS:', mapping_refusals, 'per-mode/saved-data filename refusals')
        collision_recipe_self_test(args.project_root)
        return 0
    if (args.kind not in MODES or args.mode not in MODES[args.kind] or args.executable is None or
            args.work_dir is None or args.bundle is None):
        parser.error('valid kind/mode, executable, work-dir and explicit dependency bundle required')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='query-' + args.kind + '-' + args.mode + '-',
                                dir=args.work_dir.resolve()))
    report = {'status': 'RUNNING', 'kind': args.kind, 'mode': args.mode,
              'fixture_preparation_manifest': SUPPLIERS[args.kind]['manifest'],
              'scope': 'Finite genuine native query controls; external legacy graph contract; '
                       'foreign World/Store, arbitrary caller unwind and full SL OPEN'}
    before = literal_before = None
    root = executable = ownership = fixture_root = None
    launch_attempted = protocol_complete = False
    child = None

    def fail(key, error):
        record_failure(report, key, error)

    def load(name, relative):
        spec = importlib.util.spec_from_file_location(name, root / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def guarded():
        result = ownership.protected(executable)
        for relative, expected in {**bundle['sources'], **bundle['helpers']}.items():
            path = (root / relative).resolve(strict=True)
            require(path.is_relative_to(root), 'Bundle path outside actual source root')
            require(pin(path) == expected, 'Selected source/helper mismatch: ' + relative)
            result[str(path)] = expected
        for path in (Path(__file__).resolve(), args.bundle.resolve(strict=True)):
            result[str(path)] = pin(path)
        return result

    try:
        print(json.dumps({'retained_work': str(out), 'stage': 'PREPARING'}), flush=True)
        root = (args.project_root if args.project_root is not None else
                Path(__file__).resolve().parents[3]).resolve(strict=True)
        metadata = literal_metadata(args.bundle)
        require(stat.S_ISREG(metadata.st_mode) and metadata.st_size <= LIMIT, 'Bundle file type/extent')
        bundle = bundle_payload(args.bundle.read_bytes())
        report['dependency_bundle'] = bundle
        report['bundle_pin'] = pin(args.bundle)
        for relative, expected in bundle['helpers'].items():
            require(pin(root / relative) == expected, 'Frozen actual helper changed: ' + relative)
        ownership = load('query_actual_ownership', 'tests/tools/world/test_execution_ownership.py')
        require(stat.S_ISREG(literal_metadata(args.executable).st_mode), 'Executable is not a regular file')
        executable = args.executable.resolve(strict=True)
        before = guarded()
        report['guard_before'] = before
        require(tuple(ownership.TABLES) == ('application_owner', 'biped', 'render_resources',
                'display_sdk', 'loading_draw', 'resource_lookup_common'), 'Expected exact six current tables')
        report['tables'] = list(ownership.TABLES)
        support = load('query_actual_capture', 'tests/reference/client/statistics/support.py')
        source_saving = load('query_actual_seed', 'tests/tools/world/test_source_saving.py')
        if args.kind == 'collision':
            recipe = load('query_actual_collision_recipe', 'tests/tools/world/collision_query_inputs.py')
            construction = load('query_actual_construction_seed', 'tests/tools/world/test_source_world_construction.py')
        else:
            recipe = load('query_actual_recipe', 'tests/tools/world/world_query_inputs.py')
        fixture_root = out / 'owned'
        require(str(fixture_root).isascii() and not fixture_root.exists(), 'Fresh ASCII native fixture root required')
        input_names = expected_input_names(bundle, args.kind, args.mode)
        prepared = recipe.prepared(args.mode, source_saving, construction) if args.kind == 'collision' else (
            recipe.prepared(args.kind, args.mode, source_saving))
        require(type(prepared) is dict and set(prepared) == set(input_names),
                'Exact authored input names required')
        (fixture_root / 'world/region').mkdir(parents=True)
        (fixture_root / 'snapshots').mkdir()
        for relative, value in prepared.items():
            require(type(value) is bytes and len(value) <= LIMIT, 'Literal authored input extent/type')
            path = fixture_root / relative
            require(path.resolve().is_relative_to(fixture_root.resolve()), 'Literal input outside owned root')
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open('xb') as stream:
                stream.write(value)
        literal_before = fixture_files(fixture_root)
        report['literal_before'] = literal_before
        require(set(literal_before) == set(input_names), 'Unexpected initial fixture file set')
        command = [str(executable), args.mode, str(fixture_root)]
        report['command'] = command
        launch_attempted = True
        child = support.run_bounded(command, out, 'native', timeout=TIMEOUT, limit=LIMIT,
                                    env=os.environ.copy())
        report['capture'] = child.capture_report
        report['checks'] = completed(child, args.kind, args.mode, command)
        protocol_complete = True
        report['status'] = 'PASS'
    except BaseException as error:
        fail('initial_failure', error)
        if getattr(error, 'capture_report', None) is not None:
            report['failed_capture'] = error.capture_report
    finally:
        report.update(native_launch_attempted=launch_attempted, native_child_returned=child is not None,
                      native_protocol_complete=protocol_complete)
        report['guards_unchanged'] = False
        if before is not None:
            try:
                after = guarded()
                report['guard_after'] = after
                require(after == before, 'Source/product/helper/table/bundle set changed')
                report['guards_unchanged'] = True
            except BaseException as error:
                fail('terminal_guard_failure', error)
        else:
            report['status'] = 'FAIL'
        if literal_before is not None:
            try:
                terminal_literal_guard(report, fixture_root, literal_before, protocol_complete)
            except BaseException as error:
                fail('literal_guard_failure', error)
        elif fixture_root is not None:
            report['literal_snapshot_not_established'] = True
            try:
                # Setup already failed: retain partial files without promoting it to PASS.
                report['literal_partial_after'] = fixture_files(fixture_root)
            except BaseException as error:
                fail('literal_partial_guard_failure', error)
        byte_map = {}
        capture_files = {}
        for leaf in ('native.stdout', 'native.stderr', 'native.capture.json'):
            path = out / leaf
            try:
                if not path.exists():
                    capture_files[leaf] = {'missing': True}
                    continue
                metadata = literal_metadata(path)
                require(stat.S_ISREG(metadata.st_mode) and metadata.st_size <= LIMIT,
                        'Retained file type/extent: ' + leaf)
                data = path.read_bytes()
                expected = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                require(pin(path) == expected, 'Capture changed during terminal read')
                byte_map[leaf] = data
                capture_files[leaf] = expected
            except BaseException as error:
                capture_files[leaf] = {'failure': {'type': type(error).__name__, 'message': str(error)}}
                if child is not None:
                    fail('retention_read_failure', error)
        report['capture_files'] = capture_files
        if child is not None:
            try:
                verify_retained(child, byte_map)
                report['retained_capture_verified'] = True
            except BaseException as error:
                fail('retention_failure', error)
                report['retained_capture_verified'] = False
        else:
            report['retained_capture_verified'] = None
            report['retention_scope'] = ('No returned child; partial files retained/pinned; '
                                         'original preflight/spawn/capture failure remains FAIL')
        with (out / 'report.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(report, indent=2) + '\n')
    if report['status'] != 'PASS':
        print('FAIL: actual World query control; retained', out / 'report.json', file=sys.stderr)
        return 1
    print('PASS:', report['checks'], 'actual query checks;kind=' + args.kind + ';mode=' + args.mode +
          ';retained', out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
