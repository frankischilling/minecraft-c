"""Pure protocol corruption controls; no original or native execution credit."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace

import observation as O
import packet as P
import run_native as N


def compound(values):
    return {'kind': 10, 'map_null': False, 'values': values,
            'source_order': [{'key': {'utf16': [ord(c) for c in k]}, 'tag': copy.deepcopy(v)} for k, v in values.items()]}


class ProtocolControls(unittest.TestCase):
    def test_exact_inventory(self):
        rows = O.profiles()
        self.assertEqual(len(rows), 48)
        self.assertEqual([r[0] for r in rows if r[1] == 5], list(range(39, 44)))
        self.assertEqual(rows[44:], [[44, 0, 0, 10, 1], [45, 0, 0, 11, 1], [46, 1, 0, 9, 1], [47, 1, 0, 11, 1]])

    def test_changed_literal_profiles_rejected(self):
        text = (O.HERE / 'profiles.tsv').read_text()
        for changed in (text.replace('0 0 0 0 1', '0 0 1 0 1'), text.rsplit('\n', 2)[0] + '\n', text + text[:10]):
            with tempfile.TemporaryDirectory() as d:
                (Path(d) / 'profiles.tsv').write_text(changed)
                with self.assertRaises(ValueError):
                    O.profiles(d)

    def test_json_duplicate_and_nonfinite(self):
        for text in ('{"case":0,"case":1}', '{"x":NaN}', '{"x":Infinity}', '{"x":-Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                O.loads(text)

    def test_typed_equality(self):
        self.assertFalse(O.exact(True, 1))
        self.assertFalse(O.exact({'x': 1}, {'x': 1, 'y': 2}))
        self.assertTrue(O.exact({'x': [None, False, '00']}, {'x': [None, False, '00']}))

    def test_scalar_raw_bits_and_signed_ranges(self):
        for value in ({'kind': 1, 'value': -128}, {'kind': 2, 'value': 32767}, {'kind': 3, 'value': -(1 << 31)},
                      {'kind': 5, 'value': {'F': '7fc01234'}}, {'kind': 6, 'value': {'D': '8000000000000000'}},
                      {'kind': 8, 'value': {'utf16': [0, 0xd800, 0xffff]}}, {'kind': 8, 'value': None}):
            O.tag(value)
        for value in ({'kind': 1, 'value': True}, {'kind': 1, 'value': 128}, {'kind': 2, 'value': -32769},
                      {'kind': 5, 'value': {'D': '0000000000000000'}}, {'kind': 6, 'value': {'D': 'A' * 16}},
                      {'kind': 8, 'value': {'utf16': [-1]}}):
            with self.subTest(value=value), self.assertRaises(ValueError):
                O.tag(value)

    def test_missing_map_present_null_and_null_map_are_distinct(self):
        values = [compound({}), compound({'xTile': None}), {'kind': 10, 'map_null': True, 'values': {}, 'source_order': []}]
        for value in values:
            O.tag(value)
        self.assertEqual(len({json.dumps(O.canonical_tag(v), sort_keys=True) for v in values}), 3)

    def test_compound_traversal_is_complete_unique_and_raw(self):
        value = compound({'z': {'kind': 1, 'value': -1}, 'a': {'kind': 2, 'value': -32768}})
        O.tag(value)
        changed = copy.deepcopy(value); changed['source_order'].reverse()
        O.tag(changed)
        self.assertTrue(O.exact(O.canonical_tag(value), O.canonical_tag(changed)))
        for mode in ('duplicate', 'missing', 'changed_value', 'changed_key'):
            changed = copy.deepcopy(value)
            if mode == 'duplicate':
                changed['source_order'].append(copy.deepcopy(changed['source_order'][0]))
            elif mode == 'missing':
                changed['source_order'].pop()
            elif mode == 'changed_value':
                changed['source_order'][0]['tag']['value'] = 1
            else:
                changed['source_order'][0]['key']['utf16'] = [ord('q')]
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                O.tag(changed)

    def test_list_null_and_wrong_numeric_element_are_not_coerced(self):
        value = {'kind': 9, 'element_type': 6, 'values': [{'kind': 6, 'value': {'D': '3fb999999999999a'}}, None,
                                                        {'kind': 5, 'value': {'F': '3e4ccccd'}}]}
        O.tag(value)
        self.assertTrue(O.exact(O.canonical_tag(value), value))

    def test_code_corruption_and_missing_complete_entry(self):
        pins = P.load(O.HERE / 'source-pins.json')
        codes = []
        for owner, name, descriptor, _ in O.ROOTS:
            method = next(m for m in pins['classes'][owner]['methods'] if m['name'] == name and m['descriptor'] == descriptor)
            codes.append({'event': 'code', 'class': owner, 'method': name, 'descriptor': descriptor,
                          'bytes': method['code_length'], 'sha256': method['code_sha256']})
        P.code_records(codes, pins)
        for mode in ('duplicate', 'missing', 'hash', 'extent'):
            changed = copy.deepcopy(codes)
            if mode == 'duplicate':
                changed.append(copy.deepcopy(changed[0]))
            elif mode == 'missing':
                changed.pop()
            elif mode == 'hash':
                changed[0]['sha256'] = '0' * 64
            else:
                changed[0]['bytes'] += 1
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                P.code_records(changed, pins)

    def test_source_only_rows_cannot_enter_projection(self):
        with self.assertRaises(ValueError):
            O.project({'case': 44})

    def test_truncated_rows_and_nonexact_pairing(self):
        with self.assertRaises(ValueError):
            O.parse_rows('ARROW_NBT {}\n')
        with self.assertRaises(ValueError):
            O.compare([], [])
        left = [{'case': i, 'value': False} for i in range(44)]
        right = copy.deepcopy(left); right[23]['value'] = 0
        with self.assertRaises(ValueError):
            O.compare(left, right)

    def test_actual_complete_four_vm_packet(self):
        finite, _ = P.read()
        self.assertEqual(len(finite['rows']), 44)
        self.assertEqual([len(rows) for rows in finite['raw_runs']], [48] * 4)
        self.assertEqual([len(calls) for calls in finite['traces']], [48, 48])
        self.assertEqual([row['case'] for row in finite['raw_runs'][0] if row['domain'] == 'source-only'], [44, 45, 46, 47])

    def test_actual_source_trace_corruption(self):
        finite, _ = P.read()
        for mode in ('lost_event', 'return_pc', 'lost_store', 'shooter_write', 'list_failure_site', 'factory_catch'):
            case = 26 if mode == 'list_failure_site' else 42 if mode == 'factory_catch' else 0
            trace = copy.deepcopy(finite['traces'][0][case])
            values = [e['value'] for e in trace['events']]
            if mode == 'lost_event':
                trace['events'].pop(0)
            elif mode == 'return_pc':
                next(e for e in values if e['kind'] == 'RETURN' and e['site']['class'] == 'sl')['site']['pc'] = 100
            elif mode == 'lost_store':
                trace['events'] = [e for e in trace['events'] if not (e['value']['kind'] == 'STORE' and e['value']['field'] == 'sl.d')]
                for n, event in enumerate(trace['events']):
                    event['sequence'] = n
            elif mode == 'shooter_write':
                next(e for e in values if e['kind'] == 'STORE' and e['field'] == 'sl.d')['field'] = 'sl.c'
            else:
                error = next(e for e in values if e['kind'] == 'EXCEPTION')
                for frame in error['stack']:
                    if frame['class'] == ('sn' if mode == 'list_failure_site' else 'jc'):
                        frame['pc'] = 0
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                O.source_call(trace, finite['raw_runs'][2][case])

    def test_native_transport_receipt_corruptions(self):
        finite, _ = P.read()
        # These are synthetic transport controls over retained source rows,
        # not observations of a native execution.
        rows = copy.deepcopy(finite['raw_runs'][0][:44])
        for row in rows:
            if row['input'][1] in (1, 3, 4) and row['case'] != 37:
                row['tag_after']['values'].pop('retained_unknown')
                row['tag_after']['source_order'] = [item for item in row['tag_after']['source_order'] if item['key']['utf16'] != list(map(ord, 'retained_unknown'))]
        lines = ['16 achievements', '151 recipes']
        for row in rows:
            if row['case'] == 43:
                lines.append('Skipping Entity with id NoSuchArrowType')
            lines.append('ARROW_NBT ' + json.dumps(row, separators=(',', ':')))
        lines.append('Arrow NBT reference: 123 checks;44 genuine profiles')
        data = ('\n'.join(lines) + '\n').encode()
        receipt = {'exit': 0, 'timed_out': False, 'output_overflow': False, 'pipe_capture_complete': True, 'reaped': True,
                   'reader_errors': {}, 'timeout_seconds': N.TIMEOUT, 'limit': N.STREAM_LIMIT, 'stdout_bytes': len(data), 'stderr_bytes': 0}
        actual, checks = N.completed_native(SimpleNamespace(returncode=0, stdout=data, stderr=b'', capture_report=receipt))
        self.assertEqual((len(actual), checks), (44, 123))
        for key, value in (('exit', 1), ('timed_out', True), ('output_overflow', True), ('pipe_capture_complete', False),
                           ('reaped', False), ('reader_errors', {'stdout': 'lost'}), ('stdout_bytes', len(data) - 1),
                           ('stderr_bytes', False), ('stderr_bytes', 0.0),
                           ('stdout_bytes', float(len(data))),
                           ('timeout_seconds', float(N.TIMEOUT)), ('limit', float(N.STREAM_LIMIT))):
            changed = copy.deepcopy(receipt); changed[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                N.completed_native(SimpleNamespace(returncode=0, stdout=data, stderr=b'', capture_report=changed))
        for changed_data in (data[:-1], data + b'EXTRA\n', data.replace(b'123 checks', b'0 checks'), data.replace(b'Skipping Entity', b'Suppressed Entity')):
            changed = copy.deepcopy(receipt); changed['stdout_bytes'] = len(changed_data)
            with self.subTest(data=changed_data[-100:]), self.assertRaises(ValueError):
                N.completed_native(SimpleNamespace(returncode=0, stdout=changed_data, stderr=b'', capture_report=changed))

    def test_native_own_receipt_corruptions(self):
        finite, _ = P.read()
        row = copy.deepcopy(finite['raw_runs'][0][7])
        row['native'] = {'status': 3, 'base_pc': 0, 'factory_pc': 0, 'wrapper_pc': 0,
                         'arrow_site': {'class': 'sl', 'method': 'a', 'descriptor': '(Lnu;)V', 'pc': 4},
                         'arrow_child': {'class': 'nu', 'method': 'd', 'descriptor': '(Ljava/lang/String;)S', 'pc': 25}, 'arrow_returned': False}
        N.native_receipt(row)
        for key, value in (('status', 0), ('base_pc', 311), ('factory_pc', 66), ('arrow_returned', True), ('arrow_child', None)):
            changed = copy.deepcopy(row); changed['native'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                N.native_receipt(changed)


if __name__ == '__main__':
    unittest.main()
