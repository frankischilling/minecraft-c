"""Compact integrity controls for the finite original nested Stair getter proof."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('nested_stair_packet', HERE / 'packet.py')
P = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P)


class PacketTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.observations = P.read(HERE / 'observations.json')
        cls.pins = P.read(HERE / 'source-pins.json')

    def corrupt(self, mutate):
        observations = copy.deepcopy(self.observations)
        mutate(observations)
        with self.assertRaises((ValueError, KeyError, IndexError, TypeError)):
            P.audit(observations, self.pins)

    def test_actual_source_and_current_file_audit(self):
        self.assertEqual(P.audit_public(), {'rows': 30, 'returned': 20, 'source_failures': 10,
            'getter_entries': 80, 'live_code_bodies': 2, 'exact_nested_pcs': [5, 5], 'network': 0, 'native_links': 0})

    def test_declared_mutable_field(self):
        self.corrupt(lambda d: d['owners'].update(field_final=True))

    def test_boolean_integer_types_remain_distinct(self):
        self.corrupt(lambda d: d['inputs'][0].update(call=False))

    def test_missing_or_reordered_call(self):
        self.corrupt(lambda d: d['returns'].pop())
        self.corrupt(lambda d: d['inputs'].reverse())

    def test_late_texture_is_actual(self):
        self.corrupt(lambda d: d['returns'][10].update(texture=4))

    def test_signed_extreme_side(self):
        self.corrupt(lambda d: d['inputs'][8].update(side=2147483647))

    def test_exact_nested_null_alias(self):
        self.corrupt(lambda d: d['inputs'][20].update(inner_source=5))

    def test_live_code_body(self):
        self.corrupt(lambda d: d['events'][0].update(sha256='0' * 64))

    def test_actual_receiver_identity(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'getter-entry').update(receiver=0))

    def test_actual_getter_order(self):
        def mutation(data):
            entries = [i for i, e in enumerate(data['events']) if e['event'] == 'getter-entry']
            a, b = entries[0], entries[1]
            data['events'][a], data['events'][b] = data['events'][b], data['events'][a]
        self.corrupt(mutation)

    def test_both_nested_source_frames(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'source-exception')['frames'].pop())

    def test_exact_reached_source_pc(self):
        self.corrupt(lambda d: next(e for e in d['events'] if e['event'] == 'source-exception')['frames'][0]['site'].update(pc=0))

    def test_no_source_state_repair(self):
        self.corrupt(lambda d: d['returns'][20].update(inner_source=5))

    def test_no_cycle_or_external_effect_claim(self):
        self.corrupt(lambda d: d['complete'].update(cycle_executed=True))
        self.corrupt(lambda d: d['complete'].update(native_links=1))

    def test_class_code_binding(self):
        pins = copy.deepcopy(self.pins)
        pins['classes']['ss']['method']['code_sha256'] = '0' * 64
        with self.assertRaises(ValueError):
            P.audit(self.observations, pins)

    def test_duplicate_fields(self):
        with self.assertRaises(ValueError):
            json.loads('{"call":0,"call":1}', object_pairs_hook=P.unique)


if __name__ == '__main__':
    unittest.main()
