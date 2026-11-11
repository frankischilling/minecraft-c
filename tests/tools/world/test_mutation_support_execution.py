"""Retain real boundary-Ladder support refusal controls and bounded receipts."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
TIMEOUT = 180
MODES = ('descendant', 'source-descendant')
RECEIPT_KEYS = {'command', 'exit', 'timeout_seconds', 'timed_out', 'pipe_capture_complete',
                'reader_errors', 'output_overflow', 'stdout_file', 'stderr_file', 'stdout_bytes',
                'stderr_bytes', 'stdout_sha256', 'stderr_sha256', 'stdout_limit', 'stderr_limit'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def pin(path):
    require(path.is_file() and not path.is_symlink(), 'Regular literal input file required')
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepared():
    """Author one real source Chunk at (1,0); the native root is generated."""
    saving = module('mutation_support_source_inputs', ROOT / 'tests/tools/world/test_source_saving.py')
    construction = module('mutation_support_source_construction',
                          ROOT / 'tests/tools/world/test_source_world_construction.py')
    info, image = saving.seed('empty')
    require(len(image) == 12288 and struct.unpack_from('>I', image)[0] == (2 << 8) | 1,
            'Canonical empty seed image')
    length = struct.unpack_from('>i', image, 8192)[0]
    require(1 < length < 4092 and image[8196] == 2, 'Canonical selector2 record')
    decoder = zlib.decompressobj()
    raw = decoder.decompress(image[8197:8196 + length], saving.BOUND + 1)
    require(len(raw) <= saving.BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, 'Complete bounded seed decompression')
    original = saving.Nbt(raw).root()
    item = saving.item()
    old_pos = saving.named(9, 'Pos', b'\6' + struct.pack('>i3d', 3, 8.25, 64., 8.25))
    new_pos = saving.named(9, 'Pos', b'\6' + struct.pack('>i3d', 3, 24.25, 64., 8.25))
    require(item.count(old_pos) == 1, 'One declared actual Item position')
    item = item.replace(old_pos, new_pos, 1)
    replacements = ((saving.named(3, 'xPos', bytes(4)), saving.named(3, 'xPos', struct.pack('>i', 1))),
                    (saving.named(9, 'Entities', b'\12' + bytes(4)),
                     saving.named(9, 'Entities', b'\12' + struct.pack('>i', 1) + item)))
    for before, after in replacements:
        require(raw.count(before) == 1, 'Exactly one declared stored tag')
        raw = raw.replace(before, after, 1)
    changed = saving.Nbt(raw).root()
    old_level, level = original['Level'][1], changed['Level'][1]
    require(level['xPos'] == [3, 1] and level['zPos'] == [3, 0] and
            level['Entities'][0] == 9 and level['Entities'][1][0] == 10 and
            len(level['Entities'][1][1]) == 1 and level['TileEntities'] == [9, [10, []]] and
            {k: v for k, v in level.items() if k not in ('xPos', 'Entities')} ==
            {k: v for k, v in old_level.items() if k not in ('xPos', 'Entities')} and
            {k: v for k, v in original.items() if k != 'Level'} ==
            {k: v for k, v in changed.items() if k != 'Level'},
            'Only declared xPos and genuine Item list changed')
    actual = level['Entities'][1][1][0]
    require(actual['id'] == [8, 'Item'] and
            actual['Pos'] == [9, [6, [struct.unpack('>Q', struct.pack('>d', value))[0]
                                      for value in (24.25, 64., 8.25)]]] and
            actual['Item'][1]['id'] == [2, 262] and actual['Item'][1]['Count'] == [1, 5] and
            actual['Item'][1]['Damage'] == [2, 7], 'Literal cold Chunk actual Item operands')
    compressed = zlib.compress(raw)
    require(len(compressed) + 5 < 4096, 'One-sector source record bound')
    region = bytearray(12288)
    struct.pack_into('>I', region, 4, (2 << 8) | 1)
    struct.pack_into('>i', region, 4096 + 4, 123)
    record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
    region[8192:8192 + len(record)] = record
    require(struct.unpack_from('>I', region)[0] == 0 and
            struct.unpack_from('>I', region, 4)[0] == (2 << 8) | 1 and
            not any(region[8:4096]), 'Exactly source slot1; generated root absent')
    return {'world/level.dat': bytes(info), 'world/region/r.0.0.mcr': bytes(region),
            'world/data/idcounts.dat': construction.raw_ids(),
            'world/preserve.bin': b'owned mutation support unrelated input\0\xff'}


def result_line(mode):
    raw = 'IO_ERROR' if mode == 'source-descendant' else 'BUSY'
    return (f'MUTATION_SUPPORT_RESULT mode={mode} raw={raw} support_raw={raw} native=BUSY '
            'callbacks=1 ladder=65 metadata=4 retained=1 drained=1 replay=0')


def validate_receipt(receipt, command, output, mode):
    require(type(receipt) is dict and set(receipt) == RECEIPT_KEYS, 'Closed process receipt')
    require(type(receipt['command']) is list and receipt['command'] == command and
            all(type(word) is str for word in command), 'Actual process command')
    require(type(receipt['exit']) is int and receipt['exit'] == 0 and
            type(receipt['timeout_seconds']) is int and receipt['timeout_seconds'] == TIMEOUT and
            receipt['timed_out'] is False and receipt['pipe_capture_complete'] is True and
            receipt['output_overflow'] is False and type(receipt['reader_errors']) is dict and
            receipt['reader_errors'] == {}, 'Complete successful bounded process')
    streams = {}
    for name, limit in (('stdout', 4 * 1024 * 1024), ('stderr', 1024 * 1024)):
        filename = mode + '.' + name
        path = output / filename
        require(receipt[name + '_file'] == filename and path.is_file() and not path.is_symlink(),
                'Literal captured stream file')
        require(type(receipt[name + '_limit']) is int and receipt[name + '_limit'] == limit and
                type(receipt[name + '_bytes']) is int and
                0 <= receipt[name + '_bytes'] <= limit and
                receipt[name + '_bytes'] == path.stat().st_size and
                receipt[name + '_sha256'] == pin(path), 'Actual captured extent/hash')
        streams[name] = path.read_bytes()
    require(streams['stderr'] == b'' and streams['stdout'].endswith(b'\n'), 'Native output completion')
    stdout = streams['stdout'].replace(b'\r\n', b'\n')
    require(b'\r' not in stdout, 'Native output newlines')
    lines = stdout.decode('ascii').splitlines()
    require(len(lines) == 6 and lines[:4] == [f'MUTATION_SUPPORT_BEGIN schema=1 mode={mode}',
            '16 achievements', '151 recipes', result_line(mode)] and
            lines[4] == 'MUTATION_SUPPORT_OPEN finite-native-boundary/full-wake/Arrow',
            'Exact finite mode and control sequence')
    footer = re.fullmatch(r'PASS: ([1-9][0-9]{0,5}) actual owned mutation support checks;mode=' +
                          re.escape(mode), lines[5])
    require(footer is not None, 'Native terminal check count')
    return int(footer[1])


def self_test():
    inputs = prepared()
    require(set(inputs) == {'world/level.dat', 'world/region/r.0.0.mcr',
                            'world/data/idcounts.dat', 'world/preserve.bin'} and
            all(type(value) is bytes and value for value in inputs.values()), 'Closed authored inputs')
    with tempfile.TemporaryDirectory(prefix='mutation-support-schema-') as work:
        output = Path(work)
        mode = MODES[0]
        stdout = ('\n'.join([f'MUTATION_SUPPORT_BEGIN schema=1 mode={mode}', '16 achievements',
            '151 recipes', result_line(mode), 'MUTATION_SUPPORT_OPEN finite-native-boundary/full-wake/Arrow',
            f'PASS: 1 actual owned mutation support checks;mode={mode}']) + '\n').encode('ascii')
        (output / (mode + '.stdout')).write_bytes(stdout)
        (output / (mode + '.stderr')).write_bytes(b'')
        command = ['synthetic-native', mode, 'synthetic-input']
        receipt = {'command': command, 'exit': 0, 'timeout_seconds': TIMEOUT, 'timed_out': False,
            'pipe_capture_complete': True, 'reader_errors': {}, 'output_overflow': False,
            'stdout_file': mode + '.stdout', 'stderr_file': mode + '.stderr',
            'stdout_bytes': len(stdout), 'stderr_bytes': 0, 'stdout_sha256': pin(output / (mode + '.stdout')),
            'stderr_sha256': pin(output / (mode + '.stderr')), 'stdout_limit': 4 * 1024 * 1024,
            'stderr_limit': 1024 * 1024}
        require(validate_receipt(receipt, command, output, mode) == 1, 'Synthetic receipt positive')
        refused = 0
        for key, value in (('extra', 1), ('exit', False), ('exit', 1), ('timed_out', True),
                ('pipe_capture_complete', False), ('output_overflow', True),
                ('reader_errors', {'stdout': ['lost']}), ('stdout_bytes', False),
                ('stdout_sha256', '0' * 64), ('stdout_file', '../outside'), ('command', ['other'])):
            bad = copy.deepcopy(receipt)
            bad[key] = value
            try:
                validate_receipt(bad, command, output, mode)
            except ValueError:
                refused += 1
            else:
                raise ValueError('Malformed synthetic process receipt accepted: ' + key)
        require(refused == 11, 'Every malformed synthetic receipt refused')
    print('PASS: authored slot1 Item input; 11 malformed synthetic receipts refused; no native run')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--work-dir', type=Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    require(args.executable is not None and args.work_dir is not None, 'Executable and work directory required')
    executable = args.executable.resolve(strict=True)
    require(executable.is_file() and not executable.is_symlink(), 'Literal native executable')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='mutation-support-', dir=args.work_dir.resolve()))
    capture_path = ROOT / 'tests/reference/client/controller/capture.py'
    capture = module('mutation_support_bounded_capture', capture_path)
    report = {'schema': 'MUTATION_SUPPORT_EXECUTION_RUN_V1', 'status': 'FAIL', 'modes': {},
        'scope': 'Two native boundary controls with actual SourceSaving Item activation and opaque mutation calls. '
                 'No original comparison, terrain-generation, full wake or Arrow qualification.'}
    before = None
    paths = []
    try:
        for mode in MODES:
            for relative, data in prepared().items():
                path = output / mode / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        paths = [executable, Path(__file__).resolve(), capture_path,
            ROOT / 'tests/world/runtime/world_access/test_mutation_support_execution.c',
            ROOT / 'tests/world/runtime/world_access/test_block_scalar_execution.c',
            ROOT / 'tests/world/runtime/world_access/test_collision_query_execution.c',
            ROOT / 'tests/world/runtime/tile_entities/test_tile_entities.c',
            ROOT / 'tests/tools/world/test_source_saving.py',
            ROOT / 'tests/tools/world/test_source_world_construction.py',
            ROOT / 'tests/tools/world/collision_query_inputs.py']
        paths.extend(path for mode in MODES for path in (output / mode).rglob('*') if path.is_file())
        for folder in ('src', 'include'):
            paths.extend(path for path in (ROOT / folder).rglob('*') if path.suffix in ('.c', '.h', '.inc'))
        paths = sorted(set(paths))
        before = {str(path): pin(path) for path in paths}
        report['input_before_sha256'] = before
        for mode in MODES:
            require(before == {str(path): pin(path) for path in paths}, 'Inputs unchanged before native entry')
            command = [str(executable), mode, str(output / mode)]
            receipt = capture.capture(command, output, mode, TIMEOUT)
            report['modes'][mode] = {'process': receipt}
            checks = validate_receipt(receipt, command, output, mode)
            report['modes'][mode]['checks'] = checks
            report['modes'][mode]['receipt_sha256'] = pin(output / (mode + '.capture.json'))
        report['status'] = 'PASS_FINITE_NATIVE_CONTROLS_ONLY'
    except Exception as error:
        report['failure'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        report['input_after_sha256'] = {str(path): pin(path) for path in paths}
        report['inputs_unchanged'] = before is not None and before == report['input_after_sha256']
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'run.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if before is not None:
            require(report['inputs_unchanged'], 'Executed input preservation')
    print('PASS: 2 actual owned mutation support modes; capture=' + str(output))


if __name__ == '__main__':
    main()
