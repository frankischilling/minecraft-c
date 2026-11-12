"""Pure source-packet corruption controls; no JVM or native execution."""
import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest

import leaf_ledger as L
import observation as O
import packet as P


def row_text(rows):
    return '\n'.join(O.PREFIX + json.dumps(row, separators=(',', ':')) for row in rows) + '\n'


class SourceControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.finite, cls.provenance = P.read()
        cls.capture = P.load(P.HERE / 'capture-receipt.json')
        cls.pins = P.source_pins()
        cls.helpers = P.load(P.HERE / 'helper-pins.json')
        cls.raw = [O.strict_rows(v['stdout']) for v in cls.finite['source_streams']]
        cls.records = [[O.loads(line) for line in text.splitlines()] for text in cls.finite['source_journals']]
        cls.traces = [[r for r in records if r['event'] == 'call'] for records in cls.records]

    def test_actual_complete_packet_and_domain_counts(self):
        c = self.finite['counts']
        self.assertEqual((c['VMs'], c['calls'], c['normal_returns'], c['source_exceptions']), (4, 96, 60, 36))
        self.assertEqual((c['ordinary_common_scalars'], c['additional_passive_child_scalars']), (9270, 252))
        self.assertEqual(c['source_events_each'], [2671, 2671])
        self.assertEqual([row['case'] for row in self.finite['passive_children']], list(O.INVISIBLE))

    def test_exact_captured_suppliers_and_separate_current_processing(self):
        P.validate_capture(self.capture)
        proof = P.processing_derivation()
        self.assertNotEqual(proof['captured_validator_sha256'], proof['current_validator_sha256'])
        self.assertIn('source_call', proof['captured_functions_preserved'])
        with tempfile.TemporaryDirectory() as name:
            path = Path(name)
            for relative in P.CAPTURED.values():
                target = path / relative; target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(P.HERE / relative, target)
            P.validate_capture(self.capture, path)
            with (path / 'SkeletonFireObservation.java').open('ab') as f:
                f.write(b'\n')
            with self.assertRaises(ValueError):
                P.validate_capture(self.capture, path)

    def test_literal_profile_mutation_rejected(self):
        text = (P.HERE / 'profiles.tsv').read_text()
        for changed in (text.replace('41200000', '411fffff', 1), text.rsplit('\n', 2)[0] + '\n'):
            with tempfile.TemporaryDirectory() as name:
                (Path(name) / 'profiles.tsv').write_text(changed)
                with self.assertRaises(ValueError):
                    O.strict_profiles(name)

    def test_duplicate_nonfinite_and_typed_comparison(self):
        for text in ('{"case":0,"case":1}', '{"x":NaN}', '{"x":Infinity}', '{"x":-Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                O.loads(text)
        self.assertFalse(O.exact(True, 1))
        changed = copy.deepcopy(self.finite['ordinary_common'])
        changed[0]['returned'] = 1
        with self.assertRaises(ValueError):
            O.compare_rows(self.finite['ordinary_common'], changed, range(24))

    def test_raw_stream_hash_and_complete_footer(self):
        stream = copy.deepcopy(self.finite['source_streams'][0]); receipt = self.capture['ordinary'][0]
        stream['stdout'] = stream['stdout'].replace('SKELETON_FIRE ', 'SKELETON_FIRX ', 1)
        with self.assertRaises(ValueError):
            P.stream_rows(stream, receipt, self.pins)
        stream = copy.deepcopy(self.finite['source_streams'][0]); stream['stdout'] = stream['stdout'].rsplit('SKELETON_FIRE_COMPLETE', 1)[0]
        receipt = copy.deepcopy(receipt); receipt['stdout_bytes'] = len(stream['stdout'].encode()); receipt['stdout_sha256'] = P.data_digest(stream['stdout'])
        with self.assertRaises(ValueError):
            P.stream_rows(stream, receipt, self.pins)

    def test_process_receipt_cannot_lower_completion_guards(self):
        for key, value in (('exit', 1), ('timed_out', True), ('pipe_capture_complete', False),
                           ('output_overflow', True), ('reader_errors', {'stdout': 'lost'}), ('timeout_seconds', 239)):
            receipt = copy.deepcopy(self.capture['ordinary'][0]); receipt[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                P.process_receipt(receipt, 240)
        post = P.load(P.HERE / 'source-postflight.json'); post['runs'][2]['target_receipt']['reaped'] = False
        with self.assertRaises(ValueError):
            P.validate_postflight(post)

    def test_auxiliary_wolf_yaw_exclusion_is_precise_and_within_run_still_checked(self):
        rows = copy.deepcopy(self.raw[0]); row = rows[0]
        for side in ('before', 'after'):
            row[side]['wolf']['angles'][0] = '3f800000'
        O.strict_rows(row_text(rows))
        self.assertTrue(O.exact(O.common_projection(row), self.finite['ordinary_common'][0]))
        row['before']['wolf']['angles'][0] = '40000000'
        with self.assertRaises(ValueError):
            O.strict_rows(row_text(rows))
        for case in (6, 8):
            row = copy.deepcopy(self.raw[0][case]); row['before']['wolf']['angles'][0] = '3f800000'
            self.assertFalse(O.exact(O.common_projection(row), self.finite['ordinary_common'][case]))
        row = copy.deepcopy(self.raw[0][0]); row['before']['wolf']['angles'][1] = '3f800000'
        self.assertFalse(O.exact(O.common_projection(row), self.finite['ordinary_common'][0]))

    def test_ordinary_invisible_child_is_never_filled(self):
        rows = copy.deepcopy(self.raw[0]); rows[12]['arrow_after'] = copy.deepcopy(self.traces[0][12]['final_arrow'])
        with self.assertRaises(ValueError):
            O.strict_rows(row_text(rows))
        with self.assertRaises(ValueError):
            O.local_child_projection(7, self.traces[0][7]['final_arrow'])
        with self.assertRaises(ValueError):
            O.local_child_projection(12, None)
        for case in O.INVISIBLE:
            actual = O.local_child_projection(case, self.traces[0][case]['final_arrow'])
            expected = next(row for row in self.finite['passive_children'] if row['case'] == case)
            self.assertTrue(O.exact(actual, expected))

    def test_closed_callbacks_and_actual_bow_pitch_arithmetic(self):
        row = copy.deepcopy(self.raw[2][7]); row['callbacks'][0]['unexpected'] = True
        with self.assertRaises(ValueError):
            O.validate_callbacks(row)
        row = copy.deepcopy(self.raw[2][7]); row['callbacks'][0]['pitch'] = '3f800000'
        with self.assertRaises(ValueError):
            O.strict_source_call(self.traces[0][7], row)
        row = copy.deepcopy(self.raw[2][17]); row['callbacks'].append(copy.deepcopy(self.raw[2][7]['callbacks'][-1]))
        with self.assertRaises(ValueError):
            O.validate_callbacks(row)

    def test_source_journal_return_random_and_exception_boundary_corruptions(self):
        for mode in ('missing_event', 'return_pc', 'lost_gaussian', 'float_seed', 'outer_failure_site'):
            case = 13 if mode == 'outer_failure_site' else 7
            trace = copy.deepcopy(self.traces[0][case]); values = [e['value'] for e in trace['events']]
            if mode == 'missing_event':
                trace['events'].pop(0)
            elif mode == 'return_pc':
                next(v for v in values if v['kind'] == 'RETURN' and v['site']['class'] == 'fr')['site']['pc'] = 187
            elif mode == 'lost_gaussian':
                event = next(e for e in trace['events'] if e['value']['kind'] == 'RETURN' and e['value']['site']['method'] == 'nextGaussian')
                trace['events'].remove(event)
                for i, e in enumerate(trace['events']):
                    e['sequence'] = i
            elif mode == 'float_seed':
                event = next(v for v in values if v['kind'] == 'RETURN' and v['site']['method'] == 'nextFloat')
                event['random'][0] = '0000000000000000'
            else:
                event = next(v for v in values if v['kind'] == 'EXCEPTION')
                for frame in event['stack']:
                    if frame['class'] == 'fr':
                        frame['pc'] = 133
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                O.strict_source_call(trace, self.raw[2][case])

    def test_live_original_and_authored_Code_authorities(self):
        codes = [r for r in self.records[0] if r['event'] != 'call']
        P.code_records(codes, self.pins, self.helpers)
        for mode in ('duplicate', 'missing_parent', 'hash', 'extent_type', 'helper_as_original'):
            changed = copy.deepcopy(codes)
            if mode == 'duplicate':
                changed.append(copy.deepcopy(changed[0]))
            elif mode == 'missing_parent':
                changed = [c for c in changed if c['class'] != 'fr']
            elif mode == 'hash':
                changed[0]['sha256'] = '0' * 64
            elif mode == 'extent_type':
                next(c for c in changed if c['bytes'] == 1)['bytes'] = True
            else:
                next(c for c in changed if c['event'] == 'helper-code')['event'] = 'code'
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                P.code_records(changed, self.pins, self.helpers)

    def test_helpers_are_metadata_bound_to_captured_compilation(self):
        changed = copy.deepcopy(self.helpers)
        changed['classes']['SkeletonFireObservation$Probe']['class_sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            P.helper_metadata(changed, self.capture)
        changed = copy.deepcopy(self.helpers['classes']['SkeletonFireObservation$Probe']); changed['Code_bytes'] = '00'
        with self.assertRaises(ValueError):
            P.metadata_class(changed)

    def test_captured_validator_bodies_cannot_be_replaced_by_current_processing(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name); (path / 'captured').mkdir()
            shutil.copyfile(P.HERE / 'captured/observation.py', path / 'captured/observation.py')
            shutil.copyfile(P.HERE / 'observation.py', path / 'observation.py')
            P.processing_derivation(path)
            with (path / 'observation.py').open('a') as f:
                f.write('\ndef source_call(trace, row):\n    return {}\n')
            with self.assertRaises(ValueError):
                P.processing_derivation(path)

    def test_source_raw_leaf_inventory_is_disjoint(self):
        total = 0
        for rows in self.raw:
            groups = L.partition(rows); paths = [tuple(v) for values in groups.values() for v in values]
            self.assertEqual(len(paths), len(set(paths)))
            self.assertEqual(len(paths), 20618)
            self.assertEqual(len(groups['unposed_auxiliary_Wolf_Math_random_constructor_yaw']), 44)
            total += len(paths)
        self.assertEqual(total, 82472)
        with self.assertRaises(ValueError):
            L.partition([self.raw[0][0], self.raw[0][0]])

    def test_callback_prefix_policy_removes_only_declared_outcomes(self):
        for case in (15, 17):
            row = self.finite['ordinary_common'][case]
            projected = O.prefix_projection(row)
            self.assertEqual(set(row) - set(projected), {'returned', 'exception'})
            changed = copy.deepcopy(row); changed['returned'] = None; changed['exception'] = None
            self.assertTrue(O.exact(projected, O.prefix_projection(changed)))
            changed['after']['actor']['cooldown'] = 30
            self.assertFalse(O.exact(projected, O.prefix_projection(changed)))
        with self.assertRaises(ValueError):
            O.prefix_projection(self.finite['ordinary_common'][14])


if __name__ == '__main__':
    unittest.main()
