"""Real source Store identity-index lifecycle; no original or performance equivalence."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True
BOUND = 1048576
MODES = ('indexed', 'linear', 'indexed-bulk')


def require(value, message):
    if not value:
        raise AssertionError(message)


def read(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= BOUND,
            'bounded nonsymlink regular evidence')
    with path.open('rb') as source:
        value = source.read(BOUND + 1)
    require(len(value) <= BOUND, 'evidence growth bound')
    return value


def digest(path):
    require(path.is_file() and not path.is_symlink(), 'regular helper/product')
    result, total = hashlib.sha256(), 0
    with path.open('rb') as source:
        for block in iter(lambda: source.read(BOUND), b''):
            total += len(block)
            require(total <= 256 * BOUND, 'bounded helper/native product')
            result.update(block)
    return {'bytes': total, 'sha256': result.hexdigest()}


def inventory(root):
    pending, result, entries, total = [(root, 0)], {}, 0, 0
    while pending:
        directory, depth = pending.pop()
        require(depth <= 3, 'bounded disposable World depth')
        for path in sorted(directory.iterdir()):
            entries += 1
            require(entries <= 1024 and not path.is_symlink(), 'bounded nonsymlink World inventory')
            if path.is_dir():
                pending.append((path, depth + 1))
            else:
                value = read(path)
                total += len(value)
                require(total <= 16 * BOUND, 'bounded World aggregate')
                result[path.relative_to(root).as_posix()] = {
                    'bytes': len(value), 'sha256': hashlib.sha256(value).hexdigest()}
    return dict(sorted(result.items()))


def authored_inputs(parser, root):
    directory = root / 'world'
    region = directory / 'region'
    region.mkdir(parents=True)
    metadata, seed = parser.seed('empty')
    (directory / 'level.dat').write_bytes(metadata)
    size = struct.unpack_from('>I', seed, 8192)[0]
    require(1 < size < 4092 and seed[8196] == 2, 'canonical source seed selector2 extent')
    decoder = zlib.decompressobj()
    template = decoder.decompress(seed[8197:8196 + size], BOUND + 1)
    require(len(template) <= BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, 'bounded complete authored source seed')
    records = []
    for slot, stored_x in [(0, 0), (1, 2)]:
        # The second real record deliberately disagrees with its requested slot.
        # Actual LD import reaches its relocation dependency; no failure flag is set.
        old = parser.named(3, 'xPos', struct.pack('>i', 0))
        require(template.count(old) == 1, 'one exact authored xPos field')
        raw = template.replace(old, parser.named(3, 'xPos', struct.pack('>i', stored_x)), 1)
        decoded = parser.Nbt(raw).root()
        level = decoded['Level'][1]
        require(decoded['Level'][0] == 10 and level['xPos'] == [3, stored_x] and
                level['zPos'] == [3, 0] and level['Entities'] == [9, [10, []]] and
                level['TileEntities'] == [9, [10, []]], 'exact authored coordinates and empty typed Lists')
        packed = zlib.compress(raw)
        require(len(packed) + 5 <= 4096, 'authored record allocation')
        records.append((slot, packed))
    image = bytearray(4096 * 4)
    for sector, (slot, packed) in enumerate(records, 2):
        struct.pack_into('>I', image, slot * 4, (sector << 8) | 1)
        struct.pack_into('>i', image, 4096 + slot * 4, 123)
        encoded = struct.pack('>I', len(packed) + 1) + b'\2' + packed
        image[sector * 4096:sector * 4096 + len(encoded)] = encoded
    (region / 'r.0.0.mcr').write_bytes(image)
    return inventory(directory)


def row(stdout, prefix):
    found = [line for line in stdout.splitlines() if line.startswith(prefix + ' ')]
    require(len(found) == 1, 'one actual native ' + prefix + ' receipt')
    pairs = [part.split('=', 1) for part in found[0].split()[1:]]
    require(all(len(pair) == 2 for pair in pairs) and len(dict(pairs)) == len(pairs),
            'unique complete native receipt fields')
    return dict(pairs)


def native(command, root, record, capture):
    """Persist the primary process outcome before any fallible World snapshot."""
    first = time.time_ns()
    record.update(command=command, started_ns=first)
    try:
        result = capture.run_bounded(command, root, 'native', timeout=90, limit=BOUND)
    except BaseException as error:
        actual = getattr(error, 'capture_report', {})
        kind = 'timeout' if actual.get('timed_out') else 'capture-error'
        if actual.get('exit') not in (None, 0) and kind != 'timeout':
            kind = 'native-nonzero'
        if any(item.get('phase') == 'open' for item in actual.get('reader_errors', {}).get('process', [])):
            kind = 'launch-error'
        record['execution'] = {'kind': kind, 'error': repr(error), 'returned_ns': time.time_ns(), 'capture': actual}
        for name in ['native.stdout', 'native.stderr']:
            try:
                if not (root / name).exists():
                    (root / name).write_bytes(b'')
            except Exception as capture_error:
                record.setdefault('capture_errors', {})[name] = repr(capture_error)
        try:
            (root / 'execution.json').write_text(json.dumps(record['execution'], indent=2) + '\n', encoding='utf-8')
        except Exception as capture_error:
            record.setdefault('capture_errors', {})['execution.json'] = repr(capture_error)
        raise
    record['execution'] = {'kind': 'returned', 'exit': result.returncode, 'returned_ns': time.time_ns(),
                           'capture': result.capture_report}
    capture_errors = {}
    try:
        (root / 'execution.json').write_text(json.dumps(record['execution'], indent=2) + '\n', encoding='utf-8')
    except Exception as error:
        capture_errors['execution.json'] = repr(error)
    if capture_errors:
        record['capture_errors'] = capture_errors
    # The primary native nonzero is reported before a secondary capture error.
    require(result.returncode == 0, 'actual native profile returned nonzero: ' + str(result.returncode))
    require(not capture_errors, 'native evidence capture failed')
    require(result.capture_report.get('pipe_capture_complete') and result.capture_report.get('reaped') and
            not result.capture_report.get('reader_errors') and not result.capture_report.get('timed_out') and
            not result.capture_report.get('output_overflow'), 'actual live-bounded capture and process completion')
    require(len(result.stdout) <= BOUND and len(result.stderr) <= BOUND, 'bounded actual native logs')
    return result


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--executable', type=Path, required=True)
    args.add_argument('--source-root', type=Path, required=True)
    args.add_argument('--work-dir', type=Path, required=True)
    args.add_argument('--mode', choices=MODES, action='append')
    options = args.parse_args()
    source = options.source_root.resolve(strict=True)
    executable = options.executable.resolve(strict=True)
    parser_path = source / 'tests/tools/world/test_source_saving.py'
    capture_path = source / 'tests/reference/client/statistics/support.py'
    paths = [Path(__file__).resolve(), parser_path, capture_path, executable,
             source / 'tests/world/storage/test_chunk_lookup.c',
             source / 'tests/world/runtime/saving/mob_nbt_source_support.inc',
             source / 'tests/world/runtime/tile_entities/test_tile_entities.c',
             source / 'src/world/storage/chunk_lookup_internal.h']
    before = {str(path): digest(path) for path in paths}
    spec = importlib.util.spec_from_file_location('store_index_authored_source', parser_path)
    parser = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser)
    spec = importlib.util.spec_from_file_location('store_index_bounded_capture', capture_path)
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    options.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='store-index-', dir=options.work_dir.resolve()))
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before, 'profiles': []}
    (output / 'INPUT-FREEZE.json').write_text(json.dumps(before, indent=2) + '\n', encoding='utf-8')
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    primary = False
    try:
        modes = options.mode or MODES
        require(len(modes) == len(set(modes)), 'unique selected native profiles')
        for mode in modes:
            root = output / mode
            root.mkdir()
            authored = authored_inputs(parser, root)
            record = {'status': 'FAIL', 'mode': mode, 'authored_inputs': authored}
            report['profiles'].append(record)
            try:
                result = native([str(executable), str(root), mode], root, record, capture)
                stdout = result.stdout.decode('utf-8', 'strict')
                stderr = result.stderr.decode('utf-8', 'replace')
                require(not any(marker in stdout + stderr for marker in
                                ['ERROR: AddressSanitizer', 'LeakSanitizer:', 'runtime error:']),
                        'no retained sanitizer failure')
                owner, case, retire, finish = [row(stdout, prefix) for prefix in
                    ['STORE_INDEX_OWNER', 'STORE_INDEX_CASE', 'STORE_INDEX_RETIRE', 'STORE_INDEX_FINISH']]
                require(all(item['mode'] == mode for item in [owner, case, retire, finish]), 'exact mode receipts')
                for key in ['actual-world', 'loaded-stored']:
                    require(owner.get(key) == '1', 'actual source owner/loaded identity')
                require(owner.get('fixed-prefix-allocations') == case.get('fixed-prefix-allocations') == '2',
                        'fixed Handler-prefix allocation count remains separate')
                for key in ['loaded-stored', 'generated', 'copied-stored', 'coordinate-alias', 'canonical-visits',
                            'passive-queries', 'busy-unchanged', 'removal-order', 'failed-attached', 'no-clock-or-source-fields']:
                    require(case.get(key) == '1', 'actual bounded Store lifecycle control: ' + key)
                numbers = {key: int(case[key]) for key in ['retained', 'indexed', 'disabled', 'capacity', 'live-bytes',
                    'peak-bytes', 'optional-limit', 'base-owner', 'max-owner', 'current-remaining', 'pointer-reuses', 'reuse-attempts']}
                require(numbers['base-owner'] == int(owner['base']) and numbers['max-owner'] == int(owner['max-owner']) and
                        numbers['max-owner'] >= numbers['base-owner'] > 0 and
                        numbers['optional-limit'] == min(262144, numbers['max-owner'] - numbers['base-owner']) and
                        0 <= numbers['live-bytes'] <= numbers['peak-bytes'] <= numbers['optional-limit'] and
                        numbers['current-remaining'] == numbers['max-owner'] - numbers['base-owner'] - numbers['live-bytes'] and
                        1 <= numbers['retained'] <= 512 and numbers['reuse-attempts'] == 32 and
                        0 <= numbers['pointer-reuses'] <= 32, 'honest scoped live/peak/budget and allocator receipts')
                require((numbers['indexed'], numbers['disabled']) == ((0, 1) if mode == 'linear' else (1, 0)),
                        'real budget-disabled and indexed semantic paths')
                require(retire.get('source-final-released') == '1' and
                        int(retire['historical-peak']) == numbers['peak-bytes'] and
                        (retire.get('explicit-empty'), int(retire['bulk-retained']), retire.get('bulk-indexed')) ==
                        (('0', int(retire['bulk-retained']), '1') if mode == 'indexed-bulk' else ('1', 0, '0')) and
                        (mode != 'indexed-bulk' or int(retire['bulk-retained']) >= 8),
                        'real empty drain or nonempty indexed final bulk retirement')
                require(finish.get('cleanup') == finish.get('source-files-closed') == '1' and int(finish['checks']) > 20,
                        'actual checked native cleanup')
                after = inventory(root / 'world')
                require(set(after) == set(authored) | {'session.lock'} and
                        all(after.get(name) == pin for name, pin in authored.items()) and
                        after['session.lock']['bytes'] == 8 and read(root / 'world/session.lock') ==
                        struct.pack('>q', int(owner['source-e'])),
                        'actual constructor token only; metadata/region bytes unchanged')
                record.update(status='PASS', native_owner=owner, native_case=case, native_retire=retire,
                              native_finish=finish, game_after=after,
                              pointer_reuse_scope='Observed real address reuse' if numbers['pointer-reuses'] else
                                                  'No real address recurrence observed; reuse qualification remains open')
            finally:
                # Secondary filesystem failure cannot replace a primary native/timeout exception.
                active_error = sys.exc_info()[0] is not None
                try:
                    record['final_inventory'] = inventory(root / 'world')
                except Exception as error:
                    record['snapshot_error'] = repr(error)
                    if not active_error:
                        raise
        report['status'] = 'PASS'
    except BaseException as error:
        primary = True
        report['status'] = 'FAIL'
        report['error'] = {'type': type(error).__name__, 'error': str(error)}
        raise
    finally:
        after, errors = {}, {}
        for path in paths:
            try:
                after[str(path)] = digest(path)
            except Exception as error:
                errors[str(path)] = repr(error)
        report.update(inputs_after=after, input_errors=errors, inputs_unchanged=before == after)
        if before != after or errors:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'helper/product/source continuity failed'
        try:
            (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        except Exception:
            if not primary:
                raise
        if not primary and (before != after or errors):
            raise RuntimeError(report['continuity_error'])
    print(f'Source Store index {len(report["profiles"])} profiles PASS; {output / "report.json"}')


if __name__ == '__main__':
    main()
