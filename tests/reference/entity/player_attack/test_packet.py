"""Corruption controls for the selected original Player attack comparison."""
import copy
import json
from pathlib import Path
import unittest
from compare import compare, rows


class PlayerAttackPacket(unittest.TestCase):
    def setUp(self):
        self.expected = json.loads(Path(__file__).with_name('finite.json').read_text())['rows']
        self.text = '\n'.join('PLAYER_ATTACK ' + ' '.join(f'{k}={v}' for k, v in row.items())
                              for row in self.expected)

    def test_selected_state_and_inputs_unchanged(self):
        before = copy.deepcopy(self.expected)
        compare(rows(self.text), self.expected)
        self.assertEqual(self.expected, before)

    def test_each_semantic_integer_is_compared(self):
        for i, row in enumerate(self.expected):
            for key, value in row.items():
                if type(value) is int:
                    changed = copy.deepcopy(self.expected)
                    changed[i][key] += 1
                    with self.subTest(profile=i, field=key), self.assertRaises(ValueError):
                        compare(changed, self.expected)

    def test_boolean_is_not_an_integer_observation(self):
        for which in (0, 1):
            values = [copy.deepcopy(self.expected), copy.deepcopy(self.expected)]
            values[which][0]['dying'] = False
            with self.assertRaises(ValueError):
                compare(*values)

    def test_repeated_missing_and_reordered_profiles(self):
        lines = self.text.splitlines()
        for changed in (lines[:-1], lines + lines[:1], list(reversed(lines))):
            with self.assertRaises(ValueError):
                rows('\n'.join(changed))

    def test_duplicate_extra_and_missing_fields(self):
        for changed in (self.text.replace('health=9', 'health=9 health=9', 1),
                        self.text.replace('health=9', 'health=9 extra=0', 1),
                        self.text.replace(' health=9', '', 1)):
            with self.assertRaises(ValueError):
                rows(changed)

    def test_malformed_integer_spellings(self):
        for value in ('True', '1.0', '+9', '09', '-0'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                rows(self.text.replace('health=9', 'health=' + value, 1))


if __name__ == '__main__':
    unittest.main()
