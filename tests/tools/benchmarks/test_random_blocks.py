"""Audit retained receipts and reject corruption without a native benchmark.

The synthetic streams and artifacts are test data, not original-client
observations or performance results. Fixed Random words encode the declared
Java sequence. Retained public observations are audited separately below.
"""
import copy
import functools
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'random_blocks_cpu', ROOT / 'tests/tools/world/benchmark_random_blocks.py')
CPU = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CPU)

MODES = ('grass_bright', 'leaves_flag0', 'leaves_connected')
CALLS = {'grass_bright': 32768, 'leaves_flag0': 32768, 'leaves_connected': 512}
ORDER = ['detailed', 'failure', 'failure', 'detailed',
         'detailed', 'failure', 'failure', 'detailed']
SOURCES = ('src/world/runtime/mutation/random_blocks.c', 'src/world/runtime/blocks.c',
           'tests/benchmarks/random_blocks.c', 'tests/benchmarks/random_blocks_adapter.h',
           'tests/benchmarks/random_blocks_detailed.c', 'tests/benchmarks/random_blocks_failure.c',
           'tests/tools/world/benchmark_random_blocks.py')


def wire(label='detailed', measure=True):
    count = 9 if measure else 1
    lines = ['16 achievements', '151 recipes',
             f'RANDOM_BLOCKS_CPU_CONTEXT adapter={label} samples={count} '
             f'measure={int(measure)} modes=3']
    for mode in MODES:
        for ordinal in range(count):
            timed = int(measure and ordinal >= 2)
            supplied_after = 'c9005540666d' if mode == 'grass_bright' else '0005deece66d'
            lines.append(
                f'RANDOM_BLOCKS_CPU_SAMPLE mode={mode} ordinal={ordinal} timed={timed} '
                f'ticks={1000 + ordinal if timed else 0} frequency=10000000 '
                f'calls={CALLS[mode]} returned={CALLS[mode]} owners-same=1 random-checked=1 '
                f'scratch-same=1 holds-same=1 events={int(mode == "grass_bright")} '
                f'cache=1 scratch={int(mode == "leaves_connected")} '
                f'supplied-before=0005deece66d supplied-after={supplied_after} '
                'world-before=0005deece634 world-after=0005deece634')
    lines.append('RANDOM_BLOCKS_CPU_FINISH checks=100 cleanup=1 modes=3')
    return '\n'.join(lines)


@functools.cache
def source_plane_bytes(mode, after=False):
    """Independent fixed fixture planes for the three declared input scenes."""
    arrays = [bytearray(size) for size in (32768, 16384, 16384, 16384, 256)]

    def put(x, y, z, block):
        arrays[0][x * 2048 + z * 128 + y] = block
        arrays[4][z * 16 + x] = max(arrays[4][z * 16 + x], y + 1)

    if mode == 'grass_bright':
        put(8, 64, 8, 2)
        put(7, 64, 8, 2 if after else 3)
        arrays[2][(8 * 2048 + 8 * 128 + 65) // 2] = 0xf0
        arrays[2][(7 * 2048 + 8 * 128 + 65) // 2] = 0xf0
    elif mode == 'leaves_flag0':
        put(8, 64, 8, 18)
    else:
        for x in range(1, 15):
            for z in range(1, 15):
                for y in range(1, 27):
                    put(x, y, z, 17 if x == 8 else 18)
        if not after:
            remaining = 512
            for y in range(5, 23):
                for z in range(5, 11):
                    for x in (5, 6, 7, 9, 10):
                        if remaining:
                            index = x * 2048 + z * 128 + y
                            arrays[1][index // 2] |= 8 << (4 * (index % 2))
                            remaining -= 1
    return tuple(bytes(array) for array in arrays)


def snapshot_bytes(planes=None, scratch=None, light=(), membership=(), ordered=()):
    planes = planes if planes is not None else [bytes(size) for size in
                                               (32768, 16384, 16384, 16384, 256)]
    parts = [b'RBS1']
    for plane in planes:
        parts.extend((struct.pack('>I', len(plane)), plane))
    parts.append(struct.pack('>I', 0xffffffff if scratch is None else len(scratch) // 4))
    if scratch is not None:
        parts.append(scratch)
    parts.append(struct.pack('>I', len(light)))
    parts.extend(struct.pack('>IIiiiiii', *row) for row in light)
    parts.append(struct.pack('>II', len(membership), len(ordered)))
    parts.extend(struct.pack('>iiiiqq', *row) for row in membership)
    parts.extend(struct.pack('>iiiiqqi', *row) for row in ordered)
    return b''.join(parts)


def event_bytes(rows=()):
    return b'RBE1' + struct.pack('>I', len(rows)) + b''.join(
        struct.pack('>Iiiiiii', *row) for row in rows)


def scalar_state(mode, phase):
    supplied = 'c9005540666d' if mode == 'grass_bright' and phase == 'after' else '0005deece66d'
    return {'supplied': [supplied, 0, '0000000000000000'],
            'world_random': ['0005deece634', 0, '0000000000000000'],
            'g': 0, 'Q': 0, 'h': 0, 'f': 0, 'M': 0,
            'chunk_flags': [int(phase == 'after'), 1, 1, 0, 0], 'last_save': 0,
            'chunk_fields': [0, 0, 0], 'minimum_height': 0, 'chunk_a': 0,
            'profile': ['0000000000000001', 1, '0000000000000000'],
            'holds': [0, 0, 0], 'cache': 1, 'entities': 0, 'weather': 0, 'players': 0,
            'generation_randoms': [['000000000001', 0, '0000000000000000'],
                                  ['000000000002', 0, '0000000000000000']]}


@functools.cache
def cached_artifact_items(measure=False):
    """Portable complete synthetic formats, not observed native state values."""
    result = {}
    for mode in MODES:
        for ordinal in range(9 if measure else 1):
            base = f'{mode}/sample{ordinal}.'
            before_scratch = bytes(131072) if mode == 'leaves_connected' and ordinal else None
            after_scratch = bytes(131072) if mode == 'leaves_connected' else None
            result[base + 'before.bin'] = snapshot_bytes(source_plane_bytes(mode), before_scratch)
            result[base + 'after.bin'] = snapshot_bytes(source_plane_bytes(mode, True), after_scratch)
            events = [(1, 7, 64, 8, 0, 0, 0)] if mode == 'grass_bright' else []
            result[base + 'events.bin'] = event_bytes(events)
            state = {'before': scalar_state(mode, 'before'), 'after': scalar_state(mode, 'after'),
                     'calls': CALLS[mode], 'returned': CALLS[mode], 'events': len(events),
                     'owners_same': True, 'world_fields_same': True, 'holds_same': True,
                     'scratch_same': True, 'scratch_before': int(before_scratch is not None),
                     'scratch_after': int(after_scratch is not None), 'error': [0, 0, 0, 0, 0]}
            result[base + 'state.json'] = (json.dumps(state, separators=(',', ':')) + '\n').encode()
    return tuple(result.items())


def artifact_bytes(measure=False):
    return dict(cached_artifact_items(measure))


def receipt_bytes(raw):
    return {'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


@functools.cache
def cached_state_receipts(measure):
    return tuple((name, len(raw), hashlib.sha256(raw).hexdigest())
                 for name, raw in cached_artifact_items(measure))


def state_receipts(measure=True):
    return {name: {'bytes': length, 'sha256': digest}
            for name, length, digest in cached_state_receipts(measure)}


def native_checks(measure=True):
    count = 27 if measure else 3
    return {'samples': count, 'snapshots': count * 2, 'source_planes': count * 10,
            'ordered_events': True, 'source_mutations': True, 'random_triples': True,
            'queue_identity_joins': True}


def input_receipts(measure=True):
    return {f'{mode}/sample{ordinal}/world0/region/r.0.0.mcr':
            receipt_bytes(bytes(12288))
            for mode in MODES for ordinal in range(9 if measure else 1)}


def profile(label, measure=True):
    stdout = wire(label, measure)
    authored = input_receipts(measure)
    return {'label': label, 'status': 'PASS', 'returncode': 0, 'stderr': '',
            'stdout': stdout, 'parsed': CPU.parse_cpu_run(stdout, measure),
            'state_files': state_receipts(measure), 'inputs_before': authored,
            'inputs_after': copy.deepcopy(authored), 'native_checks': native_checks(measure),
            'states': {name: json.loads(raw) for name, raw in cached_artifact_items(measure)
                       if name.endswith('.state.json')}}


def report(measure=True):
    profiles = [profile(label, measure) for label in ORDER]
    sources = {name: hashlib.sha256(name.encode()).hexdigest() for name in SOURCES}
    products = {label: hashlib.sha256(label.encode()).hexdigest()
                for label in ('detailed', 'failure')}
    return {'status': 'PASS', 'mode': 'measure' if measure else 'controls', 'rounds': 4,
            'order': ORDER.copy(), 'profiles': profiles, 'errors': [], 'unchanged': True,
            'source_before': sources, 'source_after': copy.deepcopy(sources),
            'products_before': products, 'products_after': copy.deepcopy(products),
            'semantic_comparison': CPU.semantic_profile(profiles[0]),
            'summary': {label: CPU.summarize(profiles, label)
                        for label in ('detailed', 'failure')}}


def write_artifacts(root, measure=False):
    for name, raw in artifact_bytes(measure).items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)


def rebind_state_receipt(profile_value, name):
    raw = (json.dumps(profile_value['states'][name], separators=(',', ':')) + '\n').encode()
    profile_value['state_files'][name] = receipt_bytes(raw)


def expand_guard_manifests(receipt):
    """Validate lossless manifest deduplication before auditing observations."""
    manifests = receipt['guard_manifests']
    if not isinstance(manifests, dict) or not manifests:
        raise ValueError('Retained guard manifest registry')
    for identity, values in manifests.items():
        if (not isinstance(identity, str) or not re.fullmatch(r'[0-9a-f]{64}', identity) or
                not isinstance(values, dict) or len(values) < 25 or
                not all(isinstance(name, str) and isinstance(value, str) and
                        re.fullmatch(r'[0-9a-f]{64}', value) for name, value in values.items())):
            raise ValueError('Complete typed guard manifest')
        raw = json.dumps(values, sort_keys=True, separators=(',', ':')).encode()
        if hashlib.sha256(raw).hexdigest() != identity:
            raise ValueError('Exact guard manifest identity')
    used = set()

    def expand(value):
        if isinstance(value, dict):
            if 'guard_manifest' in value:
                if set(value) != {'guard_manifest'} or value['guard_manifest'] not in manifests:
                    raise ValueError('Exact retained guard reference')
                used.add(value['guard_manifest'])
                return dict(manifests[value['guard_manifest']])
            return {key: expand(child) for key, child in value.items()}
        if isinstance(value, list):
            return [expand(child) for child in value]
        return value

    result = expand({key: value for key, value in receipt.items() if key != 'guard_manifests'})
    if used != set(manifests):
        raise ValueError('Every retained guard manifest has an actual reference')
    return result


class RandomBlockRetainedReceiptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = json.loads((ROOT / 'tests/benchmarks/random_blocks_windows.json').read_text())
        cls.receipt = expand_guard_manifests(cls.raw)
        frame_spec = importlib.util.spec_from_file_location(
            'random_blocks_retained_frames', ROOT / 'scripts/benchmarks/client_frames_run.py')
        cls.frame = importlib.util.module_from_spec(frame_spec)
        frame_spec.loader.exec_module(cls.frame)

    def test_both_complete_retained_cpu_cohorts(self):
        self.assertEqual(self.receipt['schema_version'], 1)
        semantics = []
        for name in ('cpu_preformat', 'cpu_final'):
            with self.subTest(cohort=name):
                cohort = self.receipt[name]
                self.assertEqual(cohort['status'], 'PASS')
                self.assertEqual(cohort['mode'], 'measure')
                self.assertEqual(cohort['rounds'], 4)
                self.assertEqual(cohort['order'], ORDER)
                self.assertEqual([p['label'] for p in cohort['profiles']], ORDER)
                semantics.append(CPU.audit_receipt(cohort))
                samples = [row for p in cohort['profiles'] for row in p['parsed']['samples']]
                self.assertEqual(len(samples), 216)
                self.assertEqual(sum(row['timed'] for row in samples), 168)
                self.assertEqual(sum(row['calls'] for row in samples), 4755456)
                self.assertTrue(all(row['returned'] == row['calls'] for row in samples))
                for label in ('detailed', 'failure'):
                    self.assertEqual(len([p for p in cohort['profiles'] if p['label'] == label]), 4)
                    self.assertEqual(cohort['summary'][label], CPU.summarize(cohort['profiles'], label))
                    for mode in MODES:
                        self.assertEqual(cohort['summary'][label][mode]['samples'], 28)
        self.assertEqual(semantics[0], semantics[1])

    def test_all_six_retained_frame_attempts_remain_unqualified(self):
        frames = self.receipt['frames']
        profiles = frames['profiles']
        self.assertEqual([(p['round'], p['label']) for p in profiles],
                         [(0, 'before'), (0, 'after'), (1, 'after'),
                          (1, 'before'), (2, 'before'), (2, 'after')])
        self.assertEqual([p['observation']['status'] for p in profiles],
                         ['FAIL', 'PASS', 'PASS', 'FAIL', 'PASS', 'PASS'])
        self.assertEqual([p['driver_exit'] for p in profiles], [1, 0, 0, 1, 0, 0])
        self.assertEqual(len({p['stdout_sha256'] for p in profiles}), 6)
        for index, profile_value in enumerate(profiles):
            with self.subTest(attempt=index, label=profile_value['label']):
                observation = profile_value['observation']
                self.assertIsInstance(profile_value['stdout'], str)
                self.assertEqual(hashlib.sha256(profile_value['stdout'].encode()).hexdigest(),
                                 profile_value['stdout_sha256'])
                self.assertEqual(self.frame.summarize(profile_value['stdout']), observation['summary'])
                self.assertTrue(observation['inputs_before'])
                self.assertTrue(observation['assets_before'])
                self.assertEqual(observation['inputs_before'], observation['inputs_after'])
                self.assertEqual(observation['assets_before'], observation['assets_after'])
                self.assertIs(observation['inputs_unchanged'], True)
                self.assertIs(observation['assets_unchanged'], True)
                self.assertIs(observation['summary']['comparison_qualified'],
                              observation['status'] == 'PASS')
        qualification = self.frame.qualify_cohort(profiles)
        self.assertEqual(qualification, {'status': 'FAIL', 'all_intervals_qualified': False,
                                        'authored_scene_and_camera_matched': False})
        for key, value in qualification.items():
            self.assertEqual(frames[key], value)
        self.assertIs(self.receipt['fps_comparison_qualified'], False)
        self.assertIs(self.receipt['fps_improvement_claimed'], False)

    def test_failed_attempts_and_manifest_transport_are_preserved(self):
        failures = self.receipt['attempt_failures']
        self.assertEqual(len(failures['builds']), 2)
        self.assertTrue(all(attempt['status'] == 'FAIL' for attempt in failures['builds']))
        for attempt in failures['builds']:
            self.assertEqual(hashlib.sha256(attempt['log'].encode()).hexdigest(),
                             attempt['published_log_sha256'])
            self.assertRegex(attempt['original_log_sha256'], r'^[0-9a-f]{64}$')
        self.assertEqual(failures['initial_controls']['status'], 'FAIL')
        self.assertTrue(failures['initial_controls']['errors'])
        for mutation in ('bad-map-byte', 'missing-reference', 'extra-reference-field'):
            value = copy.deepcopy(self.raw)
            identity = next(iter(value['guard_manifests']))
            if mutation == 'bad-map-byte':
                name = next(iter(value['guard_manifests'][identity]))
                original = value['guard_manifests'][identity][name]
                changed = ('1' if original[0] == '0' else '0') + original[1:]
                value['guard_manifests'][identity][name] = changed
            elif mutation == 'missing-reference':
                value['test_reference'] = {'guard_manifest': '0' * 64}
            else:
                value['test_reference'] = {'guard_manifest': identity, 'extra': 1}
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    expand_guard_manifests(value)


class RandomBlockStreamTests(unittest.TestCase):
    def test_complete_controls_and_measured_streams(self):
        for measure in (False, True):
            for label in ('detailed', 'failure'):
                with self.subTest(measure=measure, label=label):
                    parsed = CPU.parse_cpu_run(wire(label, measure), measure)
                    self.assertEqual(len(parsed['samples']), 27 if measure else 3)
                    self.assertEqual(sum(row['timed'] for row in parsed['samples']),
                                     21 if measure else 0)
                    self.assertEqual(parsed['samples'][0]['supplied-after'], 'c9005540666d')

    def test_every_missing_or_extra_record_is_rejected(self):
        for measure in (False, True):
            lines = wire(measure=measure).splitlines()
            for index in range(len(lines)):
                for mutation in ('missing', 'duplicate'):
                    changed = lines[:index] + lines[index + 1:] if mutation == 'missing' else \
                        lines[:index] + [lines[index]] + lines[index:]
                    with self.subTest(measure=measure, index=index, mutation=mutation):
                        with self.assertRaises(ValueError):
                            CPU.parse_cpu_run('\n'.join(changed), measure)

    def test_preserved_count_does_not_allow_reordering(self):
        lines = wire().splitlines()
        for left, right in ((3, 4), (3, 12), (0, 1)):
            changed = lines.copy()
            changed[left], changed[right] = changed[right], changed[left]
            with self.subTest(left=left, right=right):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run('\n'.join(changed), True)

    def test_wrong_work_context_and_finish(self):
        for before, after in (
                ('samples=9', 'samples=8'), ('measure=1', 'measure=0'), ('modes=3', 'modes=2'),
                ('adapter=detailed', 'adapter=unknown'), ('ordinal=0', 'ordinal=1'),
                ('calls=32768', 'calls=32767'), ('returned=32768', 'returned=32767'),
                ('frequency=10000000', 'frequency=0'), ('timed=1', 'timed=0'),
                ('ticks=1002', 'ticks=0'), ('cleanup=1', 'cleanup=0'),
                ('checks=100', 'checks=0'), ('CPU_SAMPLE', 'CPU_UNKNOWN')):
            with self.subTest(before=before):
                self.assertIn(before, wire())
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace(before, after, 1), True)
        with self.assertRaises(ValueError):
            CPU.parse_cpu_run(wire(measure=False), True)
        with self.assertRaises(ValueError):
            CPU.parse_cpu_run(wire(), False)

    def test_actual_ownership_scratch_and_source_events_are_required(self):
        for field in ('owners-same', 'random-checked', 'scratch-same', 'holds-same', 'cache'):
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace(f'{field}=1', f'{field}=0', 1), True)
        for before, after in (('events=1', 'events=0'), ('events=0', 'events=1'),
                              ('scratch=0', 'scratch=1'), ('scratch=1', 'scratch=0')):
            with self.subTest(before=before):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace(before, after, 1), True)

    def test_world_and_supplied_random_words(self):
        for field, value in (('supplied-before', '0005deece66d'),
                             ('supplied-after', 'c9005540666d'),
                             ('world-before', '0005deece634'), ('world-after', '0005deece634')):
            for changed in ('000000000000', value.upper(), value + '0', 'not-a-state'):
                with self.subTest(field=field, changed=changed):
                    with self.assertRaises(ValueError):
                        CPU.parse_cpu_run(wire().replace(f'{field}={value}',
                                                        f'{field}={changed}', 1), True)

    def test_duplicate_missing_and_extra_fields(self):
        for token in ('adapter=detailed', 'ordinal=0', 'owners-same=1',
                      'supplied-before=0005deece66d'):
            for changed in (token + ' ' + token, '', token + ' fabricated=1'):
                with self.subTest(token=token, changed=changed):
                    with self.assertRaises(ValueError):
                        CPU.parse_cpu_run(wire().replace(token, changed, 1), True)

    def test_unsigned_values_have_an_exact_domain(self):
        for value in ('-1', '+1', '1.5', 'NaN', str(1 << 64)):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace('ticks=1002', 'ticks=' + value, 1), True)


class RandomBlockArtifactTests(unittest.TestCase):
    def test_exact_complete_artifact_set(self):
        for measure in (False, True):
            with tempfile.TemporaryDirectory(prefix='mc-random-block-artifacts-') as temporary:
                root = Path(temporary)
                write_artifacts(root, measure)
                receipts = CPU.validate_artifacts(root, measure)
                self.assertEqual(receipts, state_receipts(measure))
                self.assertEqual(len(receipts), 108 if measure else 12)

    def test_missing_extra_and_truncated_artifacts(self):
        for mutation in ('missing', 'extra', 'truncated', 'nonobject-state'):
            with self.subTest(mutation=mutation):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-artifacts-') as temporary:
                    root = Path(temporary)
                    write_artifacts(root)
                    path = root / 'grass_bright/sample0.before.bin'
                    if mutation == 'missing':
                        path.unlink()
                    elif mutation == 'extra':
                        (root / 'unexpected.bin').write_bytes(b'')
                    elif mutation == 'truncated':
                        path.write_bytes(bytes(82175))
                    else:
                        (root / 'grass_bright/sample0.state.json').write_text('[]')
                    with self.assertRaises(ValueError):
                        CPU.validate_artifacts(root, False)

    def test_complete_names_do_not_excuse_dishonest_receipts(self):
        for field, value in (('bytes', -1), ('bytes', True), ('bytes', '82176'),
                             ('sha256', '0' * 63), ('sha256', 'A' * 64),
                             ('sha256', 'z' * 64)):
            receipts = state_receipts()
            receipts['grass_bright/sample0.before.bin'][field] = value
            with self.subTest(field=field, value=value):
                with self.assertRaises(ValueError):
                    CPU.validate_file_receipts(receipts, True)
        for mutation in ('missing', 'extra-field'):
            receipts = state_receipts()
            row = receipts['grass_bright/sample0.before.bin']
            if mutation == 'missing':
                del row['sha256']
            else:
                row['fabricated'] = 1
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.validate_file_receipts(receipts, True)

    def test_one_byte_plane_queue_or_event_difference_rejects_cohort(self):
        for part, suffix, offset in (('plane', 'after.bin', 8),
                                    ('queue', 'after.bin', 82216),
                                    ('event', 'events.bin', 12)):
            with self.subTest(part=part):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-artifacts-') as temporary:
                    root = Path(temporary)
                    left, right = root / 'left', root / 'right'
                    write_artifacts(left)
                    write_artifacts(right)
                    if part == 'queue':
                        raw = snapshot_bytes(source_plane_bytes('grass_bright', True),
                                             light=[(0, 0, 1, 2, 3, 4, 5, 6)])
                        for directory in (left, right):
                            (directory / 'grass_bright/sample0.after.bin').write_bytes(raw)
                    profiles = [profile(label, False) for label in ('detailed', 'failure')]
                    profiles[0]['state_files'] = CPU.validate_artifacts(left, False)
                    changed = right / f'grass_bright/sample0.{suffix}'
                    data = bytearray(changed.read_bytes())
                    data[offset] ^= 1
                    changed.write_bytes(data)
                    profiles[1]['state_files'] = CPU.validate_artifacts(right, False)
                    self.assertNotEqual(profiles[0]['state_files'], profiles[1]['state_files'])
                    with self.assertRaises(ValueError):
                        CPU.compare_cpu_runs(profiles, ['detailed', 'failure'])


class RandomBlockBinaryFormatTests(unittest.TestCase):
    def test_complete_snapshot_preserves_planes_scratch_and_queue_joins(self):
        light = [(0, 0, -1, 2, 3, 4, 5, 6), (1, 1, 7, 8, 9, 10, 11, 12),
                 (0, 0, -1, 2, 3, 4, 5, 6)]
        tick = (-1, 2, 3, 18, -(1 << 63), (1 << 63) - 1)
        ordered = [tick + (0,), (4, 5, 6, 2, 7, 8, -1)]
        scratch = struct.pack('>i', -2) * 32768
        planes = source_plane_bytes('leaves_connected')
        parsed = CPU.parse_snapshot(snapshot_bytes(planes, scratch, light, [tick], ordered))
        self.assertEqual(parsed, {'arrays': list(planes), 'scratch': scratch, 'light': light,
                                  'membership': [tick], 'ordered': ordered})
        self.assertIsNone(CPU.parse_snapshot(snapshot_bytes())['scratch'])

    def test_snapshot_rejects_missing_trailing_and_wrong_declared_extents(self):
        raw = snapshot_bytes(light=[(0, 0, 1, 2, 3, 4, 5, 6)],
                             membership=[(1, 2, 3, 18, 4, 5)],
                             ordered=[(1, 2, 3, 18, 4, 5, 0)])
        for cut in (0, 3, 7, 32775, 82200, 82203, 82207, 82239, len(raw) - 1):
            with self.subTest(cut=cut):
                with self.assertRaises(ValueError):
                    CPU.parse_snapshot(raw[:cut])
        for changed in (b'WRNG' + raw[4:], raw + b'\0'):
            with self.subTest(change=changed[:4], length=len(changed)):
                with self.assertRaises(ValueError):
                    CPU.parse_snapshot(changed)
        for offset, value in ((4, 32767), (32776, 16383), (82200, 32767),
                               (82204, 1000001), (82240, 129), (82244, 129)):
            changed = bytearray(raw)
            struct.pack_into('>I', changed, offset, value)
            with self.subTest(offset=offset, value=value):
                with self.assertRaises(ValueError):
                    CPU.parse_snapshot(changed)
        scratch = snapshot_bytes(scratch=bytes(131072))
        with self.assertRaises(ValueError):
            CPU.parse_snapshot(scratch[:-1])

    def test_light_first_identity_and_aliased_contents_must_join(self):
        for rows in (
                [(1, 0, 1, 2, 3, 4, 5, 6)],
                [(0, 2, 1, 2, 3, 4, 5, 6)],
                [(0, 0, 1, 2, 3, 4, 5, 6), (0, 1, 1, 2, 3, 4, 5, 6)],
                [(0, 0, 1, 2, 3, 4, 5, 6), (0, 0, 9, 2, 3, 4, 5, 6)],
                [(0, 0, 1, 2, 3, 4, 5, 6), (0, 0, 1, 2, 3, 4, 5, 6),
                 (1, 0, 1, 2, 3, 4, 5, 6)]):
            with self.subTest(rows=rows):
                with self.assertRaises(ValueError):
                    CPU.parse_snapshot(snapshot_bytes(light=rows))

    def test_scheduler_identity_requires_matching_actual_occurrence(self):
        member = (1, 2, 3, 18, 4, 5)
        for ordered in (member + (1,), member + (-2,), (1, 2, 3, 17, 4, 5, 0),
                        (1, 2, 3, 18, 40, 5, 0), (1, 2, 3, 18, 4, 50, 0)):
            with self.subTest(ordered=ordered):
                with self.assertRaises(ValueError):
                    CPU.parse_snapshot(snapshot_bytes(membership=[member], ordered=[ordered]))

    def test_complete_ordered_event_format_and_corruptions(self):
        rows = [(1, 7, 64, 8, 0, 0, 0), (2, -1, 2, 3, 4, 5, 6)]
        raw = event_bytes(rows)
        self.assertEqual(CPU.parse_events(raw), rows)
        self.assertEqual(CPU.parse_events(event_bytes()), [])
        for changed in (b'', raw[:7], raw[:-1], raw + b'\0', b'WRNG' + raw[4:],
                        b'RBE1' + struct.pack('>I', 1) + raw[8:],
                        b'RBE1' + struct.pack('>I', 4097),
                        event_bytes([(0, 7, 64, 8, 0, 0, 0)]),
                        event_bytes([(3, 7, 64, 8, 0, 0, 0)])):
            with self.subTest(length=len(changed), prefix=changed[:8]):
                with self.assertRaises(ValueError):
                    CPU.parse_events(changed)


class RandomBlockNativeArtifactTests(unittest.TestCase):
    def test_independent_complete_planes_and_native_joins(self):
        for measure in (False, True):
            with tempfile.TemporaryDirectory(prefix='mc-random-block-native-') as temporary:
                root = Path(temporary)
                write_artifacts(root, measure)
                parsed = CPU.parse_cpu_run(wire(measure=measure), measure)
                self.assertEqual(CPU.validate_native_artifacts(root, parsed), native_checks(measure))
        connected = source_plane_bytes('leaves_connected')
        self.assertEqual(sum(value.bit_count() for value in connected[1]), 512)
        self.assertEqual(source_plane_bytes('leaves_connected', True)[1], bytes(16384))

    def test_before_and_after_planes_have_independent_expected_values(self):
        for mode, phase, plane, index in (
                ('grass_bright', 'before', 0, 8 * 2048 + 8 * 128 + 64),
                ('grass_bright', 'after', 0, 7 * 2048 + 8 * 128 + 64),
                ('leaves_flag0', 'before', 2, 1),
                ('leaves_connected', 'after', 1, (5 * 2048 + 5 * 128 + 5) // 2)):
            with self.subTest(mode=mode, phase=phase, plane=plane):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-native-') as temporary:
                    root = Path(temporary)
                    write_artifacts(root)
                    path = root / mode / f'sample0.{phase}.bin'
                    old = CPU.parse_snapshot(path.read_bytes())
                    arrays = [bytearray(value) for value in old['arrays']]
                    arrays[plane][index] ^= 1
                    path.write_bytes(snapshot_bytes(arrays, old['scratch']))
                    with self.assertRaises(ValueError):
                        CPU.validate_native_artifacts(root, CPU.parse_cpu_run(wire(measure=False), False))

    def test_scratch_must_be_published_naturally_and_then_retained(self):
        for mode, ordinal, phase, scratch in (
                ('leaves_connected', 0, 'before', bytes(131072)),
                ('leaves_connected', 0, 'after', None),
                ('leaves_connected', 1, 'before', None),
                ('leaves_flag0', 0, 'after', bytes(131072))):
            with self.subTest(mode=mode, ordinal=ordinal, phase=phase):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-native-') as temporary:
                    root = Path(temporary)
                    measure = ordinal > 0
                    write_artifacts(root, measure)
                    path = root / mode / f'sample{ordinal}.{phase}.bin'
                    old = CPU.parse_snapshot(path.read_bytes())
                    path.write_bytes(snapshot_bytes(old['arrays'], scratch))
                    with self.assertRaises(ValueError):
                        CPU.validate_native_artifacts(root, CPU.parse_cpu_run(wire(measure=measure), measure))

    def test_native_random_triples_and_scalar_work_cannot_disagree(self):
        for key, value in (('seed', '000000000000'), ('world-gaussian', 1),
                           ('cached-gaussian', '8000000000000000'), ('calls', 1),
                           ('events', 0), ('owners_same', False), ('world_fields_same', False),
                           ('holds_same', False), ('scratch_same', False), ('events', True),
                           ('scratch_before', False), ('scratch_after', False)):
            with self.subTest(key=key, value=value):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-native-') as temporary:
                    root = Path(temporary)
                    write_artifacts(root)
                    path = root / 'grass_bright/sample0.state.json'
                    state = json.loads(path.read_text())
                    if key == 'seed':
                        state['after']['supplied'][0] = value
                    elif key == 'world-gaussian':
                        state['before']['world_random'][1] = value
                    elif key == 'cached-gaussian':
                        state['after']['supplied'][2] = value
                    else:
                        state[key] = value
                    path.write_text(json.dumps(state))
                    with self.assertRaises(ValueError):
                        CPU.validate_native_artifacts(root, CPU.parse_cpu_run(wire(measure=False), False))

    def test_native_events_must_join_the_recorded_source_callbacks(self):
        for rows in ([], [(1, 8, 64, 8, 0, 0, 0)], [(2, 7, 64, 8, 0, 0, 0)]):
            with self.subTest(rows=rows):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-native-') as temporary:
                    root = Path(temporary)
                    write_artifacts(root)
                    (root / 'grass_bright/sample0.events.bin').write_bytes(event_bytes(rows))
                    with self.assertRaises(ValueError):
                        CPU.validate_native_artifacts(root, CPU.parse_cpu_run(wire(measure=False), False))


class RandomBlockStateTransportTests(unittest.TestCase):
    def test_only_unchanged_cave_constructor_seed_word_is_transported(self):
        state = profile('detailed', False)['states']['grass_bright/sample0.state.json']
        original = copy.deepcopy(state)
        normalized = CPU.normalize_native_state(state)
        marker = normalized['before']['generation_randoms'][1][0]
        self.assertIsInstance(marker, str)
        self.assertNotRegex(marker, r'^[0-9a-f]{12}$')
        expected = copy.deepcopy(original)
        for phase in ('before', 'after'):
            expected[phase]['generation_randoms'][1][0] = marker
        self.assertEqual(normalized, expected)
        self.assertEqual(state, original)
        normalized['before']['generation_randoms'][0][0] = '000000000000'
        self.assertEqual(state, original)

    def test_independent_unchanged_cave_values_keep_distinct_bound_raw_evidence(self):
        value = report()
        last = value['profiles'][-1]
        name = 'grass_bright/sample0.state.json'
        first_state = copy.deepcopy(value['profiles'][0]['states'][name])
        for phase in ('before', 'after'):
            last['states'][name][phase]['generation_randoms'][1] = [
                '123456789abc', 0, '0000000000000000']
        rebind_state_receipt(last, name)
        self.assertNotEqual(last['state_files'][name], value['profiles'][0]['state_files'][name])
        self.assertNotEqual(last['states'][name], first_state)
        self.assertEqual(CPU.audit_receipt(value), value['semantic_comparison'])
        self.assertNotEqual(last['states'][name], first_state)

    def test_progressed_or_malformed_generation_random_is_rejected(self):
        for mutation in ('progressed-cave', 'progressed-generator', 'missing-owner',
                         'short-triple', 'bad-seed', 'bad-cache-bits', 'bool-cache-flag'):
            state = profile('detailed', False)['states']['grass_bright/sample0.state.json']
            if mutation.startswith('progressed'):
                index = int(mutation == 'progressed-cave')
                state['after']['generation_randoms'][index][0] = 'ffffffffffff'
            elif mutation == 'missing-owner':
                state['before']['generation_randoms'].pop()
            elif mutation == 'short-triple':
                state['before']['generation_randoms'][1].pop()
            else:
                for phase in ('before', 'after'):
                    triple = state[phase]['generation_randoms'][1]
                    if mutation == 'bad-seed':
                        triple[0] = 'short'
                    elif mutation == 'bad-cache-bits':
                        triple[2] = 'short'
                    else:
                        triple[1] = False
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.normalize_native_state(state)

    def test_every_field_outside_unchanged_cave_entropy_remains_exact(self):
        for mutation in ('generator-seed', 'cave-cache-tail', 'world-field',
                         'world-field-bool', 'world-field-float', 'native-error', 'scratch-flag'):
            value = report()
            last = value['profiles'][-1]
            name = 'grass_bright/sample0.state.json'
            state = last['states'][name]
            if mutation == 'generator-seed':
                for phase in ('before', 'after'):
                    state[phase]['generation_randoms'][0][0] = 'abcdef012345'
            elif mutation == 'cave-cache-tail':
                for phase in ('before', 'after'):
                    state[phase]['generation_randoms'][1][1:] = [1, '8000000000000000']
            elif mutation == 'world-field':
                state['after']['g'] += 1
            elif mutation in ('world-field-bool', 'world-field-float'):
                for phase in ('before', 'after'):
                    state[phase]['g'] = False if mutation == 'world-field-bool' else 0.0
            elif mutation == 'native-error':
                state['error'][0] = 1
            else:
                state['scratch_after'] = 1
            rebind_state_receipt(last, name)
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_exact_raw_scalar_encoding_refuses_hidden_duplicate_fields(self):
        for mutation in ('whitespace', 'duplicate-key', 'trailing-newline', 'no-final-newline'):
            with self.subTest(mutation=mutation):
                with tempfile.TemporaryDirectory(prefix='mc-random-block-state-') as temporary:
                    root = Path(temporary)
                    write_artifacts(root)
                    self.assertEqual(CPU.state_receipts(root, False), profile('detailed', False)['states'])
                    path = root / 'grass_bright/sample0.state.json'
                    raw = path.read_bytes()
                    if mutation == 'whitespace':
                        raw = raw.replace(b'"calls":', b'"calls": ', 1)
                    elif mutation == 'duplicate-key':
                        raw = raw.replace(b'"calls":32768', b'"calls":32768,"calls":32768', 1)
                    elif mutation == 'trailing-newline':
                        raw += b'\n'
                    else:
                        raw = raw[:-1]
                    path.write_bytes(raw)
                    with self.assertRaises(ValueError):
                        CPU.state_receipts(root, False)

    def test_full_retained_states_must_match_json_hashes_and_complete_names(self):
        for mutation in ('hash-disagrees', 'state-missing', 'state-extra'):
            value = report()
            first = value['profiles'][0]
            name = 'grass_bright/sample0.state.json'
            if mutation == 'hash-disagrees':
                first['states'][name]['after']['g'] += 1
            elif mutation == 'state-missing':
                del first['states'][name]
            else:
                first['states']['extra.state.json'] = copy.deepcopy(first['states'][name])
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_state_record_requires_zero_error_and_actual_native_integer_flags(self):
        row = CPU.parse_cpu_run(wire(measure=False), False)['samples'][0]
        for field, changed in (('error', [1, 0, 0, 0, 0]), ('error', [False, 0, 0, 0, 0]),
                               ('events', True), ('scratch_before', False), ('scratch_after', False)):
            state = profile('detailed', False)['states']['grass_bright/sample0.state.json']
            state[field] = changed
            with self.subTest(field=field, changed=changed):
                with self.assertRaises(ValueError):
                    CPU.validate_state_record(state, row)


class RandomBlockCohortTests(unittest.TestCase):
    def test_complete_four_round_receipt(self):
        for measure in (False, True):
            value = report(measure)
            self.assertEqual(CPU.audit_receipt(value), value['semantic_comparison'])
            self.assertEqual([p['label'] for p in value['profiles']], ORDER)
            for label in ('detailed', 'failure'):
                for mode in MODES:
                    summary = value['summary'][label][mode]
                    self.assertEqual(summary['samples'], 28 if measure else 0)
                    if measure:
                        self.assertEqual(summary['median_ns_per_call'], 100500 / CALLS[mode])

    def test_dropped_added_or_reordered_process_is_rejected(self):
        for mutation in ('drop', 'add', 'swap', 'relabel', 'drop-order-and-profile'):
            value = report()
            if mutation == 'drop':
                value['profiles'].pop()
            elif mutation == 'add':
                value['profiles'].append(copy.deepcopy(value['profiles'][-1]))
            elif mutation == 'swap':
                value['profiles'][0], value['profiles'][1] = value['profiles'][1], value['profiles'][0]
            elif mutation == 'relabel':
                value['profiles'][0]['label'] = 'failure'
            else:
                value['profiles'].pop()
                value['order'].pop()
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_expected_order_is_not_an_unordered_label_set(self):
        profiles = [profile(label) for label in ORDER]
        for mutation in ('drop', 'swap', 'one-label'):
            changed = copy.deepcopy(profiles)
            if mutation == 'drop':
                changed.pop()
            elif mutation == 'swap':
                changed[0], changed[1] = changed[1], changed[0]
            else:
                changed = [changed[0], changed[3]]
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.compare_cpu_runs(changed, ORDER)

    def test_raw_stream_is_reparsed_even_when_every_parsed_copy_agrees(self):
        profiles = [profile(label) for label in ORDER]
        for row in profiles:
            row['parsed']['samples'][0]['calls'] = 1
        with self.assertRaises(ValueError):
            CPU.compare_cpu_runs(profiles, ORDER)
        profiles = [profile(label) for label in ORDER]
        profiles[0]['stdout'] = profiles[0]['stdout'].replace('calls=32768', 'calls=1', 1)
        with self.assertRaises(ValueError):
            CPU.compare_cpu_runs(profiles, ORDER)
        profiles = [profile(label) for label in ORDER]
        profiles[0]['stdout'] = wire('failure')
        profiles[0]['parsed'] = CPU.parse_cpu_run(profiles[0]['stdout'], True)
        with self.assertRaises(ValueError):
            CPU.compare_cpu_runs(profiles, ORDER)

    def test_failed_process_cannot_support_a_passing_receipt(self):
        for field, changed in (('status', 'FAIL'), ('returncode', 1), ('returncode', False),
                               ('stderr', 'native failure'), ('error', 'failed attempt'),
                               ('error', None)):
            value = report()
            value['profiles'][0][field] = changed
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)
        for field, changed in (('status', 'FAIL'), ('errors', ['failed attempt']),
                               ('unchanged', False), ('mode', 'unknown'), ('rounds', 0),
                               ('rounds', True)):
            value = report()
            value[field] = changed
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_missing_artifact_and_profile_mode_are_rejected(self):
        for mutation in ('missing-artifact', 'extra-artifact', 'missing-mode', 'wrong-mode'):
            value = report()
            first = value['profiles'][0]
            if mutation == 'missing-artifact':
                first['state_files'].pop('leaves_connected/sample8.after.bin')
            elif mutation == 'extra-artifact':
                first['state_files']['extra'] = receipt_bytes(b'')
            elif mutation == 'missing-mode':
                first['parsed']['samples'] = first['parsed']['samples'][:18]
            else:
                first['parsed']['context']['measure'] = '0'
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_source_and_both_product_guards_are_required(self):
        for mutation in ('source-change', 'source-drop', 'source-identity', 'product-change',
                         'product-drop', 'product-identity'):
            value = report()
            if mutation == 'source-change':
                value['source_after'][SOURCES[0]] = '0' * 64
            elif mutation == 'source-drop':
                del value['source_before'][SOURCES[0]]
                del value['source_after'][SOURCES[0]]
            elif mutation == 'source-identity':
                value['source_before'][SOURCES[0]] = value['source_after'][SOURCES[0]] = 'short'
            elif mutation == 'product-change':
                value['products_after']['failure'] = '0' * 64
            elif mutation == 'product-drop':
                del value['products_before']['failure']
                del value['products_after']['failure']
            else:
                value['products_before']['failure'] = value['products_after']['failure'] = 'short'
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_authored_inputs_cannot_be_lost_or_changed(self):
        for mutation in ('changed', 'missing', 'same-count-wrong-name'):
            value = report()
            first = value['profiles'][0]
            name = next(iter(first['inputs_before']))
            if mutation == 'changed':
                first['inputs_after'][name]['sha256'] = '0' * 64
            elif mutation == 'missing':
                del first['inputs_before'][name]
                del first['inputs_after'][name]
            else:
                first['inputs_before']['unexpected'] = first['inputs_before'].pop(name)
                first['inputs_after']['unexpected'] = first['inputs_after'].pop(name)
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_equal_but_invalid_input_receipts_are_rejected(self):
        for field, changed in (('bytes', 12287), ('bytes', True), ('bytes', '12288'),
                               ('sha256', 'short'), ('sha256', 'A' * 64)):
            value = report()
            first = value['profiles'][0]
            name = next(iter(first['inputs_before']))
            first['inputs_before'][name][field] = changed
            first['inputs_after'][name][field] = changed
            with self.subTest(field=field, changed=changed):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_all_profiles_must_consume_identical_declared_inputs(self):
        value = report()
        last = value['profiles'][-1]
        name = next(iter(last['inputs_before']))
        last['inputs_before'][name]['sha256'] = '0' * 64
        last['inputs_after'][name]['sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            CPU.audit_receipt(value)

    def test_summary_and_semantic_identity_cannot_drop_work(self):
        for mutation in ('drop-summary-mode', 'change-summary', 'change-semantic', 'extra-error'):
            value = report()
            if mutation == 'drop-summary-mode':
                del value['summary']['failure']['leaves_connected']
            elif mutation == 'change-summary':
                value['summary']['detailed']['grass_bright']['nanoseconds_per_call'].pop()
            elif mutation == 'change-semantic':
                value['semantic_comparison']['samples'][0]['returned'] -= 1
            else:
                value['errors'].append('discarded process failed')
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)

    def test_native_checks_are_complete_typed_and_cannot_claim_missing_work(self):
        for mutation in ('missing-key', 'extra-key', 'samples', 'snapshots', 'source_planes',
                         'ordered_events', 'source_mutations', 'random_triples',
                         'queue_identity_joins', 'integer-bool'):
            value = report()
            checks = value['profiles'][0]['native_checks']
            if mutation == 'missing-key':
                del checks['queue_identity_joins']
            elif mutation == 'extra-key':
                checks['fabricated'] = True
            elif mutation == 'integer-bool':
                checks['ordered_events'] = 1
            elif mutation in ('samples', 'snapshots', 'source_planes'):
                checks[mutation] -= 1
            else:
                checks[mutation] = False
            with self.subTest(mutation=mutation):
                with self.assertRaises(ValueError):
                    CPU.audit_receipt(value)


if __name__ == '__main__':
    unittest.main()
