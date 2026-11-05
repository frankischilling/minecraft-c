"""Refuse incomplete or inconsistent fixed-work EW render receipts."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    'ew_render_cpu', ROOT / 'tests/tools/world/benchmark_chunk_cache_render.py')
CPU = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CPU)


def wire(label='public', measure=True):
    count = 7 if measure else 0
    rows = ['16 achievements', '151 recipes',
            f'EW_RENDER_CPU_CONTEXT adapter={label} success-error=' +
            ('full' if label == 'public' else 'untouched') +
            f' error-bytes=7352 prefix-bytes=200 registry-bytes=208 calls=65536 points=1024 '
            f'samples={count} chunks=9 planes=123 coordinates=456']
    for index, mode in enumerate(CPU.MODES):
        effect = int(mode in ('brightness', 'min_brightness'))
        rows.append(f'EW_RENDER_CPU_MODE mode={mode} expected={1000 + index} chunk-a-after={effect}')
        for sample in range(count):
            rows.append(f'EW_RENDER_CPU_SAMPLE mode={mode} sample={sample} ticks=10000 '
                        f'frequency=10000000 calls=65536 result={1000 + index} planes=123 '
                        f'chunk-a-before=0 chunk-a-after={effect} owners-same=1 world-same=1 '
                        'classes-except-chunk-a-same=1 random-same=1 holds-same=1 acquisition-gets=0')
    rows.append('EW_RENDER_CPU_FINISH checks=100 cleanup=1 genuine-world=1 genuine-ew=1 '
                f'modes=6 measure={int(measure)}')
    return '\n'.join(rows)


class RenderReceiptTests(unittest.TestCase):
    def test_retained_receipt(self):
        receipt = json.loads((ROOT / 'tests/benchmarks/chunk_cache_render_windows.json').read_text())
        self.assertEqual(receipt['qualification']['fixed_work_cpu'], 'PASS')
        self.assertFalse(receipt['qualification']['fps_improvement_claim'])
        streams = []

        def visit(value):
            if isinstance(value, dict):
                if {'text', 'encoding', 'bytes', 'sha256'} <= set(value):
                    raw = value['text'].encode(value['encoding'])
                    self.assertEqual(len(raw), value['bytes'])
                    self.assertEqual(hashlib.sha256(raw).hexdigest(), value['sha256'])
                    streams.append(raw)
                for child in value.values():
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)

        visit(receipt)
        self.assertEqual(len(streams), 38)
        profiles = receipt['fixed_work_cpu']['processes']
        self.assertEqual([(p['round'], p['label']) for p in profiles],
                         [(i, label) for i in range(4) for label in
                          (('public', 'failure') if i % 2 == 0 else ('failure', 'public'))])
        parsed = []
        for profile in profiles:
            row = CPU.parse_cpu_run(profile['stdout']['text'], True)
            self.assertEqual({key: profile[key] for key in CPU.KEYS}, row)
            parsed.append(dict(row, label=profile['label']))
        self.assertEqual(sum(len(p['samples']) for p in parsed), 336)
        self.assertEqual(CPU.compare_cpu_runs(parsed), receipt['fixed_work_cpu']['semantic_identity'])
        for label in ('public', 'failure'):
            self.assertEqual(CPU.summarize(parsed, label), receipt['fixed_work_cpu']['summary'][label])

        frame_spec = importlib.util.spec_from_file_location(
            'retained_render_frames', ROOT / 'scripts/benchmarks/client_frames_run.py')
        frame = importlib.util.module_from_spec(frame_spec)
        frame_spec.loader.exec_module(frame)
        for name, expected in (('uninstrumented_frame_comparison', ['FAIL', 'PASS', 'PASS', 'FAIL', 'PASS', 'PASS']),
                               ('earlier_uninstrumented_baseline', ['PASS', 'FAIL', 'PASS'])):
            cohort = receipt[name]
            self.assertEqual(cohort['status'], 'FAIL')
            self.assertEqual([p['status'] for p in cohort['profiles']], expected)
            for profile in cohort['profiles']:
                self.assertEqual(frame.summarize(profile['stdout']['text']), profile['summary'])
            self.assertEqual(frame.qualify_cohort([
                dict(driver_exit=p['driver_exit'], observation=dict(status=p['status'], summary=p['summary']))
                for p in cohort['profiles']])['status'], 'FAIL')

    def test_complete_profiles(self):
        for measure in (False, True):
            for label in ('public', 'failure'):
                row = CPU.parse_cpu_run(wire(label, measure), measure)
                self.assertEqual(len(row['samples']), 42 if measure else 0)
                self.assertEqual(len(row['modes']), 6)

    def test_matched_cohort(self):
        profiles = [CPU.parse_cpu_run(wire(label), True) for label in ('public', 'failure')]
        CPU.compare_cpu_runs(profiles)

    def test_missing_and_extra_records(self):
        rows = wire().splitlines()
        for index in range(len(rows)):
            with self.subTest(index=index, mutation='missing'):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run('\n'.join(rows[:index] + rows[index + 1:]), True)
            with self.subTest(index=index, mutation='duplicate'):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run('\n'.join(rows[:index] + [rows[index]] + rows[index:]), True)

    def test_native_guard_refusals(self):
        for field in ('owners-same', 'world-same', 'classes-except-chunk-a-same',
                      'random-same', 'holds-same'):
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace(field + '=1', field + '=0', 1), True)

    def test_wrong_work_values_and_effects(self):
        for old, new in (
                ('calls=65536', 'calls=65535'), ('points=1024', 'points=1023'),
                ('chunks=9', 'chunks=8'), ('samples=7', 'samples=6'),
                ('chunk-a-before=0', 'chunk-a-before=1'),
                ('chunk-a-after=1 owners-same', 'chunk-a-after=0 owners-same'),
                ('result=1000', 'result=999'), ('acquisition-gets=0', 'acquisition-gets=1'),
                ('frequency=10000000', 'frequency=0'), ('ticks=10000', 'ticks=0'),
                ('sample=1', 'sample=0'), ('planes=123 chunk-a-before', 'planes=124 chunk-a-before'),
                ('cleanup=1', 'cleanup=0'), ('genuine-ew=1', 'genuine-ew=0'),
                ('measure=1', 'measure=0'), ('success-error=full', 'success-error=untouched')):
            with self.subTest(old=old):
                self.assertIn(old, wire())
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace(old, new, 1), True)

    def test_reordered_modes(self):
        rows = wire().splitlines()
        rows[3:11], rows[11:19] = rows[11:19], rows[3:11]
        with self.assertRaises(ValueError):
            CPU.parse_cpu_run('\n'.join(rows), True)

    def test_duplicate_fields(self):
        with self.assertRaises(ValueError):
            CPU.parse_cpu_run(wire().replace('ticks=10000', 'ticks=10000 ticks=10000', 1), True)

    def test_unsigned_extent(self):
        for value in ('-1', str(1 << 64), '1.5'):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    CPU.parse_cpu_run(wire().replace('ticks=10000', 'ticks=' + value, 1), True)

    def test_cohort_must_contain_both_contracts(self):
        public = CPU.parse_cpu_run(wire(), True)
        with self.assertRaises(ValueError):
            CPU.compare_cpu_runs([public, copy.deepcopy(public)])

    def test_cohort_must_match_semantics(self):
        profiles = [CPU.parse_cpu_run(wire(label), True) for label in ('public', 'failure')]
        profiles[1]['modes'][0]['expected'] = '999'
        for sample in profiles[1]['samples'][:7]:
            sample['result'] = '999'
        with self.assertRaises(ValueError):
            CPU.compare_cpu_runs(profiles)

    def test_cohort_rechecks_raw_contract(self):
        profiles = [CPU.parse_cpu_run(wire(label), True) for label in ('public', 'failure')]
        profiles[1]['samples'][0]['random-same'] = '0'
        with self.assertRaises(ValueError):
            CPU.compare_cpu_runs(profiles)

    def test_controls_have_no_timing_samples(self):
        with self.assertRaises(ValueError):
            CPU.parse_cpu_run(wire(measure=True), False)
        with self.assertRaises(ValueError):
            CPU.parse_cpu_run(wire(measure=False), True)


if __name__ == '__main__':
    unittest.main()
