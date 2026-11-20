"""Ordinary setter diagnostics on authored Source Worlds; one call per process."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import types
import zlib

sys.dont_write_bytecode = True
BOUND = 1024 * 1024
APIS = ('block', 'block-meta', 'notify', 'notify-meta', 'metadata')
CASES = tuple((api, scene, poison) for api in APIS
              for scene in ('equal', 'changed', 'outside') for poison in ('a5', '5a', 'null')) + tuple(
    ('metadata-observe', scene, poison) for scene in ('equal', 'changed')
    for poison in ('a5', '5a', 'null'))
FINISH_KEYS = {'api', 'scene', 'poison', 'checks', 'physical_calls',
               'canonical_checked', 'cleanup', 't1_restored', 'layout'}
CAPTURE_KEYS = {'command', 'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete',
                'reader_errors', 'output_overflow', 'stdout_file', 'stderr_file',
                'stdout_bytes', 'stderr_bytes', 'stdout_sha256', 'stderr_sha256',
                'stdout_limit', 'stderr_limit'}


def require(value, message):
    if not value:
        raise AssertionError(message)


def regular(path):
    require(not path.is_symlink() and not getattr(path, 'is_junction', lambda: False)(),
            'no linked input or evidence path: ' + str(path))


def read_stable(path, bound):
    regular(path)
    with path.open('rb') as stream:
        first = stream.read(bound + 1)
    require(len(first) <= bound, 'bounded file: ' + str(path))
    with path.open('rb') as stream:
        second = stream.read(bound + 1)
    require(first == second, 'stable file: ' + str(path))
    return first


def pin(path):
    regular(path)

    def one():
        digest = hashlib.sha256()
        size = 0
        with path.open('rb') as stream:
            for part in iter(lambda: stream.read(65536), b''):
                digest.update(part)
                size += len(part)
        return dict(bytes=size, sha256=digest.hexdigest())

    first = one()
    require(first == one(), 'stable input fingerprint: ' + str(path))
    return first


def input_inventory(paths):
    result = {}
    for path in paths:
        try:
            result[str(path)] = pin(path)
        except BaseException as error:
            result[str(path)] = dict(error=repr(error))
    return result


def source_module(path, expected, name):
    """Execute the stable, hash-bound source; never read a cached bytecode file."""
    raw = read_stable(path, BOUND)
    require(dict(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()) == expected,
            'imported source matches its initial fingerprint')
    module = types.ModuleType(name)
    module.__file__ = str(path)
    sys.modules[name] = module
    try:
        exec(compile(raw, str(path), 'exec'), module.__dict__)
    except BaseException:
        sys.modules.pop(name, None)
        raise
    return module


def observe_world(world):
    """Retain bounded partial files too, including when authoring/native execution failed."""
    result = dict(exists=None, directories=[], files={}, errors=[])
    try:
        regular(world)
        result['exists'] = world.exists()
        if not result['exists']:
            return result
        require(world.is_dir(), 'World root is a directory')
        pending = [world]
        count = 0
        while pending:
            directory = pending.pop()
            with os.scandir(directory) as entries:
                for entry in entries:
                    count += 1
                    require(count <= 8, 'bounded authored World inventory')
                    path = Path(entry.path)
                    relative = path.relative_to(world).as_posix()
                    regular(path)
                    if path.is_dir():
                        result['directories'].append(relative)
                        pending.append(path)
                    else:
                        require(path.is_file(), 'ordinary World file')
                        raw = read_stable(path, BOUND)
                        result['files'][relative] = dict(bytes=len(raw),
                            sha256=hashlib.sha256(raw).hexdigest())
        result['directories'].sort()
    except BaseException as error:
        result['errors'].append(repr(error))
    return result


def checked_world(observation, constructed=False):
    directories = ['data', 'region'] if constructed else ['region']
    require(observation['exists'] and not observation['errors'] and
            observation['directories'] == directories, 'complete ordinary authored World inventory')
    return observation['files']


def observe_capture(directory):
    result = dict(exists=None, files={}, errors=[])
    try:
        regular(directory)
        result['exists'] = directory.exists()
        if not result['exists']:
            return result
        with os.scandir(directory) as entries:
            for index, entry in enumerate(entries):
                require(index < 3 and entry.name in
                        ('native.stdout', 'native.stderr', 'native.capture.json'),
                        'bounded closed retained capture inventory')
                path = Path(entry.path)
                require(path.is_file(), 'ordinary capture file')
                raw = read_stable(path, 4 * BOUND)
                result['files'][entry.name] = dict(bytes=len(raw),
                    sha256=hashlib.sha256(raw).hexdigest())
    except BaseException as error:
        result['errors'].append(repr(error))
    return result


def author(root, api, scene, saving):
    info, image = saving.seed('empty')
    require(len(image) == 12288 and image[8196] == 2, 'canonical authored one-sector seed')
    length = struct.unpack_from('>i', image, 8192)[0]
    require(1 < length <= 4092, 'bounded canonical record')
    decoder = zlib.decompressobj()
    wire = decoder.decompress(image[8197:8196 + length], BOUND + 1)
    require(len(wire) <= BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, 'bounded complete canonical NBT')
    expected = copy.deepcopy(saving.Nbt(wire).root())
    metadata = api.startswith('metadata')
    air = not metadata and scene != 'equal'
    cell = (8 * 16 + 8) * 128 + 64
    planes = {'Blocks': bytearray(32768), 'Data': bytearray(16384),
              'SkyLight': bytearray(16384), 'BlockLight': bytearray(16384),
              'HeightMap': bytearray([64]) * 256}
    if not air:
        planes['Blocks'][cell] = 1
        planes['HeightMap'][136] = 65
        if scene == 'equal':
            planes['Data'][cell // 2] = 3
    for name, array in planes.items():
        old = bytes([64 if name == 'HeightMap' else 0]) * len(array)
        old_tag = saving.named(7, name, struct.pack('>i', len(old)) + old)
        require(wire.count(old_tag) == 1, 'one canonical plane ' + name)
        value = bytes(array)
        wire = wire.replace(old_tag, saving.named(7, name, struct.pack('>i', len(value)) + value), 1)
        expected['Level'][1][name] = [7, value]
    require(saving.Nbt(wire).root() == expected, 'complete authored NBT field continuity')
    compressed = zlib.compress(wire)
    require(len(compressed) + 5 <= 4096, 'one-sector authored record')
    prepared = bytearray(12288)
    struct.pack_into('>I', prepared, 0, (2 << 8) | 1)
    struct.pack_into('>i', prepared, 4096, 123)
    record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
    prepared[8192:8192 + len(record)] = record
    (root / 'world/region').mkdir(parents=True)
    (root / 'world/level.dat').write_bytes(info)
    (root / 'world/region/r.0.0.mcr').write_bytes(prepared)
    return checked_world(observe_world(root / 'world'))


def strict_json(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'unique JSON keys')
            result[key] = value
        return result

    def invalid(value):
        raise AssertionError('noninteger/nonfinite JSON number: ' + value)

    return json.loads(raw, object_pairs_hook=pairs, parse_constant=invalid, parse_float=invalid)


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def parse_native(raw, api, scene, poison):
    require((api, scene, poison) in CASES, 'declared native case')
    require(type(raw) is bytes and len(raw) <= BOUND and b'\0' not in raw,
            'bounded native transport')
    normalized = raw.replace(b'\r\n', b'\n')
    require(b'\r' not in normalized, 'LF or CRLF native transport')
    lines = normalized.decode('ascii', 'strict').split('\n')
    prefix = 'ORDINARY_PROJECTION_FINISH '
    require(len(lines) == 4 and lines[:2] == ['16 achievements', '151 recipes'] and
            lines[2].startswith(prefix) and lines[3] == '',
            'exact two bootstrap lines and one terminal JSON receipt')
    finish = strict_json(lines[2][len(prefix):])
    require(type(finish) is dict and set(finish) == FINISH_KEYS,
            'closed native receipt fields')
    require(all(type(finish[key]) is str for key in ('api', 'scene', 'poison')) and
            (finish['api'], finish['scene'], finish['poison']) == (api, scene, poison),
            'actual native operands')
    require(integer(finish['checks'], 21, (1 << 32) - 1), 'actual positive check count')
    for key, expected in [('physical_calls', 1), ('canonical_checked', int(poison != 'null')),
                          ('cleanup', 1), ('t1_restored', 1)]:
        require(type(finish[key]) is int and finish[key] == expected,
                'actual integer native receipt flag: ' + key)
    layout = finish['layout']
    require(type(layout) is list and len(layout) == 4 and
            all(integer(value, 0, (1 << 64) - 1) for value in layout) and
            layout[0] == sum(layout[1:]) and layout[1] > 0,
            'compiler-derived exhaustive V3 classification; no fixed ABI fingerprint')
    return finish


def validate_capture(receipt, directory, command, bounded_capture):
    require(type(receipt) is dict and set(receipt) == CAPTURE_KEYS, 'closed capture receipt')
    require(receipt['command'] == command and type(receipt['command']) is list and
            all(type(value) is str for value in receipt['command']), 'actual captured command')
    require(type(receipt['exit']) is int and receipt['exit'] == 0 and
            type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == 25 and
            receipt['timed_out'] is False and receipt['output_overflow'] is False and
            receipt['pipe_capture_complete'] is True and receipt['reader_errors'] == {},
            'successful complete bounded native capture')
    streams = {}
    for name, limit in [('stdout', bounded_capture.STDOUT_LIMIT),
                        ('stderr', bounded_capture.STDERR_LIMIT)]:
        require(type(receipt[name + '_limit']) is int and receipt[name + '_limit'] == limit and
                integer(receipt[name + '_bytes'], 0, limit) and
                receipt[name + '_file'] == 'native.' + name,
                'exact bounded stream receipt: ' + name)
        raw = read_stable(directory / ('native.' + name), limit)
        require(len(raw) == receipt[name + '_bytes'] and
                type(receipt[name + '_sha256']) is str and
                hashlib.sha256(raw).hexdigest() == receipt[name + '_sha256'],
                'retained raw stream matches capture receipt: ' + name)
        streams[name] = raw
    require(streams['stderr'] == b'', 'empty actual native stderr')
    stored = strict_json(read_stable(directory / 'native.capture.json', BOUND).decode('utf-8'))
    require(type(stored) is dict and stored == receipt and
            all(type(stored[key]) is type(receipt[key]) for key in receipt),
            'retained capture receipt matches actual helper result and field types')
    bounded_capture.require_success(receipt, empty_stderr=True)
    return streams['stdout']


def native_root(requested, report):
    """Keep native Region paths short and ASCII independently of artifact/repository paths."""
    candidates = [requested] if requested is not None else [Path(tempfile.gettempdir())]
    if requested is None:
        candidates += ([Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'Temp',
                        Path(os.environ.get('PUBLIC', 'C:/Users/Public'))]
                       if os.name == 'nt' else [Path('/tmp')])
    report['native_temp_attempts'] = []
    for base in dict.fromkeys(candidates):
        attempt = dict(base=str(base))
        report['native_temp_attempts'].append(attempt)
        try:
            base = base.resolve(strict=True)
            regular(base)
            require(base.is_dir(), 'existing native temporary directory')
            base.as_posix().encode('ascii', 'strict')
            # Include the longest case/Region path and space for native temporary suffixes.
            require(len(str(base / 'op-12345678/50/world/region/r.0.0.mcr')) + 40 < 240,
                    'short native Region path budget')
            selected = Path(tempfile.mkdtemp(prefix='op-', dir=base))
            attempt.update(status='SELECTED', root=str(selected))
            return selected
        except (OSError, ValueError, UnicodeError, AssertionError) as error:
            attempt.update(status='UNUSABLE_BEFORE_NATIVE', error=repr(error))
    raise AssertionError('no writable short ASCII native temporary root; use --native-temp-dir')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--native-temp-dir', type=Path,
                        help='existing short ASCII temporary directory (native Worlds are retained)')
    args = parser.parse_args()
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='projection-', dir=args.work_dir.resolve()))
    report = dict(schema='ORDINARY_SETTER_PROJECTION_CONTROLS_V2', status='FAIL',
                  cases=[], no_retries=True, retained=str(output))
    inputs = []
    failure = None
    before = {}
    try:
        source = args.source_root.resolve(strict=True)
        executable = args.executable.resolve(strict=True)
        helper = source / 'tests/tools/world/test_source_saving.py'
        capture_helper = source / 'tests/reference/client/controller/capture.py'
        inputs = [Path(__file__).resolve(), executable, helper, capture_helper,
                  source / 'tests/world/runtime/test_ordinary_projection.c',
                  source / 'tests/benchmarks/quiet_setter_diagnostics.h']
        before = input_inventory(inputs)
        report['inputs_before'] = before
        require(all('error' not in value for value in before.values()), 'readable initial guarded inputs')
        saving = source_module(helper, before[str(helper)], 'ordinary_projection_authored_source')
        bounded_capture = source_module(capture_helper, before[str(capture_helper)],
                                        'ordinary_projection_bounded_capture')
        require(len(CASES) == len(set(CASES)) == 51, 'exact finite fresh case set')
        worlds = native_root(args.native_temp_dir, report)
        report['native_worlds'] = str(worlds)
        report['planned_cases'] = [dict(ordinal=i, api=api, scene=scene, poison=poison)
                                   for i, (api, scene, poison) in enumerate(CASES)]
        for ordinal, (api, scene, poison) in enumerate(CASES):
            root = worlds / f'{ordinal:02d}'
            artifacts = output / f'{ordinal:02d}'
            row = dict(ordinal=ordinal, api=api, scene=scene, poison=poison,
                       status='FAIL', native_root=str(root), artifacts=str(artifacts))
            report['cases'].append(row)
            case_failure = None
            try:
                artifacts.mkdir()
                root.mkdir()
                row['authored'] = author(root, api, scene, saving)
                command = [str(executable), str(root).encode('utf-8').hex(), api, scene, poison]
                row['command'] = command
                receipt = bounded_capture.capture(command, artifacts, 'native', timeout=25,
                                                  working_directory=root)
                row['capture'] = receipt
                raw = validate_capture(receipt, artifacts, command, bounded_capture)
                row['finish'] = parse_native(raw, api, scene, poison)
            except BaseException as error:
                row['error'] = repr(error)
                case_failure = error
            finally:
                row['world_after'] = observe_world(root / 'world')
                row['capture_files_after'] = observe_capture(artifacts)
                try:
                    observed_capture = row['capture_files_after']
                    require(observed_capture['exists'] and not observed_capture['errors'] and
                            set(observed_capture['files']) ==
                            {'native.stdout', 'native.stderr', 'native.capture.json'},
                            'complete final retained capture inventory')
                    after = checked_world(row['world_after'], constructed=True)
                    authored = row.get('authored')
                    require(authored is not None and set(after) == set(authored) | {'session.lock'} and
                            all(after[name] == value for name, value in authored.items()) and
                            after['session.lock']['bytes'] == 8,
                            'authored files unchanged except actual eight-byte constructor token')
                except BaseException as error:
                    row['inventory_error'] = repr(error)
                    if case_failure is None:
                        case_failure = error
                if case_failure is None:
                    row['status'] = 'PASS'
            if case_failure is not None:
                raise case_failure
        require(len(report['cases']) == 51, 'all physical cases completed without retry')
        report['status'] = 'PASS_51_ORDINARY_SETTER_CASES'
    except BaseException as error:
        failure = error
        report['error'] = repr(error)
    finally:
        after = input_inventory(inputs)
        report['inputs_after'] = after
        report['inputs_unchanged'] = bool(before) and after == before and all(
            'error' not in value for value in after.values())
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
            report['input_guard_error'] = 'controller/helper/product inputs missing, unreadable, or changed'
            if failure is None:
                failure = AssertionError(report['input_guard_error'])
        (output / 'report.json').write_text(json.dumps(report, indent=2, allow_nan=False) + '\n',
                                          encoding='utf-8')
    print(json.dumps(dict(status=report['status'], cases=len(report['cases']), retained=str(output))))
    if failure is not None:
        raise failure


if __name__ == '__main__':
    main()
