"""Validate actual complete execution, bounded live observations and independent RNG/LCG replay."""
import hashlib
import json
import re
import struct
from pathlib import Path

MULT = 0x5deece66d
MASK = (1 << 48) - 1


def need(ok, message):
    if not ok:
        raise ValueError(message)


def rows(path):
    need(path.stat().st_size < 8 * 1024 * 1024, 'bounded observation transport')
    return [json.loads(line) for line in path.read_text().splitlines()]


def i32(x):
    return (x + (1 << 31)) % (1 << 32) - (1 << 31)


def step(x):
    return i32(x * 3 + 1013904223)


def f32(x):
    return struct.unpack('>f', struct.pack('>f', x))[0]


def fhex(x):
    return struct.pack('>f', x).hex()


class Random:
    def __init__(self, seed, internal=False):
        self.seed = seed if internal else (seed ^ MULT) & MASK
        self.calls = []

    def bits(self, width):
        self.seed = (self.seed * MULT + 11) & MASK
        return self.seed >> (48 - width)

    def integer(self, bound):
        while True:
            bits = self.bits(31)
            value = (bound * bits) >> 31 if bound & (bound - 1) == 0 else bits % bound
            if bound & (bound - 1) == 0 or bits - value + bound - 1 < 1 << 31:
                self.calls.append(('nextInt', [bound], value, f'{self.seed:012x}'))
                return value

    def floating(self):
        value = self.bits(24) / (1 << 24)
        self.calls.append(('nextFloat', [], {'F': fhex(value)}, f'{self.seed:012x}'))
        return value

    def double(self):
        value = ((self.bits(26) << 27) + self.bits(27)) / (1 << 53)
        self.calls.append(('nextDouble', [], {'D': struct.pack('>d', value).hex()}, f'{self.seed:012x}'))
        return value


def frame_oracle(row, order):
    """Source literal replay, using read-only observed actual JDK Set order, never source execution."""
    index = row['index']
    random = Random(row['r_seed_input'])
    g, q = row['g_input'], row['Q_input']
    q -= q > 0
    acquisitions = completed = ambient = climate = lightning = 0
    first_samples, first_calls = [], []
    sound = False
    for cx, cz in ([] if index == 0 else order):
        acquisitions += 1
        if q == 0:
            ambient += 1
            g = step(g)
            if index not in (2, 12):
                v = g >> 2
                x, y, z = (v & 15) + cx * 16, (v >> 16) & 127, ((v >> 8) & 15) + cz * 16
                random.integer(8)
                distance = x * x + (y - 64) ** 2 + (z - 4) ** 2
                if 4 < distance < 64:
                    random.floating()
                    if index == 4:
                        sound = True
                        break
                    q = random.integer(12000) + 6000
                    sound = True
        if index == 13:
            break
        ticket = random.integer(100000)
        if ticket == 0 and index == 11:
            g = step(g)
            lightning += 1
        climate_ticket = random.integer(16)
        if climate_ticket == 0:
            g = step(g)
            climate += 1
        for sample in range(80):
            g = step(g)
            if index == 7:
                break
            if acquisitions == 1:
                v = g >> 2
                x, y, z = v & 15, (v >> 16) & 127, (v >> 8) & 15
                first_samples.append((g, [x, y, z], (x << 11) | (z << 7) | y))
            if index == 8:
                break
        if acquisitions == 1:
            first_calls = list(random.calls)
        if index in (7, 8):
            break
        completed += 1
        if index == 5 and sound:
            break
    return dict(g=g, Q=q, seed=f'{random.seed:012x}', acquisitions=acquisitions,
                completed=completed, ambient=ambient, climate=climate, lightning=lightning,
                samples=first_samples, first_calls=first_calls)


def validate(directory, pins):
    directory = Path(directory)
    normal = rows(directory / 'normal/observations.ndjson')
    debug = rows(directory / 'debugger/target-observations.ndjson')
    need(normal == debug, 'ordinary/read-only-debugger deterministic observations')
    frames = [r for r in normal if r['event'] == 'frame']
    blocks = [r for r in normal if r['event'] == 'block']
    need(len(frames) == 14 and len(blocks) == 18, '14 complete World/18 actual Block profiles')
    records = rows(directory / 'debugger/checkpoints.ndjson')
    code = [r for r in records if r['event'] == 'code-pin']
    for pin in code:
        m = next(m for m in pins['classes'][pin['class']]['methods'] if (m['name'], m['descriptor']) == (pin['method'], pin['descriptor']))
        need((pin['bytes'], pin['sha256']) == (m['code_bytes'], m['code_sha256']), 'actual live original Code')
    need(len(code) == 19, 'all selected live method pins')
    points = [r for r in records if r['event'] == 'checkpoint']
    summaries = {r['case']: r for r in records if r['event'] == 'summary'}
    need(set(summaries) == set(range(14)) | set(range(100, 118)), 'actual active-call lifetime summaries')
    order = next(p['actual_P_order'] for p in points if 'actual_P_order' in p)
    need(len(order) == len({tuple(p) for p in order}) == 361 and set(map(tuple, order)) == {(x, z) for x in range(-9, 10) for z in range(-9, 10)}, 'actual full radius9 Set membership/order')
    need(order[0] == [-2, -2], 'actual captured first Chunk')
    set_hash = hashlib.sha256(b''.join(struct.pack('>ii', *p) for p in order)).hexdigest()
    randoms = [r for r in records if r['event'] == 'random']
    exceptions = [r for r in records if r['event'] == 'source_exception']
    for index, row in enumerate(frames):
        need(row['index'] == index and row['h_unchanged'] == 1013904223, 'frame identity/final h unchanged')
        cells = row['row'].split()
        need(len(cells) == 16 and len(row['hash_row'].split()) == 7, 'frame/hash transport schema')
        expected = frame_oracle(row, order)
        need(cells[4] == f'{expected["g"] & 0xffffffff:08x}' and int(cells[5]) == expected['Q'], 'full literal g/Q replay')
        need(cells[6] == ('-' if index == 13 else expected['seed']), 'full World Random seed replay')
        need(int(cells[3]) == (-1 if index in (4, 5, 7, 8, 13) else 1), 'actual return/failure outcome')
        summary = summaries[index]
        need((summary['acquisitions'], summary['completed_chunks'], summary['ambient_steps'], summary['climate_steps'], summary['lightning_steps']) ==
             (expected['acquisitions'], expected['completed'], expected['ambient'], expected['climate'], expected['lightning']), 'actual branch/Chunk checkpoints versus independent replay')
        samples = [p for p in points if p['case'] == index and p['location']['pc'] == 951]
        need([(p['g'], p['xyz'], p['packed']) for p in samples] == expected['samples'], 'actual first-Chunk raw sampling/g/packing')
        calls = [r for r in randoms if r['case'] == index and r['role'] == 'world']
        need([(r['method'], r['arguments'], r['result'], r['seed']) for r in calls] == expected['first_calls'], 'actual first-Chunk World RNG argument/return/state replay')
        if index not in (0, 5):
            need(row['hash_row'].split()[-1] == set_hash, 'actual final captured-set iteration hash')
        if index in (9, 10):
            need(summary['observed_tick_calls'] == 0 and summary['tick_scope'] == 'whole-method', 'actual whole-method selected Block invocation count')
        if index == 6:
            need(all(p['held_class'] == 'li' and p['raw_id'] == 2 for p in samples), 'actual fallback raw bytes despite virtual-air semantics')
            need(summary['observed_tick_calls'] == 80, 'actual first Chunk remote Grass dispatches')
        if index == 12:
            need(all(p['held_class'] == 'lm' and p['held_coords'] == [0, 0] and p['raw_id'] == 1 for p in samples), 'actual normal Chunk identity alias')
    for index, pc in [(7, 944), (8, 982), (13, 470)]:
        need(any(r['case'] == index and r['exception'] == 'java.lang.NullPointerException' and r['location'] == {'class': 'fd', 'method': 'n', 'descriptor': '()V', 'pc': pc} for r in exceptions), 'actual failing source BCI ' + str(pc))
    need(any(r['case'] == 5 and r['exception'] == 'java.util.ConcurrentModificationException' and any(f['class'] == 'fd' and f['method'] == 'n' and f['pc'] == 172 for f in r['source_frames']) for r in exceptions), 'actual captured JDK iterator fail-fast')
    for index, row in enumerate(blocks):
        need(row['index'] == index and not row['parameter_is_world_random'] and row['ice_opacity'] == 3 and row['scratch_same'], 'actual Block fixture identity/inputs')
        cells = row['row'].split()
        need(len(cells) == 15, 'Block row transport schema')
        for role, initial, column in [('parameter', 4096 if index == 1 else 0, 6), ('world', 89, 7)]:
            random = Random(initial)
            draws = [r for r in randoms if r['case'] == 100 + index and r['role'] == role]
            for draw in draws:
                value = random.integer(draw['arguments'][0]) if draw['method'] == 'nextInt' else {'F': fhex(random.floating())}
                need(draw['result'] == value and draw['seed'] == f'{random.seed:012x}', 'actual supplied/World Block RNG return/state replay')
            need(cells[column] == f'{random.seed:012x}', 'actual final Block RNG state')
        world_draws = [r for r in randoms if r['case'] == 100 + index and r['role'] == 'world']
        math_draws = [r for r in randoms if r['case'] == 100 + index and r['role'] == 'math']
        if index in (16, 17):
            count = 16 if index == 16 else 4
            need(len(world_draws) == len(math_draws) == count and all(r['method'] == 'nextFloat' for r in world_draws), 'actual four World floats/four shared Math doubles per published item')
            # Recover only the observed stream's start from its read-only post-state.
            # This is scalar replay, never a target/default-clock input.
            initial = int(math_draws[0]['seed'], 16)
            inverse = pow(MULT, -1, 1 << 48)
            for _ in range(2):
                initial = ((initial - 11) * inverse) & MASK
            math = Random(initial, internal=True)
            for draw in math_draws:
                need(draw['result'] == {'D': struct.pack('>d', math.double()).hex()} and draw['seed'] == f'{math.seed:012x}', 'observed shared Math scalar replay')
            need(len(row['items']) == (4 if index == 16 else 1), 'actual published dropped Item census')
        else:
            need(not math_draws, 'no unrelated Math stream claimed')
    need(blocks[6]['scratch_hash'] == '-' and blocks[7]['scratch_hash'] == hashlib.sha256(bytes(32768 * 4)).hexdigest(), 'Leaves absent-area fresh zero scratch')
    need(blocks[9]['scratch_hash'] == blocks[10]['scratch_hash'] and blocks[9]['row'].split()[-1] == '-2', 'Leaves stale negative scratch retained across Worlds')
    initialized = []
    for mode in ('normal', 'debugger'):
        log = (directory / mode / 'class-init.log').read_text()
        actual = set(re.findall(r"Initializing '([^']+)'", log)) & set(pins['jar_class_names'])
        need(actual == set(pins['initialized_original_classes']), 'exact actual original class initialization closure')
        need(not {'net/minecraft/client/Minecraft', 'px', 'yo', 'vy'} & actual, 'excluded client/device initialization')
        bindings = [line.split() for line in (directory / mode / 'target.stdout').read_text().splitlines() if line.startswith('BIND ')]
        need(len(bindings) == len(pins['classes']), 'all source class runtime origins checked')
        for _, name, extent, digest in bindings:
            need(int(extent) == pins['classes'][name]['class_bytes'] and digest == pins['classes'][name]['class_sha256'], 'actual original class-resource bytes')
        initialized.append(len(actual))
    streams = {'sound_rows': [], 'cell_rows': [], 'range_rows': []}
    for frame in frames:
        counters = dict(SOUND=0, CELL=0, RANGE=0)
        for event in frame['events']:
            cells = event.split()
            kind = cells[0]
            if kind not in counters:
                continue
            # Only the checked immutable sound name is removed from numeric transport.
            if kind == 'SOUND':
                need(cells[1] == 'ambient.cave.cave', 'actual sound name')
                values = cells[2:]
            else:
                values = cells[1:]
            streams[{'SOUND': 'sound_rows', 'CELL': 'cell_rows', 'RANGE': 'range_rows'}[kind]].append('RANDOM_' + kind + ' ' + str(frame['index']) + ' ' + str(counters[kind]) + ' ' + ' '.join(values))
            counters[kind] += 1
    observations = {'frame_rows': [r['row'] for r in frames], 'hash_rows': [r['hash_row'] for r in frames], 'frame_records': frames,
                    **streams, 'block_rows': [r['row'] for r in blocks], 'block_records': blocks,
                    'jdi_summaries': [summaries[n] for n in sorted(summaries)],
                    'coverage': {'complete_world_profiles': 14, 'block_profiles': 18, 'class_pins': len(pins['classes']), 'initialized_original_classes': initialized,
                                 'live_method_pins': len(code), 'detailed_samples': sum(r['event'] == 'checkpoint' and r['location']['pc'] == 951 for r in records),
                                 'rng_returns': len(randoms), 'exception_events': len(exceptions)},
                    'limits': ['Authored cached/saved Chunks; no generation qualification.', 'Actual HD replacement is a declared genuine-class fixture input.',
                               'Authored callback exceptions retain original Java exception classes; native IO failures pair reached numeric effects only.',
                               'Detailed sample stream is first Chunk only; complete loop fields use independent literal/RNG replay and actual Chunk tail checkpoints.',
                               'Block-drop shared Math/default constructor clocks are observed, never rebound or claimed identical across separate processes.',
                               'Provider insertion-list construction history is outside compared array/membership hashes.']}
    (directory / 'observations.json').write_text(json.dumps(observations, indent=2) + '\n', newline='\n')
    return observations['coverage']
