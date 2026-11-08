"""Pure parser/corruption controls; these are not original execution evidence."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace

import observation as O
import packet as P
import run_native as N


class ProtocolControls(unittest.TestCase):
    def test_inventory(self):
        self.assertEqual(len(O.profiles()), 40)
        self.assertEqual(O.profiles()[18][10], 16)

    def test_raw_arguments(self):
        self.assertEqual(O.arguments(O.profiles()[4])['position'], ['8000000000000000', '0000000000000000', '8000000000000000'])
        self.assertEqual(O.arguments(O.profiles()[6])['position'][0], '7ff8000000001234')
        self.assertEqual(O.arguments(O.profiles()[34])['speed'], '7fc01234')

    def test_duplicate_key(self):
        with self.assertRaises(ValueError):
            O.loads('{"case":0,"case":1}')

    def test_nonfinite_json(self):
        for token in ('NaN', 'Infinity', '-Infinity'):
            with self.subTest(token=token), self.assertRaises(ValueError):
                O.loads(token)

    def test_typed_comparison(self):
        self.assertFalse(O.exact(True, 1))
        self.assertFalse(O.exact({'a': 1}, {'a': 1, 'b': 2}))
        self.assertTrue(O.exact({'a': [False, None, '00']}, {'a': [False, None, '00']}))

    def test_words(self):
        for value in ('0', '0' * 17, 'A' * 16, 0, True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                O.word(value, 16)

    def test_random_snapshot(self):
        O.stream(['0000000000001234', False, '8000000000000000'])
        for value in (['0001000000000000', False, '0' * 16], ['0' * 16, 0, '0' * 16], ['0' * 16, False]):
            with self.subTest(value=value), self.assertRaises(ValueError):
                O.stream(value)

    def test_actual_cached_transition(self):
        O.gaussian_transition(['0000000000000001', True, '3ff0000000000000'], ['0000000000000001', False, '3ff0000000000000'], '3ff0000000000000')
        with self.assertRaises(ValueError):
            O.gaussian_transition(['0000000000000001', True, '3ff0000000000000'], ['0000000000000002', False, '3ff0000000000000'], '3ff0000000000000')

    def test_truncated_inventory(self):
        with self.assertRaises(ValueError):
            O.parse_rows('ARROW_CONSTRUCTION {}\n')

    def test_changed_profile_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'profiles.tsv'
            path.write_text((O.HERE / 'profiles.tsv').read_text().replace('18 3 1 0', '17 3 1 0'), encoding='ascii')
            with self.assertRaises(ValueError):
                O.profiles(directory)

    def test_wrong_snapshot_payload(self):
        # A closed leaf admits declared type independently from actual payload.
        value = {'class': 'sl', 'id': 0, 'world': None, 'arrow': dict(zip('abcdefghijk', [False, 0, None, -1, -1, -1, 0, 0, False, 0, 0])),
                 'position': ['0' * 16] * 3, 'motion': ['0' * 16] * 3, 'history': ['0' * 16] * 6,
                 'angles': ['0' * 8] * 4, 'dimensions': ['0' * 8] * 3, 'bounds': None, 'dead': False, 'random': None,
                 'watcher': {'map_null': False, 'dirty': True, 'records': [{'key': 16, 'type': 99, 'id': 16, 'value': {'kind': 'Byte', 'value': 0}, 'dirty': False}]}}
        O.snapshot(value)
        for replacement in ({'kind': 'Byte', 'value': True}, {'kind': 'String', 'value': None}, {'kind': 'Unknown', 'value': 0}):
            changed = copy.deepcopy(value); changed['watcher']['records'][0]['value'] = replacement
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                O.snapshot(changed)

    def test_published_actual_packet(self):
        finite, _ = P.read()
        self.assertEqual(len(finite['rows']), 40)
        self.assertEqual(sum(row['returned'] for row in finite['rows']), 37)
        self.assertEqual([len(calls) for calls in finite['traces']], [40, 40])

    def test_actual_trace_corruptions(self):
        finite, _ = P.read(); raw = finite['raw_runs'][2]
        for mode in ('missing_event', 'wrong_pc', 'lost_gaussian', 'wrong_cache', 'cold_math'):
            case = 0 if mode == 'cold_math' else 16
            trace = copy.deepcopy(finite['traces'][0][case])
            if mode == 'missing_event':
                trace['events'].pop(0)
            elif mode == 'wrong_pc':
                next(e['value'] for e in trace['events'] if e['value']['kind'] == 'RETURN' and e['value']['site']['class'] == 'sl')['site']['pc'] = 197
            elif mode == 'lost_gaussian':
                e = next(e['value'] for e in trace['events'] if e['value']['kind'] == 'RETURN' and e['value']['site']['class'] == 'java.util.Random')
                e['site']['class'] = 'unentered'
            elif mode == 'wrong_cache':
                e = next(e['value'] for e in trace['events'] if e['value']['kind'] == 'RETURN' and e['value']['site']['class'] == 'java.util.Random')
                e['random'][0] = '0000000000000000'
            else:
                trace['math_initialized_after'] = True
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                O.source_call(trace, raw[case])

    def test_live_code_corruptions(self):
        finite, _ = P.read(); pins = P.load(O.HERE / 'source-pins.json')
        for mode in ('hash', 'extent', 'method', 'duplicate', 'missing'):
            codes = copy.deepcopy(finite['code_runs'][0])
            if mode == 'hash':
                codes[0]['sha256'] = '0' * 64
            elif mode == 'extent':
                codes[0]['bytes'] += 1
            elif mode == 'method':
                codes[0]['method'] = 'replacement'
            elif mode == 'duplicate':
                codes.append(copy.deepcopy(codes[0]))
            else:
                codes = [c for c in codes if c['class'] != 'sl']
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                P.code_records(codes, pins)

    def test_native_process_corruptions(self):
        finite, _ = P.read()
        lines = ['ARROW_CONSTRUCTION ' + json.dumps(row, separators=(',', ':')) for row in finite['raw_runs'][0]]
        lines[1:1] = ['16 achievements', '151 recipes']
        lines.append('Arrow construction reference: 123 checks;40 genuine profiles')
        data = ('\n'.join(lines) + '\n').encode()
        receipt = {'exit': 0, 'timed_out': False, 'output_overflow': False, 'pipe_capture_complete': True, 'reaped': True,
                   'reader_errors': {}, 'timeout_seconds': N.TIMEOUT, 'limit': N.STREAM_LIMIT, 'stdout_bytes': len(data), 'stderr_bytes': 0}
        rows, checks = N.completed_native(SimpleNamespace(returncode=0, stdout=data, stderr=b'', capture_report=receipt))
        self.assertEqual((len(rows), checks), (40, 123))
        for key, value in (('exit', 1), ('timed_out', True), ('output_overflow', True), ('pipe_capture_complete', False),
                           ('reaped', False), ('reader_errors', {'stdout': 'lost'}), ('stdout_bytes', len(data) - 1)):
            changed = copy.deepcopy(receipt); changed[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                N.completed_native(SimpleNamespace(returncode=0, stdout=data, stderr=b'', capture_report=changed))
        for changed_data in (data[:-1], data.replace(b'123 checks', b'0 checks'), data + b'EXTRA\n', data.replace(b'16 achievements\n', b'')):
            changed = copy.deepcopy(receipt); changed['stdout_bytes'] = len(changed_data)
            with self.subTest(data=changed_data[-80:]), self.assertRaises(ValueError):
                N.completed_native(SimpleNamespace(returncode=0, stdout=changed_data, stderr=b'', capture_report=changed))

    def test_seeded_returned_locals_even_when_velocity_hides_them(self):
        finite, _ = P.read()
        for case in (16, 17, 18, 20, 21, 22):
            row = copy.deepcopy(finite['raw_runs'][2][case])
            row['native'] = {'gaussian_values': copy.deepcopy(finite['receipts'][0][case]['gaussian_values'])}
            N.seeded_gaussians(finite, row)
            for axis in range(3):
                changed = copy.deepcopy(row); changed['native']['gaussian_values'][axis] = '0000000000000000'
                with self.subTest(case=case, axis=axis), self.assertRaises(ValueError):
                    N.seeded_gaussians(finite, changed)


if __name__ == '__main__':
    unittest.main()
