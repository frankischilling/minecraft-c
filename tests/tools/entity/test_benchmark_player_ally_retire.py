"""Portable corruption controls for the native retirement timing boundary."""
import importlib.util
from pathlib import Path
import tempfile
import unittest


PATH = Path(__file__).with_name('benchmark_player_ally_retire.py')
SPEC = importlib.util.spec_from_file_location('ally_retire_benchmark_controls', PATH)
BENCHMARK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARK)


def payload(variant):
    lines = ['ALLY_RETIRE_V2\t' + BENCHMARK.ADAPTERS[variant] + '\t10000000\t2\t21']
    for mode in ('unique', 'duplicate'):
        for count in BENCHMARK.COUNTS:
            for sample in range(23):
                failed = int(variant == 'fallback' and count > 8)
                cells = ('RETIRE', mode, count, sample, int(sample >= 2), 10,
                         count if mode == 'unique' else 1, count * (count + 1), failed)
                lines.append('\t'.join(map(str, cells)))
    return lines


class RetirementProtocol(unittest.TestCase):
    def read(self, lines, variant='candidate'):
        with tempfile.TemporaryDirectory(prefix='ally-retire-protocol-') as directory:
            path = Path(directory) / 'timing.tsv'
            path.write_text('\n'.join(lines) + '\n', encoding='ascii')
            return BENCHMARK.read_rows(path, variant)

    def test_complete_distinct_adapters(self):
        for variant in BENCHMARK.VARIANTS:
            with self.subTest(variant=variant):
                rows = self.read(payload(variant), variant)
                self.assertEqual(len(rows), 230)
                self.assertEqual(sum(row['timed'] for row in rows), 210)
                self.assertEqual(rows[-1]['occurrences'], 1024)

    def test_incomplete_or_duplicate_interval(self):
        original = payload('candidate')
        for lines in (original[:-1], original + [original[-1]],
                      original[:10] + original[11:] + [original[10]]):
            with self.subTest(lines=len(lines)):
                with self.assertRaises(ValueError):
                    self.read(lines)

    def test_adapter_and_clock_identity(self):
        original = payload('candidate')
        for header in ('ALLY_RETIRE_V1\t10000000\t2\t21',
                       'ALLY_RETIRE_V2\tquadratic\t10000000\t2\t21',
                       'ALLY_RETIRE_V2\tpointer-count\t0\t2\t21',
                       'ALLY_RETIRE_V2\tpointer-count\t10000000\t0\t23'):
            with self.subTest(header=header):
                with self.assertRaises(ValueError):
                    self.read([header] + original[1:])

    def test_changed_work_or_timing_fields(self):
        original = payload('candidate')
        for column, value in ((0, 'RETURN'), (1, 'duplicate'), (2, '1024'), (3, '1'),
                              (4, '1'), (5, '-1'), (5, '0.5'), (6, '0'), (7, '0'), (8, '1')):
            lines = original.copy()
            cells = lines[1].split('\t')
            cells[column] = value
            lines[1] = '\t'.join(cells)
            with self.subTest(column=column, value=value):
                with self.assertRaises(ValueError):
                    self.read(lines)

    def test_fallback_requires_actual_failed_request(self):
        lines = payload('fallback')
        cells = lines[47].split('\t')  # First count64 interval must fail optional allocation.
        self.assertEqual(cells[2], '64')
        cells[8] = '0'
        lines[47] = '\t'.join(cells)
        with self.assertRaises(ValueError):
            self.read(lines, 'fallback')

    def test_tiny_fallback_does_not_claim_an_allocation(self):
        lines = payload('fallback')
        cells = lines[1].split('\t')
        cells[8] = '1'
        lines[1] = '\t'.join(cells)
        with self.assertRaises(ValueError):
            self.read(lines, 'fallback')


if __name__ == '__main__':
    unittest.main()
