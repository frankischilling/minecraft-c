"""Compare fixed-work genuine EW Block reads with exact values and ownership checks.

This optional CPU benchmark does not measure scene FPS or original-game parity.
The two executables share a compiled workload object and use the same function
pointer dispatch. Their adapters select the public or failure-only accessor.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import re
import statistics
import struct
import subprocess
import sys
import tempfile
import time
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def inventory(root):
    files = [path for path in root.rglob('*') if path.is_file()]
    require(len(files) <= 32 and all(path.stat().st_size <= 1048576 for path in files),
            'bounded authored save files')
    return {path.relative_to(root).as_posix(): {
                'bytes': path.stat().st_size, 'sha256': digest(path)} for path in files}


def author(folder, saving):
    world = folder / 'world'
    regions = world / 'region'
    regions.mkdir(parents=True)
    metadata, seed = saving.seed('empty')
    (world / 'level.dat').write_bytes(metadata)
    length, = struct.unpack_from('>I', seed, 8192)
    wire = zlib.decompress(seed[8197:8196 + length])
    groups = {}
    for x in range(-1, 2):
        for z in range(-1, 2):
            raw = wire
            for key, value in (('xPos', x), ('zPos', z)):
                old = saving.named(3, key, bytes(4))
                require(raw.count(old) == 1, 'one saved coordinate field')
                raw = raw.replace(old, saving.named(3, key, struct.pack('>i', value)), 1)
            planes = {
                'Blocks': bytes(1 + (i * 13 + (x + 1) * 3 + (z + 1) * 5) % 3
                                for i in range(32768)),
                'Data': bytes((i * 19 + x + z) & 255 for i in range(16384)),
                'SkyLight': bytes((i * 7 + 0xab) & 255 for i in range(16384)),
                'BlockLight': bytes((i * 11 + 0x25) & 255 for i in range(16384)),
                'HeightMap': bytes([128]) * 256,
            }
            for key, value in planes.items():
                old = bytes([64]) * 256 if key == 'HeightMap' else bytes(len(value))
                field = saving.named(7, key, struct.pack('>i', len(old)) + old)
                require(raw.count(field) == 1, 'one saved input plane')
                raw = raw.replace(field, saving.named(
                    7, key, struct.pack('>i', len(value)) + value), 1)
            decoded = saving.Nbt(raw).root()['Level'][1]
            require(decoded['xPos'] == [3, x] and decoded['zPos'] == [3, z] and
                    all(decoded[key] == [7, value] for key, value in planes.items()),
                    'independently decoded authored Block, Nibble, light and height inputs')
            packed = zlib.compress(raw)
            require(len(packed) + 5 <= 4096, 'one bounded Chunk sector')
            groups.setdefault((x >> 5, z >> 5), []).append((x, z, packed))
    for (rx, rz), chunks in groups.items():
        image = bytearray((len(chunks) + 2) * 4096)
        for sector, (x, z, data) in enumerate(chunks, 2):
            slot = (x & 31) + (z & 31) * 32
            struct.pack_into('>I', image, slot * 4, (sector << 8) | 1)
            struct.pack_into('>I', image, 4096 + slot * 4, 123)
            record = struct.pack('>I', len(data) + 1) + b'\2' + data
            image[sector * 4096:sector * 4096 + len(record)] = record
        (regions / f'r.{rx}.{rz}.mcr').write_bytes(image)
    return inventory(world)


def fields(line):
    pairs = [part.split('=', 1) for part in line.split()[1:]]
    require(all(len(part) == 2 for part in pairs), 'typed sample fields')
    result = dict(pairs)
    require(len(result) == len(pairs), 'unique sample fields')
    return result


def summarize(processes, label):
    values = sorted(int(row['ticks']) * 1000 / int(row['frequency'])
                    for process in processes if process['label'] == label
                    for row in process['samples'])
    if not values:
        return None
    return {'samples': len(values), 'mean_ms': statistics.mean(values),
            'median_ms': statistics.median(values),
            'p95_ms': values[math.ceil(.95 * len(values)) - 1],
            'min_ms': values[0], 'max_ms': values[-1]}


def main():
    require(__debug__, 'Benchmark continuity checks require Python without -O')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='LABEL=PATH')
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--mode', choices=('measure', 'controls'), default='measure')
    args = parser.parse_args()
    require(1 <= args.rounds <= 10, 'bounded rounds')
    pairs = [value.split('=', 1) for value in args.executable]
    require(all(len(pair) == 2 and re.fullmatch('[a-zA-Z0-9_-]+', pair[0]) for pair in pairs),
            'safe profile labels and LABEL=PATH arguments')
    executables = {label: Path(path).resolve(strict=True) for label, path in pairs}
    require(len(executables) == len(pairs), 'unique profile labels')
    source_names = (
        'tests/benchmarks/chunk_cache.c', 'tests/benchmarks/chunk_cache_adapter.h',
        'tests/benchmarks/chunk_cache_public.c', 'tests/benchmarks/chunk_cache_failure.c',
        'tests/benchmarks/chunk_cache_error_equal.inc', 'tests/benchmarks/world_queries.c',
        'tests/world/runtime/saving/mob_nbt_source_support.inc',
        'tests/world/runtime/tile_entities/test_tile_entities.c',
        'tests/tools/world/test_source_saving.py', 'cmake/tests/source_world_saving.cmake',
    )
    inputs = [Path(__file__).resolve(), *(ROOT / name for name in source_names),
              *executables.values()]
    before = {str(path): digest(path) for path in inputs}
    args.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='ew-cache-', dir=args.artifacts.resolve()))
    report = {
        'status': 'FAIL', 'scope': __doc__, 'mode': args.mode, 'inputs_before': before,
        'processes': [], 'rounds': args.rounds,
        'host': {'platform': platform.platform(), 'machine': platform.machine(),
                 'processor': os.environ.get('PROCESSOR_IDENTIFIER', platform.processor())},
        'workload': {'chunks': 9, 'points': 1024, 'calls_per_sample': 65536,
                     'samples_per_process': 7 if args.mode == 'measure' else 0,
                     'failure_controls': 8, 'success_branch_controls': 5,
                     'point_success_controls': 1024},
        'limits': [
            'Finite genuine World/EW CPU calls; no scene FPS or original-reference claim.',
            'The adapters call actual production accessors without duplicating their bodies.',
            'Both profiles use identical function-pointer dispatch and value-output work.',
            'Full public success errors and untouched private success sentinels are checked separately.',
            'Failure comparisons cover scalar/string/identity/nested fields without padding; NBT payload unions remain inactive with OK statuses.',
            'No affinity or scheduler controls; all samples and process order are retained.',
        ],
    }
    saving = load('ew_cpu_saved_inputs', ROOT / 'tests/tools/world/test_source_saving.py')
    expected = None
    try:
        for round_index in range(args.rounds):
            order = list(executables)
            if round_index % 2:
                order.reverse()
            for label in order:
                folder = output / f'{round_index}-{label}'
                authored = author(folder, saving)
                command = [str(executables[label]), str(folder), args.mode]
                started = time.monotonic()
                result = subprocess.run(command, cwd=folder, capture_output=True, timeout=120)
                (folder / 'native.stdout').write_bytes(result.stdout)
                (folder / 'native.stderr').write_bytes(result.stderr)
                process = {'round': round_index, 'label': label, 'command': command,
                           'seconds': time.monotonic() - started,
                           'returncode': result.returncode, 'authored_inputs': authored}
                report['processes'].append(process)
                require(result.returncode == 0 and not result.stderr, str(folder))
                lines = result.stdout.decode('ascii').splitlines()
                context = [fields(line) for line in lines if line.startswith('EW_CPU_CONTEXT ')]
                samples = [fields(line) for line in lines if line.startswith('EW_CPU_SAMPLE ')]
                controls = [line for line in lines if line.startswith('EW_CPU_CONTROL ')]
                finish = [line for line in lines if line.startswith('EW_CPU_FINISH ')]
                require(len(context) == 1 and len(controls) == 13 and len(finish) == 1 and
                        re.fullmatch(r'EW_CPU_FINISH checks=[1-9][0-9]* cleanup=1 '
                                     r'genuine-world=1 genuine-ew=1 gl=0', finish[0]),
                        'complete genuine EW process')
                require((context[0]['adapter'], context[0]['success-error']) in
                        (('public', 'full'), ('failure', 'untouched')),
                        'actual accessor adapter and success-error contract')
                require(len(samples) == (7 if args.mode == 'measure' else 0),
                        'complete timed sample inventory')
                for index, sample in enumerate(samples):
                    require(sample['sample'] == str(index) and sample['calls'] == '65536' and
                            int(sample['ticks']) > 0 and int(sample['frequency']) > 0,
                            'same finite source work')
                normalized = {
                    'context': {key: value for key, value in context[0].items()
                                if key not in ('adapter', 'success-error')},
                    'controls': controls, 'finish': finish,
                    'samples': [{key: value for key, value in row.items()
                                 if key not in ('ticks', 'frequency')} for row in samples],
                }
                if expected is None:
                    expected = normalized
                require(expected == normalized,
                        'equal values, failures, array/owner/hold effects and source work')
                after = inventory(folder / 'world')
                require(set(after) == set(authored) | {'session.lock'} and
                        all(after[key] == value for key, value in authored.items()) and
                        after['session.lock']['bytes'] == 8, 'unchanged authored saves')
                process.update(context=context[0], samples=samples, controls=controls,
                               finish=finish[0], semantic_equal=True,
                               authored_inputs_unchanged=True)
                print(f'{label} round{round_index}: PASS {process["seconds"]:.3f}s', flush=True)
        report.update(status='PASS', semantic_equivalence=True,
                      summary={label: value for label in executables
                               if (value := summarize(report['processes'], label)) is not None})
    finally:
        report['inputs_after'] = {str(path): digest(path) for path in inputs}
        report['inputs_unchanged'] = report['inputs_after'] == before
        if not report['inputs_unchanged']:
            report['status'] = 'FAIL'
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'status': report['status'], 'report': str(output / 'report.json'),
                          'summary': report.get('summary')}), flush=True)
    require(report['status'] == 'PASS', 'qualified EW CPU run')


if __name__ == '__main__':
    main()
