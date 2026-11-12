"""Benchmark fixed loaded-Chunk queries and retain value, owner and input continuity evidence.

This is a CPU microbenchmark, not a frame-rate or original-game parity test.
Use Release binaries from the same compiler and machine for comparisons.
Every sample is retained; no timing threshold determines success.
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
import tempfile
import time
import zlib


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fields(line):
    values = [value.split('=', 1) for value in line.split()[1:]]
    assert all(len(value) == 2 for value in values)
    assert len(dict(values)) == len(values)
    return dict(values)


def main():
    if not __debug__:
        raise RuntimeError('Benchmark continuity checks require Python without -O')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='LABEL=PATH')
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--scope', choices=('indexed', 'mutation'), default='indexed')
    parser.add_argument('--chunks', choices=(1, 289, 410, 512), type=int, default=1)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    assert 1 <= args.rounds <= 10
    root = Path(__file__).resolve().parents[3]
    executables = dict(value.split('=', 1) for value in args.executable)
    executables = {label: Path(path).resolve(strict=True) for label, path in executables.items()}
    assert len(executables) == len(args.executable) and all(
        label and all(c.isascii() and (c.isalnum() or c in '-_') for c in label)
        for label in executables)
    inputs = [root / 'tests/benchmarks/world_queries.c', Path(__file__),
              root / 'tests/tools/world/test_owned_cache.py',
              root / 'tests/tools/world/test_source_saving.py',
              root / 'tests/world/runtime/saving/mob_nbt_source_support.inc',
              root / 'tests/world/runtime/tile_entities/test_tile_entities.c', *executables.values()]
    inputs.extend(path for folder in ('src', 'include') for path in (root / folder).rglob('*')
                  if path.is_file() and path.suffix in ('.c', '.h', '.inc'))
    inputs = sorted(set(inputs))
    before = {str(path): digest(path) for path in inputs}
    owned = module('query_benchmark_input_inventory', root / 'tests/tools/world/test_owned_cache.py')
    saving = module('query_benchmark_saved_inputs', root / 'tests/tools/world/test_source_saving.py')
    # Change authored on-disk arrays before construction. No runtime owner or
    # Source field is repaired. Inputs remain periodic and one-sector bounded.
    original_seed = saving.seed
    arrays = {'Blocks': bytes((1 if i % 3 == 0 else 3 if i % 3 == 1 else 0)
                              for i in range(32768)),
              'Data': bytes((i * 37 + 0x53) & 255 for i in range(16384)),
              'SkyLight': bytes((i * 19 + 0x7b) & 255 for i in range(16384)),
              'BlockLight': bytes((i * 11 + 0x2d) & 255 for i in range(16384))}

    def varied_seed(mode):
        metadata, original = original_seed(mode)
        length, = struct.unpack_from('>i', original, 8192)
        wire = zlib.decompress(original[8197:8196 + length])
        for key, value in arrays.items():
            old = saving.named(7, key, struct.pack('>i', len(value)) + bytes(len(value)))
            new = saving.named(7, key, struct.pack('>i', len(value)) + value)
            assert wire.count(old) == 1
            wire = wire.replace(old, new, 1)
        parsed = saving.Nbt(wire).root()['Level'][1]
        assert all(bytes(parsed[key][1]) == value for key, value in arrays.items())
        compressed = zlib.compress(wire)
        assert len(compressed) + 5 < 4096
        image = bytearray(original)
        image[8192:] = bytes(4096)
        record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
        image[8192:8192 + len(record)] = record
        return metadata, image

    saving.seed = varied_seed

    def source_inputs(folder):
        world, regions = folder / 'world', folder / 'world/region'
        regions.mkdir(parents=True)
        metadata, original = saving.seed('empty')
        (world / 'level.dat').write_bytes(metadata)
        length, = struct.unpack_from('>i', original, 8192)
        base = zlib.decompress(original[8197:8196 + length])
        if args.chunks == 1:
            coordinates = ((0, 0),)
        elif args.chunks == 289:
            coordinates = tuple((x, z) for x in range(-8, 9) for z in range(-8, 9))
        else:
            coordinates = tuple((index // 16 - 16, index % 16 - 8)
                                for index in range(args.chunks))
        grouped = {}
        for x, z in coordinates:
            wire = base
            for key, value in (('xPos', x), ('zPos', z)):
                old = saving.named(3, key, bytes(4))
                assert wire.count(old) == 1
                wire = wire.replace(old, saving.named(3, key, struct.pack('>i', value)), 1)
            parsed = saving.Nbt(wire).root()['Level'][1]
            assert parsed['xPos'] == [3, x] and parsed['zPos'] == [3, z]
            assert all(bytes(parsed[key][1]) == value for key, value in arrays.items())
            compressed = zlib.compress(wire)
            assert len(compressed) + 5 <= 4096
            grouped.setdefault((x >> 5, z >> 5), []).append((x, z, compressed))
        for (rx, rz), chunks in grouped.items():
            image = bytearray((2 + len(chunks)) * 4096)
            for sector, (x, z, compressed) in enumerate(chunks, 2):
                slot = (x & 31) + (z & 31) * 32
                struct.pack_into('>I', image, slot * 4, sector << 8 | 1)
                struct.pack_into('>i', image, 4096 + slot * 4, 123)
                record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
                image[sector * 4096:sector * 4096 + len(record)] = record
            (regions / f'r.{rx}.{rz}.mcr').write_bytes(image)
        return owned.inventory(world)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='fixed-query-', dir=args.artifacts.resolve()))
    report = {'status': 'FAIL', 'description': __doc__, 'inputs_before': before,
              'host': {'platform': platform.platform(), 'machine': platform.machine(),
                       'processor': os.environ.get('PROCESSOR_IDENTIFIER', platform.processor())},
              'rounds': args.rounds, 'profiles': [],
              'scope': args.scope, 'chunks': args.chunks,
              'authored_array_hashes': {key: hashlib.sha256(value).hexdigest()
                                       for key, value in arrays.items()},
              'workload': '65536 fixed queries per sample, seven samples per operation, six operations including provider presence hits/misses; owned loaded source Chunks; no source ticks or rendering.'}
    modes = ('block', 'metadata', 'saved-light', 'owned-get', 'presence-hit', 'presence-miss')
    try:
        for round_index in range(args.rounds):
            order = list(executables)
            if round_index & 1:
                order.reverse()
            for label in order:
                folder = output / f'{round_index}-{label}'
                folder.mkdir()
                authored = source_inputs(folder)
                command = [str(executables[label]), str(folder), args.scope, str(args.chunks)]
                started = time.monotonic()
                result = subprocess.run(command, capture_output=True, timeout=180, cwd=folder)
                (folder / 'native.stdout').write_bytes(result.stdout)
                (folder / 'native.stderr').write_bytes(result.stderr)
                profile = {'label': label, 'round': round_index, 'command': command,
                           'returncode': result.returncode, 'host_seconds': time.monotonic() - started,
                           'authored_inputs': authored}
                report['profiles'].append(profile)
                assert result.returncode == 0 and not result.stderr, str(folder)
                text = result.stdout.decode('utf-8', 'strict').splitlines()
                rows = [fields(line) for line in text if line.startswith('CACHE_QUERY_SAMPLE ')]
                contexts = [fields(line) for line in text if line.startswith('CACHE_QUERY_CONTEXT ')]
                invariants = [fields(line) for line in text if line.startswith('CACHE_QUERY_INVARIANTS ')]
                finishes = [fields(line) for line in text if line.startswith('CACHE_QUERY_FINISH ')]
                assert len(rows) == 7 * len(modes) and len(contexts) == len(invariants) == len(finishes) == 1
                assert contexts[0]['scope'] == args.scope and int(contexts[0]['chunks']) == args.chunks
                assert {(row['mode'], int(row['sample'])) for row in rows} == {
                    (mode, sample) for mode in modes
                    for sample in range(7)}
                assert all(set(row) == {'mode', 'sample', 'calls', 'ticks', 'frequency', 'checksum'}
                           and int(row['calls']) == 65536 and int(row['ticks']) > 0
                           and int(row['frequency']) > 0 for row in rows)
                assert finishes[0]['cleanup'] == finishes[0]['genuine-source-world'] == '1'
                after = owned.inventory(folder / 'world')
                assert set(after) == set(authored) | {'session.lock'}
                assert all(after[name] == value for name, value in authored.items())
                assert after['session.lock']['bytes'] == 8
                profile.update(rows=rows, context=contexts[0], invariants=invariants[0], finish=finishes[0],
                               authored_inputs_unchanged=True)
                print(f'{label} round {round_index}: PASS, {profile["host_seconds"]:.3f}s')
        summary = {}
        for label in executables:
            summary[label] = {}
            for mode in modes:
                rows = [row for profile in report['profiles'] if profile['label'] == label
                        for row in profile['rows'] if row['mode'] == mode]
                assert len(rows) == 7 * args.rounds
                checksums = {row['checksum'] for row in rows}
                assert len(checksums) == 1
                nanos = sorted(int(row['ticks']) * 1e9 /
                               (int(row['frequency']) * int(row['calls'])) for row in rows)
                summary[label][mode] = {'samples': len(nanos), 'calls_per_sample': 65536,
                                       'median_ns_per_call': statistics.median(nanos),
                                       'p95_ns_per_call': nanos[math.ceil(.95 * len(nanos)) - 1],
                                       'min_ns_per_call': nanos[0], 'max_ns_per_call': nanos[-1],
                                       'checksum': next(iter(checksums))}
        for mode in modes:
            assert len({value[mode]['checksum'] for value in summary.values()}) == 1
        report.update(status='PASS', summary=summary)
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['inputs_unchanged'] = report['inputs_before'] == report['inputs_after']
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], 'report': str(output), 'summary': report.get('summary')}))
    assert report['status'] == 'PASS'


if __name__ == '__main__':
    main()
