"""Bounded negative controls for animal death rows, entropy and source authority."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from capture_reference import read_json, rows, strict_equal, unique_object, verify_pins
from validate import audit_entropy
SPEC = importlib.util.spec_from_file_location('animal_replay', HERE / 'run_native.py')
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)
JAR = None


def text(values):
    return '\n'.join('ANIMAL_DEATH ' + json.dumps(row) for row in values)


class AnimalPacket(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_json(HERE / 'finite.json')
        cls.pins = read_json(HERE / 'source-pins.json')
        cls.baseline = json.dumps(cls.packet, sort_keys=True)
        cls.expected = REPLAY.expected_rows(cls.packet)
        cls.native = copy.deepcopy(cls.expected)
        for row in cls.native:
            if row['profile'] in (33, 34):
                row['exception'] = 'IO'

    def tearDown(self):
        self.assertEqual(json.dumps(self.packet, sort_keys=True), self.baseline)
        self.assertEqual(REPLAY.expected_rows(self.packet), self.expected)

    def test_baseline_and_actual_entropy_census(self):
        self.assertEqual(len(rows(text(self.native), native=True)), 43)
        self.assertEqual(audit_entropy(self.expected), {'new_items': 38, 'Math_nextDouble_calls': 153,
                         'default_Random_occurrences': 38, 'profiles': 42, 'calls': 43})
        REPLAY.compare(self.native, self.expected)

    def test_identity_inventory_types_and_categories(self):
        for values in (self.native[:-1], [self.native[1], self.native[0], *self.native[2:]]):
            with self.assertRaises(AssertionError):
                rows(text(values), native=True)
        for key, value in (('case', False), ('profile', True), ('step', 1), ('result', 1), ('exception', 'NPE')):
            changed = copy.deepcopy(self.native)
            changed[0][key] = value
            with self.assertRaises(AssertionError):
                rows(text(changed), native=True)
        changed = copy.deepcopy(self.native)
        changed[0]['input'][0] = False
        with self.assertRaises(AssertionError):
            rows(text(changed), native=True)

    def test_constructor_formula_and_entropy_progress_mutations(self):
        for key in ('math_before', 'uniquifier_before', 'uniquifier_after'):
            changed = copy.deepcopy(self.native)
            changed[0][key] = f'{int(changed[0][key],16)^1:016x}'
            with self.assertRaises(AssertionError):
                audit_entropy(rows(text(changed), native=True))
        for key, offset in (('rotation', 0), ('rotation', 4), ('velocity', 0), ('velocity', 1), ('velocity', 2)):
            changed = copy.deepcopy(self.native)
            values = changed[0]['state']['items'][0][key]
            values[offset] = f'{int(values[offset],16)^1:0{len(values[offset])}x}'
            with self.assertRaises(AssertionError):
                audit_entropy(rows(text(changed), native=True))
        changed = copy.deepcopy(self.native)
        changed[0]['state']['math_random'] = f'{int(changed[0]["state"]["math_random"],16)^1:016x}'
        with self.assertRaises(AssertionError):
            audit_entropy(changed)

    def test_deterministic_stack_item_and_damage_mutations(self):
        for key, index in (('stack', 0), ('stack', 1), ('stack', 2), ('body', 1), ('position', 0),
                           ('previous_position', 1), ('size', 0), ('bounds', 0), ('chunk', 2), ('flags', 7)):
            changed = copy.deepcopy(self.native)
            values = changed[0]['state']['items'][0][key]
            value = values[index]
            values[index] = not value if type(value) is bool else value + 1 if type(value) is int else f'{int(value,16)^1:0{len(value)}x}'
            with self.assertRaisesRegex(AssertionError, 'case0 field state'):
                REPLAY.compare(rows(text(changed), native=True), self.expected)
        for key, index in (('health', 0), ('health', 8), ('animation', 0), ('velocity', 2)):
            changed = copy.deepcopy(self.native)
            value = changed[0]['state'][key][index]
            changed[0]['state'][key][index] = value + 1 if type(value) is int else f'{int(value,16)^1:0{len(value)}x}'
            with self.assertRaisesRegex(AssertionError, 'case0 field state'):
                REPLAY.compare(rows(text(changed), native=True), self.expected)

    def test_membership_owner_callback_and_repeat_mutations(self):
        for key, value in (('world_member', False), ('chunk_members', 0), ('stack_unique', False)):
            changed = copy.deepcopy(self.native)
            changed[0]['state']['items'][0][key] = value
            with self.assertRaises(AssertionError):
                rows(text(changed), native=True)
        for key, value in (('source_unchanged', False), ('world_loaded', 0), ('chunk_count', False)):
            changed = copy.deepcopy(self.native)
            changed[0]['state'][key] = value
            with self.assertRaises(AssertionError):
                rows(text(changed), native=True)
        changed = copy.deepcopy(self.native)
        changed[33]['state']['items'][0]['stack'][2] ^= 1
        with self.assertRaisesRegex(AssertionError, 'repeated death preserves'):
            audit_entropy(changed)
        changed = copy.deepcopy(self.native)
        changed[34]['exception'] = 'CALLBACK'
        with self.assertRaises(AssertionError):
            rows(text(changed), native=True)
        with self.assertRaisesRegex(AssertionError, 'publication failure categories'):
            REPLAY.compare(changed, self.expected)

    def test_independent_private_clock_context_is_preserved_separately(self):
        changed = copy.deepcopy(self.native)
        changed[0]['state']['items'][0]['private_random'] = f'{int(changed[0]["state"]["items"][0]["private_random"],16)^1:016x}'
        changed[0]['constructor']['actor_private_random'] = '0000000000000000'
        # Raw clock seeds are deliberately not cross-process equality claims.
        rows(text(changed), native=True)
        REPLAY.compare(changed, self.expected)
        self.assertNotEqual(changed[0]['state']['items'][0]['private_random'], self.expected[0]['state']['items'][0]['private_random'])

    def test_source_closure_duplicate_keys_and_coercion(self):
        for key in self.packet['source_sha256']:
            changed = copy.deepcopy(self.packet)
            changed['source_sha256'][key] = '0' * 64
            with self.assertRaisesRegex(AssertionError, 'capture input pins changed'):
                REPLAY.expected_rows(changed)
        with self.assertRaisesRegex(AssertionError, 'packet schema'):
            REPLAY.expected_rows({'rows': self.expected})
        with self.assertRaisesRegex(AssertionError, 'duplicate metadata key'):
            json.loads('{"case":0,"case":0}', object_pairs_hook=unique_object)
        self.assertFalse(strict_equal({'code_bytes': 1}, {'code_bytes': True}))

    def test_original_archive_and_code_pin_mutations(self):
        if JAR is None:
            self.skipTest('local original archive not supplied')
        verify_pins(JAR, self.pins)
        for key, value in (('code_bytes', True), ('code_sha256', '0' * 64)):
            changed = copy.deepcopy(self.pins)
            changed['classes']['hl']['methods'][0][key] = value
            with self.assertRaisesRegex(AssertionError, 'class/method identity'):
                verify_pins(JAR, changed)
        changed = copy.deepcopy(self.pins)
        changed['classes']['ls']['methods'].pop()
        with self.assertRaisesRegex(AssertionError, 'class/method identity'):
            verify_pins(JAR, changed)
        verify_pins(JAR, self.pins)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client-jar', type=Path)
    args, remaining = parser.parse_known_args()
    if args.client_jar is not None:
        JAR = args.client_jar.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining])
