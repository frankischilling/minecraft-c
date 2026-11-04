"""Genuine Source-stage saves with independent exact NBT and failure-prefix checks."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
MODES = ('stage-success', 'stage-level-fatal', 'stage-chunks-fatal', 'stage-null',
         'stage-reentry', 'stage-export')
BOUND = 1024 * 1024


def require(value, message):
    if not value:
        raise AssertionError(message)


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def hashes(paths):
    return {str(path): digest(path) for path in paths}


def decode_regions(helper, data):
    require(8192 <= len(data) <= BOUND and len(data) % 4096 == 0, 'bounded region')
    decoded = {}
    for slot, allocation in enumerate(struct.unpack_from('>1024I', data)):
        if allocation == 0:
            continue
        sector, count = allocation >> 8, allocation & 255
        at = sector * 4096
        require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(data),
                'actual region record extent')
        size, = struct.unpack_from('>i', data, at)
        require(1 < size <= count * 4096 - 4 and data[at + 4] == 2,
                'actual compressed NBT extent')
        decoded[slot] = helper.inflate(data[at + 5:at + 4 + size], 15)
    return decoded


def stored_region_record(data, slot):
    """Exact untouched allocation header, timestamp and whole allocated sectors."""
    require(0 <= slot < 1024, 'bounded selected slot')
    allocation, = struct.unpack_from('>I', data, slot * 4)
    sector, count = allocation >> 8, allocation & 255
    require(sector >= 2 and count > 0 and (sector + count) * 4096 <= len(data),
            'bounded untouched allocation')
    return (data[slot * 4:slot * 4 + 4], data[4096 + slot * 4:4100 + slot * 4],
            data[sector * 4096:(sector + count) * 4096])


def audit_case(authored, owned, mode, case, first, last, returncode, stdout_bytes, stderr_bytes):
    require(mode in MODES and first <= last, 'declared mode and process clock interval')
    require(len(stdout_bytes) <= BOUND and len(stderr_bytes) <= BOUND, 'bounded native output')
    stdout, stderr = stdout_bytes.decode('utf-8', 'strict'), stderr_bytes.decode('utf-8', 'replace')
    require(returncode == 0 and not any(marker in stdout + stderr for marker in
            ('ERROR: AddressSanitizer', 'LeakSanitizer:', 'runtime error:')), 'actual native checks pass')
    receipt, finish = owned.row(stdout, 'STAGE_PROGRESS_CASE'), owned.row(stdout, 'STAGE_PROGRESS_FINISH')
    require(set(finish) == {'mode', 'checks', 'cleanup', 'genuine-source-world'} and
            finish.get('mode') == mode and finish.get('cleanup') == finish.get('genuine-source-world') == '1'
            and int(finish['checks']) > 20, 'actual owner cleanup')
    require(receipt.get('mode') == mode and receipt.get('binding') == receipt.get('balanced') == '1'
            and receipt.get('blocked') == '0', 'callback bindings and holds balanced')
    require(set(receipt) == {'mode', 'calls', 'nested', 'blocked', 'status', 'owned',
                            'raw_exports', 'dirty', 'metadata', 'binding', 'balanced'}, 'exact receipt schema')
    require(receipt['owned'] == ('0' if mode == 'stage-export' else '1') and
            (int(receipt['raw_exports']) > 0 if mode == 'stage-export'
             else receipt['raw_exports'] == '0'), 'actual permanent export or owned retention')
    success = mode in ('stage-success', 'stage-null', 'stage-reentry')
    require(receipt['calls'] == ('0' if mode == 'stage-null' else
                               '1' if mode in ('stage-level-fatal', 'stage-export') else '2') and
            receipt['nested'] == ('1' if mode == 'stage-reentry' else '0') and
            receipt['status'] == ('0' if success else '3') and
            receipt['dirty'] == ('0' if success else '1') and
            receipt['metadata'] == ('1' if mode in ('stage-success', 'stage-reentry',
                                                   'stage-chunks-fatal') else '0'),
            'exact callback counts, lifecycle status and reached effects')
    snapshots = {key: authored.read(case / 'snapshots' / key) for key in
                 ('before-level.dat', 'after-level.dat', 'before-region.mcr', 'after-region.mcr')}
    prior = authored.inflate(snapshots['before-level.dat'], 31)['Data'][1]
    current = authored.inflate(snapshots['after-level.dat'], 31)['Data'][1]
    no_metadata = mode in ('stage-level-fatal', 'stage-export')
    no_chunks = no_metadata or mode == 'stage-chunks-fatal'
    if no_metadata:
        require(snapshots['before-level.dat'] == snapshots['after-level.dat'], 'LEVEL failure before metadata')
    else:
        require(current['Time'] == prior['Time'] == [4, 6000] and
                current['RandomSeed'] == prior['RandomSeed'] == [4, 23] and
                current['version'] == [3, 19132] and 'Player' not in current and
                current['LastPlayed'][0] == 4 and first <= current['LastPlayed'][1] <= last,
                'real metadata save, clock and empty Player selection')
    if no_chunks:
        require(snapshots['before-region.mcr'] == snapshots['after-region.mcr'], 'failure before Chunk writes')
    else:
        before_region = decode_regions(authored, snapshots['before-region.mcr'])
        after_region = decode_regions(authored, snapshots['after-region.mcr'])
        require(set(before_region) == set(after_region), 'all selected region slots preserved')
        written = (0, 2 + 2 * 32)
        expected_fields = {'xPos', 'zPos', 'LastUpdate', 'Blocks', 'Data', 'SkyLight',
                           'BlockLight', 'HeightMap', 'TerrainPopulated', 'Entities', 'TileEntities'}
        for slot in written:
            before_level, after_level = before_region[slot]['Level'][1], after_region[slot]['Level'][1]
            require(set(after_region[slot]) == {'Level'} and after_region[slot]['Level'][0] == 10
                    and set(after_level) == expected_fields, 'exact fresh written Chunk schema')
            for field in ('xPos', 'zPos', 'LastUpdate', 'Blocks', 'Data', 'SkyLight',
                          'BlockLight', 'HeightMap', 'TerrainPopulated'):
                require(before_level[field] == after_level[field], 'real saved Chunk field ' + field)
            for field in ('Entities', 'TileEntities'):
                require(before_level[field] == [9, [10, []]],
                        'authored written Chunk exact empty Compound list ' + field)
                require(after_level[field] == [9, [1, []]],
                        'source written Chunk exact canonical empty Byte list ' + field)
        for slot in set(before_region) - set(written):
            require(before_region[slot] == after_region[slot] and
                    stored_region_record(snapshots['before-region.mcr'], slot) ==
                    stored_region_record(snapshots['after-region.mcr'], slot),
                    'untouched Chunk exact tree/subtypes/record bytes/header/timestamp')
    return dict(status='PASS', receipt=receipt, finish=finish,
                  snapshots={key: {'bytes': len(value), 'sha256': hashlib.sha256(value).hexdigest()}
                             for key, value in snapshots.items()},
                  metadata_changed_tags=sorted(key for key in set(prior) | set(current)
                                               if prior.get(key) != current.get(key)),
                  list_rule='written authored Compound10 empty -> source Byte1 empty; untouched exact')


def listed_inputs(source, executable):
    """Current listed sources plus actual product; not a build-dependency seal."""
    relative = (
        'tests/world/runtime/saving/test_stage_progress.c',
        'tests/world/runtime/saving/test_owned_cache.c',
        'tests/world/runtime/saving/mob_nbt_source_support.inc',
        'tests/world/runtime/tile_entities/test_tile_entities.c',
        'tests/tools/world/test_source_saving.py',
        'tests/tools/world/test_owned_cache.py',
        'src/world/runtime/runtime.c',
        'src/world/runtime/saving/progress_internal.h',
        'src/world/runtime/chunk_provider.c',
        'src/world/runtime/chunk_provider_owned.inc',
        'src/world/runtime/saving/saving.c',
        'src/world/cache/cache.c',
        'src/world/cache/lifecycle.c',
        'src/client/world/load_world.c',
        'src/client/world/source_save.inc',
        'include/mc/world/runtime/saving/errors.h',
        'include/mc/world/runtime/saving/saving.h',
        'include/mc/world/cache/lifecycle.h',
    )
    paths = [Path(__file__).resolve(), executable]
    paths.extend(source / path for path in relative)
    paths = list(dict.fromkeys(paths))
    require(all(path.is_file() for path in paths), 'current listed sources/product exist')
    return paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--mode', action='append', choices=MODES)
    args = parser.parse_args()
    modes = args.mode or MODES
    require(len(modes) == len(set(modes)), 'unique profiles')
    source, executable = args.source_root.resolve(strict=True), args.executable.resolve(strict=True)
    inputs = listed_inputs(source, executable)
    before_inputs = hashes(inputs)
    authored = module('stage_authored_source', source / 'tests/tools/world/test_source_saving.py')
    owned = module('stage_owned_source', source / 'tests/tools/world/test_owned_cache.py')
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='source-stage-progress-', dir=args.work_dir.resolve()))
    report = {
        'status': 'FAIL',
        'scope': 'Authored genuine Source GAME owners; no original parity or UI claim',
        'qualification': 'Fresh public projection; historical private receipts are not rebound',
        'input_guard_scope': 'Current listed fixture/runtime sources and actual executable; not a build-dependency seal',
        'before_inputs': before_inputs,
        'requested_profiles': list(modes),
        'profiles': [],
    }
    report_path = output / 'report.json'
    try:
        for mode in modes:
            case = output / mode
            case.mkdir()
            (case / 'snapshots').mkdir()
            record = {'mode': mode, 'status': 'FAIL'}
            report['profiles'].append(record)
            record['authored'] = owned.source_inputs(authored, case, False)
            command = [str(executable), str(case), mode]
            first = time.time_ns() // 1000000
            try:
                result = subprocess.run(command, cwd=case, capture_output=True, timeout=30)
            except subprocess.TimeoutExpired as error:
                (case / 'native.stdout').write_bytes(error.stdout or b'')
                (case / 'native.stderr').write_bytes(error.stderr or b'')
                record.update(command=command, error='native timeout', timeout_seconds=30)
                raise
            last = time.time_ns() // 1000000
            (case / 'native.stdout').write_bytes(result.stdout)
            (case / 'native.stderr').write_bytes(result.stderr)
            record.update(command=command, exit=result.returncode, first_ms=first, last_ms=last)
            require(first <= last, 'actual process wall-clock interval')
            record.update(audit_case(authored, owned, mode, case, first, last, result.returncode,
                                     result.stdout, result.stderr))
            print(f'Source stage {mode} PASS checks={record["finish"]["checks"]}')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = {'type': type(error).__name__, 'message': str(error)}
        raise
    finally:
        original_error = sys.exc_info()[0] is not None
        after, errors = {}, {}
        for path in inputs:
            try:
                after[str(path)] = digest(path)
            except Exception as error:
                errors[str(path)] = repr(error)
        report['after_inputs'] = after
        report['input_errors'] = errors
        report['inputs_unchanged'] = before_inputs == after
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'current listed inputs/product changed'
        report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if not report['inputs_unchanged'] and not original_error:
            raise RuntimeError(report['continuity_error'])
    print(f'Source stage {len(report["profiles"])} profiles PASS; {report_path}')


if __name__ == '__main__':
    main()
