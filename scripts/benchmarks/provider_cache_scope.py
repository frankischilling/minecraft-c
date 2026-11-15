"""Run one fixed-work provider-cache scope scene on fresh authored World files.

Example (build mc_benchmark_provider_cache_scope with MC_BUILD_BENCHMARKS=ON):
  python scripts/benchmarks/provider_cache_scope.py --executable PATH \
      --artifacts benchmark-runs --chunks 64 --depth 2

Seven samples each issue 65,536 distributed resident GETs. Coordinate, status
and pointer folds are timed; setup, native state/ownership checks, scope closure
and cleanup are outside the clock. This single-process observation supports no
revision speedup, original-client parity or frame-rate claim.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import statistics
import struct
import sys
import tempfile
import zlib

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
BOUND = 2 * 1024 * 1024
CALLS, SAMPLES = 65536, 7
SOURCES = (
    'tests/benchmarks/provider_cache_scope.c', 'tests/benchmarks/world_queries.c',
    'tests/world/runtime/saving/mob_nbt_source_support.inc',
    'tests/world/runtime/tile_entities/test_tile_entities.c',
    'src/world/runtime/runtime.c', 'src/world/runtime/chunk_provider.c',
    'src/world/runtime/chunk_provider_owned.inc',
    'src/world/runtime/chunk_provider_internal.h', 'src/world/cache/cache.c',
    'src/world/cache/acquire.c',
)


def require(value, message):
    if not value:
        raise ValueError(message)


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def pin(path):
    require(path.is_file() and not path.is_symlink(), 'regular nonsymlink input file')
    digest = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            size += len(block)
            require(size <= 256 * BOUND, 'bounded input product')
            digest.update(block)
    return {'bytes': size, 'sha256': digest.hexdigest()}


def inventory(root):
    result, pending, entries, total = {}, [(root, 0)], 0, 0
    while pending:
        directory, depth = pending.pop()
        require(depth <= 3, 'bounded fixture directory depth')
        for path in directory.iterdir():
            entries += 1
            require(entries <= 64 and not path.is_symlink(), 'bounded nonsymlink fixture tree')
            if path.is_dir():
                pending.append((path, depth + 1))
                continue
            value = pin(path)
            total += value['bytes']
            require(value['bytes'] <= BOUND and total <= 4 * BOUND, 'bounded fixture files')
            result[path.relative_to(root).as_posix()] = value
    return dict(sorted(result.items()))


def source_inputs(directory, count, saving):
    require(count in (8, 64, 256), 'declared resident count')
    (directory / 'world/region').mkdir(parents=True)
    metadata, image = saving.seed('empty')
    (directory / 'world/level.dat').write_bytes(metadata)
    length, = struct.unpack_from('>i', image, 8192)
    require(1 < length < 4092 and image[8196] == 2, 'one-sector canonical source record')
    decoder = zlib.decompressobj()
    base = decoder.decompress(image[8197:8196 + length], BOUND + 1)
    require(len(base) <= BOUND and decoder.eof and not decoder.unused_data and
            not decoder.unconsumed_tail, 'bounded complete canonical source NBT')
    region = bytearray((2 + count) * 4096)
    for index in range(count):
        x, z = 2 * (index % 16), 2 * (index // 16)
        wire = base
        for key, coordinate in (('xPos', x), ('zPos', z)):
            old = saving.named(3, key, bytes(4))
            require(wire.count(old) == 1, 'one explicit authored coordinate')
            wire = wire.replace(old, saving.named(3, key, struct.pack('>i', coordinate)), 1)
        level = saving.Nbt(wire).root()['Level'][1]
        require(level['xPos'] == [3, x] and level['zPos'] == [3, z] and
                not level['Entities'][1][1] and not level['TileEntities'][1][1],
                'independently decoded empty authored Chunk')
        compressed = zlib.compress(wire)
        require(len(compressed) + 5 <= 4096, 'one-sector authored record')
        sector, slot = index + 2, (x & 31) + (z & 31) * 32
        struct.pack_into('>I', region, slot * 4, sector << 8 | 1)
        struct.pack_into('>i', region, 4096 + slot * 4, 123)
        record = struct.pack('>i', len(compressed) + 1) + b'\2' + compressed
        region[sector * 4096:sector * 4096 + len(record)] = record
    (directory / 'world/region/r.0.0.mcr').write_bytes(region)
    return inventory(directory)


def check_files(before, after):
    require(set(after) == set(before) | {'world/session.lock'} and
            all(after[name] == value for name, value in before.items()) and
            after['world/session.lock']['bytes'] == 8,
            'only the actual constructor eight-byte session token may change fixture files')


def fields(line, prefix, keys):
    parts = line.split(' ')
    require(parts[0] == prefix and all(parts), 'exact ' + prefix + ' record')
    result = {}
    for part in parts[1:]:
        key, separator, value = part.partition('=')
        require(separator and key not in result and re.fullmatch(r'0|[1-9][0-9]*', value),
                'unique canonical integer protocol fields')
        require(len(value) <= 20 and int(value) <= (1 << 64) - 1, 'bounded protocol integer')
        result[key] = int(value)
    require(set(result) == set(keys), 'closed ' + prefix + ' fields')
    return result


def checksum(count):
    value, mask = 1469598103934665603, (1 << 64) - 1
    for ordinal in range(CALLS):
        point = (ordinal * 73 + 17) & (count - 1)
        for coordinate in (2 * (point % 16), 2 * (point // 16)):
            value = ((value ^ (coordinate + ordinal)) * 1099511628211) & mask
    return value


def parse(stdout, count, depth):
    require(count in (8, 64, 256) and depth in (1, 2), 'declared fixed-work scene')
    require(len(stdout) <= BOUND and stdout.endswith('\n') and '\r' not in stdout,
            'bounded complete native stdout')
    lines = stdout.splitlines()
    if lines[:2] == ['16 achievements', '151 recipes']:
        lines = lines[2:]
    require(len(lines) == SAMPLES + 3, 'one context, seven samples, invariants and footer')
    context = fields(lines[0], 'PROVIDER_SCOPE_CONTEXT',
                     ('count', 'depth', 'calls', 'samples', 'pointer_bits'))
    require(context == dict(count=count, depth=depth, calls=CALLS, samples=SAMPLES,
                            pointer_bits=context['pointer_bits']) and
            context['pointer_bits'] in (32, 64), 'exact fixed-work native context')
    expected, samples = checksum(count), []
    for index, line in enumerate(lines[1:SAMPLES + 1]):
        sample = fields(line, 'PROVIDER_SCOPE_SAMPLE',
                        ('sample', 'calls', 'ticks', 'frequency', 'checksum', 'identity_guard'))
        require(sample['sample'] == index and sample['calls'] == CALLS and
                sample['ticks'] > 0 and sample['frequency'] > 0 and
                sample['checksum'] == expected and sample['identity_guard'] == 1,
                'ordered samples with exact work, checksum and identity guard')
        samples.append(sample)
    require(len({sample['frequency'] for sample in samples}) == 1, 'stable native clock frequency')
    flags = ('exact_identities', 'source_fields', 'random', 'cache', 'pending', 'diagnostics',
             'sentinels', 'ownership', 'scopes', 'generic_dispatch')
    invariants = fields(lines[-2], 'PROVIDER_SCOPE_INVARIANTS',
                        ('count', 'depth', 'calls', 'planes', 'checksum', *flags))
    require(invariants['count'] == count and invariants['depth'] == depth and
            invariants['calls'] == CALLS * SAMPLES and invariants['planes'] > 0 and
            invariants['checksum'] == expected and all(invariants[key] == 1 for key in flags),
            'complete native plane, state, Random, diagnostic, ownership and scope guards')
    finish = fields(lines[-1], 'PROVIDER_SCOPE_FINISH', ('checks', 'cleanup', 'genuine_source_world'))
    require(finish['checks'] > 0 and finish['cleanup'] == finish['genuine_source_world'] == 1,
            'checked genuine World cleanup')
    return {'context': context, 'samples': samples, 'invariants': invariants, 'finish': finish,
            'median_ns_per_get': statistics.median(
                sample['ticks'] / sample['frequency'] * 1e9 / sample['calls'] for sample in samples)}


def write_report(path, report):
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--chunks', type=int, choices=(8, 64, 256), default=64)
    parser.add_argument('--depth', type=int, choices=(1, 2), default=2)
    parser.add_argument('--timeout', type=int, choices=range(25, 121), default=120,
                        metavar='25..120', help='Native process limit in seconds (default: 120)')
    options = parser.parse_args(argv)
    executable = options.executable.resolve(strict=True)
    helpers = [ROOT / 'tests/tools/world/test_source_saving.py',
               ROOT / 'tests/reference/client/controller/capture.py']
    paths = list(dict.fromkeys([Path(__file__).resolve(), executable, *helpers,
                               *(ROOT / name for name in SOURCES)]))
    before = {str(path): pin(path) for path in paths}
    options.artifacts.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='provider-cache-scope-', dir=options.artifacts.resolve()))
    report = {'schema': 'PROVIDER_CACHE_SCOPE_RUN_V1', 'status': 'FAIL', 'scope': __doc__,
              'chunks': options.chunks, 'depth': options.depth, 'inputs_before': before,
              'qualification': 'One current executable, one fresh process. Native state guards and '
              'authored-file continuity are checked; source hashes record inputs, not a binary build proof.'}
    path = output / 'report.json'
    write_report(path, report)
    try:
        saving = load_module('provider_scope_public_seed', helpers[0])
        capture = load_module('provider_scope_public_capture', helpers[1])
        work = output / 'owned'
        report['authored_inputs'] = source_inputs(work, options.chunks, saving)
        command = [str(executable), str(work), str(options.chunks), str(options.depth)]
        report['native'] = capture.capture(command, output, 'native', options.timeout,
                                           working_directory=output)
        capture.require_success(report['native'], empty_stderr=True)
        report['files_after'] = inventory(work)
        check_files(report['authored_inputs'], report['files_after'])
        stream = output / report['native']['stdout_file']
        require(pin(stream) == {'bytes': report['native']['stdout_bytes'],
                                 'sha256': report['native']['stdout_sha256']},
                'native stdout still matches its complete process capture')
        stdout = stream.read_text(encoding='utf-8', errors='strict')
        report['observation'] = parse(stdout, options.chunks, options.depth)
        report['status'] = 'PASS_FIXED_WORK_SEMANTICS'
    except BaseException as error:
        report['error'] = repr(error)
        raise
    finally:
        active_error = sys.exc_info()[0] is not None
        after, errors = {}, {}
        for source in paths:
            try:
                after[str(source)] = pin(source)
            except Exception as error:
                errors[str(source)] = repr(error)
        report.update(inputs_after=after, input_errors=errors, inputs_unchanged=before == after)
        if before != after:
            report['status'] = 'FAIL'
            report['continuity_error'] = 'Executable, source or helper changed during the run'
        write_report(path, report)
        if before != after and not active_error:
            raise ValueError(report['continuity_error'])
    print(f'Provider cache scope PASS chunks={options.chunks} depth={options.depth}; report: {path}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
