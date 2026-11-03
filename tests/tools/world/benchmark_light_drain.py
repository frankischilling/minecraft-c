"""Benchmark fixed public Light drains on nine authored saved Chunks.

Use Release executables from the same compiler and host. All finite samples,
source effects, failure receipts and input/owner continuity are retained.
No timing threshold determines success; this does not measure scene FPS.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import statistics
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def inventory(root):
    paths = sorted(root.rglob('*'))
    require(len(paths) <= 32 and all(not path.is_symlink() for path in paths), 'bounded authored tree')
    require(all(path.stat().st_size <= 1048576 for path in paths if path.is_file()),
            'bounded authored files')
    return {path.relative_to(root).as_posix(): {'bytes': path.stat().st_size,
            'sha256': digest(path)} for path in paths if path.is_file()}


def author(folder, saving):
    world = folder / 'world'
    regions = world / 'region'
    regions.mkdir(parents=True)
    metadata, original = saving.seed('empty')
    (world / 'level.dat').write_bytes(metadata)
    length, = struct.unpack_from('>i', original, 8192)
    require(1 < length < 4092 and original[8196] == 2, 'one sector canonical seed')
    wire = zlib.decompress(original[8197:8196 + length])
    require(len(wire) < 200000, 'bounded canonical NBT')
    for key, old, new in [('SkyLight', bytes(16384), b'\xff' * 16384),
                          ('HeightMap', bytes([64]) * 256, bytes(256))]:
        old_field = saving.named(7, key, struct.pack('>i', len(old)) + old)
        require(wire.count(old_field) == 1, 'one canonical array')
        wire = wire.replace(old_field, saving.named(7, key, struct.pack('>i', len(new)) + new), 1)
    grouped = {}
    for x in range(-1, 2):
        for z in range(-1, 2):
            value = wire
            for key, coordinate in [('xPos', x), ('zPos', z)]:
                old = saving.named(3, key, bytes(4))
                require(value.count(old) == 1, 'one coordinate field')
                value = value.replace(old, saving.named(3, key, struct.pack('>i', coordinate)), 1)
            decoded = saving.Nbt(value).root()['Level'][1]
            require(decoded['xPos'] == [3, x] and decoded['zPos'] == [3, z] and
                    decoded['SkyLight'][1] == b'\xff' * 16384 and
                    decoded['HeightMap'][1] == bytes(256) and
                    decoded['Blocks'][1] == bytes(32768) and
                    decoded['Data'][1] == decoded['BlockLight'][1] == bytes(16384) and
                    decoded['Entities'] == decoded['TileEntities'] == [9, [10, []]],
                    'independently decoded authored metadata/planes/lists')
            compressed = zlib.compress(value)
            require(len(compressed) + 5 <= 4096, 'one bounded sector')
            grouped.setdefault((x >> 5, z >> 5), []).append((x, z, compressed))
    for (rx, rz), chunks in grouped.items():
        image = bytearray((len(chunks) + 2) * 4096)
        for sector, (x, z, compressed) in enumerate(chunks, 2):
            slot = (x & 31) + (z & 31) * 32
            struct.pack_into('>I', image, slot * 4, (sector << 8) | 1)
            struct.pack_into('>i', image, 4096 + slot * 4, 123)
            record = struct.pack('>i', len(compressed) + 1) + b'\x02' + compressed
            image[sector * 4096:sector * 4096 + len(record)] = record
        (regions / f'r.{rx}.{rz}.mcr').write_bytes(image)
    return inventory(world)


def fields(line):
    parts = [part.split('=', 1) for part in line.split()[1:]]
    require(all(len(part) == 2 for part in parts), 'complete receipt fields')
    result = dict(parts)
    require(len(result) == len(parts), 'unique receipt fields')
    return result


def main():
    if not __debug__:
        raise RuntimeError('Benchmark continuity checks require Python without -O')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='LABEL=PATH')
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    require(1 <= args.rounds <= 10, 'finite rounds')
    root = Path(__file__).resolve().parents[3]
    pairs = [value.split('=', 1) for value in args.executable]
    require(all(len(value) == 2 for value in pairs), 'LABEL=PATH arguments')
    executables = {label: Path(path).resolve(strict=True) for label, path in pairs}
    require(len(executables) == len(pairs) and all(label and all(
        char.isascii() and (char.isalnum() or char in '-_') for char in label)
        for label in executables), 'unique safe profile labels')
    saving_path = root / 'tests/tools/world/test_source_saving.py'
    saving = load('light_benchmark_saved_inputs', saving_path)
    inputs = [Path(__file__), root / 'tests/benchmarks/light_drain.c',
              root / 'tests/benchmarks/world_queries.c', saving_path,
              root / 'tests/world/runtime/saving/mob_nbt_source_support.inc',
              root / 'tests/world/runtime/tile_entities/test_tile_entities.c',
              *executables.values()]
    inputs.extend(path for folder in ('src', 'include') for path in (root / folder).rglob('*')
                  if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    before = {str(path): digest(path) for path in sorted(set(inputs))}
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='light-drain-', dir=args.artifacts.resolve()))
    report = {'status': 'FAIL', 'scope': __doc__, 'inputs_before': before, 'profiles': [],
              'rounds': args.rounds, 'host': {'platform': platform.platform(),
                  'machine': platform.machine(),
                  'processor': os.environ.get('PROCESSOR_IDENTIFIER', platform.processor())},
              'workload': {'chunks': 9, 'regions_submitted': 500, 'region_volume': 8,
                           'source_region_budget': 499, 'voxels_per_sample': 3992,
                           'GET_hits_per_sample': 37924, 'samples_per_process': 7},
              'limits': ['Fixed CPU work over authored source Chunks; not a scene FPS comparison.',
                         'QPC/monotonic clock helper overhead is common to all profiles.',
                         'Assertions, hashes, output and submitted work are outside timing.',
                         'No affinity or scheduler control; retain all samples and compare repeat cohorts.']}
    try:
        reference = None
        for round_index in range(args.rounds):
            order = list(executables)
            if round_index % 2:
                order.reverse()
            for label in order:
                folder = output / f'{round_index}-{label}'
                authored = author(folder, saving)
                command = [str(executables[label]), str(folder)]
                started = time.monotonic()
                result = subprocess.run(command, cwd=folder, capture_output=True, timeout=180)
                (folder / 'native.stdout').write_bytes(result.stdout)
                (folder / 'native.stderr').write_bytes(result.stderr)
                profile = {'label': label, 'round': round_index, 'command': command,
                           'returncode': result.returncode, 'seconds': time.monotonic() - started,
                           'authored_inputs': authored}
                report['profiles'].append(profile)
                require(result.returncode == 0 and not result.stderr, str(folder))
                lines = result.stdout.decode('utf-8', 'strict').splitlines()
                rows = [fields(line) for line in lines if line.startswith('LIGHT_CPU_SAMPLE ')]
                receipts = [line for line in lines if line.startswith('LIGHT_CPU_') and
                            not line.startswith('LIGHT_CPU_SAMPLE ')]
                require(len(rows) == 7 and len(receipts) == 5 and
                        receipts[-1].startswith('LIGHT_CPU_FINISH ') and
                        fields(receipts[-1])['cleanup'] == '1', 'complete checked native receipts')
                require([row['sample'] for row in rows] == [str(i) for i in range(7)] and
                        all(row['gets'] == '37924' and row['remaining'] == '1' and
                            int(row['ticks']) > 0 and int(row['frequency']) > 0 for row in rows),
                        'same source work counters and valid finite clock')
                semantic = {'receipts': receipts, 'rows': [{key: value for key, value in row.items()
                            if key not in ('ticks', 'frequency')} for row in rows]}
                if reference is None:
                    reference = semantic
                require(semantic == reference, 'equal operation/failure/queue/value receipts')
                after = inventory(folder / 'world')
                require(set(after) == set(authored) | {'session.lock'} and
                        all(after[key] == value for key, value in authored.items()) and
                        after['session.lock']['bytes'] == 8, 'authored source continuity')
                profile.update(rows=rows, receipts=receipts, semantic_equal=True,
                               authored_inputs_unchanged=True)
                print(f'{label} round{round_index}: PASS {profile["seconds"]:.3f}s', flush=True)
        summary = {}
        for label in executables:
            ms = sorted(int(row['ticks']) * 1000 / int(row['frequency'])
                        for profile in report['profiles'] if profile['label'] == label
                        for row in profile['rows'])
            require(len(ms) == args.rounds * 7, 'all finite samples')
            summary[label] = {'samples': len(ms), 'mean_ms': statistics.mean(ms),
                              'median_ms': statistics.median(ms), 'min_ms': ms[0], 'max_ms': ms[-1],
                              'p95_ms': ms[math.ceil(.95 * len(ms)) - 1]}
        report.update(status='PASS', summary=summary, semantic_equivalence=True)
    finally:
        report['inputs_after'] = {str(path): digest(Path(path)) for path in before}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'status': report['status'], 'report': str(output / 'report.json'),
                          'summary': report.get('summary')}), flush=True)
    require(report['status'] == 'PASS', 'qualified fixed-work CPU run')


if __name__ == '__main__':
    main()
