"""Genuine source-World owned-cache controls; no synthetic provider/trust flag."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
MODES = ('owned', 'provider', 'acquisition', 'lifecycle', 'saving', 'get', 'acquire',
         'progress', 'callback', 'callback-export', 'budget')
BOUND = 1024 * 1024


def require(value, message):
    if not value:
        raise AssertionError(message)


def read(path):
    require(path.is_file() and path.stat().st_size <= BOUND, 'bounded helper/output')
    return path.read_bytes()


def digest(path):
    value = hashlib.sha256()
    total = 0
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            total += len(block)
            require(total <= 256 * BOUND, 'bounded sanitized product')
            value.update(block)
    return value.hexdigest()


def inventory(root):
    pending, result, entries, total = [(root, 0)], {}, 0, 0
    while pending:
        directory, depth = pending.pop()
        require(depth <= 3, 'bounded source directory depth')
        for path in directory.iterdir():
            entries += 1
            require(entries <= 64 and not path.is_symlink(),
                    'bounded nonsymlink authored source tree')
            if path.is_dir():
                pending.append((path, depth + 1))
                continue
            data = read(path)
            total += len(data)
            require(total <= 16 * BOUND, 'bounded source files')
            result[path.relative_to(root).as_posix()] = {
                'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    return dict(sorted(result.items()))


def source_inputs(module, root, entity):
    """Explicit authored coordinates, keeping the canonical seed's actual wire shapes."""
    world, regions = root / 'world', root / 'world/region'
    regions.mkdir(parents=True)
    metadata, _ = module.seed('empty')
    (world / 'level.dat').write_bytes(metadata)
    grouped = {}
    for index, (x, z) in enumerate(((0, 0), (2, 2), (4, -3), (7, 7))):
        _, image = module.seed('null-stack' if entity and index == 0 else 'empty')
        length, = struct.unpack_from('>i', image, 8192)
        require(1 < length < 4092 and image[8196] == 2, 'canonical authored one-sector seed')
        decoder = zlib.decompressobj()
        wire = decoder.decompress(image[8197:8196 + length], BOUND + 1)
        require(len(wire) <= BOUND and decoder.eof and not decoder.unused_data and
                not decoder.unconsumed_tail, 'bounded authored seed decode')
        for key, coordinate in (('xPos', x), ('zPos', z)):
            old = module.named(3, key, struct.pack('>i', 0))
            require(wire.count(old) == 1, 'one explicit authored coordinate field')
            wire = wire.replace(old, module.named(3, key, struct.pack('>i', coordinate)), 1)
        parsed = module.Nbt(wire).root()
        require(parsed['Level'][1]['xPos'] == [3, x] and
                parsed['Level'][1]['zPos'] == [3, z], 'independently parsed authored coordinates')
        compressed = zlib.compress(wire)
        require(len(compressed) + 5 <= 4096, 'bounded single-sector authored record')
        grouped.setdefault((x >> 5, z >> 5), []).append((x, z, compressed))
    for (rx, rz), chunks in grouped.items():
        image = bytearray((2 + len(chunks)) * 4096)
        for sector, (x, z, compressed) in enumerate(chunks, 2):
            slot = (x & 31) + (z & 31) * 32
            struct.pack_into('>I', image, slot * 4, sector << 8 | 1)
            struct.pack_into('>i', image, 4096 + slot * 4, 123)
            record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
            image[sector * 4096:sector * 4096 + len(record)] = record
        require(len(image) <= BOUND, 'bounded actual source region input')
        (regions / f'r.{rx}.{rz}.mcr').write_bytes(image)
    return inventory(world)


def row(stdout, prefix):
    found = [line for line in stdout.splitlines() if line.startswith(prefix + ' ')]
    require(len(found) == 1, 'one checked native ' + prefix)
    pairs = [cell.split('=', 1) for cell in found[0].split()[1:]]
    require(all(len(pair) == 2 for pair in pairs) and len(dict(pairs)) == len(pairs),
            'unique checked native receipt fields')
    return dict(pairs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--mode', action='append', choices=MODES)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    executable = args.executable.resolve(strict=True)
    helpers = [Path(__file__).resolve(), source / 'tests/tools/world/test_source_saving.py',
               source / 'tests/world/runtime/saving/test_owned_cache.c',
               source / 'tests/world/runtime/saving/mob_nbt_source_support.inc',
               source / 'tests/world/runtime/tile_entities/test_tile_entities.c',
               source / 'src/world/runtime/chunk_provider_internal.h', executable]
    before = {str(path): digest(path) for path in helpers}
    spec = importlib.util.spec_from_file_location('owned_cache_authored_source_inputs', helpers[1])
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    args.work_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='owned-cache-', dir=args.work_dir.resolve()))
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before, 'profiles': []}
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    try:
        modes = args.mode or MODES
        require(len(set(modes)) == len(modes), 'unique exact native profiles')
        for mode in modes:
            root = output / mode
            root.mkdir()
            authored = source_inputs(module, root, mode in ('callback', 'callback-export'))
            record = {'mode': mode, 'authored_inputs': authored, 'status': 'FAIL'}
            report['profiles'].append(record)
            command = [str(executable), str(root), mode]
            record['command'] = command
            try:
                result = subprocess.run(command, capture_output=True, timeout=25, cwd=root)
            except subprocess.TimeoutExpired as error:
                (root / 'native.stdout').write_bytes(error.stdout or b'')
                (root / 'native.stderr').write_bytes(error.stderr or b'')
                record['error'] = 'native timeout'
                raise
            (root / 'native.stdout').write_bytes(result.stdout)
            (root / 'native.stderr').write_bytes(result.stderr)
            record['exit'] = result.returncode
            require(len(result.stdout) <= BOUND and len(result.stderr) <= BOUND,
                    'bounded retained native output')
            stdout = result.stdout.decode('utf-8', 'strict')
            stderr = result.stderr.decode('utf-8', 'replace')
            require(result.returncode == 0, 'actual native profile returned success')
            require(not any(marker in stdout + stderr for marker in
                            ('ERROR: AddressSanitizer', 'LeakSanitizer:', 'runtime error:')),
                    'no retained sanitizer failure')
            finish, case = row(stdout, 'OWNED_CACHE_FINISH'), row(stdout, 'OWNED_CACHE_CASE')
            require(set(finish) == {'mode', 'checks', 'cleanup', 'genuine-source-world'} and
                    finish['mode'] == mode and finish['cleanup'] == finish['genuine-source-world'] == '1'
                    and finish['checks'].isdigit() and int(finish['checks']) > 20,
                    'actual source-World native cleanup receipt')
            require(case.get('mode') == mode, 'actual exact selected native case')
            if mode == 'owned':
                require(case == dict(mode=mode, hit='1', miss='1', **{'packed-alias':'1',
                        'exact-identities':'1', 'insertion-order':'1', 'generic-equal':'1',
                        'raw-publish-visible':'1'}), 'owned and generic exact semantics')
            elif mode in ('callback', 'callback-export'):
                require(case == dict(mode=mode, **{'real-entity-callback':'1', 'nested-generic':'1',
                        'outer-rebuild':str(int(mode == 'callback')),
                        'callback-export-permanent':str(int(mode.endswith('export'))),
                        'busy-lifetime':'1'}), 'real callback reentry/ownership receipt')
            elif mode == 'budget':
                require(set(case) == {'mode', 'dynamic-base', 'optional-bytes', 'exact-base-success',
                        'generic-fallback', 'exact-identity'} and
                        all(case[key] == '1' for key in ('exact-base-success', 'generic-fallback',
                                                       'exact-identity')) and
                        case['dynamic-base'].isdigit() and int(case['dynamic-base']) > 0 and
                        case['optional-bytes'].isdigit() and int(case['optional-bytes']) > 0,
                        'dynamically derived exact allocation fallback')
            else:
                require(case == dict(mode=mode, **{'irreversible-export':'1', 'retained-alias':'1',
                        'mutation-visible':'1', 'invalid-generic':'1', 'output-preserved':'1',
                        'no-rebuild':'1'}), 'actual mutable export/generic validation receipt')
            after = inventory(root / 'world')
            require(set(after) == set(authored) | {'session.lock'} and
                    all(after.get(name) == value for name, value in authored.items()) and
                    after['session.lock']['bytes'] == 8,
                    'only actual constructor session-token bytes changed')
            record.update(status='PASS', native_case=case, native_finish=finish, game_after=after,
                          native_outputs={name:digest(root/name) for name in ('native.stdout','native.stderr')})
            print(f'Owned source cache {mode} PASS checks={finish["checks"]}')
        report['status'] = 'PASS'
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        original_error = sys.exc_info()[0] is not None
        after, errors = {}, {}
        for path in helpers:
            try:
                after[str(path)] = digest(path)
            except Exception as error:
                errors[str(path)] = repr(error)
        report['inputs_after'] = after
        report['input_errors'] = errors
        report['inputs_unchanged'] = before == after
        if before != after:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'native product/helper inputs changed'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if before != after and not original_error:
            raise RuntimeError(report['continuity_error'])
    print(f'Owned source cache {len(report["profiles"])} profiles PASS; {output / "report.json"}')


if __name__ == '__main__':
    main()
