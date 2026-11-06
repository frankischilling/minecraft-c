"""Reject independent row/type/semantic/source-pin mutations in Wolf observations."""
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
SPEC = importlib.util.spec_from_file_location('wolf_replay', HERE / 'run_native.py')
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)
JAR = None


def text(values):
    return '\n'.join('WOLF_ATTACK ' + json.dumps(row) for row in values)


class WolfPacket(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_json(HERE / 'finite.json')
        cls.pins = read_json(HERE / 'source-pins.json')
        cls.baseline = json.dumps(cls.packet, sort_keys=True)
        cls.expected = REPLAY.expected_rows(cls.packet)

    def tearDown(self):
        self.assertEqual(json.dumps(self.packet, sort_keys=True), self.baseline)
        self.assertEqual(REPLAY.expected_rows(self.packet), self.expected)

    def test_unchanged_baseline(self):
        self.assertEqual(len(self.expected), 24)
        REPLAY.compare(self.expected, self.expected)

    def test_count_identity_exception_effect_and_type_mutations(self):
        for values in (self.expected[:-1], [self.expected[1], self.expected[0], *self.expected[2:]]):
            with self.assertRaises(AssertionError):
                rows(text(values))
        for key, value in (('case', False), ('cooldown', True), ('exception', 'NPE'),
                           ('target_unchanged', False), ('target_attacked', 0), ('random', '1')):
            changed = copy.deepcopy(self.expected)
            changed[0][key] = value
            with self.assertRaises(AssertionError):
                rows(text(changed))
        changed = copy.deepcopy(self.expected)
        changed[0]['profile'][2] = True
        with self.assertRaises(AssertionError):
            rows(text(changed))

    def test_random_velocity_position_and_bounds_exact_comparison(self):
        for key in ('random', 'velocity', 'actor_position', 'target_position', 'actor_bounds', 'target_bounds'):
            changed = copy.deepcopy(self.expected)
            if key == 'random':
                changed[0][key] = f'{int(changed[0][key],16) ^ 1:016x}'
            else:
                changed[0][key][0] = f'{int(changed[0][key][0],16) ^ 1:016x}'
            self.assertTrue(strict_equal(rows(text(changed)), changed))
            with self.assertRaisesRegex(AssertionError, 'case0 field ' + key):
                REPLAY.compare(changed, self.expected)
            REPLAY.compare(self.expected, self.expected)

    def test_capture_source_pins(self):
        for key in self.packet['source_sha256']:
            changed = copy.deepcopy(self.packet)
            changed['source_sha256'][key] = '0' * 64
            with self.assertRaisesRegex(AssertionError, 'capture input pins changed'):
                REPLAY.expected_rows(changed)
        with self.assertRaisesRegex(AssertionError, 'packet schema'):
            REPLAY.expected_rows({'rows': self.expected})

    def test_duplicate_keys_and_boolean_coercion(self):
        with self.assertRaisesRegex(AssertionError, 'duplicate metadata key'):
            json.loads('{"case":0,"case":0}', object_pairs_hook=unique_object)
        self.assertFalse(strict_equal({'code_bytes': 1}, {'code_bytes': True}))

    def test_original_archive_and_corrupt_code_identity(self):
        if JAR is None:
            self.skipTest('local original archive not supplied')
        verify_pins(JAR, self.pins)
        for key, value in (('code_bytes', True), ('code_sha256', '0' * 64)):
            changed = copy.deepcopy(self.pins)
            changed['classes']['gi']['methods'][0][key] = value
            with self.assertRaisesRegex(AssertionError, 'class/method identity'):
                verify_pins(JAR, changed)
        changed = copy.deepcopy(self.pins)
        changed['classes']['sn']['methods'].pop()
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
