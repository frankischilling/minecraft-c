"""Independent row, replay and immutable-source controls for the plant packet."""
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
from capture_reference import read_json, rows, strict_equal, verify_pins
REPLAY_SPEC = importlib.util.spec_from_file_location('plant_replay', HERE / 'run_native.py')
REPLAY = importlib.util.module_from_spec(REPLAY_SPEC)
REPLAY_SPEC.loader.exec_module(REPLAY)
compare, expected_rows = REPLAY.compare, REPLAY.expected_rows

JAR = None


class PlantPacket(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.packet = read_json(HERE / 'finite.json')
        cls.pins = read_json(HERE / 'source-pins.json')
        cls.baseline = json.dumps(cls.packet, sort_keys=True)
        cls.expected = expected_rows(cls.packet)

    def tearDown(self):
        self.assertEqual(json.dumps(self.packet, sort_keys=True), self.baseline)
        self.assertEqual(expected_rows(self.packet), self.expected)

    def test_unchanged_baseline(self):
        self.assertEqual(len(self.expected), 125)
        compare(self.expected, self.expected)

    def test_row_identity_and_extent_mutations(self):
        mutations = []
        dropped = self.expected[:-1]
        mutations.append(dropped)
        reordered = list(self.expected)
        reordered[0], reordered[1] = reordered[1], reordered[0]
        mutations.append(reordered)
        for cell, value in ((2, '31'), (70, 'NPE'), (71, '191'), (72, '1234567')):
            changed = list(self.expected)
            cells = changed[0].split()
            cells[cell] = value
            changed[0] = ' '.join(cells)
            mutations.append(changed)
        null_backing = list(self.expected)
        null_backing[118] += ' 00000000'
        mutations.append(null_backing)
        for changed in mutations:
            with self.subTest(changed=changed[0][:75]):
                with self.assertRaises(AssertionError):
                    rows('\n'.join(changed))

    def test_semantic_words_and_retained_fields_are_compared(self):
        # Independent one-bit changes to light/color scratch, Block bound,
        # current UV, a vertex position/UV, and untouched normal/padding words.
        for cell in (8, 36, 48, 58, 72, 75, 78, 79):
            with self.subTest(cell=cell):
                changed = list(self.expected)
                cells = changed[0].split()
                width = len(cells[cell])
                cells[cell] = f'{int(cells[cell],16) ^ 1:0{width}x}'
                changed[0] = ' '.join(cells)
                self.assertEqual(rows('\n'.join(changed)), changed)
                with self.assertRaisesRegex(AssertionError, f'row0 cell{cell} differs'):
                    compare(changed, self.expected)
                compare(self.expected, self.expected)

    def test_capture_input_identity_mutations(self):
        for key in self.packet['source_sha256']:
            changed = copy.deepcopy(self.packet)
            changed['source_sha256'][key] = '0' * 64
            with self.assertRaisesRegex(AssertionError, 'capture source pins changed'):
                expected_rows(changed)
        for changed in ({'rows': self.expected}, {**self.packet, 'extra': True},
                        {**self.packet, 'rows': [True, *self.expected[1:]]}):
            with self.assertRaisesRegex(AssertionError, 'packet schema'):
                expected_rows(changed)

    def test_duplicate_metadata_rejected(self):
        spec = importlib.util.spec_from_file_location('plant_metadata', HERE / 'capture_reference.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with self.assertRaisesRegex(AssertionError, 'duplicate metadata key'):
            json.loads('{"rows":[],"rows":[]}', object_pairs_hook=module.unique_object)

    def test_numeric_boolean_identity_is_distinct(self):
        self.assertFalse(strict_equal({'code_bytes': 1}, {'code_bytes': True}))
        self.assertFalse(strict_equal([0], [False]))

    def test_original_archive_and_changed_identity_pins(self):
        if JAR is None:
            self.skipTest('local original archive not supplied')
        verify_pins(JAR, self.pins)
        changes = []
        for field in ('class_sha256', 'class_bytes'):
            changed = copy.deepcopy(self.pins)
            changed['classes']['cv'][field] = '0' * 64 if field.endswith('sha256') else True
            changes.append(changed)
        for key in ('code_sha256', 'code_bytes'):
            changed = copy.deepcopy(self.pins)
            changed['classes']['cv']['methods'][0][key] = '0' * 64 if key.endswith('sha256') else True
            changes.append(changed)
        changed = copy.deepcopy(self.pins)
        changed['classes']['cv']['methods'].pop()
        changes.append(changed)
        for changed in changes:
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
