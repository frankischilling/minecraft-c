"""Compare genuine Grass and Leaves calls with complete retained CPU attempts.

The detailed adapter builds the current production leaf source with two getter
aliases. Both executables use one compiled workload object. Construction,
inspection, serialization and output are outside the sampled leaf calls.
These declared cache-hit workloads do not measure scene FPS.
"""
import argparse
import copy
import hashlib
import json
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
MODES = ('grass_bright', 'leaves_flag0', 'leaves_connected')
CALLS = dict(zip(MODES, (32768, 32768, 512)))


def require(value, message):
    if not value:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def connected_targets():
    return [(x, y, z) for y in range(5, 23) for z in range(5, 11)
            for x in (5, 6, 7, 9, 10)][:512]


def inputs(mode):
    """Author serialized inputs before loading; never repair a live trajectory."""
    require(mode in MODES, 'Declared workload')
    arrays = [bytearray(length) for length in (32768, 16384, 16384, 16384, 256)]

    def nibble(array, x, y, z, value):
        index = (x << 11) | (z << 7) | y
        shift = (index & 1) * 4
        array[index >> 1] = (array[index >> 1] & ~(15 << shift)) | (value << shift)

    def cell(x, y, z, block, metadata=0):
        arrays[0][(x << 11) | (z << 7) | y] = block
        nibble(arrays[1], x, y, z, metadata)
        if block:
            arrays[4][(z << 4) | x] = max(arrays[4][(z << 4) | x], y + 1)

    if mode == 'grass_bright':
        cell(8, 64, 8, 2)
        cell(7, 64, 8, 3)
        nibble(arrays[2], 8, 65, 8, 15)
        nibble(arrays[2], 7, 65, 8, 15)
    elif mode == 'leaves_flag0':
        cell(8, 64, 8, 18)
    else:
        for x in range(1, 15):
            for z in range(1, 15):
                for y in range(1, 27):
                    cell(x, y, z, 17 if x == 8 else 18)
        for x, y, z in connected_targets():
            nibble(arrays[1], x, y, z, 8)
    return arrays


def tag(kind, name, value):
    encoded = name.encode('ascii')
    return bytes([kind]) + struct.pack('>H', len(encoded)) + encoded + value


def authored_world(path, mode):
    arrays = inputs(mode)
    names = ('Blocks', 'Data', 'SkyLight', 'BlockLight', 'HeightMap')
    fields = [tag(3, 'xPos', bytes(4)), tag(3, 'zPos', bytes(4)),
              tag(4, 'LastUpdate', bytes(8))]
    fields += [tag(7, name, struct.pack('>i', len(data)) + data)
               for name, data in zip(names, arrays)]
    fields += [tag(1, 'TerrainPopulated', b'\1'), tag(9, 'Entities', b'\x0a\0\0\0\0'),
               tag(9, 'TileEntities', b'\x0a\0\0\0\0')]
    data = tag(10, '', tag(10, 'Level', b''.join(fields) + b'\0') + b'\0')
    payload = b'\2' + zlib.compress(data)
    record = struct.pack('>I', len(payload)) + payload
    sectors = (len(record) + 4095) // 4096
    header = bytearray(8192)
    struct.pack_into('>I', header, 0, (2 << 8) | sectors)
    region = path / 'region'
    region.mkdir(parents=True)
    (region / 'r.0.0.mcr').write_bytes(header + record + bytes(sectors * 4096 - len(record)))


def author_inputs(path, measure):
    for mode in MODES:
        for ordinal in range(9 if measure else 1):
            authored_world(path / mode / f'sample{ordinal}' / 'world0', mode)


def file_receipts(path):
    return {p.relative_to(path).as_posix(): {'bytes': p.stat().st_size, 'sha256': digest(p)}
            for p in sorted(path.rglob('*')) if p.is_file()}


def input_receipts(path):
    # Genuine fresh Store construction also writes level/session files outside
    # sampling. These immutable serialized Chunk inputs remain separate.
    return {name: value for name, value in file_receipts(path).items() if name.endswith('.mcr')}


def source_receipts():
    paths = [ROOT / 'CMakeLists.txt', ROOT / '.clang-format']
    for name in ('src', 'include', 'cmake', 'tests/benchmarks', 'tests/world/runtime'):
        paths += [p for p in (ROOT / name).rglob('*') if p.is_file() and
                  p.suffix in ('.c', '.h', '.inc', '.cmake')]
    paths.append(Path(__file__).resolve())
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(set(paths))}


def summarize(profiles, label):
    result = {}
    for mode in MODES:
        rows = [row for profile in profiles if profile['label'] == label
                for row in profile['parsed']['samples']
                if row['mode'] == mode and row['timed'] == 1]
        values = [1e9 * row['ticks'] / row['frequency'] / row['calls'] for row in rows]
        result[mode] = {'samples': len(values), 'nanoseconds_per_call': values}
        if values:
            result[mode]['median_ns_per_call'] = statistics.median(values)
    return result


def fields(line):
    pairs = [part.split('=', 1) for part in line.split()[1:]]
    require(all(len(pair) == 2 for pair in pairs) and len(dict(pairs)) == len(pairs),
            'Complete unique typed fields')
    return dict(pairs)


def unsigned(value):
    require(re.fullmatch(r'[0-9]+', value) is not None, 'Unsigned decimal observation')
    result = int(value)
    require(result < 1 << 64, 'Unsigned 64-bit observation')
    return result


def seed(value):
    return (value ^ 0x5deece66d) & ((1 << 48) - 1)


def grass_random_after():
    state = seed(0)
    for _ in range(CALLS['grass_bright']):
        for bound in (3, 5, 3):
            while True:
                state = (state * 0x5deece66d + 11) & ((1 << 48) - 1)
                bits = state >> 17
                value = bits % bound
                if bits - value + bound - 1 < 1 << 31:
                    break
    return state


GRASS_RANDOM_AFTER = grass_random_after()


def parse_cpu_run(stdout, measure):
    lines = stdout.splitlines()
    count = 9 if measure else 1
    require(len(lines) == 4 + 3 * count and
            lines[:2] == ['16 achievements', '151 recipes'], 'Complete ordered CPU records')
    require(lines[2].startswith('RANDOM_BLOCKS_CPU_CONTEXT '), 'One context')
    context = fields(lines[2])
    require(set(context) == {'adapter', 'samples', 'measure', 'modes'} and
            context['adapter'] in ('detailed', 'failure') and
            unsigned(context['samples']) == count and
            unsigned(context['measure']) == int(measure) and unsigned(context['modes']) == 3,
            'Exact adapter and fixed-work context')
    samples = []
    index = 3
    for mode in MODES:
        for ordinal in range(count):
            line = lines[index]
            require(line.startswith('RANDOM_BLOCKS_CPU_SAMPLE '), 'Ordered sample record')
            row = fields(line)
            expected = {'mode', 'ordinal', 'timed', 'ticks', 'frequency', 'calls', 'returned',
                        'owners-same', 'random-checked', 'scratch-same', 'holds-same', 'events',
                        'cache', 'scratch', 'supplied-before', 'supplied-after',
                        'world-before', 'world-after'}
            require(set(row) == expected and row['mode'] == mode, 'Exact sample schema and order')
            numbers = {key: unsigned(value) for key, value in row.items()
                       if key not in ('mode', 'supplied-before', 'supplied-after',
                                      'world-before', 'world-after')}
            require(numbers['ordinal'] == ordinal and
                    numbers['timed'] == int(measure and ordinal >= 2) and
                    numbers['calls'] == numbers['returned'] == CALLS[mode] and
                    numbers['frequency'] > 0 and
                    (numbers['ticks'] > 0 if numbers['timed'] else numbers['ticks'] >= 0) and
                    all(numbers[key] == 1 for key in
                        ('owners-same', 'random-checked', 'scratch-same', 'holds-same', 'cache')) and
                    numbers['scratch'] == int(mode == 'leaves_connected') and
                    (numbers['events'] > 0 if mode == 'grass_bright' else numbers['events'] == 0),
                    'Full source calls, mutation event and native ownership guards')
            randoms = {key: value for key, value in row.items() if key.endswith(('before', 'after'))}
            require(all(re.fullmatch(r'[0-9a-f]{12}', value) for value in randoms.values()),
                    'Exact Java Random state words')
            require(int(row['supplied-before'], 16) == seed(0) and
                    int(row['world-before'], 16) == int(row['world-after'], 16) == seed(89) and
                    int(row['supplied-after'], 16) ==
                    (GRASS_RANDOM_AFTER if mode == 'grass_bright' else seed(0)),
                    'Independent complete deterministic World and supplied Random progression')
            samples.append({'mode': mode, **numbers, **randoms})
            index += 1
    require(re.fullmatch(r'RANDOM_BLOCKS_CPU_FINISH checks=[1-9][0-9]* cleanup=1 modes=3',
                         lines[index]) is not None, 'Complete native cleanup')
    return {'context': context, 'samples': samples, 'finish': lines[index]}


def semantic_profile(profile):
    parsed = profile['parsed']
    return {'context': {k: v for k, v in parsed['context'].items() if k != 'adapter'},
            'samples': [{k: v for k, v in row.items() if k not in ('ticks', 'frequency')}
                        for row in parsed['samples']], 'finish': parsed['finish'],
            'state_files': {k: v for k, v in profile['state_files'].items() if not k.endswith('.json')},
            'states': {k: json.dumps(normalize_native_state(v), separators=(',', ':'))
                       for k, v in profile['states'].items()}}


def compare_cpu_runs(profiles, expected_order):
    require(profiles and [p['label'] for p in profiles] == expected_order and
            set(expected_order) == {'detailed', 'failure'}, 'Complete declared ordered cohort')
    for profile in profiles:
        require(profile['status'] == 'PASS' and type(profile['returncode']) is int and
                profile['returncode'] == 0 and profile['stderr'] == '' and 'error' not in profile,
                'Every native attempt qualified')
        parsed = profile['parsed']
        require(parsed == parse_cpu_run(profile['stdout'], parsed['context']['measure'] == '1') and
                parsed['context']['adapter'] == profile['label'], 'Retained raw CPU stream and adapter')
    result = semantic_profile(profiles[0])
    require(all(semantic_profile(profile) == result for profile in profiles),
            'Exact source state, event order, Random, calls, scratch and ownership across cohort')
    return result


def artifact_names(measure):
    return {f'{mode}/sample{ordinal}.{suffix}' for mode in MODES
            for ordinal in range(9 if measure else 1)
            for suffix in ('before.bin', 'after.bin', 'events.bin', 'state.json')}


def input_names(measure):
    return {f'{mode}/sample{ordinal}/world0/region/r.0.0.mcr' for mode in MODES
            for ordinal in range(9 if measure else 1)}


def validate_input_receipts(receipts, measure):
    require(set(receipts) == input_names(measure), 'Every declared authored region input')
    require(all(set(value) == {'bytes', 'sha256'} and type(value['bytes']) is int and
                value['bytes'] >= 12288 and re.fullmatch(r'[0-9a-f]{64}', value['sha256'])
                for value in receipts.values()), 'Complete authored input identities')


def validate_file_receipts(receipts, measure):
    require(set(receipts) == artifact_names(measure), 'Every complete native state artifact')
    for name, receipt in receipts.items():
        require(set(receipt) == {'bytes', 'sha256'} and type(receipt['bytes']) is int and
                receipt['bytes'] >= 0 and re.fullmatch(r'[0-9a-f]{64}', receipt['sha256']),
                'Complete exact-byte artifact receipt')
        if name.endswith(('before.bin', 'after.bin')):
            require(receipt['bytes'] >= 82176, 'Complete Chunk planes and state')
        elif name.endswith('state.json'):
            require(receipt['bytes'] > 0, 'Structured native state')


def validate_artifacts(path, measure):
    receipts = file_receipts(path)
    validate_file_receipts(receipts, measure)
    for name in receipts:
        if name.endswith('state.json'):
            require(isinstance(json.loads((path / name).read_text()), dict), 'Structured native state')
    return receipts


def parse_snapshot(data):
    """Read the complete portable state stream, including queue identity joins."""
    offset = 0

    def take(size):
        nonlocal offset
        require(0 <= size <= len(data) - offset, 'Complete snapshot payload')
        value = data[offset:offset + size]
        offset += size
        return value

    def word():
        return struct.unpack('>I', take(4))[0]

    require(take(4) == b'RBS1', 'Native snapshot signature')
    arrays = []
    for size in (32768, 16384, 16384, 16384, 256):
        require(word() == size, 'Exact complete source plane length')
        arrays.append(take(size))
    length = word()
    require(length in (0xffffffff, 32768), 'Genuine retained Leaves scratch extent')
    scratch = None if length == 0xffffffff else take(4 * length)
    count = word()
    require(count <= 1000000, 'Bounded complete light queue')
    light = []
    for index in range(count):
        row = struct.unpack('>IIiiiiii', take(32))
        require(row[0] <= index and row[1] in (0, 1), 'Light identity and actual type join')
        if row[0] < index:
            require(light[row[0]][0] == row[0] and row[1:] == light[row[0]][1:],
                    'First-occurrence light identity and complete shared fields')
        light.append(row)
    membership_count, ordered_count = word(), word()
    require(membership_count <= 128 and ordered_count <= 128, 'Declared complete scheduler extents')
    membership = [struct.unpack('>iiiiqq', take(32)) for _ in range(membership_count)]
    ordered = [struct.unpack('>iiiiqqi', take(36)) for _ in range(ordered_count)]
    for row in ordered:
        require(row[-1] == -1 or 0 <= row[-1] < len(membership), 'Actual scheduler identity join')
        if row[-1] >= 0:
            require(tuple(row[:-1]) == membership[row[-1]], 'Shared scheduler occurrence contents')
    require(offset == len(data), 'No omitted or trailing snapshot fields')
    return {'arrays': arrays, 'scratch': scratch, 'light': light,
            'ordered': ordered, 'membership': membership}


def parse_events(data):
    require(len(data) >= 8 and data[:4] == b'RBE1', 'Native ordered event signature')
    count = struct.unpack_from('>I', data, 4)[0]
    require(count <= 4096 and len(data) == 8 + 28 * count, 'Complete bounded ordered event stream')
    events = [struct.unpack_from('>Iiiiiii', data, 8 + 28 * index) for index in range(count)]
    require(all(row[0] in (1, 2) for row in events), 'Actual cell/range callback kinds')
    return events


def normalize_native_state(state):
    """Transport only an unreached cave owner's verified unchanged constructor word.

    Raw complete triples remain in each profile and bind to its exact JSON bytes.
    No game owner, Random state, clock input or sampled source work is changed.
    """
    triples = [state[phase]['generation_randoms'] for phase in ('before', 'after')]
    for pair in triples:
        require(isinstance(pair, list) and len(pair) == 2, 'Both actual generation Random owners')
        for value in pair:
            require(isinstance(value, list) and len(value) == 3 and
                    re.fullmatch(r'[0-9a-f]{12}', value[0]) and type(value[1]) is int and
                    value[1] in (0, 1) and re.fullmatch(r'[0-9a-f]{16}', value[2]),
                    'Complete actual generation Random triples')
    require(triples[0] == triples[1], 'Both unreached generation Random owners remain unchanged')
    require(triples[0][1][1:] == [0, '0000000000000000'], 'Actual untouched cave constructor cache')
    normalized = copy.deepcopy(state)
    for phase in ('before', 'after'):
        normalized[phase]['generation_randoms'][1][0] = 'unchanged independent cave constructor word'
    return normalized


def state_receipts(path, measure):
    states = {}
    for name in sorted(artifact_names(measure)):
        if name.endswith('.state.json'):
            raw = (path / name).read_bytes()
            state = json.loads(raw)
            require((json.dumps(state, separators=(',', ':')) + '\n').encode() == raw,
                    'Exact retained native scalar state encoding')
            normalize_native_state(state)
            states[name] = state
    return states


def validate_state_record(state, row):
    normalize_native_state(state)
    require(all(type(state[key]) is int for key in
                ('calls', 'returned', 'events', 'scratch_after', 'scratch_before')) and
            state['calls'] == state['returned'] == row['calls'] and
            state['events'] == row['events'] and
            all(state[key] is True for key in
                ('owners_same', 'world_fields_same', 'holds_same', 'scratch_same')) and
            state['scratch_after'] == row['scratch'] and
            state['scratch_before'] == int(row['mode'] == 'leaves_connected' and row['ordinal'] > 0) and
            state['error'] == [0, 0, 0, 0, 0] and
            all(type(value) is int for value in state['error']),
            'Native scalar/owner/event/error work joins')
    for phase in ('before', 'after'):
        require(state[phase]['supplied'][0] == row['supplied-' + phase] and
                state[phase]['world_random'][0] == row['world-' + phase] and
                type(state[phase]['supplied'][1]) is int and
                type(state[phase]['world_random'][1]) is int and
                state[phase]['supplied'][1:] == [0, '0000000000000000'] and
                state[phase]['world_random'][1:] == [0, '0000000000000000'],
                'Complete deterministic Random triples')


def validate_native_artifacts(path, parsed):
    """Join independent authored-plane/Random checks with the actual native rows."""
    for row in parsed['samples']:
        prefix = path / row['mode'] / f"sample{row['ordinal']}"
        before = parse_snapshot(Path(str(prefix) + '.before.bin').read_bytes())
        after = parse_snapshot(Path(str(prefix) + '.after.bin').read_bytes())
        expected = inputs(row['mode'])
        require(before['arrays'] == [bytes(value) for value in expected], 'Actual loaded authored source planes')
        if row['mode'] == 'grass_bright':
            expected[0][(7 << 11) | (8 << 7) | 64] = 2
        elif row['mode'] == 'leaves_connected':
            expected = inputs('leaves_connected')
            for x, y, z in connected_targets():
                index = (x << 11) | (z << 7) | y
                expected[1][index >> 1] &= ~(15 << ((index & 1) * 4))
        require(after['arrays'] == [bytes(value) for value in expected], 'Complete independent source mutation planes')
        require((after['scratch'] is not None) == bool(row['scratch']) and
                (before['scratch'] is not None) == (row['mode'] == 'leaves_connected' and row['ordinal'] > 0),
                'Natural persistent scratch publication across fresh Worlds')
        events = parse_events(Path(str(prefix) + '.events.bin').read_bytes())
        require(len(events) == row['events'], 'Ordered callbacks joined to actual native sample')
        require(events == ([(1, 7, 64, 8, 0, 0, 0)] if row['mode'] == 'grass_bright' else []),
                'Complete independent declared source callback order and coordinates')
        state = json.loads(Path(str(prefix) + '.state.json').read_text())
        validate_state_record(state, row)
    return {'samples': len(parsed['samples']), 'snapshots': 2 * len(parsed['samples']),
            'source_planes': 10 * len(parsed['samples']), 'ordered_events': True,
            'source_mutations': True, 'random_triples': True, 'queue_identity_joins': True}


def cohort_order(rounds):
    require(type(rounds) is int and 1 <= rounds <= 10, 'Bounded declared rounds')
    return [label for index in range(rounds)
            for label in (('detailed', 'failure') if index % 2 == 0 else ('failure', 'detailed'))]


def audit_receipt(report):
    """Reject lost attempts, contradictory raw streams and incomplete state guards."""
    require(report['status'] == 'PASS' and report['errors'] == [] and
            report['mode'] in ('measure', 'controls') and report['unchanged'] is True,
            'Qualified complete receipt')
    expected = cohort_order(report['rounds'])
    require(report['order'] == expected, 'Declared AB/BA order')
    required_sources = {'src/world/runtime/mutation/random_blocks.c',
                        'src/world/runtime/blocks.c', 'tests/benchmarks/random_blocks.c',
                        'tests/benchmarks/random_blocks_adapter.h',
                        'tests/benchmarks/random_blocks_detailed.c',
                        'tests/benchmarks/random_blocks_failure.c',
                        'tests/tools/world/benchmark_random_blocks.py'}
    require(required_sources <= set(report['source_before']) and
            report['source_before'] == report['source_after'] and
            all(re.fullmatch(r'[0-9a-f]{64}', value)
                for value in report['source_before'].values()), 'Unchanged complete source guard')
    require(set(report['products_before']) == {'detailed', 'failure'} and
            report['products_before'] == report['products_after'] and
            all(re.fullmatch(r'[0-9a-f]{64}', value)
                for value in report['products_before'].values()), 'Unchanged both executables')
    measure = report['mode'] == 'measure'
    anchor_inputs = None
    for profile in report['profiles']:
        require(profile['parsed']['context']['measure'] == str(int(measure)), 'Actual measurement mode')
        validate_file_receipts(profile['state_files'], measure)
        checks = profile['native_checks']
        require(checks == {'samples': 3 * (9 if measure else 1),
                'snapshots': 6 * (9 if measure else 1), 'source_planes': 30 * (9 if measure else 1),
                'ordered_events': True, 'source_mutations': True, 'random_triples': True,
                'queue_identity_joins': True} and
                all(type(checks[key]) is int for key in ('samples', 'snapshots', 'source_planes')) and
                all(type(checks[key]) is bool for key in
                    ('ordered_events', 'source_mutations', 'random_triples', 'queue_identity_joins')),
                'Complete independent native state audit')
        require(set(profile['states']) == {name for name in artifact_names(measure)
                                           if name.endswith('.state.json')}, 'Every retained scalar state')
        for name, state in profile['states'].items():
            normalize_native_state(state)
            raw = (json.dumps(state, separators=(',', ':')) + '\n').encode()
            require(profile['state_files'][name] == {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()},
                    'Retained full actual scalar states bind exact native JSON bytes')
        for row in profile['parsed']['samples']:
            name = f"{row['mode']}/sample{row['ordinal']}.state.json"
            validate_state_record(profile['states'][name], row)
        validate_input_receipts(profile['inputs_before'], measure)
        require(profile['inputs_before'] == profile['inputs_after'], 'Every authored region input unchanged')
        if anchor_inputs is None:
            anchor_inputs = profile['inputs_before']
        require(profile['inputs_before'] == anchor_inputs, 'Identical declared inputs across cohort')
    result = compare_cpu_runs(report['profiles'], expected)
    require(report['semantic_comparison'] == result and
            report['summary'] == {label: summarize(report['profiles'], label)
                                  for label in ('detailed', 'failure')},
            'Complete semantic comparison and all measured samples')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', action='append', required=True, metavar='detailed=PATH|failure=PATH')
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=4)
    parser.add_argument('--mode', choices=('measure', 'controls'), default='measure')
    args = parser.parse_args()
    pairs = [value.split('=', 1) for value in args.executable]
    require(1 <= args.rounds <= 10 and len(pairs) == 2 and all(len(pair) == 2 for pair in pairs),
            'Two bounded adapter profiles')
    executables = {label: Path(path).resolve(strict=True) for label, path in pairs}
    require(set(executables) == {'detailed', 'failure'}, 'One detailed and one failure executable')
    measure = args.mode == 'measure'
    args.artifacts.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix='random-blocks-', dir=args.artifacts)).resolve()
    source_before = source_receipts()
    products_before = {label: digest(path) for label, path in executables.items()}
    order = cohort_order(args.rounds)
    report = {'status': 'FAIL', 'mode': args.mode, 'rounds': args.rounds, 'order': order, 'profiles': [],
              'platform': platform.platform(), 'source_before': source_before,
              'products_before': products_before, 'errors': [],
              'scope': 'Fixed-work direct genuine Grass and Leaves cache-hit workloads. '
                       'All samples and failed processes retained; no passing subset, '
                       'overhead subtraction or FPS claim. Source planes, scratch and queues '
                       'and events compare byte for byte. Full native scalar states remain retained; '
                       'only the verified unchanged unreached cave constructor Random state word '
                       'is normalized across processes. No entropy or game state is repaired.'}
    (directory / 'release.json').write_text(json.dumps(report, indent=2) + '\n')
    anchor = None
    try:
        for ordinal, label in enumerate(order):
            run = directory / f'{ordinal}-{label}'
            authored = run / 'inputs'
            output = run / 'output'
            author_inputs(authored, measure)
            for mode in MODES:
                (output / mode).mkdir(parents=True)
            inputs_before = input_receipts(authored)
            command = [str(executables[label]), str(authored), str(output), args.mode]
            profile = {'label': label, 'argv': command, 'status': 'FAIL', 'inputs_before': inputs_before}
            report['profiles'].append(profile)
            start = time.monotonic()
            try:
                result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=180)
                (run / 'stdout').write_bytes(result.stdout)
                (run / 'stderr').write_bytes(result.stderr)
                profile.update(returncode=result.returncode, seconds=time.monotonic() - start,
                               stdout=result.stdout.decode('utf8', errors='strict'),
                               stderr=result.stderr.decode('utf8', errors='strict'))
                require(result.returncode == 0 and not result.stderr, 'Successful complete native process')
                profile['parsed'] = parse_cpu_run(profile['stdout'], measure)
                require(profile['parsed']['context']['adapter'] == label, 'Actual selected adapter')
                profile['state_files'] = validate_artifacts(output, measure)
                profile['native_checks'] = validate_native_artifacts(output, profile['parsed'])
                profile['states'] = state_receipts(output, measure)
                profile['inputs_after'] = input_receipts(authored)
                require(inputs_before == profile['inputs_after'], 'Authored serialized input unchanged')
                if anchor is None:
                    anchor = output
                else:
                    require(all((anchor / name).read_bytes() == (output / name).read_bytes()
                                for name in profile['state_files'] if not name.endswith('.json')),
                            'Byte-exact complete native planes, scratch, queues and ordered events')
                profile['status'] = 'PASS'
            except Exception as error:
                if isinstance(error, subprocess.TimeoutExpired):
                    raw_out, raw_error = error.stdout or b'', error.stderr or b''
                    (run / 'stdout').write_bytes(raw_out)
                    (run / 'stderr').write_bytes(raw_error)
                    profile.update(returncode=None, seconds=time.monotonic() - start,
                                   stdout=raw_out.decode('utf8', errors='replace'),
                                   stderr=raw_error.decode('utf8', errors='replace'))
                profile['error'] = str(error)
                report['errors'].append(f'process{ordinal}: {error}')
        require(not report['errors'], 'Every declared process qualified')
        report['semantic_comparison'] = compare_cpu_runs(report['profiles'], order)
        report['summary'] = {label: summarize(report['profiles'], label) for label in executables}
        report['status'] = 'PASS'
    except Exception as error:
        report['errors'].append(str(error))
    finally:
        report['source_after'] = source_receipts()
        report['products_after'] = {label: digest(path) for label, path in executables.items()}
        report['unchanged'] = source_before == report['source_after'] and products_before == report['products_after']
        if not report['unchanged']:
            report['status'] = 'FAIL'
            report['errors'].append('Source or executable changed during cohort')
        if report['status'] == 'PASS':
            try:
                audit_receipt(report)
            except Exception as error:
                report['status'] = 'FAIL'
                report['errors'].append(str(error))
        (directory / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(report['status'], directory / 'report.json')
        for error in report['errors']:
            print(error, file=sys.stderr)
    return 0 if report['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
