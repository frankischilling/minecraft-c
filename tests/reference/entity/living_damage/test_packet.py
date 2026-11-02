"""Reject row, primitive, scope, owner and source-pin mutations in Living damage."""
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
SPEC = importlib.util.spec_from_file_location('living_replay', HERE / 'run_native.py')
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)
JAR = None


def text(values):
    return '\n'.join('LIVING_DAMAGE ' + json.dumps(row) for row in values)


class LivingPacket(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_json(HERE / 'finite.json')
        cls.pins = read_json(HERE / 'source-pins.json')
        cls.baseline = json.dumps(cls.packet, sort_keys=True)
        cls.expected = REPLAY.expected_rows(cls.packet)
        cls.native = copy.deepcopy(cls.expected)
        cls.native[19].update(result=None, exception='UNIMPLEMENTED', completion=None)
        cls.native[21]['exception'] = 'IO'

    def tearDown(self):
        self.assertEqual(json.dumps(self.packet, sort_keys=True), self.baseline)
        self.assertEqual(REPLAY.expected_rows(self.packet), self.expected)

    def test_unchanged_baseline_and_explicit_scope_pairs(self):
        self.assertEqual(len(rows(text(self.expected))), 27)
        self.assertEqual(len(rows(text(self.native), native=True)), 27)
        REPLAY.compare(self.native, self.expected)
        self.assertNotEqual(self.native[19]['exception'], self.expected[19]['exception'])
        self.assertNotEqual(self.native[21]['exception'], self.expected[21]['exception'])

    def test_count_identity_schema_and_type_mutations(self):
        for value in (self.native[:-1], [self.native[1], self.native[0], *self.native[2:]]):
            with self.assertRaises(AssertionError):
                rows(text(value), native=True)
        for key, value in (('case', False), ('profile', True), ('amount', True), ('step', 1),
                           ('exception', 'NPE'), ('scope', 'DEATH_ENTRY'), ('result', 1)):
            changed = copy.deepcopy(self.native)
            changed[0][key] = value
            with self.assertRaises(AssertionError):
                rows(text(changed), native=True)
        changed = copy.deepcopy(self.native)
        changed[0]['input'][0] = False
        with self.assertRaises(AssertionError):
            rows(text(changed), native=True)

    def test_exact_primitive_and_random_comparison(self):
        for key in ('random', 'math_random', 'world_random', 'source_random', 'yaw', 'animation',
                    'velocity', 'position', 'bounds', 'health', 'source_cooldown', 'attacked', 'dying'):
            changed = copy.deepcopy(self.native)
            value = changed[0]['state'][key]
            if type(value) is str:
                changed[0]['state'][key] = f'{int(value,16) ^ 1:0{len(value)}x}'
            elif type(value) is bool:
                changed[0]['state'][key] = not value
            elif type(value) is int:
                changed[0]['state'][key] += 1
            elif type(value[0]) is int:
                value[0] += 1
            else:
                value[0] = f'{int(value[0],16) ^ 1:0{len(value[0])}x}'
            rows(text(changed), native=True)
            with self.assertRaisesRegex(AssertionError, 'case0 field state'):
                REPLAY.compare(changed, self.expected)

    def test_sound_and_owner_mutations(self):
        for key in ('volume', 'pitch', 'position'):
            changed = copy.deepcopy(self.native)
            sound = changed[0]['state']['sounds'][0]
            if key == 'position':
                sound[key][0] = f'{int(sound[key][0],16)^1:016x}'
            else:
                sound[key] = f'{int(sound[key],16)^1:08x}'
            with self.assertRaisesRegex(AssertionError, 'case0 field state'):
                REPLAY.compare(rows(text(changed), native=True), self.expected)
        changed = copy.deepcopy(self.native)
        changed[0]['state']['source_unchanged'] = False
        with self.assertRaisesRegex(AssertionError, 'retained source owner'):
            rows(text(changed), native=True)

    def test_death_checkpoint_and_distinct_failure_categories(self):
        for index, key, value in ((19, 'exception', 'NONE'), (19, 'result', True),
                                  (19, 'completion', self.expected[19]['completion']),
                                  (21, 'exception', 'CALLBACK')):
            changed = copy.deepcopy(self.native)
            changed[index][key] = value
            with self.assertRaises(AssertionError):
                rows(text(changed), native=True)
        changed = copy.deepcopy(self.native)
        changed[19]['state']['dying'] = True
        with self.assertRaises(AssertionError):
            REPLAY.compare(rows(text(changed), native=True), self.expected)
        changed = copy.deepcopy(self.expected)
        changed[19]['completion']['state']['dying'] = False
        with self.assertRaisesRegex(AssertionError, 'death completion'):
            REPLAY.compare(self.native, changed)

    def test_capture_pins_duplicate_keys_and_boolean_coercion(self):
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

    def test_original_archive_and_corrupt_code_identity(self):
        if JAR is None:
            self.skipTest('local original archive not supplied')
        verify_pins(JAR, self.pins)
        for key, value in (('code_bytes', True), ('code_sha256', '0' * 64)):
            changed = copy.deepcopy(self.pins)
            changed['classes']['ls']['methods'][0][key] = value
            with self.assertRaisesRegex(AssertionError, 'class/method identity'):
                verify_pins(JAR, changed)
        changed = copy.deepcopy(self.pins)
        changed['classes']['dl']['methods'].pop()
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
